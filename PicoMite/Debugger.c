/***********************************************************************************************************************
PicoMite MMBasic

Debugger.c

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

#include "MMBasic_Includes.h"
//#include "AllCommands.h"
#include "Debugger.h"
#include "Hardware_Includes.h"
#include "class/cdc/cdc_device.h"

#ifdef PICOMITEVGA
#endif

#ifdef PICOMITE
#endif

#ifdef PICOMITEWEB
#endif

//
// It is expected that the user will be using the MicroMite Control Centre and have a mouse attached to the system.   The Debugger
// will work with just a keyboard, but functionality and power of the Debugger will be reduced in this configuration.  The
// terminal screen of MicroMite Control Centre needs to be expanded from what it normally defaults to.   The reason is the
// Debugger needs screen space to display its information while at the same time preserving much of the screen that the BASIC
// program being debugged needs to provide input and output.
//
// The user needs to do issue commands within MMBASIC at the prompt to set the display options:
//			> OPTION ESCAPE ON
// 			> Option Display 45,145
//                      > Option Colourcode On
//
// The user also needs to install a recent version of the MicroMite Control Panel.   (The Debugger needs Cursor Save/Restore
// capability and that feature was not enabled on older versions of the MMCC.)
//
// After bringing up the Debugger and using it, the user may find that the calibration of the mouse cursor is not very accurate.
// The user may need to adjust the Click Y shift=? and Click X shift=?  In MMCC.inf.   MMCC.inf is located in
// C:\Users\EDN\AppData\Local\MMedit5  on my system.  On my Windows system I needed to make Click Y shift=15 for reasonable
// accuracy.
//
//

int n_disp_elements = 0;
int dbg_n_scrn_lines = 0;
struct var_disp_element var_disp_list[DISPLAY_LIST_SIZE] = {{"", T_NOTYPE, 0.0}};
struct dbg_screen_info dbg_scrn_lines[N_DBG_CODE_LINES] = {{NULL, -1}};
int top_scrn_line_cnt = -1;

int Rem_cmd = -1, Return_cmd = -1, GoTo_cmd = -1, On_cmd = -1;

extern jmp_buf mark;
jmp_buf dbg_cmd_prompt, tmp_jmp_buf;

int debugger_scrn_x_column = DBG_X_COLUMN;
int debugger_scrn_y_size = DBG_Y_ROW;
int Mouse_X, Mouse_Y;

unsigned char *MMDebug_Brk_Pnt_Addr = NULL;

void printLine(int);
void SetColour(unsigned char *, int);
char *findLine(int);

unsigned char *b_pnt; // These two variables are very important.  This is a pointer to the code location where the break point
                      // triggered. and the BASIC interpreter suspends itself and gives up control to the Debugger
int b_pnt_line_count; // This it how many lines into the BASIC program the break point trigger happened

unsigned char *Debugger(unsigned char *p) {
   unsigned char *ptr, *pp;
   char str[20];
   static int terminal_at_correct_size = 0;
   int i, c;

   MMDebug_Brk_Pnt_Addr = NULL; // and clear any Break Points.   These will get set appropriately with
   MMDebug_Level_Change = -1;   // a Step or Go command
   MMDebug = 0;

   if (!terminal_at_correct_size++)
      setup_terminal_screen_to_145x45();

   if (Rem_cmd == -1)
      Debugger_find_special_cmds();

   b_pnt = p;
   b_pnt_line_count = Debug_CountLines(p);

   if (!Option.ColourCode || Option.Height != 45 ||
       Option.Width != 145) {   // If the user doesn't have options set up correctly, we fix
      Option.ColourCode = true; // that problem here.   But the user will need to re-enter
      Option.Height = 45;       // the Debugger to have things correct.
      Option.Width = 145;
      SaveOptions();
      setup_terminal_screen_to_145x45();
      MMPrintString("\r\n\r\nOptions were not set correctly.\r\nThat is fixed now.\r\n");
      //		return p;
   }

   VT100_save_cursor();
   VT100_Mouse_Mode_On();
   VT100_Clear_Debugger_Screen();

   VT100_goto_xy(debugger_scrn_x_column + 1, debugger_scrn_y_size + 1);
   VT100_forground_white();
   VT100_background_black();

   //--------------------------------------------

   if (top_scrn_line_cnt < 0)
      top_scrn_line_cnt = b_pnt_line_count - 5;

   if (b_pnt_line_count > top_scrn_line_cnt + 20)
      top_scrn_line_cnt = b_pnt_line_count - 5;

   if (b_pnt_line_count < top_scrn_line_cnt)
      top_scrn_line_cnt = b_pnt_line_count - 5;

   if (top_scrn_line_cnt < 1)
      top_scrn_line_cnt = 1;

   VT100_forground_white();
   VT100_background_black();

   if (b_pnt_line_count > 1)
      Debugger_find_variables_used_by_line(b_pnt_line_count - 1);
   Debugger_find_variables_used_by_line(b_pnt_line_count); // <----<<<   may need to be deleted

   Debugger_highlight_line(b_pnt_line_count);

   while (1) {

      if (!Debugger_input_pending()) {
         Debugger_Paint_Code_Lines(top_scrn_line_cnt);
         Debugger_var_display();
         Debugger_Prompt();
         Debugger_Status_Info();
      }

      c = Debugger_getc();
      c = toupper(c);

      switch (c) {

      case -1:     // if the person is using TeraTerm, a Mouse Click down and a Mouse Click up gets reported.  
	 break;    // Debugger_getc() eats the Mouse Click up and returns -1.   This is the final action to eat 
                   // the extra mouse click.    
      case '.':    
         break;

      case '?':
         break;

      case MOUSE_CLICK:
         VT100_goto_xy(100, debugger_scrn_y_size); // Report Mouse Click location
         VT100_forground_red();
         VT100_background_yellow();  // it may make sense to leave this coordinate display for a short while
         MMPrintString(" (");        // because users may need to adjust the cursor offsets on their
         IntToStr(str, Mouse_X, 10); // MMCC displays
         MMPrintString(str);
         MMPrintString(","); // But if not....   this code can be safely deleted
         IntToStr(str, Mouse_Y, 10);
         MMPrintString(str);
         MMPrintString(") ");
         VT100_background_black();
         VT100_forground_white();

         if (Mouse_Y < 1 || Mouse_Y > debugger_scrn_y_size - 3 || Mouse_X < debugger_scrn_x_column || Mouse_X > 145) {
            Debugger_display_error_message(" ??? Poorly Placed Mouse Click. ");
            break;
         }

         Debugger_find_variables_used_by_line(top_scrn_line_cnt + Mouse_Y - 1);
         Debugger_highlight_line(top_scrn_line_cnt + Mouse_Y - 1);
         Debugger_Pause(250);

         VT100_goto_xy(debugger_scrn_x_column + 1, 1);
         break;

      case HOME:
         top_scrn_line_cnt = 1;
         break;

      case END:
         top_scrn_line_cnt = Debug_CountLines((unsigned char *)0x20000000) - debugger_scrn_y_size - 5; 
         break;

      case PUP:
         top_scrn_line_cnt -= (debugger_scrn_y_size * 4) / 5;
         if (top_scrn_line_cnt < 1)
            top_scrn_line_cnt = 1;
         break;

      case PDOWN:
         top_scrn_line_cnt += (debugger_scrn_y_size * 4) / 5;
         if (top_scrn_line_cnt > Debug_CountLines((unsigned char *)0x20000000) - debugger_scrn_y_size)
            top_scrn_line_cnt = Debug_CountLines((unsigned char *)0x20000000) - debugger_scrn_y_size + 5;
         break;

      case UP:
         top_scrn_line_cnt--;
         if (top_scrn_line_cnt < 1)
            top_scrn_line_cnt = 1;
         continue;

      case DOWN:
         top_scrn_line_cnt++;
         if (top_scrn_line_cnt > Debug_CountLines((unsigned char *)0x20000000) - debugger_scrn_y_size)
            top_scrn_line_cnt = Debug_CountLines((unsigned char *)0x20000000) - debugger_scrn_y_size + 5;
         continue;

      case 's':
      case 'S':
      case STEP:
         Debugger_set_screen_up_for_return();
         MMDebug = true;              // STEP is the easiest.   We just leave the MMDebug flag enabled
         MMDebug_Brk_Pnt_Addr = NULL; // and clear any Break Points.  The main interpreter loop will stop
         return p;                    // after the next command is executed.
         break;

      case STEP_OUT:
         if (gosubindex == 0) {
            VT100_goto_xy(80, debugger_scrn_y_size);
            VT100_forground_red();
            VT100_background_yellow();
            MMPrintString(" --Nothing to Step Out of--");
            VT100_background_black();
            VT100_forground_white();
            break;
         }

         MMDebug = false; // STEP-OUT is fairly easy.

         if (gosubstack[gosubindex - 1] == NULL) { // This is a user defined function.  Tell the
            MMDebug_Brk_Pnt_Addr = NULL;           // interpreter to stop when it sees the lower level
            MMDebug_Level_Change = gosubindex - 1;
         } else {                                              // Otherwise it is a subroutine
            MMDebug_Brk_Pnt_Addr = gosubstack[gosubindex - 1]; // We load the Break Pnt address with the top of
            while (*MMDebug_Brk_Pnt_Addr == 0)                 // the Return stack.
               MMDebug_Brk_Pnt_Addr++;                         // and bump past the EOL if necessary
         }

         Debugger_set_screen_up_for_return();
         return p;
         break;
      case 'o':
      case 'O':
      case STEP_OVER:
         VT100_forground_red();     // STEP_OVER can step over Gosub's, For/Next loops, Do loops and expressions
         VT100_background_yellow(); // that get evaluated using multiple line user defined functions.   It can
         pp = p;                    // also step over multiple commands on a single line.
         pp = GetNextCommand(pp, NULL, (unsigned char *)"Unable to Step-Over");

         if (*pp == GoTo_cmd || *pp == On_cmd || *pp == cmdIF) { // Check for simple commands that change the program flow
            Debugger_set_screen_up_for_return();
            MMDebug_Brk_Pnt_Addr = NULL; // We will just set the MMDebug flag to stop execution instead
            MMDebug = true;              // of trying to figure out where the Break Points should be
            return b_pnt;
         }

         if (*pp == cmdSELECT_CASE) {
            i = 1;
            while (1) {
               pp = GetNextCommand(pp, NULL, (unsigned char *)"No matching END SELECT");
               if (*pp == cmdSELECT_CASE)
                  i++; // entered a nested Select Case
               if (*pp == cmdEND_SELECT)
                  i--; // exited a nested Select Case

               if (i == 0)
                  break; // found our matching End Select
            }
            skipelement(pp);
            pp++;
            Debugger_set_screen_up_for_return();
            MMDebug_Brk_Pnt_Addr = pp;
            MMDebug = false;
            return b_pnt;
         } else

             if (*pp == cmdFOR) {
            i = 1;
            while (1) {
               pp = GetNextCommand(pp, NULL, (unsigned char *)"No matching NEXT");
               if (*pp == cmdFOR)
                  i++; // entered a nested For/Next loop
               if (*pp == cmdNEXT)
                  i--; // exited a nested For/Next loop

               if (i == 0)
                  break; // found our matching NEXT
            }
            skipelement(pp);
            pp++;
            Debugger_set_screen_up_for_return();
            MMDebug_Brk_Pnt_Addr = pp;
            MMDebug = false;
            return b_pnt;
         } else

             if (*pp == cmdDO) {
            i = 1;
            while (1) {
               pp = GetNextCommand(pp, NULL, (unsigned char *)"No matching LOOP");
               if (*pp == cmdDO)
                  i++; // entered a nested DO or WHILE loop
               if (*pp == cmdLOOP)
                  i--; // exited a nested loop

               if (i == 0)
                  break; // found our matching LOOP or WEND stmt
            }
            skipelement(pp);
            pp++;
            Debugger_set_screen_up_for_return();
            MMDebug_Brk_Pnt_Addr = pp;
            MMDebug = false;
            return b_pnt;
         }

         /*
          *
          *  If we get to this point, we aren't handling a For/Next or a Do/Loop or a subroutine.   The user just wants to step
          * over a line that will get bogged down in lots of noisy BASIC statements like user defined function being evaluated.
          *
          */

         VT100_goto_xy(DBG_X_COLUMN + 1, b_pnt_line_count - top_scrn_line_cnt + 2); // +2 because we highlight the
         VT100_forground_red();                                                     // next line, not the current line
         VT100_background_yellow();
         IntToStr(str, b_pnt_line_count + 1, 10);
         MMPrintString(str);
         MMPrintString(":");

         ptr = LocateProgramStatement(b_pnt_line_count + 1);
         Debugger_Pause(150);

         Debugger_set_screen_up_for_return();
         MMDebug_Brk_Pnt_Addr = ptr;
         MMDebug = false;

         return b_pnt;

      case BRK_PNT_and_GO:
         ptr = LocateProgramStatement(top_scrn_line_cnt + Mouse_Y - 1);
         VT100_goto_xy(DBG_X_COLUMN + 1, Mouse_Y);
         VT100_forground_red();
         VT100_background_yellow();
         IntToStr(str, top_scrn_line_cnt + Mouse_Y - 1, 10);
         MMPrintString(str);
         MMPrintString(":");
         Debugger_Pause(100);

         Debugger_set_screen_up_for_return();
         MMDebug_Brk_Pnt_Addr = ptr;
         MMDebug = false;

         return b_pnt;

      case GO:
         Debugger_set_screen_up_for_return();
         MMDebug_Brk_Pnt_Addr = NULL;
         MMDebug = false;

         return b_pnt;

      case EDIT_VALUE: {
         int reason;
         jmp_buf /* dbg_edit_box,*/ tmp_jmp_buf;

         memcpy(tmp_jmp_buf, mark, sizeof(jmp_buf)); // For sure the user is going to have syntax errors
                                                     // that cause the interpretor to have convolusions.
                                                     // When that happens, we want control to come back to
                                                     // the debug session and not end up at a command prompt

         while (1) {
            unsigned char *ttp = NULL, *ucp;

            Debugger_Open_Edit_Box();

            reason = setjmp(mark);
            if (reason) {
               VT100_goto_xy(DBG_X_COLUMN + 14, 21);
               VT100_forground_red();
               VT100_background_yellow();
               MMPrintString("---Syntax Error---");
            }

            VT100_goto_xy(DBG_X_COLUMN + 12, 15);
            VT100_forground_cyan();
            VT100_background_black();
            MMPrintString(" > ");

            Debugger_getline((char *)inpbuf);
            if (strlen((char *)inpbuf) < 1)
               break;

            if (inpbuf[0] >= DBG_BUTTON_STEP && inpbuf[0] <= DBG_BUTTON_PAGE_DOWN) {
               VT100_goto_xy(80, debugger_scrn_y_size);
               VT100_forground_cyan();
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
            }
            ucp = inpbuf;
            skipspace(ucp);

            multi = false;
            tokenise(true); // and tokenise it (the result is in tknbuf)
            memset(ucp, 0, STRINGSIZE);
            tknbuf[strlen((char *)tknbuf)] = 0;
            tknbuf[strlen((char *)tknbuf) + 1] = 0;
            ttp = nextstmt; // save the globals used by commands

            VT100_goto_xy(DBG_X_COLUMN + 12, 17);
            ScrewUpTimer = 1000;
            ExecuteProgram(tknbuf); // actually do the work
            ScrewUpTimer = 0;

            TempMemoryIsChanged = true; // signal that temporary memory should be checked
            nextstmt = ttp;

            Debugger_var_display();

            while (Debugger_input_pending()) // eat any extra characters
               Debugger_get_char();

            while (!Debugger_input_pending()) // now wait for the user to view the results
               ;
         }

         Debugger_get_char();
         memcpy(mark, tmp_jmp_buf, sizeof(jmp_buf)); // Restore MMBasic's abort longjmp()
         VT100_forground_white();
         VT100_background_black();
         break;
      }
      case 'q':
      case 'Q':
      case QUIT:
         Debugger_set_screen_up_for_return();
         MMDebug_Brk_Pnt_Addr = NULL; // Disable all Break Pnt's
         MMDebug = false;
         cmd_end();
         break;

      default:
         Debugger_display_error_message(" ??? Invalid debugger command. ");
         break;
      }
   }

   VT100_restore_cursor();
   MMDebug = false;

   return p;
}

