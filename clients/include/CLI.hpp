#pragma once

#include <ncurses.h>

// NCURSES:
// Usually for every function there are 3 more versions:
// 		Fun(string)         			<-- perform action on stdscr window
// 		wFun(win, string)   			<-- perform action on win window
// 		mvFun(y, x, string) 			<-- move cursor to (y, x) and then perform action on stdscr window (combination of move(y,x) and printw(string))
// 		mvwFun(win, y, x, string)		<-- move cursor to (y, x) and then perform action on win window (combination of wmove(win, y,x) and wprintw(win, string))
// 		[# marks a function that has the w*, mv*, and mvw* variations]
//
// ===========================================================================================================================================================================
// Base functions:
//	initscr()					<-- start curses mode initializes the default window: stdscr
//	noecho()					<-- do no echo typed chars
//	raw()						<-- disable line buffering, disables signals
//	cbreak()					<-- disable line buffering, keeps signals enabled
//	keypad(stdscr, flag)		<-- toggle use of special keys like f(n), arrows, esc, CTRL+D, ...
//
//	refresh() [wrefresh(win)]	<-- draws (i.e. makes visible) the stdscr [win] with its modifications, must be called
//									after outputting data or modifying the windows
// 	wnoutrefresh(win)			<-- updates the (global) cursor buffer with the logical content of win, (mind the depth-test for overlapping windows)
//	doupdate()					<-- actually sending the cursor buffer to tty to change the appareance
//									N.B. refresh = wnoutrefresh + doupdate, so for multiple windows is good 
//										practice to call wnoutrefresh for each win (mind depth-test) and doupdate at the end only once
//										
//	start_color()				<-- start the color functionality	(has_colors() to check if the terminal supports colors)
//	endwin()					<-- greceful termination of ncurses
//
//	hline(char, len) #			<-- draws an horizontal line of len char chars from current cursor pos
//	vline(char, len) #			<-- draws a vertical line of len char chars from current cursor pos
//
//	getmaxyx(win, &row, &col)   <-- get number of cols and rows of this window [getmaxyx(stdscr, y, x) = LINES, COLS]
//	move(y, x) #				<-- move the cursor to the positon y, x
//	getyx(win, y, x) #			<-- get cursor position, relative to that window (is a macro so is not necessary to pass args as ref)
//	curs_set(x)					<-- change the visibility of the cursor: x=0[hidden] x=1[visible] x=2[very visible]
//
//	LINES and COLS				<-- macros that store the current indexes available for rows and cols of the terminal
//
// ===========================================================================================================================================================================
// I/O:
//		getch() #				<-- get a single char from stdin (but it only reads its once it receives enter/EOF, unless raw/cbreak are used) (noecho disables output)
//		getstr(char*) #			<-- equivalent to multiple calls of getch() until enter\EOF is passed 
//		scanw(...) #			<-- similar behaviour to C-scanf() [there's also vwscanw() #,  linked to C-vscanf()]
//									(string is stored inside the buffer argument)
//		addch(char) #			<-- put a single character into the current cursor location and advance the position of the cursor, can add attributes to the chars printed
//		addstr(char*) #			<-- prints a string, similar to calling addch() for each char of the string
//		printw("...", args) #	<-- print formatted output printf()-style
//
// ===========================================================================================================================================================================
// Attributes: customization when outputting characters, attribute values (A_BOLD, A_UNDERLINE, A_*) create a bitmask and threfore can be combined bitwising
//		A_NORMAL        Normal display (no highlight)
//		A_STANDOUT      Best highlighting mode of the terminal.
//		A_UNDERLINE     Underlining
//		A_REVERSE       Reverse video
//		A_BLINK         Blinking
//		A_DIM           Half bright
//		A_BOLD          Extra bright or bold
//		A_PROTECT       Protected mode
//		A_INVIS         Invisible or blank mode
//		A_ALTCHARSET    Alternate character set
//		A_CHARTEXT      Bit-mask to extract a character
//
//	attron(ATTR)										<-- activate ATTR on every subsequent output
//	attroff(ATTR)										<-- deactivate ATTR on every subsequent output
//	attrset(ATTR)										<-- overrides current attributes with ATTR
//															N.B. attr[on|off|set](ATTR) have an attr_(ATTR, *ops) equivalent to pass additional data to the caller
//																N. B. also attr_[on|off|set](ATTR, nullptr) = attr[on|off|set](ATTR)
//	attr_ok(ATTR)										<-- checks if attrs is supported by terminal
//	attr_get(attr_t *attrs, short *pair, void *opts)	<-- stores inside attrs and pair, the current attribute and color conf of the output
//	standend()											<-- disables all the current active attributes
//	chgat(nChars, ATTR, color, nullptr) #				<-- to change chars already present on the window use this, 
//															it applies ATTR on nChars (set it -1 to change until the end of the line) chars counted from the current cursor pos,
//															set color = 0 for no color, last one is always nullptr
//															N.B. each attr* function has a wattr* version to apply the change on a specific window
//
// ===========================================================================================================================================================================
// Windows:
//	WINDOW*	newwin(int height, int width, int starty, int startx)		<-- allocates memory for a sub window, starty:startx in the top left corner
//																			it is virtual in a sense that
//																			it just groups the chars inside it so they don't belong to stdscr anymore,
//																			without applying box() on it is therefore invisible
//
//	some members of WINDOW to manually modify the size, position or border char of the window
//		WINDOW.height
//		WINDOW.width
//		WINDOW.starty
//		WINDOW.startx
//		WINDOW.border.ls
//		WINDOW.border.rs
//		WINDOW.border.ts
//		WINDOW.border.bs
//		WINDOW.border.tl
//		WINDOW.border.tr
//		WINDOW.border.bl
//		WINDOW.border.br
//
//	delwin(WINDOW *local_win)				<-- cleas window memory
//											N. B. to clean its box call: wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' '), in fact
//											box(local_win, ' ', ' ') won't work becase it will not cancel the corners
//
//	getbegyx(local_win, y, x)				<-- get the window start coors, absolute: so relative to the screen
//	getparyx(child, y, x)					<-- get the window start coors, relative to its parsent screen (getparyx(stdscr, y, x) sets x=y=-1)
//	getmaxyxl(local_win, height, width)		<-- get the windows size
//
//	scr_dump("debug.dump")					<-- dumps screen info (so every window inside) to a file for debugging purpuses
//	scr_restore("debug.dump")				<-- restore the whole screen
//
//	putwin(win, "debug.dump")				<-- dumps win info to a file for debugging purpuses
//	getwin("debug.dump")					<-- restore win
//
// 	copywin (const srcWin, destWin,			<-- to copy a window into another
// 		0, 0,    		<- start coors of src to copy
// 		0, 0,    		<- start coors of dst to put values
// 		4, 19,   		<- maxRow and maxCol of dest to copy data to
// 		true/false)	    <- true: copy only non-space chars, false: copy every char
//
// ===========================================================================================================================================================================
// Borders: to separate windows
//	box(win, vertLineChar, horLineChar)						<-- draws a box around the window, putting vertLineChar
//																			on the vertical lines and horLineChar on the horizontal ones,
//																			passing 0 (not '0') draws a line; box, unlike wborder, will not draw the corners of the box
//	wborder(local_win, ls, rs, ts, bs, tl, tr, bl, br)		<-- draws a more complex box around the window, passing 0 (not '0'!) draws a simple line,  parameters:
// 																1. win: the window on which to operate
// 																2. ls: character to be used for the left side of the window 
// 																3. rs: character to be used for the right side of the window 
// 																4. ts: character to be used for the top side of the window 
// 																5. bs: character to be used for the bottom side of the window 
// 																6. tl: character to be used for the top left corner of the window 
// 																7. tr: character to be used for the top right corner of the window 
// 																8. bl: character to be used for the bottom left corner of the window 
// 																9. br: character to be used for the bottom right corner of the window
//
// ===========================================================================================================================================================================
// Colors: are definied in pairs (foreground [=text color] and background), base colors (COLOR_*) are defined inside ncurses.h
//		COLOR_BLACK   0
//		COLOR_RED     1
//		COLOR_GREEN   2
//		COLOR_YELLOW  3
//		COLOR_BLUE    4
//		COLOR_MAGENTA 5
//		COLOR_CYAN    6
//		COLOR_WHITE   7
//
//	init_pair(idPair, foreColor, backColor)				<-- creates a couple of two colors (later it can be used with attron(COLOR_PAIR(idPair)) )
//	init_color(COLOR_NAME, r, g, b)						<-- to change an existing color called COLOR_NAME with the new rgb (ranging from 0 to 1000)
//
//	can_change_color()						<-- to check wether the terminal allows chaning colors
//	color_content(COLOR_NAME)				<-- get foreground/background of COLOR_NAME
//	pair_content(COLOR_NAME)				<-- get rgb of COLOR_NAME
//
// ===========================================================================================================================================================================
// Appendix:
// To switch temporarly to normal tty mode and stop ncurses:
// 		initscr();
// 		... do something
// 		def_prog_mode();			<-- save ncurses state
// 		endwin();
// 		system("/bin/sh");
// 		reset_prog_mode();		<-- reset ncurses to use it again
// 		... do something else with ncurses
// 		endwin();
//
// Typical input loop:
//		while((ch = getch()) != <TERMINATION_LOOP_CHAR>)
// 		{
//			switch(ch)
// 			{
//				case KEY_*:
//					do something
// 					break;
//				case KEY_**:
//					do something else
// 					break;
//				...
// 			}
// 		}
//
// Basic pattern to cancel a char when del/backspace is pressed:
//		if ((ch == KEY_BACKSPACE || ch == 127) && (pos > 0))
//		{
//			pos--;
//			int y, x;
//			getyx(stdscr, y, x);
//			mvdelch(y, x - 1);
//			move(y, x - 1);
//			refresh();
//		}
//
// ASC characters:
// 		Upper left corner        --> ACS_ULCORNER
// 		Lower left corner        --> ACS_LLCORNER
// 		Lower right corner       --> ACS_LRCORNER
// 		Tee pointing right       --> ACS_LTEE
// 		Tee pointing left        --> ACS_RTEE
// 		Tee pointing up          --> ACS_BTEE
// 		Tee pointing down        --> ACS_TTEE
// 		Horizontal line          --> ACS_HLINE
// 		Vertical line            --> ACS_VLINE
// 		Large Plus or cross over --> ACS_PLUS
// 		Scan Line 1              --> ACS_S1
// 		Scan Line 3              --> ACS_S3
// 		Scan Line 7              --> ACS_S7
// 		Scan Line 9              --> ACS_S9
// 		Diamond                  --> ACS_DIAMOND
// 		Checker board (stipple)  --> ACS_CKBOARD
// 		Degree Symbol            --> ACS_DEGREE
// 		Plus/Minus Symbol        --> ACS_PLMINUS
// 		Bullet                   --> ACS_BULLET
// 		Arrow Pointing Left      --> ACS_LARROW
// 		Arrow Pointing Right     --> ACS_RARROW
// 		Arrow Pointing Down      --> ACS_DARROW
// 		Arrow Pointing Up        --> ACS_UARROW
// 		Board of squares         --> ACS_BOARD
// 		Lantern Symbol           --> ACS_LANTERN
// 		Solid Square Block       --> ACS_BLOCK
// 		Less/Equal sign          --> ACS_LEQUAL
// 		Greater/Equal sign       --> ACS_GEQUAL
// 		Pi                       --> ACS_PI
// 		Not equal                --> ACS_NEQUAL
// 		UK pound sign            --> ACS_STERLING
// ===========================================================================================================================================================================

class CLI
{
	public:
		CLI(void);
		~CLI(void);

		CLI(CLI const& other) = delete;
		CLI& operator=(CLI const& other) = delete;
		CLI(CLI& other) = delete;
		CLI& operator=(CLI& other) = delete;

		void loop(void);

	private:

};
