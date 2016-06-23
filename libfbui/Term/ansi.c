
#include <linux/fb.h>

#include "fbterm.h"


void fbtermEraseLine ();
void fbtermEraseToEOL ();
void fbtermEraseFromBOL ();
void fbtermEraseDisplay ();
void fbtermEraseToEOD ();
void fbtermEraseFromBOD ();
void fbtermInsertChars (unsigned int);
void fbtermEraseNChars (unsigned int);
void fbtermDeleteChars (unsigned int);
void fbtermScrollUp (unsigned int);
void fbtermScrollDown (unsigned int);
void fbtermNextPos ();

int savedcursor_x, savedcursor_y;
int saved_blink, saved_bold, saved_invisible, saved_reverse, saved_underline, saved_altcharset;
int linewrap_pending;

extern wchar_t last_char;
extern int cursor_x, cursor_y, cursor_x0, cursor_y0, cell_w, cell_h;
extern int terminal_width, terminal_height;
extern int blink_mode, bold_mode, invisible_mode, reverse_mode, underline_mode, altcharset_mode;
extern int autowrap;
extern int force_cursor_mode;
extern int region_top, region_bottom;
extern int (*fbtermWriteChar) (wchar_t);


extern char* tabstops;

extern unsigned long color[];
extern int default_bgcolor, default_fgcolor, fgcolor, bgcolor;
extern ggi_visual_t vis;

int min (int a, int b)
{
	return (b<a?b:a);
}


void MoveCursor (int x, int y)
{
	debug (DEBUG_DETAIL, "Entering with cursor position (%d,%d)", cursor_x/cell_w+1, cursor_y/cell_h+1);

	cursor_x = x;
	cursor_y = y;
	while (cursor_y >= region_bottom * cell_h)
	{
		fbtermScrollUp (1);
		cursor_y -= cell_h;
	}
	if (autowrap && cursor_x > (terminal_width-1) * cell_w)
	{
		cursor_x = cursor_x0 + (terminal_width-1) * cell_w;
		linewrap_pending = 1;
	}
	else
	{
		linewrap_pending = 0;
	}
	debug (DEBUG_DETAIL, "Leaving with cursor position (%d,%d)", cursor_x/cell_w+1, cursor_y/cell_h+1);
	if (linewrap_pending) debug (DEBUG_DETAIL, "Line wrap pending");
}


/*------------------------------------------------------------------------------
| Cursor motion                                                                |
------------------------------------------------------------------------------*/

void do_CR ()
{
	debug (DEBUG_DETAIL, "Special character code [CR] detected");
	MoveCursor (cursor_x0, cursor_y);
}

void do_LF ()
{
	debug (DEBUG_DETAIL, "Special character code [LF] detected");
	MoveCursor (cursor_x, cursor_y + cell_h);
}

void do_cub (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'cub %d' detected", p1);
	/* Move cursor p1 characters to the left */
	MoveCursor (cursor_x - (cell_w * p1), cursor_y);
}

void do_cud (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'cud %d' detected", p1);
	/* Move cursor down p1 lines */
	MoveCursor (cursor_x, cursor_y + (cell_h * p1));
}

void do_cuf (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'cuf %d' detected", p1);
	/* Move cursor p1 characters to the right */
	MoveCursor (cursor_x + (cell_w * p1), cursor_y);
}

void do_cup (unsigned int row, unsigned int column)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'cup %d %d' detected", row, column);
	MoveCursor (cursor_x0 + (column - 1) * cell_w, cursor_y0 + (row - 1) * cell_h);
}

void do_cuu (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'cuu %d' detected", p1);
	/* Move cursor up p1 lines */
	MoveCursor (cursor_x, cursor_y - (cell_h * p1));
}

void do_home ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'home' detected");
	MoveCursor (cursor_x0, cursor_y0);
}


/*------------------------------------------------------------------------------
| Mode handling                                                                |
------------------------------------------------------------------------------*/