void Debugger_Pause(int ms) {
   InkeyTimer = 0; // BASIC is suspended, so we can grab the
                   // InkeyTimer instead of creating our own timer
   while (InkeyTimer < ms)
      ; // a brief pause to let the user see the message
}

void Debugger_display_error_message(char *err) {
   int l, i;

   l = strlen(err);
   VT100_goto_xy(debugger_scrn_x_column + 1, debugger_scrn_y_size);
   VT100_forground_red();
   VT100_background_yellow();
   MMPrintString(err);

   Debugger_Pause(500);

   VT100_forground_white(); // restore the screen
   VT100_goto_xy(debugger_scrn_x_column + 1, debugger_scrn_y_size);
   VT100_forground_cyan();
   VT100_background_black();
   for (i = 0; i < l; i++)
      MMPrintString("_");
   VT100_forground_white();
}

void Debugger_highlight_line(int ln) {
   int row;
   unsigned char str[MAXSTRLEN];
   unsigned char *ptr;

   ptr = LocateProgramStatement(ln); // blink the selected line
   row = ln - top_scrn_line_cnt + 1;

   if (row < 1) {
      VT100_goto_xy(80, debugger_scrn_y_size);               // this can be removed later, after debug
      VT100_forground_red();                                 // this can be removed later, after debug
      VT100_background_yellow();                             // this can be removed later, after debug
      MMPrintString(" --Can't highlight off screen line--"); // this can be removed later, after debug
      VT100_background_black();                              // this can be removed later, after debug
      VT100_forground_white();                               // this can be removed later, after debug
      return;
   }

   VT100_goto_xy(debugger_scrn_x_column + 1, row);
   IntToStr((char *)str, ln, 10);

   VT100_forground_yellow();

   MMPrintString((char *)str);
   MMPrintString(":");
   if (ln < 100)
      MMPrintString("  ");
   else if (ln < 10)
      MMPrintString(" ");

   VT100_goto_xy(debugger_scrn_x_column + 5, row);
   llist(str, ptr);
   MMPrintString((char *)str);
   VT100_forground_white();
}

