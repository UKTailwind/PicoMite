#include <stdio.h>
#include <picomite/pio.h>
int main(void)
{
    for (int i = 0; i < 3; ++i)
        if (pm_pio_available(i))
            printf("PIO%d: available; SM0 FIFO: TX=%lu RX=%lu\n", i,
                   (unsigned long)pm_pio_flevel(i, 0, 1),
                   (unsigned long)pm_pio_flevel(i, 0, 0));
        else
            printf("PIO%d: reserved\n", i);
    return 0;
}
