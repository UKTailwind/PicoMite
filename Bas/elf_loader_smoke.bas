' Compile elf_loader_smoke.c to elf_loader_smoke.o using the external compiler.
' Put the .o next to this .bas before loading/saving to PicoMite.
CSUB Hello
  "elf_loader_smoke.o"
END CSUB
Hello