void add_variable_to_display_list(unsigned char *str) {
   int i, j, l, vi;
   struct var_disp_element tmp;
   void *ptr;

   l = strlen((char *)str);
   for (i = 0; i < l; i++)
      str[i] = toupper(str[i]);

   // don't add something that is not currently defined as a variable

   if (str[l] == '!')
      ptr = findvar(str, V_NOFIND_NULL | T_IMPLIED | T_NBR);
   else if (str[l] == '$')
      ptr = findvar(str, V_NOFIND_NULL | T_IMPLIED | T_STR);
   else if (str[l] == '%')
      ptr = findvar(str, V_NOFIND_NULL | T_IMPLIED | T_INT);
   else
      ptr = findvar(str, V_NOFIND_NULL);

   vi = VarIndex; // remember where it was found (assumming it was found)
   if (ptr == NULL)
      return;

   for (i = 0; i < n_disp_elements; i++) { // check if the specified variable is already in our display list
      if (strcmp((char *)var_disp_list[i].name, (char *)str) == 0) {
         tmp = var_disp_list[i];
         for (j = i; j > 0; j--)
            var_disp_list[j] = var_disp_list[j - 1]; // slide the list down to where we found the variable
         var_disp_list[0] = tmp;                     // and put the specified variable at the top of the list
         return;
      }
   }

   for (j = DISPLAY_LIST_SIZE - 1; j > 0; j--)
      var_disp_list[j] = var_disp_list[j - 1]; // slide the list down to where we found the variable
   n_disp_elements++;
   if (n_disp_elements > DISPLAY_LIST_SIZE)
      n_disp_elements = DISPLAY_LIST_SIZE;

   strcpy((char *)var_disp_list[0].name, (char *)str);

   var_disp_list[0].type = vartbl[vi].type;
   var_disp_list[0].index = vi;
   switch (vartbl[vi].type) {
   case T_NBR:
      var_disp_list[0].val.f = (MMFLOAT)0.0;
      break;
   case T_INT:
      var_disp_list[0].val.i = (long long int)0;
      break;
   case T_STR:
      var_disp_list[0].val.s = vartbl[vi].val.s;
      break;
   case T_NOTYPE:
      var_disp_list[0].val.s = (void *)NULL;
      break;
   default:
      var_disp_list[0].val.s = (void *)NULL;
      break;
   }
}

