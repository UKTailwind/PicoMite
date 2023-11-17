/***********************************************************************************************************************
PicoMite MMBasic

Memory.c

<COPYRIGHT HOLDERS>  E. David Neufeld
Copyright (c) 2023, <COPYRIGHT HOLDERS> All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following
conditions are met: 1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the
following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
disclaimer in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original
copyright message be displayed on the console at startup (additional copyright messages may be added). 4.	All advertising
materials mentioning features or use of this software must display the following acknowledgement: This product includes software
developed by the <copyright holder>. 5.	Neither the name of the <copyright holder> nor the names of its contributors may be used
to endorse or promote products derived from this software without specific prior written permission. THIS SOFTWARE IS PROVIDED BY
<COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************************************************************************************/

#define INCLUDE_FUNCTION_DEFINES

#include "Hardware_Includes.h"
#include "MMBasic_Includes.h"
#include "Debugger.h"
#include "class/cdc/cdc_device.h"

#ifdef PICOMITEVGA
#endif

#ifdef PICOMITE
#endif

#ifdef PICOMITEWEB
#endif

//
//
// The user needs to do > Option Display 45,145
//                      > Option Colourcode On
//
// prior to using this debugger.....
//
//
extern int
    debugger_scrn_x_column; // Actually not the 'size'  Instead, it is the column position for the start of the debugger area
extern int debugger_scrn_y_size;
extern int Mouse_X, Mouse_Y;

char TeraTerm_active = 0;

void printLine(int);
void SetColour(unsigned char *, int);
char *findLine(int);

unsigned char *LocateProgramStatement(int line) {
   unsigned char *p, *previous_stmt;
   int cnt = 0;

   previous_stmt = p = ProgMemory;

   while (1) {
      if (*p == 0xff || (p[0] == 0 && p[1] == 0)) // end of the program
         return NULL;

      if (*p == T_NEWLINE) {
         previous_stmt = p;
         p++; // and step over the new line marker
         cnt++;
         if (cnt >= line)
            return previous_stmt;
         continue;
      }
      if (*p == T_LINENBR) {
         p += 3; // and step over the line number
         continue;
      }
      if (*p == T_LABEL) {
         p += p[0] + 2; // still looking! skip over the label
         continue;
      }
      p++;
   }
   return NULL;
}

short int dbg_next_chars[N_DEBUG_UNGETCHAR] = {[0 ... N_DEBUG_UNGETCHAR - 1] = -1};

void Debugger_unget_char(int c) {
   int i;

   for (i = N_DEBUG_UNGETCHAR - 2; i > 0; i--)
      dbg_next_chars[i + 1] = dbg_next_chars[i];

   dbg_next_chars[0] = c;
}

int Debugger_input_pending() {
   int c;

   if (dbg_next_chars[0] != -1)
      return true;

   c = tud_cdc_read_char();
   if (c == -1)     // if there is nothing in the 'unget' buffer and calling tud_cdc_read_char() returns
      return false; // nothing, we can tell the caller that no input is pending.

   Debugger_unget_char(c);

   return true;
}

int Debugger_get_char() {
   int c, i;

   if (dbg_next_chars[0] != -1) {
      c = dbg_next_chars[0];
      for (i = 0; i < N_DEBUG_UNGETCHAR - 2; i++)
         dbg_next_chars[i] = dbg_next_chars[i + 1];
      dbg_next_chars[N_DEBUG_UNGETCHAR - 1] = -1;

      return c;
   }
   c = tud_cdc_read_char();

   return c;
}

