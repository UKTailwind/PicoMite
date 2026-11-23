/* app.ld: a loadable program for the PicoMite (tools/sdk). Linked as one block at __app_base (0x11000000, the bottom
   of the PSRAM, where the loader usually puts it, unless build.py --base says otherwise; elsewhere it is moved by its
   relocations) and loaded as one: code, constants, the init/fini arrays and data, the SRAM part's bytes if it has
   one, then .bss, which the loader zeroes. mkimage.py makes the file from the ELF this gives with --emit-relocs. */
ENTRY(_start)

SECTIONS
{
    . = DEFINED(__app_base) ? __app_base : 0x11000000;
    __image_start = .;
    .text : {
        KEEP(*(.text.entry))                    /* crt0.c's _start */
        *(.text .text.* .gnu.linkonce.t.*)
        *(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
    }
    .rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) }
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    }
    .preinit_array : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    }
    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    }
    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    }
    .data : {
        . = ALIGN(4);
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(4);
    }
    /* the SRAM part (the sources' .sram, .sram.* and .sram_bss sections, if any): linked at __sram_link (0x20000000,
       PMIMG_SRAM_LINK, from where the loader moves it by the relocations to the block of the SRAM heap it takes for
       the run), its bytes stored in the image after the rest (from __sram_load), which the loader copies there,
       zeroing .sram_bss after them. A call between the two parts goes through a veneer of the linker's (mkimage.py
       relocates it); any other PC-relative reference between them stops mkimage.py. .bss is placed past those bytes
       first, so that the location counter only ever moves on */
    __sram_load = ALIGN(8);
    __image_end = __sram_load + SIZEOF(.sram);
    .bss ALIGN(__image_end, 8) (NOLOAD) : {
        . = ALIGN(8);
        __bss_start__ = .;
        *(.bss .bss.* .gnu.linkonce.b.* COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    }
    end = .;
    .sram (DEFINED(__sram_link) ? __sram_link : 0x20000000) : AT(__sram_load) {
        __sram_start = .;
        *(.sram .sram.*)
        . = ALIGN(8);
    }
    .sram_bss (NOLOAD) : {
        *(.sram_bss .sram_bss.*)
        . = ALIGN(8);
        __sram_end = .;
    }
    /DISCARD/ : { *(.comment) *(.note*) }
}