int is_at_start_of_line(unsigned char *p) {
   unsigned char *p2;
   int ln;

   ln = Debug_CountLines(p);
   p2 = LocateProgramStatement(ln);

   if (p == p2)
      return 1;
   else
      return 0;
}

void Debugger_Paint_Code_Lines(int start_line) {
   int j, k = 0, l, l2;
   unsigned char *pp; //, *ptr;
   unsigned char str[STRINGSIZE];

   for (j = top_scrn_line_cnt; j < top_scrn_line_cnt + N_DBG_CODE_LINES; j++) {

      if (Debugger_input_pending())
         return;

      VT100_goto_xy(debugger_scrn_x_column + 1, j - top_scrn_line_cnt + 1);

      VT100_all_attr_off();

      if (b_pnt_line_count == j) {
         VT100_background_yellow();
         VT100_forground_red();
      }

      IntToStr((char *)str, j, 10);
      MMPrintString((char *)str);
      MMPrintString(":");
      if (j < 100)
         MMPrintString("  ");
      else if (j < 10)
         MMPrintString(" ");

      pp = LocateProgramStatement(j);
      VT100_background_black();
      VT100_Clear_to_EOL();

      if (pp == NULL) // end of the program
         break;

      llist(str, b_pnt);
      l2 = strlen((char *)str);

      llist(str, pp);
      l = strlen((char *)str);

      VT100_goto_xy(debugger_scrn_x_column + 5, j - top_scrn_line_cnt + 1);
      SetColour(NULL, true);

      if (b_pnt_line_count == j) {
         k = 0;
         if (l2 < l) { // if true, we are in the middle part of the line.
            for (k = 0; k < (l - l2); k++) {
               SetColour((unsigned char *)&str[k], true);
               MMputchar(str[k], 0);
            }
         }

         VT100_background_yellow();
         VT100_forground_red();
         for (; k < l; k++) {
            if (str[k] == ':')    // Using a ':' to terminate the active statement element fails if we have a quoted string
               break;             // with a colon in it.  Only part of the element will be high lighted.   But this saves us
            MMputchar(str[k], 0); // a lot of stack space because we don't need a 2nd string to llist() into looking for
         }                        // the true end of the element.   The user still gets enough of the element highlighted
                                  // to know where the next execution of logic is happening.
         VT100_forground_white();
         VT100_background_black();
         SetColour(NULL, true);
         for (; k < l; k++) {
            if (k >= l)
               break;
            SetColour((unsigned char *)&str[k], true);
            MMputchar(str[k], 0);
         }

         VT100_forground_white();
         VT100_background_black();
         VT100_Clear_to_EOL();
      } else {
         for (k = 0; k < 145 - debugger_scrn_x_column - 7; k++) { // Simple loop to display the line if this isn't
            if (k >= l)                                           // the screen row we are Break Pointed on.
               break;
            SetColour((unsigned char *)&str[k], true);
            MMputchar(str[k], 0);
         }
         SetColour(NULL, true);
      }
   }
}

