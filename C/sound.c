/* sound.c - MMbg's sounds (PLAY BBC SOUND / ENVELOPE on the noise channel,
   as GuSnd plays them), a tone, the volume and RBOX.  Needs OPTION AUDIO;
   without it the first PLAY fails and pm_try says why. */
#include <stdio.h>
#include <picomite/system.h>
#include <picomite/basic.h>
#include <picomite/graphics.h>
#include <picomite/audio.h>

static void sounds(void *p)
{
    static const int env2[14] = {2, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 12, 12};
    static const int env3[14] = {3, 1, 0, 0, 0, 0, 0, 0, 127, -40, 0, -127, 81, 0};
    char what[16];
    int i;
    (void)p;
    pm_play_bbc_envelope(env2);
    pm_play_bbc_envelope(env3);
    printf("dice\n");
    pm_play_bbc_sound(0x11, 0, 20, 30);
    for (i = 1; i <= 4; i++)
    {
        pm_play_bbc_sound(i == 1 ? 0x10 : 0, -i * 3, 7, 0);
        pm_play_bbc_sound(0, 0, 7, 1 + (i > 2));
    }
    printf("a hit (envelope 3)\n");
    pm_play_bbc_sound(0x10, 3, 6, 0);
    printf("playing: %s\n", pm_sound(what, sizeof what));
    pm_pause(1500);
    pm_play_stop();
    printf("tone 440/660 Hz, louder and quieter\n");
    pm_play_tone(440, 660, 1500);
    for (i = 100; i >= 20; i -= 20)
        pm_play_volume(i, PM_DEFAULT), pm_pause(300);
    pm_play_volume(100, 100);
    pm_play_stop();
}

int main(void)
{
    int w = pm_hres(), h = pm_vres();
    pm_cls(PM_BLACK);
    pm_draw_rbox(w / 4, h / 4, w / 2, h / 2, PM_DEFAULT, PM_WHITE, PM_BLUE);
    pm_draw_rbox(w / 4 + 20, h / 4 + 20, w / 2 - 40, h / 2 - 40, 30, PM_YELLOW, PM_DEFAULT);
    pm_text(w / 2, h / 2, "RBOX", "CM", PM_DEFAULT, PM_DEFAULT, PM_WHITE, PM_TRANSPARENT);
    if (pm_try(sounds, NULL))
        printf("no sound: %s\n", pm_errmsg());
    return 0;
}