void do_blink ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'blink' or part of 'sgr' detected");
	debug (DEBUG_DETAIL, "Entering blink mode");
	blink_mode = 1;
}

void do_bold ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'bold' or part of 'sgr' detected");
	debug (DEBUG_DETAIL, "Entering bold mode");
	bold_mode = 1;
}

void do_invis ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'invis' or part of 'sgr' detected");
	debug (DEBUG_DETAIL, "Entering invisible mode");
	invisible_mode = 1;
}

void do_rev ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'rev|smso' or part of 'sgr' detected");
	debug (DEBUG_DETAIL, "Entering reverse video mode");
	reverse_mode = 1;
}

void do_rmacs ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'rmacs' or part of 'sgr|sgr0' detected");
	debug (DEBUG_DETAIL, "Leaving altcharset mode");
	altcharset_mode = 0;
}

void do_smacs ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'smacs' or part of 'sgr' detected");
	debug (DEBUG_DETAIL, "Entering altcharset mode");
	altcharset_mode = 1;
}

void do_smul ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'smul' or part of 'sgr' detected");
	debug (DEBUG_DETAIL, "Entering underline mode");
	underline_mode = 1;
}

void do_leave_all_modes ()
{
	debug (DEBUG_DETAIL, "Leaving all modes except altcharset");
	bold_mode = 0;
	blink_mode = 0;
	underline_mode = 0;
	reverse_mode = 0;
	invisible_mode = 0;
	altcharset_mode = 0;
	fgcolor = default_fgcolor;
	bgcolor = default_bgcolor;
}

/*------------------------------------------------------------------------------
| Area clear                                                                   |
------------------------------------------------------------------------------*/

void do_ed ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'ed' detected");
	fbtermEraseToEOD ();
}

void do_el ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'el' detected");
	fbtermEraseToEOL ();
}

void do_el1 ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'el1' detected");
	fbtermEraseFromBOL ();
}

/*------------------------------------------------------------------------------
| Insert/Delete characters                                                     |
------------------------------------------------------------------------------*/

void do_ich (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'ich %d' detected", p1);
	/* Insert p1 characters at current position */
	fbtermInsertChars (p1);
}

void do_dch (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'dch %d' detected", p1);
	/* Delete p1 characters */
	fbtermDeleteChars (p1);
}

void do_ech (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'ech %d' detected", p1);
	/* Erase p1 characters, starting at current position */
	fbtermEraseNChars (p1);
}


/*------------------------------------------------------------------------------
| Scrolling                                                                    |
------------------------------------------------------------------------------*/

void do_indn (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'indn %d' detected", p1);
	/* Scroll forward p1 lines */
	fbtermScrollUp (p1);
}

void do_rin (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'rin %d' detected", p1);
	/* Scroll back p1 lines */
	fbtermScrollDown (p1);
}

void do_change_region (unsigned int p1, unsigned int p2)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'csr %d %d' detected", p1, p2);
	/* Set scrolling region to lines p1->p2 */
	region_top = p1-1;
	region_bottom = p2;
}

/*------------------------------------------------------------------------------
| Tabs                                                                         |
------------------------------------------------------------------------------*/

void do_cbt ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'cbt' detected");
	/* Back to preceding tap stop */
	do
	{
		MoveCursor (cursor_x - cell_w, cursor_y);
	}
	while (cursor_x/cell_w && !tabstops[cursor_x/cell_w]);
}

void do_ht ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'ht' detected");
	/* Go to the next tab stop */
	do
	{
		MoveCursor (cursor_x + cell_w, cursor_y);
	}
	while (cursor_x/cell_w < terminal_width-1 && !tabstops[cursor_x/cell_w]);
}

void do_hts ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'hts' detected");
	/* Set a tab stop in current column */
	tabstops[cursor_x/cell_w] = 1;
}

void do_tbc ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'tbc' detected");
	/* Clear all tab stops */
	memset (tabstops, 0, terminal_width);
	/*tabstops[0] = 1;
	tabstops[terminal_width] = 1;*/
}

