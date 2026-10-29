/* Added to the link of PicoMite.elf (CMakeLists.txt): the firmware must end
   before FLASH_TARGET_OFFSET (configuration.h, exported by misc/FileIO.c as
   __pm_flash_target_offset), where the flash storage starts. */
ASSERT(__flash_binary_end <= ORIGIN(FLASH) + __pm_flash_target_offset,
       "The firmware runs into the flash storage: raise FLASH_TARGET_OFFSET in configuration.h")
