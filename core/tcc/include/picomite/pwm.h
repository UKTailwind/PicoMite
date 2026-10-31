/* PicoMite TinyCC PWM (API 9); see docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_PWM_H
#define _PICOMITE_PWM_H

void pm_pwm_open(int slice, unsigned frequency_hz, double duty_a, double duty_b);
void pm_pwm_set_duty(int slice, double duty_a, double duty_b);
void pm_pwm_set_frequency(int slice, unsigned frequency_hz);
void pm_pwm_close(int slice);

#endif
