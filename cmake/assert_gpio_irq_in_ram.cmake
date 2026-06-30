# Fail-loud guard for the gpio_default_irq_handler RAM relocation.
#
# relocate_gpio_irq_to_ram.cmake depends on the SDK keeping the function named
# gpio_default_irq_handler in its own .text.* section. If a future SDK renames,
# inlines or restructures it, the objcopy rename silently no-ops and the handler
# quietly falls back to flash - a timing regression with no other symptom. This
# parses the link map and aborts the build if the handler is not in RAM.
#
# Required -D arg: MAP_FILE (path to the linker .map).

if(NOT EXISTS "${MAP_FILE}")
    message(FATAL_ERROR "assert_gpio_irq_in_ram: map file not found: ${MAP_FILE}")
endif()

file(READ "${MAP_FILE}" _map)

# Match the section name (.text.* before relocation, .time_critical.* after)
# followed by the load address on the next map line.
string(REGEX MATCH
    "\\.(text|time_critical)\\.gpio_default_irq_handler[ \t\r\n]+0x([0-9a-fA-F]+)"
    _hit "${_map}")
if(NOT _hit)
    message(FATAL_ERROR
        "assert_gpio_irq_in_ram: gpio_default_irq_handler not found in ${MAP_FILE}")
endif()

set(_addr "${CMAKE_MATCH_2}")
string(SUBSTRING "${_addr}" 0 2 _hi)   # 0x20.. = RAM, 0x10.. = flash (XIP)
if(_hi STREQUAL "20")
    message(STATUS "gpio_default_irq_handler is in RAM (0x${_addr}) - OK")
else()
    message(FATAL_ERROR
        "gpio_default_irq_handler is in flash (0x${_addr}), not RAM - the SDK "
        "section rename in relocate_gpio_irq_to_ram.cmake did not take. GPIO "
        "interrupt timing (IR/COUNT/FREQ/PS2) would regress.")
endif()