int Debugger_getc(void) {
   int c, c1, c2;

   while ((c = Debugger_get_char()) == -1)
      ;

   if (c != 0x1b) /* if not an escape sequence, we just return the character */
      return c;

   InkeyTimer = 0;
   while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 40) /* get 2nd character of sequence */
      ;
   if (c == -1)
      return -1;

   if (c != '[') // must be a square bracket
      return -1;

   while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 90)
      ; // get the third char with delay
   if (c == -1)
      return -1;

   if (c == 'A')
      return UP; // the arrow keys are three chars
   if (c == 'B')
      return DOWN;
   if (c == 'C')
      return RIGHT;
   if (c == 'D')
      return LEFT;

   if (c == 'M') { // We have a Mouse Cursor Position report coming!
      while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 120)
         ; // get the 4th char which should be a space

      if (TeraTerm_active == 0 && c == '#') {                          // The first time MMDebug sees ESC [ M # the remaining
         TeraTerm_active = 1;                                          // characters of the sequence need to be ate.   The reason
         while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 140)   // is the Mouse Click Down has already been seen and sent
            ;                                                          // to the command processor.   We don't want a double click on
         while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 160)   // what ever the user is pointing at.
            ;
         return -1;
      }

      if (TeraTerm_active == 1 && c == ' ') {                          // If TeraTerm has been detected, we don't alow the
         while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 140)   // mouse click down to be reported.   The reason is the
            ;                                                          // mouse click up will generate noise if the user has an
         while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 160)   // input statement active.
            ;       
         return -1;
      }

      while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 140)
         ; // get the 5th char which should be the X coordinate
      if (c == -1)
         return -1;
      Mouse_X = c - 32;

      while ((c = tud_cdc_read_char()) == -1 && InkeyTimer < 160)
         ; // get the 6th char which should be the Y coordinate
      if (c == -1)
         return -1;
      Mouse_Y = c - 32;

      //----------------------------------------------------------------------------------------------
      //------ Check if any of the command buttons have been clicked on the Prompt Line       --------
      //------ at the bottom of the screen.   If so, return the proper ascii code             --------
      //----------------------------------------------------------------------------------------------
      if (Mouse_Y >= debugger_scrn_y_size) // the user clicked too low on the screen.
         return MOUSE_CLICK;               // out of the debug window

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_PAGE_UP) { // check if it can be a Page-Up
         if (Mouse_Y >= debugger_scrn_y_size - 5 && Mouse_Y <= debugger_scrn_y_size - 4)
            return PUP;
      }

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_PAGE_DOWN) { // check if it can be a Page-Down
         if (Mouse_Y >= debugger_scrn_y_size - 2 && Mouse_Y <= debugger_scrn_y_size - 1)
            return PDOWN;
      }

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_LINE_UP) { // check if it can be a Line-Up
         if (Mouse_Y >= debugger_scrn_y_size - 5 && Mouse_Y <= debugger_scrn_y_size - 4)
            return UP;
      }

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_LINE_DOWN) { // check if it can be a Line-Down
         if (Mouse_Y >= debugger_scrn_y_size - 2 && Mouse_Y <= debugger_scrn_y_size - 1)
            return DOWN;
      }

      //----------------------------------------------------------------------------------------------
      //------ Done checking for Page Up/Down buttons and Line Up/Down buttons                --------
      //------                                                                                --------
      //------ Now check for Step buttons                                                     --------
      //----------------------------------------------------------------------------------------------

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_STEP &&
          Mouse_X <= DBG_X_COLUMN + DBG_BUTTON_STEP + 5) {      // check if it can be a Step
         if (Mouse_Y >= debugger_scrn_y_size - 2 && Mouse_Y <= debugger_scrn_y_size - 1)
            return STEP;
      }

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_STEP_OVER &&
          Mouse_X <= DBG_X_COLUMN + DBG_BUTTON_STEP_OVER + 5) { // check if it can be a Step-Over
         if (Mouse_Y >= debugger_scrn_y_size - 2 && Mouse_Y <= debugger_scrn_y_size - 1)
            return STEP_OVER;
      }

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_STEP_OUT &&
          Mouse_X <= DBG_X_COLUMN + DBG_BUTTON_STEP_OUT + 5) {  // check if it can be a Step-Out
         if (Mouse_Y >= debugger_scrn_y_size - 2 && Mouse_Y <= debugger_scrn_y_size - 1)
            return STEP_OUT;
      }

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_GO &&
          Mouse_X <= DBG_X_COLUMN + DBG_BUTTON_GO + 4) {        // check if it can be a Go command
         if (Mouse_Y >= debugger_scrn_y_size - 2 && Mouse_Y <= debugger_scrn_y_size - 1)
            return GO;
      }

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_QUIT &&
	  Mouse_X <= DBG_X_COLUMN + DBG_BUTTON_QUIT + 4) {      // check if it can be a Quit command
         if (Mouse_Y >= debugger_scrn_y_size - 2 && Mouse_Y <= debugger_scrn_y_size - 1)
            return QUIT;
      }

      //---------------------------------------------------------------------------------------------------------------------
      //------ Done checking for Step commands.  Now check for Display-Variable and 'Set Break Point & Go' command   --------
      //---------------------------------------------------------------------------------------------------------------------

      if (Mouse_X >= DBG_X_COLUMN + DBG_BUTTON_EDIT_VALUE &&
          Mouse_X <= DBG_X_COLUMN + DBG_BUTTON_EDIT_VALUE + 6) { // check if it can be a Edit Value
         if (Mouse_Y >= debugger_scrn_y_size - 2 && Mouse_Y <= debugger_scrn_y_size - 1)
            return EDIT_VALUE;
      }

      if (Mouse_X >= DBG_X_COLUMN && Mouse_X <= DBG_X_COLUMN + 5) {
         if (Mouse_Y >= 1 && Mouse_Y <= debugger_scrn_y_size - 5)
            return BRK_PNT_and_GO;
      }

      return MOUSE_CLICK;
   }

   if (c < '1' && c > '6') // 3rd character must be ascii 1 to 6
      return -1;

   while ((c1 = tud_cdc_read_char()) == -1 && InkeyTimer < 100) // get the fourth char
      ; 
   if (c1 == -1)
      return -1;

   if (c1 == '~') { // all 4 char codes must be terminated with ~
      if (c == '1')
         return HOME;
      if (c == '2')
         return INSERT;
      if (c == '3')
         return DEL;
      if (c == '4')
         return END;
      if (c == '5')
         return PUP;
      if (c == '6')
         return PDOWN;
      return -1;
   }

   while ((c2 = tud_cdc_read_char()) == -1 && InkeyTimer < 110) // get the fifth char
      ;
   if (c2 == -1)
      return -1;

   if (c2 == '~') { // must be a ~
      if (c == '1') {
         if (c1 >= '1' && c1 <= '5')
            return F1 + (c1 - '1'); // F1 to F5
         if (c1 >= '7' && c1 <= '9')
            return F6 + (c1 - '7'); // F6 to F8
      }
      if (c == '2') {
         if (c1 == '0' || c1 == '1')
            return F9 + (c1 - '0'); // F9  or F10
         if (c1 == '3' || c1 == '4')
            return F11 + (c1 - '3'); // F11 or F12
      }
   }
   return -1;
}

