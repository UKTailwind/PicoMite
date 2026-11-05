#ifndef _PICOMITE_GUI_H
#define _PICOMITE_GUI_H

#define PM_GUI_DOWN 1
#define PM_GUI_UP 2

typedef void (*pm_gui_callback)(int kind, int x, int y, int ref);

int pm_touch(const char *key);
int pm_click(const char *key);
void pm_gui(const char *subcommand);

int pm_ctrlval_get_number(int ref, double *out);
int pm_ctrlval_set_number(int ref, double value);
int pm_ctrlval_get_text(int ref, char *buffer, unsigned size);
int pm_ctrlval_set_text(int ref, const char *value);

void pm_gui_on_event(pm_gui_callback callback);
void pm_gui_poll(void);

#endif