void dump_current_program_info(unsigned char *p, int row) {
   /*
           char str[250];
           unsigned char *pp;
           int i;

           pp = p;

           VT100_goto_xy( 1, row );
           VT100_forground_white();
           VT100_background_black();
           Debug_Info_hex(" ProgMemory: 0x", (int) ProgMemory);
           Debug_Info_hex(" Entry: 0x", (int) p);
           Debug_Info("  Line:", b_pnt_line_count);
           Debug_Info(" LastLine:",  Debug_CountLines( (unsigned char *) 0x20000000 ) );
           Debug_Info_hex(" End: 0x", (int)  LocateProgramStatement(Debug_CountLines( (unsigned char *) 0x20000000 ) )  );
           Debug_Info_hex(" b_pnt: 0x", (int) b_pnt );
           Debug_Info(" gosubindex:",  gosubindex);
           Debug_Info_hex(" gosub stk: 0x", (int) gosubstack[gosubindex - 1] );


           VT100_goto_xy( 1, row+1 );
           llist((unsigned char *) str, pp);
           MMPrintString(str);
           MMPrintString("                                                           ");

           VT100_goto_xy( 1, row+2 );
           for(i=-4; i<43; i++ ) {
               if (i==0)
                       MMPrintString("|");
               IntToStr(str, (int) pp[i], 16);
               while (strlen(str)<3)
                  strcat(str," ");
               MMPrintString(str);
           }

           VT100_goto_xy( 1, row+3 );
           for(i=-4; i<43; i++ ) {
               if (i==0)
                       MMPrintString("|");
               MMPrintString(" ");
               if ( pp[i] > 31 && pp[i]<127 )
                       MMputchar(pp[i],1);
               else
                       MMputchar('.',1);
               MMPrintString(" ");
           }
   */
}