void do_clear_current_tab ()
{
	/* vt100 but not in terminfo */
	tabstops[cursor_x/cell_w] = 0;
}

/*------------------------------------------------------------------------------
| Colors                                                                       |
------------------------------------------------------------------------------*/

void do_op ()
{
	debug (DEBUG_DETAIL, "Escape Sequence 'op' detected");
	/* Reset fg and bg colors to their default values */
	fgcolor = default_fgcolor;
	bgcolor = default_bgcolor;
}

void do_setab (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'setab %d' detected", p1);
	/* Set bg color to color #p1 */
	if (p1 == 9) bgcolor = default_bgcolor;
	else bgcolor = p1;
}

void do_setaf (unsigned int p1)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'setaf %d' detected", p1);
	/* Set fg color to color #p1 */
	if (p1 == 9) fgcolor = default_fgcolor;
	else fgcolor = p1;
}

/*------------------------------------------------------------------------------
| User strings                                                                 |
------------------------------------------------------------------------------*/

void do_identify (unsigned char *shellinput, size_t *shellinput_size)
{
	debug (DEBUG_DETAIL, "User string 'u9' or Enquiry detected");
	/* Enquire sequence sent to identify the terminal type
       Should answer by sending the u8 user string	*/
	memcpy (shellinput + *shellinput_size, "\033[?1;0c", 7);
	*shellinput_size += 7;
}

void do_send_cursor_position (unsigned char *shellinput, size_t *shellinput_size)
{
	debug (DEBUG_DETAIL, "Query cursor position detected");
	/* Cursor position request */
	*shellinput_size += sprintf ((char*)shellinput, "\033[%d;%dR", cursor_y / cell_h + 1, cursor_x / cell_w + 1);
}

void do_send_status (unsigned char *shellinput, size_t *shellinput_size)
{
	debug (DEBUG_DETAIL, "Terminal status report requested");
	/* Request terminal status
       \E[0n if OK, \E[3n if NOK (but we're always OK, aren't we?) */
	memcpy (shellinput + *shellinput_size, "\033[0n", 4);
	*shellinput_size += 4;
}


/*------------------------------------------------------------------------------
| Others                                                                       |
------------------------------------------------------------------------------*/

void do_BELL ()
{
	debug (DEBUG_DETAIL, "Special character code [BELL] detected");
	/* This should be a visual bell! */
}