void Debugger_getline(char *buff) {
   int c;
   char *ptr;

   ptr = buff;
   *ptr = 0;

   while (1) {
      while ((c = Debugger_getc()) == -1) // wait for a character
         ;
      if (c == '\n' || c == '\r')
         return;

      if (c == MOUSE_CLICK) { // the user doesn't want to keep typing.
         ptr = buff;          // they clicked somewhere on the screen
         *ptr = MOUSE_CLICK;
         *ptr++ = 0;
         //			Debugger_unget_char( c);
         {
            char str[50];
            VT100_goto_xy(60, debugger_scrn_y_size);
            VT100_forground_red();
            VT100_background_yellow();
            MMPrintString(" (");
            IntToStr(str, Mouse_X, 10);
            MMPrintString(str);
            MMPrintString(",");
            IntToStr(str, Mouse_Y, 10);
            MMPrintString(str);
            MMPrintString(") ");
            VT100_background_black();
            VT100_forground_white();
            Debugger_Pause(1500);
         }

         return;
      }

      if (c == '\b') { // handle the backspace
         if (ptr > buff) {
            MMPrintString("\b \b");
            ptr--;
            *ptr = 0;
         }
         continue;
      }
      if ((ptr - buff) < 60) { // even an 60 character line is excessive for line size
         *ptr++ = c;           // that the debugger needs from the user.
         *ptr = 0;
         MMputchar(c, 1); // and echo the character so the user can see what they typed
      }
   }
}

void VT100_Clear_Debugger_Screen() {
   int i;

   VT100_forground_cyan();
   for (i = 1; i < debugger_scrn_y_size; i++) {
      VT100_goto_xy(debugger_scrn_x_column, i);
      VT100_Clear_to_EOL();
      MMPrintString("|");
   }

   VT100_goto_xy(debugger_scrn_x_column, debugger_scrn_y_size);

   for (i = 1; i < 145 - debugger_scrn_x_column + 2; i++) {
      MMPrintString("_");
   }

   VT100_forground_white();
}

