/* TinyCC: <picomite/audio.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_AUDIO_H
#define _PICOMITE_AUDIO_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

void pm_play_modfile(const char *file);
void pm_play_modsample(int sample, int channel, int volume);

void pm_play_sound(int channel, const char *position, const char *type, double frequency, int volume);
void pm_play_stop(void);

void pm_play_tone(double left, double right, int duration);
void pm_play_wav(const char *file);
void pm_play_flac(const char *file);
void pm_play_mp3(const char *file);
void pm_play_midifile(const char *file);
void pm_play_pause(void);
void pm_play_resume(void);
void pm_play_next(void);
void pm_play_previous(void);
void pm_play_close(void);
void pm_play_volume(int left, int right);

void pm_play_bbc_sound(int channel, int amplitude, int pitch, int duration);
void pm_play_bbc_envelope(const int e[14]);
char *pm_sound(char *buf, int size);

#endif
