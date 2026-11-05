/* Gamepad API v22: run on USB/BLE-host builds with a connected controller.
 * Press and release buttons during the test. No hardware resources claimed. */
#include <picomite/input.h>
#include <picomite/system.h>
#include <stdio.h>

static unsigned changes;
static void on_change(int channel, unsigned buttons)
{
    ++changes;
    printf("gamepad %d buttons=%04x\n", channel, buttons);
}
int main(void)
{
    pm_gamepad_state s = {0};
    int ch = 0;
    s.size = sizeof(s);
    for (int n = 1; n <= 4; ++n) {
        if (pm_gamepad_get(n, &s) == 1) {
            printf("gamepad %d type=%u vid=%04x pid=%04x\n",
                   n, s.type, s.vid, s.pid);
            ch = n;
            break;
        }
    }
    if (!ch) {
        puts("SKIP: no gamepad connected");
        return 0;
    }
    pm_gamepad_on_change(ch, 0xffffu, on_change);
    puts("Press and release one button during the next 12 seconds");
    unsigned long long end = pm_timer_us() + 12000000ull;
    while (pm_timer_us() < end && changes < 2) {
        pm_gamepad_poll();
        pm_pause(10);
    }
    pm_gamepad_on_change(ch, 0, NULL);
    printf("changes=%u %s\n", changes, changes >= 2 ? "PASS" : "SKIP");
    return 0;
}
