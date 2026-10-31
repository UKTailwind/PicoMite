/* Before running: SETPIN GPx, PWM0A (choose a supported PWM0A GPIO).
   Sweep the duty cycle of PWM slice 0, output A. */
#include <picomite/pwm.h>
#include <picomite.h>
#include <stdio.h>

int main(void)
{
    pm_pwm_open(0, 1000, 0.0, -1.0);
    for (int duty = 0; duty <= 100; duty += 10)
    {
        pm_pwm_set_duty(0, (double)duty, -1.0);
        printf("PWM0A: %d%%\n", duty);
        pm_pause(100);
    }
    /* PWM remains running at the last duty, as with BASIC PWM. */
    return 0;
}
