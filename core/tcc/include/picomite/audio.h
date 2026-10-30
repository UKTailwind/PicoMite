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

#endif /* _PICOMITE_AUDIO_H */