void Debugger_Prompt() {

   VT100_forground_white();
   VT100_background_red();

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_LINE_UP, debugger_scrn_y_size - 5);
   MMPrintString(" Line ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_LINE_UP, debugger_scrn_y_size - 4);
   MMPrintString("  UP  ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_LINE_DOWN, debugger_scrn_y_size - 2);
   MMPrintString(" Line ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_LINE_DOWN, debugger_scrn_y_size - 1);
   MMPrintString(" DOWN ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_PAGE_UP, debugger_scrn_y_size - 5);
   MMPrintString(" Page ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_PAGE_UP, debugger_scrn_y_size - 4);
   MMPrintString("  UP  ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_PAGE_DOWN, debugger_scrn_y_size - 2);
   MMPrintString(" Page ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_PAGE_DOWN, debugger_scrn_y_size - 1);
   MMPrintString(" DOWN ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_STEP, debugger_scrn_y_size - 2);
   MMPrintString(" Step ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_STEP, debugger_scrn_y_size - 1);
   MMPrintString("      ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_STEP_OVER, debugger_scrn_y_size - 2);
   MMPrintString(" Step ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_STEP_OVER, debugger_scrn_y_size - 1);
   MMPrintString(" Over ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_STEP_OUT, debugger_scrn_y_size - 2);
   MMPrintString(" Step ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_STEP_OUT, debugger_scrn_y_size - 1);
   MMPrintString(" Out  ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_GO, debugger_scrn_y_size - 2);
   MMPrintString("    ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_GO, debugger_scrn_y_size - 1);
   MMPrintString(" GO ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_QUIT, debugger_scrn_y_size - 2);
   MMPrintString("      ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_QUIT, debugger_scrn_y_size - 1);
   MMPrintString(" Quit ");

   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_EDIT_VALUE, debugger_scrn_y_size - 2);
   MMPrintString(" Edit  ");
   VT100_goto_xy(debugger_scrn_x_column + DBG_BUTTON_EDIT_VALUE, debugger_scrn_y_size - 1);
   MMPrintString(" Value ");
}

void VT100_goto_xy(int x, int y) { // Move cursor to screen's (x,y) coordinates.   x & y are 1 based.
   char s[12];

   MMPrintString("\033[");
   IntToStr(s, y, 10);
   MMPrintString(s);
   MMPrintString(";");
   IntToStr(s, x, 10);
   MMPrintString(s);
   MMPrintString("H");
}

void setup_terminal_screen_to_145x45(void) {
   char tmp[30] = {0};
   MMPrintString("\033[8;");
   IntToStr(tmp, 45, 10);
   strcat(tmp, ";");
   MMPrintString(tmp);
   IntToStr(tmp, 145, 10);
   strcat(tmp, "t");
   MMPrintString(tmp);
}

void VT100_Mouse_Mode_On() { MMPrintString("\033[?1000h"); }
void VT100_Clear_Screen() { MMPrintString("\033[2J2J"); }
void VT100_Clear_to_EOL() { MMPrintString("\033[K"); } // clear to the end of the line on a vt100 emulator
void VT100_all_attr_off() { MMPrintString("\033[m"); }
void VT100_bold_on() { MMPrintString("\033[1m"); }
void VT100_low_intensity() { MMPrintString("\033[2m"); }
void VT100_underline_on() { MMPrintString("\033[4m"); }
void VT100_save_cursor() { MMPrintString("\0337"); }
void VT100_restore_cursor() { MMPrintString("\0338"); }
void VT100_forground_black() { MMPrintString("\033[30m"); }
void VT100_forground_red() { MMPrintString("\033[31m"); }
void VT100_forground_green() { MMPrintString("\033[32m"); }
void VT100_forground_yellow() { MMPrintString("\033[33m"); }
void VT100_forground_blue() { MMPrintString("\033[34m"); }
void VT100_forground_cyan() { MMPrintString("\033[36m"); }
void VT100_forground_magenta() { MMPrintString("\033[35m"); }
void VT100_forground_white() { MMPrintString("\033[37m"); }
void VT100_forground_grey() { MMPrintString("\033[90m"); }

void VT100_background_black() { MMPrintString("\033[40m"); }
void VT100_background_red() { MMPrintString("\033[41m"); }
void VT100_background_green() { MMPrintString("\033[42m"); }
void VT100_background_yellow() { MMPrintString("\033[43m"); }
void VT100_background_blue() { MMPrintString("\033[44m"); }
void VT100_background_cyan() { MMPrintString("\033[46m"); }
void VT100_background_magenta() { MMPrintString("\033[45m"); }
void VT100_background_white() { MMPrintString("\033[47m"); }
void VT100_background_grey() { MMPrintString("\033[100m"); }

void Debug_Info(char *title, int value) {
   char tmp[100];

   MMPrintString(title);
   IntToStr(tmp, value, 10);
   MMPrintString(tmp);
}

void Debug_Info_hex(char *title, int value) {
   char tmp[100];

   MMPrintString(title);
   IntToStr(tmp, value, 16);

   if (strlen(tmp) < 2)
      MMPrintString("0");

   MMPrintString(tmp);
}

void Debug_Info_char(char *title, unsigned char value) {
   char tmp[10];

   MMPrintString(title);
   tmp[0] = value;
   tmp[1] = 0;
   MMPrintString(tmp);
}

void Debugger_var_display() {
   int i, j, l, nl, avail, vi;
   union u_val *uv_ptr;
   unsigned char c;
   char disp[40];
   void *ptr;

   for (i = 0; i < n_disp_elements; i++) {
      VT100_goto_xy(DBG_VAR_COLUMN, i + 1);
      VT100_all_attr_off();
      VT100_Clear_to_EOL(); // first we put the variable's name in place
      VT100_forground_cyan();
      MMPrintString("|");
      VT100_forground_yellow();

      strcpy(disp, (char *)var_disp_list[i].name);
      if (strlen(disp) > 10) {
         VT100_underline_on(); // if we can't display the entire variable name, we underline it
         disp[11] = 0;         // so the user knows some information is missing
      }

      l = strlen((char *)var_disp_list[i].name);
      c = var_disp_list[i].name[l];

      if (c == '!')
         ptr = findvar(var_disp_list[i].name, V_NOFIND_NULL | T_IMPLIED | T_NBR);
      else if (c == '$')
         ptr = findvar(var_disp_list[i].name, V_NOFIND_NULL | T_IMPLIED | T_STR);
      else if (c == '%')
         ptr = findvar(var_disp_list[i].name, V_NOFIND_NULL | T_IMPLIED | T_INT);
      else

         ptr = findvar(var_disp_list[i].name, V_NOFIND_NULL);

      if (ptr == NULL) {
         for (j = i; j < n_disp_elements - 1; j++)
            var_disp_list[j] = var_disp_list[j + 1];
         n_disp_elements--;
         i--;
         continue;
      }

      vi = VarIndex;

      MMPrintString(disp);
      nl = strlen(disp);

      VT100_all_attr_off();
      VT100_goto_xy(DBG_VAR_VAL_COLUMN, i + 1); // now we attempt to display the value of the variable

      /*
                      if (ptr == NULL)  {
                              VT100_background_red();
                              MMPrintString("   ?   ");
                              VT100_background_black();
                      } else {
      */
      if (vartbl[vi].type & T_PTR)                  // Get a pointer to what ever value we are going
         uv_ptr = (union u_val *)vartbl[vi].val.ia; // to display.   If this is a ptr to another variable,
      else                                          // we dereference it before continueing.
         uv_ptr = &vartbl[vi].val;

      switch (vartbl[vi].type & (T_NOTYPE | T_STR | T_INT | T_NBR)) {
      case T_NBR:
         VT100_forground_green();
         if (var_disp_list[i].val.f != uv_ptr->f) { // check if the variable's value has changed
            VT100_forground_red();
            var_disp_list[i].val.f = uv_ptr->f;
         }
         FloatToStr(disp, uv_ptr->f, 5, 2, ' ');
         disp[10] = 0;
         MMPrintString(disp);
         break;

      case T_INT:
         VT100_forground_green();
         if (var_disp_list[i].val.i != uv_ptr->i) { // check if the variable's value has changed
            VT100_forground_red();
            var_disp_list[i].val.i = uv_ptr->i;
         }
         IntToStr(disp, uv_ptr->i, 10);
         VT100_goto_xy( 145 - 2 - strlen(disp) , i + 1); // line up integers so they are positioned the
         MMPrintString(disp);                            // same as the mantisa for real numbers
         break;

      case T_STR:
         VT100_forground_magenta();
         if (var_disp_list[i].val.s != uv_ptr->s) { // check if the storage area for the variable
            VT100_forground_red();                  // has changed.   Note that this doesn't necessarily
            var_disp_list[i].val.s = uv_ptr->s;     // mean the value has changed.  Just the storage
         }                                          // allocated to hold it has changed.
         strncpy(disp, (char *)&uv_ptr->s[1], 35);
         l = uv_ptr->s[0]; // chop off the string at an appropriate length
         if (l > sizeof(disp))
            l = sizeof(disp) - 1;
         disp[l] = 0;
         avail = 145 - (DBG_VAR_COLUMN + nl);
         if (l < avail) { // plenty of space, right justify the entire string
            VT100_goto_xy(145 - l + 1, i + 1);
            MMPrintString(disp);
         } else {
            VT100_goto_xy(145 - avail + 2, i + 1); // leave a space between name and str value
            VT100_underline_on();                  // if we can't display the entire string, we want
            VT100_forground_magenta();
            disp[avail] = 0; // to caution the user some of it is not shown
            MMPrintString(disp);
         }
         break;

      case T_NOTYPE:
         MMPrintString("No Type");
         break;

      default:
         VT100_forground_red();
         MMPrintString("? Huh ?");
         break;
         //			}

         VT100_forground_yellow();
         VT100_background_black();
      }
   }
}

// count the number of lines up to and including the line pointed to by the argument
// used for error reporting in programs that do not use line numbers.
//
// This code was stolen from MMBasic.c   The hackery to not count the title comment at the
// start of the program causes a lot of inconsistancy.   It is a small function so it is
// cleaner to just duplicate it here and use this version within the Debugger.
//
// I think the MMBasic Trace command should just include the first (title) line in its count.
// But in the mean time...   We duplicate the functionality here:

int Debug_CountLines(unsigned char *target) {
   unsigned char *p;
   int cnt;

   p = ProgMemory;
   cnt = 0;

   //  if(ProgMemory[0]==1 && ProgMemory[1]==39 && ProgMemory[2]==35)cnt=-1;		// Remove the hackery that makes everything
   //  inconsistent
   //  else cnt = 0;									// depending on whether the user put a title line at the
   //  start.

   while (1) {
      if (*p == 0xff || (p[0] == 0 && p[1] == 0)) // end of the program
         return cnt;

      if (*p == T_NEWLINE) {
         p++; // and step over the line number
         cnt++;
         if (p >= target)
            return cnt;
         continue;
      }

      if (*p == T_LINENBR) {
         p += 3; // and step over the line number
         continue;
      }

      if (*p == T_LABEL) {
         p += p[0] + 2; // still looking! skip over the label
         continue;
      }

      if (p++ > target)
         return cnt;
   }
   return cnt;
}

void Debugger_find_special_cmds() {

   Rem_cmd = GetCommandValue((unsigned char *)"Rem");
   Return_cmd = GetCommandValue((unsigned char *)"Return");
   GoTo_cmd = GetCommandValue((unsigned char *)"GoTo");
   On_cmd = GetCommandValue((unsigned char *)"On");
}

void ClearDebugger() {
   int i;

   n_disp_elements = 0;

   MMDebug_Brk_Pnt_Addr = NULL; // We don't clear the MMDebug flag because ClearDebugger() only gets called from the
                                // MMBasic cmd_run() function.   At that point, the user has typed 'RUN' and may have
                                // requested the debugger to take control.   If Ctrl-D has been typed, and captured by
                                // the MMDebug flag, we want that to stay active.

   for (i = 0; i < DISPLAY_LIST_SIZE; i++) {
      var_disp_list[i].name[0] = 0;
      var_disp_list[i].type = 0;
      var_disp_list[i].index = 0;
      var_disp_list[i].val.fa = NULL;
   }
}

#define prt_spaces(n)                                                                                                            \
   for (int j = 0; j < n; j++)                                                                                                   \
      MMputchar(' ', 0);
#define duplicate_chars(c, n)                                                                                                    \
   for (int j = 0; j < n; j++)                                                                                                   \
      MMputchar(c, 0);

void Debugger_Open_Edit_Box() {
   int i;

   VT100_forground_white();
   VT100_background_black();
   VT100_goto_xy(DBG_X_COLUMN + 8, 13);
   prt_spaces(72);

   VT100_goto_xy(DBG_X_COLUMN + 9, 14);
   MMPrintString(" ------Enter Print of Variable or Assign New Value to a Variable----- ");
   for (i = 0; i < 6; i++) {
      VT100_goto_xy(DBG_X_COLUMN + 9, 15 + i);
      MMputchar(' ', 0);
      MMputchar('|', 0);
      prt_spaces(66);
      MMputchar('|', 0);
      MMputchar(' ', 0);
   }
   VT100_goto_xy(DBG_X_COLUMN + 9, 21);
   MMputchar(' ', 0);
   duplicate_chars('-', 68);
   MMputchar(' ', 0);
   VT100_goto_xy(DBG_X_COLUMN + 8, 22);
   prt_spaces(72);
}

void Debugger_set_screen_up_for_return() {
   VT100_forground_white();
   VT100_background_black();
   VT100_restore_cursor();
}

