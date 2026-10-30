/* PicoMite TinyCC: <picomite/audio.h> - PLAY.  (API version 1.)
   Each function is the PLAY subcommand of its name, its arguments in their
   BASIC order, checked the same way and failing with the same MMBasic
   errors.  An argument BASIC lets you leave out is PM_DEFAULT
   (<picomite/graphics.h>). */
#ifndef _PICOMITE_AUDIO_H
#define _PICOMITE_AUDIO_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

void pm_play_modfile(const char *file);                     /* PLAY MODFILE file$ */
void pm_play_modsample(int sample, int channel, int volume); /* PLAY MODSAMPLE sample, channel [, volume] */
/* PLAY SOUND channel, position, type, frequency [, volume]: position "L",
   "R" or "B", type "S" "Q" "T" "W" "O" "P" "N" (as PLAY SOUND) */
void pm_play_sound(int channel, const char *position, const char *type, double frequency, int volume);
void pm_play_stop(void);                                    /* PLAY STOP */

/* (API version 4) */
void pm_play_tone(double left, double right, int duration); /* PLAY TONE left, right [, ms] */
void pm_play_wav(const char *file);                          /* PLAY WAV file$ (or a directory) */
void pm_play_flac(const char *file);                         /* PLAY FLAC file$ */
void pm_play_mp3(const char *file);                          /* PLAY MP3 file$ */
void pm_play_midifile(const char *file);                     /* PLAY MIDIFILE file$ */
void pm_play_pause(void);                                    /* PLAY PAUSE */
void pm_play_resume(void);                                   /* PLAY RESUME */
void pm_play_next(void);                                     /* PLAY NEXT */
void pm_play_previous(void);                                 /* PLAY PREVIOUS */
void pm_play_close(void);                                    /* PLAY CLOSE */
void pm_play_volume(int left, int right);                    /* PLAY VOLUME left [, right] (0..100) */
/* PLAY BBC SOUND channel, amplitude, pitch, duration and PLAY BBC ENVELOPE
   n, T, PI1, PI2, PI3, PN1, PN2, PN3, AA, AD, AS, AR, ALA, ALD (e[0..13]):
   the BBC Micro's SOUND and ENVELOPE */
void pm_play_bbc_sound(int channel, int amplitude, int pitch, int duration);
void pm_play_bbc_envelope(const int e[14]);
char *pm_sound(char *buf, int size);                         /* MM.INFO(SOUND): "OFF", "WAV", ... */

#endif /* _PICOMITE_AUDIO_H */
