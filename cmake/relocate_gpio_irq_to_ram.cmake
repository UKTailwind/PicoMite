# Relocate the Pico SDK's GPIO IRQ dispatcher into RAM without forking the SDK.
#
# The SDK's shared GPIO interrupt handler, gpio_default_irq_handler() in
# hardware_gpio/gpio.c, is the function that runs from the IO_IRQ_BANK0 vector
# and dispatches to the user callback. By default it lives in flash, so every
# GPIO interrupt pays XIP latency before reaching PicoMite's callback - which
# is timing-critical for IR, COUNT/FREQ, PS2 etc. PicoMite's own callback is
# already RAM-resident (__not_in_flash_func); this puts the dispatcher there
# too, with no change to the callback or its registration.
#
# Mechanism: the SDK is compiled with -ffunction-sections, so the handler sits
# in its own input section ".text.gpio_default_irq_handler". We rename that
# section to ".time_critical.gpio_default_irq_handler" in the compiled object.
# The SDK linker script already globs *(.time_critical*) into the RAM region
# (loaded from flash, copied at boot), so the existing copy machinery does the
# rest - exactly as if the source used __not_in_flash_func.
#
# Invoked as a PRE_LINK step. Idempotent: on an incremental build where gpio.c
# was not recompiled the section is already renamed, and objcopy's rename of a
# non-existent section is a silent no-op.
#
# Required -D args: OBJ_ROOT (CMakeFiles/<target>.dir), OBJCOPY (path).

if(NOT OBJCOPY)
    message(FATAL_ERROR "relocate_gpio_irq_to_ram: OBJCOPY not provided")
endif()

file(GLOB_RECURSE _all "${OBJ_ROOT}/*.obj")
set(_objs "")
foreach(_o IN LISTS _all)
    if(_o MATCHES "hardware_gpio/gpio\\.c\\.obj$")
        list(APPEND _objs "${_o}")
    endif()
endforeach()
if(NOT _objs)
    message(FATAL_ERROR
        "relocate_gpio_irq_to_ram: no hardware_gpio/gpio.c.obj found under ${OBJ_ROOT}")
endif()

foreach(_o IN LISTS _objs)
    execute_process(
        COMMAND "${OBJCOPY}" --rename-section
            .text.gpio_default_irq_handler=.time_critical.gpio_default_irq_handler
            "${_o}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "relocate_gpio_irq_to_ram: objcopy failed (${_rc}) on ${_o}")
    endif()
endforeach()
