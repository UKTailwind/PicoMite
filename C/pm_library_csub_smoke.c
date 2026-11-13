/* External ELF CSUB / on-device TinyCC library-loading test.
 * Build as a relocatable ARMv6-M/soft-float ET_REL object for RP2040/RP2350.
 */
extern void pm_print(const char *);
void LibraryProbe(void)
{
    pm_print("LIBRARY C/ELF CSUB OK\r\n");
}