void dump_mem(unsigned char *p, int row) {
   /*
           char str[250];
           unsigned char *pp;
           int i;

           pp = p;

           VT100_goto_xy( 1, row );
           VT100_forground_white();
           VT100_background_black();


           VT100_goto_xy( 1, row+1 );
           llist((unsigned char *) str, pp);
           MMPrintString(str);
           MMPrintString("                                                           ");

           VT100_goto_xy( 1, row+2 );
           for(i=-4; i<38; i++ ) {
               if (i==0)
                       MMPrintString("|");
               IntToStr(str, (int) pp[i], 16);
               while (strlen(str)<3)
                  strcat(str," ");
               MMPrintString(str);
           }

           VT100_goto_xy( 1, row+3 );
           for(i=-4; i<38; i++ ) {
               if (i==0)
                       MMPrintString("|");
               MMPrintString(" ");
               if ( pp[i] > 31 && pp[i]<127 )
                       MMputchar(pp[i],1);
               else
                       MMputchar('.',1);
               MMPrintString(" ");
           }
   */
}

void Debugger_find_variables_used_by_line(int ln) {
   unsigned char var_name[MAXVARLEN + 1];
   unsigned char str[MAXSTRLEN];
   unsigned char *ptr, *p, *wp, *ps;
   int i, j, l = -1, row;

   ptr = LocateProgramStatement(ln); // blink the selected line
   row = ln - top_scrn_line_cnt + 1;

   if (row < 1)
      return;

   VT100_goto_xy(debugger_scrn_x_column + 1, row);
   IntToStr((char *)str, ln, 10);

   VT100_forground_yellow();

   MMPrintString((char *)str);
   MMPrintString(":");
   VT100_goto_xy(debugger_scrn_x_column + 5, row);

   if (*ptr == 0xff || (ptr[0] == 0 && ptr[1] == 0)) // end of the program
      return;

   if (*ptr == T_NEWLINE)
      ptr++; // and step over the new line marker

   if (*ptr == T_LINENBR)
      ptr += 3; // and step over the line number

   if (*ptr == T_LABEL)
      ptr += ptr[0] + 2; // still looking! skip over the label

   p = llist(str, ptr); // p points to the next line.   The line we are dealing with is
                        // between ptr and p

   for (i = 0; i < p - ptr; i++) // get a copy of the tokenized line.   We will parse through this
      str[i] = ptr[i];           // copy to find variable names.   We get more than we need just to make
                                 // sure we have the complete line to parse.

   p = wp = str;
   while (1) {
      if (*p == 0x01)
         break; // End of Line
      if (*p == 0x00 && *(p + 1) == 0x00)
         break; // End of Program
      *wp++ = *p++;
      while (*(p - 1) == ' ' && *p == ' ')
         p++;
   }

   *wp = 0;
   *(wp + 1) = 0x01; // add a new line command just so we know it is there to make searches easier
   l = wp - str;
   p = str;

   // p now points at the start of logic on the line

   ps = p;     // save the start of the logic on the line
   while (1) { // now we need to know where the end of the line is
      if (*p == 0x01)
         break; // End of Line
      if (*p == 0x00 && *(p + 1) == 0x00)
         break; // End of Program
      p++;
   }
   p--; // p now points at the end of the line  (the last character of the line)

   l = p - ps; // l holds the length of the logic

   for (i = 0; i < l; i++) { // first we kill any literal text so we don't get confused and think there is a variable name there
      if (ps[i] == '\"') {
         ps[i] = 0xff;
         for (j = i + 1; j < l; j++) {
            if (ps[j] == '\"') {
               ps[j] = 0xff; // found the trailing quote.  kill it
               i = j;        // and jump i ahead to continue the scan across the line's logic
               break;
            }
            ps[j] = 0xff; // still inside the quoted text.  kill it
         }
      }
   }

   for (i = 0; i < l; i++) // now kill any comments at the end of the line
      if (ps[i] == '\'') {
         ps[i] = 0x00; // mark the new end of line
         for (j = i + 1; j < l; j++)
            ps[j] = 0xff;
         l = i; // shorten the length
         break;
      }

   for (i = 0; i < l; i++) { // check if this is a Rem command.  If so, there are no variables.  We bug out.
      if (ps[i] == Rem_cmd)
         return;
   }

   for (i = 0; i < l; i++) {
      if (ps[i] >= C_BASETOKEN) // kill any commands
         ps[i] = 0xff;
      if (ps[i] == ' ') // kill any spaces
         ps[i] = 0xff;
   }

   //--------- Ready to scan across the line and pick out the variables in it ---------------
   for (i = 0; i < l; i++) {
      if (isnamestart(ps[i])) {
         p = &ps[i];
         wp = var_name;

         *wp++ = *p++;
         *wp = 0;

         while (isnamechar(*p) && MAXVARLEN > (wp - var_name)) {
            *wp++ = *p++;
            *wp = 0;
         }

         if (isnameend(*p)) {
            *wp++ = *p++;
            *wp = 0;
         }

         while (*p == ' ')
            p++;
         i += p - &ps[i];
         if (*p != '(')
            add_variable_to_display_list(var_name);
      }
   }
}

void Debugger_Status_Info() {
   char str[10];
   if (gosubindex != 0) {
      VT100_goto_xy(145 - 9, debugger_scrn_y_size);
      VT100_forground_red();
      VT100_background_yellow();
      MMPrintString("Level:");
      IntToStr(str, gosubindex, 6);
      MMPrintString(str);
   }
}

