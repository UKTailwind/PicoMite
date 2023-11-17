/***********************************************************************************************************************
PicoMite MMBasic

Debugger.h

<COPYRIGHT HOLDERS>  E. David Neufeld
Copyright (c) 2023, <COPYRIGHT HOLDERS> All rights reserved. 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: 
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original
        copyright message be displayed on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes 
        software developed by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this 
        software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

************************************************************************************************************************/


/**********************************************************************************
 the C language function associated with commands, functions or operators should be
 declared here
**********************************************************************************/
#if !defined(INCLUDE_COMMAND_TABLE) && !defined(INCLUDE_TOKEN_TABLE)

#define DBG_X_COLUMN		40
#define DBG_Y_ROW		30
#define DBG_VAR_COLUMN		(DBG_X_COLUMN+83)
#define DBG_VAR_VAL_COLUMN	(DBG_X_COLUMN+98)

/*
#define STEP		132
#define STEP_OUT	133
#define STEP_OVER	0x91
#define BRK_PNT_and_GO  0x92
#define GO		0x93
#define EDIT_VALUE 	0x94
#define QUIT	 	0x95
*/
#define STEP		0x8d
#define STEP_OUT	0x8e
#define STEP_OVER	0x8f
#define BRK_PNT_and_GO  0x90
#define GO		0x91
#define EDIT_VALUE 	0x92
#define QUIT	 	0x93


#define DISPLAY_LIST_SIZE	20
#define N_DBG_CODE_LINES	25 
#define N_DEBUG_UNGETCHAR	5

#define DBG_BUTTON_STEP			1
#define DBG_BUTTON_STEP_OVER		8
#define DBG_BUTTON_STEP_OUT		15
#define DBG_BUTTON_GO			22
#define DBG_BUTTON_EDIT_VALUE		40
#define DBG_BUTTON_QUIT			82
#define DBG_BUTTON_LINE_UP		93
#define DBG_BUTTON_LINE_DOWN		93
#define DBG_BUTTON_PAGE_UP		100
#define DBG_BUTTON_PAGE_DOWN		100


struct var_disp_element {
	unsigned char name[MAXVARLEN+1];
	unsigned char type;
	int           index;
	union u_val   val; };

extern int n_disp_elements;
extern struct var_disp_element var_disp_list[DISPLAY_LIST_SIZE];

struct dbg_screen_info {
	unsigned char *code;
	int            line_nbr;
};

extern int debugger_scrn_x_column, debugger_scrn_y_size;
extern int dbg_n_scrn_lines; 
extern struct dbg_screen_info dbg_scrn_lines[N_DBG_CODE_LINES]; 
extern int  Rem_cmd, Return_cmd, GoTo_cmd, On_cmd;

extern jmp_buf dbg_edit_box, tmp_jmp_buf;

unsigned char *Debugger(unsigned char *);
void ClearDebugger();
void Debugger_Prompt();
void Debugger_set_screen_up_for_return();
int  Debugger_getc();
void Debugger_unget_char(int );
int Debugger_get_char();
void Debugger_getline(char *);
int Debugger_input_pending();
void Debugger_Paint_Code_Lines( int );
void Debugger_Status_Info();
void Debugger_Open_Edit_Box();
void dump_mem( unsigned char *, int);

void Debugger_highlight_line(int );
int is_at_start_of_line(unsigned char * p);
void add_variable_to_display_list( unsigned char *);
void dump_current_program_info( unsigned char *, int ); 

void VT100_goto_xy(int, int );
void VT100_Mouse_Mode_On();
void VT100_Clear_Screen();
void VT100_Clear_to_EOL();
void VT100_Clear_Debugger_Screen();
void VT100_all_attr_off();
void VT100_bold_on();
void VT100_low_intensity();
void VT100_underline_on();
void VT100_save_cursor();
void VT100_restore_cursor();
void VT100_forground_black();
void VT100_forground_red();
void VT100_forground_green();
void VT100_forground_yellow();
void VT100_forground_blue();
void VT100_forground_cyan();
void VT100_forground_magenta();
void VT100_forground_white();
void VT100_forground_grey();

void VT100_background_black();
void VT100_background_red() ;
void VT100_background_green();
void VT100_background_yellow();
void VT100_background_blue();
void VT100_background_cyan();
void VT100_background_magenta();
void VT100_background_white();
void VT100_background_grey();

void setup_terminal_screen_to_145x45(void);

unsigned char *LocateProgramStatement( int );
int  Debug_CountLines(unsigned char *);
void Debugger_find_special_cmds();
void Debugger_find_variables_used_by_line( int );
void Debugger_var_display();
void Debugger_display_error_message(char *);
void Debugger_Pause(int );


void Debug_Info( char *, int );
void Debug_Info_hex( char *, int );
void Debug_Info_char( char *, unsigned char);

#endif




/**********************************************************************************
 All command tokens tokens (eg, PRINT, FOR, etc) should be inserted in this table
**********************************************************************************/
#ifdef INCLUDE_COMMAND_TABLE
// the format is:
//    TEXT      	TYPE                P  FUNCTION TO CALL
// where type is always T_CMD
// and P is the precedence (which is only used for operators and not commands)

//	{ (unsigned char *)"Memory",		T_CMD,				0, cmd_memory	},

#endif


/**********************************************************************************
 All other tokens (keywords, functions, operators) should be inserted in this table
**********************************************************************************/
#ifdef INCLUDE_TOKEN_TABLE
// the format is:
//    TEXT      	TYPE                P  FUNCTION TO CALL
// where type is T_NA, T_FUN, T_FNA or T_OPER argumented by the types T_STR and/or T_NBR
// and P is the precedence (which is only used for operators)

#endif


#if !defined(INCLUDE_COMMAND_TABLE) && !defined(INCLUDE_TOKEN_TABLE)
// General definitions used by other modules

#ifndef DEBUG_HEADER
#define DEBUG_HEADER
#endif
#endif