void do_rep (unsigned int count)
{
	debug (DEBUG_DETAIL, "Escape Sequence 'rep %d' detected", count);
	/* Repeat last character count times */
	while (count)
	{
		fbtermWriteChar (last_char);
		fbtermNextPos ();
		count--;
	}
}

	
int
HandleEscapeSequences (unsigned char **buf, size_t *bufsize, unsigned char *shellinput, size_t *shellinput_size)
{
	size_t index;
	unsigned char *escape;
	unsigned int param[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int in_ansi=0, in_escape=0, num_param=1, finished=0, i, ansi_question_mark=0;

	escape = *buf;
	index = 0;
	
	do
	{
		if (!in_escape && !in_ansi)
		{
			finished = 1;
			switch (escape[index])
			{
				case 000: /* ignored */
					break;
				case 005:
					if (SHELLINPUT_SIZE - *shellinput_size >= 5) do_identify (shellinput, shellinput_size);
					else return 2;
					break;
				case 007:		/* Bell (7) */
					do_BELL ();
					break;
				case 010:		/* Backspace */
					do_cub (1);
					break;
				case 011:		/* Tab expansion */
					do_ht ();
					break;
				case 012:		/* Line feed (10) */
				case 013:
				case 014:
						do_LF ();
					break;
				case 015:		/* Carriage return (13) */
					do_CR ();
					break;
				case 016:
					do_smacs ();
					break;
				case 017:
					do_rmacs ();
					break;
				case 033:		/*  Escape character */
					in_escape = 1;
					finished = 0;
					break;
				case 0177:		/* for FreeBSD */
					do_cub (1);
					break;
				default:
					return 1;
			}
			index++;
			continue;
		}
		if (in_escape && !in_ansi)
		{
			finished = 1;
			switch (escape[index])
			{
				case '[':
					in_ansi = 1;
					finished = 0;
					break;
				/* Character set designator (we don't care) */
				case '(': /* G0 */
				case ')': /* G1 */
					if (index != 1) return 1;
					finished = 0;
					break;
				/* Character set */
				case '0': /* line drawing set */
					if (index != 2) return 1;
					switch (escape[1])
					{
						case '(':
							altcharset_mode = 1;
							break;
						case ')': break;
					}
					finished = 1;
				case '1': /* alternate character ROM */
				case '2': /* alternate character ROM (graphics) */
				case 'A': /* United Kingdom */
				case 'B': /* US ASCII */
					if (index != 2) return 1;
					switch (escape[1])
					{
						case '(':
							altcharset_mode = 0;
							break;
						case ')': break;
					}
					finished = 1;
					break;
				case 'D':
					do_indn (1);
					break;
				case 'E':
					do_CR ();
					do_LF ();
					break;
				case 'H':
					do_hts ();
					break;
				case 'M':
					do_rin (1);
					break;
				case 'Z': /* identify, same as \E[c */
					if (SHELLINPUT_SIZE - *shellinput_size >= 7) do_identify (shellinput, shellinput_size);
					else return 2;
					break;
				case '>':
					/*force_cursor_mode = 1;*/
					break;
				case '=':
					/*force_cursor_mode = 0;*/
					break;
				case '7':
					/* Should we save current colors too? */
					saved_blink = blink_mode;
					saved_bold = bold_mode;
					saved_invisible = invisible_mode;
					saved_reverse = reverse_mode;
					saved_underline = underline_mode;
					saved_altcharset = altcharset_mode;
					savedcursor_x = cursor_x;
					savedcursor_y = cursor_y;
					break;
				case '8':
					blink_mode = saved_blink;
					bold_mode = saved_bold;
					invisible_mode = saved_invisible;
					reverse_mode = saved_reverse;
					underline_mode = saved_underline;
					altcharset_mode = saved_altcharset;
					cursor_x = savedcursor_x;
					cursor_y = savedcursor_y;
					break;
				default:
					return 1;
			}
			index++;
			continue;
		}
		if (in_ansi)
		{
			finished = 1;
			switch (escape[index])
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					param[num_param-1] = param[num_param-1]*10+escape[index]-48;
					finished = 0;
					break;
				case ';':
					num_param++;
					finished = 0;
					break;
				case '?':
					if (escape[index-1] != '[')
					{
						return 1;
					}
					ansi_question_mark = 1;
					finished = 0;
					break;
				case '@':
					if (!param[0]) param[0] = 1;
					do_ich (param[0]);
					break;
				case 'A':
					if (!param[0]) param[0] = 1;
					do_cuu (param[0]);
					break;
				case 'B':
					if (!param[0]) param[0] = 1;
					do_cud (param[0]);
					break;
				case 'C':
					if (!param[0]) param[0] = 1;
					do_cuf (param[0]);
					break;
				case 'D':
					if (!param[0]) param[0] = 1;
					do_cub (param[0]);
					break;
				case 'F':
					if (!param[0]) param[0] = 1;
					do_rep (param[0]);
					break;
				case 'H':
				case 'f':
					if (!param[0]) param[0] = 1;
					if (!param[1]) param[1] = 1;
					do_cup (param[0], param[1]);
					break;
				case 'J':
					switch (param[0])
					{
						case 0:
							/* Erase from cursor to end of screen */
							do_ed ();
							break;
						case 1:
							/* Erase from beginning of screen to cursor */
							/* vt100 but no associated capability */
							fbtermEraseFromBOD ();
							break;
						case 2:
							/* Erase entire screen */
							/* vt100 but no associated capability */
							fbtermEraseDisplay ();
							break;
						default:
							return 1;
					}
					break;
				case 'K':
					switch (param[0])
					{
						case 0:
							/* Erase from cursor to end of line */
							do_el ();
							break;
						case 1:
							/* Erase from beginning of line to cursor */
							do_el1 ();
							break;
						case 2:
							/* Erase entire line containing cursor */
							/* vt100 but no associated capability */
							fbtermEraseLine ();
							break;
						default:
							return 1;
					}
					break;
				case 'P':
					if (!param[0]) param[0] = 1;
					do_dch (param[0]);
					break;
				case 'S':
					if (!param[0]) param[0] = 1;
					do_indn (param[0]);
					break;
				case 'T':
					if (!param[0]) param[0] = 1;
					do_rin (param[0]);
					break;
				case 'X':
					if (!param[0]) param[0] = 1;
					do_ech (param[0]);
					break;
				case 'Z':
					do_cbt ();
					break;
				case 'c':
					if (param[0]) return 1;
					if (SHELLINPUT_SIZE - *shellinput_size >= 7) do_identify (shellinput, shellinput_size);
					else return 2;
					break;
				case 'g':
					switch (param[0])
					{
						case 3:
							do_tbc ();
							break;
						case 0:
							do_clear_current_tab ();
							break;
						default:
							return 1;
					}
					break;
				case 'h':
					if (ansi_question_mark)
					{
						for (i = 0; i < num_param; i++)
						{
							switch (param[i])
							{
								case 1:
									force_cursor_mode = 1;
									break;
								case 7:
									autowrap = 1;
									break;
								/* the other modes are not implemented */
								case 3:
								case 4:
								case 5:
								case 6:
								case 8:
								case 9:
									break;
								default:
									return 1;
							}
						}
					}
					else
					{
						if (param[0] != 20) return 1;
					}
					break;
				case 'l':
					if (ansi_question_mark)
					{
						for (i = 0; i < num_param; i++)
						{
							switch (param[i])
							{
								case 1:
									force_cursor_mode = 0;
									break;
								case 7:
									autowrap = 0;
									break;
								/* the other modes are not implemented */
								case 2:
								case 3:
								case 4:
								case 5:
								case 6:
								case 8:
								case 9:
									break;
								default:
									return 1;
							}
						}
					}
					else
					{
						if (param[0] != 20) return 1;
					}
					break;
				case 'm':
					for (i = 0; i < num_param; i++)
					{
						switch (param[i])
						{
							case 0:
								do_leave_all_modes ();
								break;
							case 1:
								do_bold ();
								break;
							case 4:
								do_smul ();
								break;
							case 5:
								do_blink ();
								break;
							case 7:
								do_rev ();
								break;
							case 8:
								do_invis ();
								break;
							default:
								if (param[i]/10 == 3)
								{
									do_setaf (param[i] - 30);
									break;
								}
								if (param[i]/10 == 4)
								{
									do_setab (param[i] - 40);
									break;
								}
								return 1;
						}
					}
					break;
				case 'n':
					switch (param[0])
					{
						case 5: /* status report */
							if (SHELLINPUT_SIZE - *shellinput_size >= 4)
								do_send_status (shellinput, shellinput_size);
							else return 2;
							break;
						case 6: /* curosr position */
							if (SHELLINPUT_SIZE - *shellinput_size >= MAX_PARAM_SIZE*2+5)
								do_send_cursor_position (shellinput, shellinput_size);
							else return 2;
							break;
						default:
							return 1;
					}
					break;
				case 'r':
					if (!param[0]) param[0] = 1;
					if (!param[1]) param[0] = terminal_height;
					do_change_region (param[0], param[1]);
					break;
				default:
					return 1;
			}
		}
		index++;
	}
	while (index<(*bufsize) && !finished && num_param<=MAX_PARAM_NUM && param[num_param]<=MAX_PARAM_VALUE);
	
	if (finished)
	{
		(*buf) += index;
		(*bufsize) -= index;
		return 0;
	}
	if (index>=(*bufsize)) return 2;
	return 1;
}
