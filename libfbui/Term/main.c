
/*=========================================================================
 *
 * fbterm, a terminal emulator based on ggiterm for FBUI.
 * Copyright (C) 2004 Zachary T Smith, fbui@comcast.net
 * Portions from ggiterm are Copyright (C) by Aurelien Reynaud.
 *
 * This module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this module; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * (See the file COPYING in the main directory of this archive for
 * more details.)
 *
 *=======================================================================*/

#include <linux/fb.h>

#include "libfbui.h"
#include "fbterm.h"

#define BLINK_TIME 500

static Display *dpy = NULL;
static Window *win = NULL;
static Font *font = NULL;

extern struct winsize ws;
extern int master_fd;

extern int terminal_width, terminal_height;

static wchar_t *exposebuffer;
static char *exposebuffer_fg;
static char *exposebuffer_bg;

/* Global variables */
int vis_w, vis_h;
int cursor_x, cursor_y, cursor_x0, cursor_y0, cell_w, cell_h;
static RGB color[16] = {
	0,
	0xb00000,
	0xb000,
	0xb0b000,
	0xb0,
	0xb000b0,
	0xb0b0,
	0xb0b0b0,

	0,
	0xff0000,
	0xff00,
	0xffff00,
	0xff,
	0xff00ff,
	0xffff,
	0xffffff,
};

int default_bgcolor, default_fgcolor, fgcolor, bgcolor;
int blink_mode = 0, bold_mode = 0, invisible_mode = 0, reverse_mode = 0, underline_mode = 0, altcharset_mode = 0;
int autowrap = 1, force_cursor_mode = 0;
int region_top, region_bottom;
extern int linewrap_pending;

extern int (*fbtermPutShellChar) (unsigned char *, size_t *, wchar_t);

void redraw ()
{
	int j;
	for (j=0; j<terminal_height; j++) {
		int i;
		for (i=0; i<terminal_width; ) {
			int ix =i + j*terminal_width;
			wchar_t ch = exposebuffer[ix];
			char s[terminal_width];
			unsigned long fg, bg;
			char my_fg = exposebuffer_fg [ix];
			char my_bg = exposebuffer_bg [ix];
			fg = color [my_fg];
			bg = color [my_bg];

			short x0,y0;
			x0 = cell_w*i;
			y0 = cell_h*j;

			char all_blanks = 1;

			/* find maximal area that has the same bg/fg colors */
			int k=1;
			s[0] = ch;
			while ((k+i)<terminal_width && exposebuffer_fg[ix+k]==my_fg &&
				exposebuffer_bg[ix+k]==my_bg) 
			{
				ch = s[k] = exposebuffer[ix+k];
				if (ch != ' ')
					all_blanks = 0;
				k++;
			}
			s[k] = 0;
			fbui_fill_area (dpy, win, x0,y0,x0+(k*cell_w)-1,y0+cell_h-1, bg);

			if (!all_blanks)
				fbui_draw_string (dpy, win, NULL, x0,y0, s, fg);
			i += k;
		}
	}
}

void resize(int new_width, int new_height)
{
	int i, j;

	if (new_width==terminal_width && new_height==terminal_height)
		return;

	unsigned long size = new_width * new_height;
	wchar_t *newbuffer = (wchar_t*) malloc(sizeof(wchar_t) * size);
	char *newbuffer_fg = malloc(size);
	char *newbuffer_bg = malloc(size);

	if (!newbuffer || !newbuffer_fg || !newbuffer_bg)
		FATAL ("out of memory in resize()");

	for (j=0;j<size;j++) {
		newbuffer[j] = ' ';
		newbuffer_fg[j] = default_fgcolor;
		newbuffer_bg[j] = default_bgcolor;
	}

	for (j=0; j<terminal_height; j++) {
		for (i=0; i<terminal_width; i++) {
			if (i < new_width && j < new_height) {
				int ix =i + j*terminal_width;
				wchar_t ch = exposebuffer[ix];
				char fg = exposebuffer_fg [ix];
				char bg = exposebuffer_bg [ix];

				int ix2 = i + j*new_width;
				newbuffer[ix2] = ch;
				newbuffer_fg[ix2] = fg;
				newbuffer_fg[ix2] = bg;
			}
		}
	}

	free (exposebuffer);
	free (exposebuffer_fg);
	free (exposebuffer_bg);

	exposebuffer = newbuffer;
	exposebuffer_fg = newbuffer_fg;
	exposebuffer_bg = newbuffer_bg;

	terminal_width = new_width;
	terminal_height = new_height;

#if 0
	redraw();

	fbui_draw_line (dpy,win,0,0,100,100,RGB_YELLOW);
	fbui_flush (dpy,win);
#endif
}

void scroll_exposebuf_up(int num, int top, int bottom)
{
	int i,j,k;
	int y;
	for (j=top+num; j<=bottom; j+=num) {
		k=0;
		while (k < num) {
			y = j+k;
			if (y < terminal_height) 
			for (i=0; i<terminal_width; i++) {
				int ix = i+terminal_width*y;
				exposebuffer [ix-terminal_width] = exposebuffer[ix];
				exposebuffer_fg [ix-terminal_width] = exposebuffer_fg [ix];
				exposebuffer_bg [ix-terminal_width] = exposebuffer_bg [ix];
			}
			k++;
		}
	}
	k=0;
	while (k < num) {
		y = bottom-k-1;
		if (y < terminal_height) 
		for (i=0; i<terminal_width; i++) {
			int ix = i+terminal_width*y;
			exposebuffer [ix] = ' ';
			exposebuffer_fg [ix] = default_bgcolor;
			exposebuffer_bg [ix] = default_bgcolor;
		}
		k++;
	}
}


void scroll_exposebuf_down()
{
	int i,j;
	for (j=terminal_height-2; j>=0; j--) {
		for (i=0; i<terminal_width; i++) {
			int ix = i+terminal_width*j;
			exposebuffer [ix+terminal_width] = exposebuffer[ix];
			exposebuffer_fg [ix+terminal_width] = exposebuffer_fg [ix];
			exposebuffer_bg [ix+terminal_width] = exposebuffer_bg [ix];
		}
	}
	for (i=0; i<terminal_width; i++) {
		exposebuffer [i] = ' ';
		exposebuffer_fg [i] = default_bgcolor;
		exposebuffer_bg [i] = default_bgcolor;
	}
}



void MoveCursor (int, int);


wchar_t AsciiGetAltChar (wchar_t charcode)
{
		switch (charcode)
		{
			case '+': charcode = '>'; break; /* vt100 '+': arrow pointing right */
			case ',': charcode = '<'; break; /* vt100 ',': arrow pointing left */
			case '-': charcode = 0136; break; /* vt100 '-': arrow pointing up */
			case '.': charcode = 'v'; break; /* vt100 '.': arrow pointing down */
			case '0': charcode = '#'; break; /* vt100 '0': solid square block */
			case '`': charcode = '+'; break; /* vt100 '`': diamond */
			case 'a': charcode = ':'; break; /* vt100 'a': checker board (stipple) */
			case 'f': charcode = '\\'; break; /* vt100 'f': degree */
			case 'g': charcode = '#'; break; /* vt100 'g': plus/minus */
			case 'h': charcode = '#'; break; /* vt100 'h': board os squares */
			case 'j': charcode = '+'; break; /* vt100 'j': rightdown corner */
			case 'k': charcode = '+'; break; /* vt100 'k': upright corner */
			case 'l': charcode = '+'; break; /* vt100 'l': upleft corner */
			case 'm': charcode = '+'; break; /* vt100 'm': downleft corner */
			case 'n': charcode = '+'; break; /* vt100 'n': crossover */
			case 'o': charcode = '~'; break; /* vt100 'o': scan line 1 */
			case 'p': charcode = '-'; break; /* vt100 'p': scan line 3 */
			case 'q': charcode = '-'; break; /* vt100 'q': horizontal line */
			case 'r': charcode = '-'; break; /* vt100 'r': scan line 7 */
			case 's': charcode = '_'; break; /* vt100 's': scan line 9 */
			case 't': charcode = '+'; break; /* vt100 't': tee pointing right */
			case 'u': charcode = '+'; break; /* vt100 'u': tee pointing left */
			case 'v': charcode = '+'; break; /* vt100 'v': tee pointing up */
			case 'w': charcode = '+'; break; /* vt100 'w': tee pointing down */
			case 'x': charcode = '|'; break; /* vt100 'x': vertical line */
			case 'y': charcode = '<'; break; /* vt100 'y': less or equal */
			case 'z': charcode = '>'; break; /* vt100 'z': more or equal */
			case '{': charcode = '*'; break; /* vt100 '{': Pi */
			case '|': charcode = '!'; break; /* vt100 '|': not equal */
			case '}': charcode = 'f'; break; /* vt100 '}': UK pound */
			case '~': charcode = 'o'; break; /* vt100 '~': bullet */
		}
		return charcode;
}


void
fbtermBlinkCursor (int force)
{
	int x,y;
	x = cell_w*(int)(cursor_x/cell_w);
	y = cell_h*(int)(cursor_y/cell_h);

	if (force != CURSOR_HIDE)
	{
		fbui_fill_area (dpy, win,
			x, y, x + cell_w - 1, y + cell_h - 1, color[fgcolor]);
	}
	else
	{
		fbui_fill_area (dpy, win,
			x, y, x + cell_w - 1, y + cell_h - 1, color[bgcolor]);
	}
	fbui_flush (dpy, win);
}

void
fbtermDeleteChars (unsigned int nb_chars)
{
	int x, y;
	
	x = cell_w * (int)(cursor_x / cell_w);
	y = cell_h * (int)(cursor_y / cell_h);
	/* Copy the characters between current+nb_chars and EOL to current position */
	/*ggiFlush (vis);*/

	fbui_flush (dpy, win);
	fbui_copy_area (dpy, win, x+(nb_chars*cell_w), y, x, y,
		vis_w-x-(nb_chars*cell_w), cell_h);
}

void
fbtermInsertChars (unsigned int nb_chars)
{
	int x, y;
	
	x = cell_w * (int)(cursor_x / cell_w);
	y = cell_h * (int)(cursor_y / cell_h);

	fbui_copy_area (dpy, win, x, y, x+(nb_chars*cell_w), y, vis_w-x, cell_h);
	fbui_fill_area (dpy, win, x, y, nb_chars*cell_w, cell_h, color[default_bgcolor]);
}

void
fbtermEraseNChars (unsigned int nb_chars)
{
	int x, y;
	
	x = cell_w * (int)(cursor_x / cell_w);
	y = cell_h * (int)(cursor_y / cell_h);

	fbui_fill_area (dpy, win, x, y, nb_chars*cell_w, cell_h, color[default_bgcolor]);
}

void
fbtermEraseToEOL ()
{
	int x, y;
	
	x = cell_w * (int)(cursor_x / cell_w);
	y = cell_h * (int)(cursor_y / cell_h);

	fbui_fill_area (dpy, win, x, y, vis_w-x, cell_h, color[default_bgcolor]);
}

void
fbtermEraseFromBOL ()
{
	int x, y;
	
	x = cell_w * (int)(cursor_x / cell_w + 1);
	y = cell_h * (int)(cursor_y / cell_h);

	fbui_fill_area (dpy, win, 0, y, x, cell_h, color[default_bgcolor]);
}

void
fbtermEraseLine ()
{
	int y;
	
	y = cell_h * (int)(cursor_y / cell_h);

	fbui_fill_area (dpy, win, 0, y, vis_w, cell_h, color[default_bgcolor]);
}

void
fbtermEraseToEOD ()
{
	int y;
	
	fbtermEraseToEOL ();
	y = cell_h * (int)(cursor_y / cell_h + 1);

	fbui_fill_area (dpy, win, 0, y, vis_w, vis_h-y, color[default_bgcolor]);
}

void
fbtermEraseFromBOD ()
{
	int y;
	
	fbtermEraseFromBOL ();
	y = cell_h * (int)(cursor_y / cell_h);

	fbui_fill_area (dpy, win, 0, 0, vis_w, y, color[default_bgcolor]);
}

void
fbtermEraseDisplay ()
{
	int i;

	fbui_fill_area (dpy, win, 0,0, 
		terminal_width*cell_w-1,
		terminal_height*cell_h-1,
		color [default_bgcolor]);
	fbui_flush (dpy, win);

	int c = default_bgcolor;
	int size = terminal_width * terminal_height;
	for (i=0; i<size; i++) {
		exposebuffer[i] = ' ';
		exposebuffer_fg[i] = c;
		exposebuffer_bg[i] = c;
	}
}

void
fbtermScrollUp (unsigned int nb_lines)
{
	debug (DEBUG_DETAIL, "Scrolling %d lines in region %d-%d", nb_lines, region_top, region_bottom);

#if 0
	/* too slow due to video memory read speed */

	fbui_copy_area (dpy, win, 0, (region_top+nb_lines)*cell_h, 
		0, region_top*cell_h,
		vis_w, (region_bottom-region_top-nb_lines)*cell_h);
	int y = (region_bottom - nb_lines)*cell_h;
	fbui_fill_area (dpy, win, 0, y, vis_w, vis_h, color[default_bgcolor]);
#endif

	scroll_exposebuf_up (nb_lines, region_top, region_bottom);
	redraw();
}

void
fbtermScrollDown (unsigned int nb_lines)
{
	debug (DEBUG_DETAIL, "Scrolling %d lines in region %d-%d", nb_lines, region_top, region_bottom);
#if 0
	/* too slow due to video memory read speed */

	fbui_copy_area (dpy, win, 0, region_top*cell_h, 
		0, (region_top+nb_lines)*cell_h,
		vis_w, (region_bottom-region_top-nb_lines)*cell_h);
	int y = region_top*cell_h;
	fbui_fill_area (dpy, win, 0, 0, vis_w, y-1, color[default_bgcolor]);
#endif

	scroll_exposebuf_down();
	redraw();
}

void
fbtermNextPos ()
{
	MoveCursor (cursor_x + cell_w, cursor_y);
}


int
FBUIWriteChar (wchar_t charcode)
{
	int cur_bgcolor, cur_fgcolor, dummy_color;
	
	if (linewrap_pending)
	{
		MoveCursor (cursor_x0, cursor_y + cell_h);
	}
	cur_bgcolor = bgcolor;
	cur_fgcolor = fgcolor;
	if (altcharset_mode)
	{
		charcode = AsciiGetAltChar (charcode);
	}
	if (reverse_mode)
	{
		dummy_color = cur_bgcolor;
		cur_bgcolor = cur_fgcolor;
		cur_fgcolor = dummy_color;
	}
	if (invisible_mode)
	{
		cur_fgcolor = cur_bgcolor;
	}
	if (bold_mode)
	{
		cur_fgcolor += 8;
	}

	fbui_fill_area (dpy, win, cursor_x, cursor_y, cursor_x + cell_w - 1, cursor_y + cell_h - 1, color[cur_bgcolor]);
	char s[2] = { (char)charcode, 0 };

	fbui_draw_string (dpy, win, NULL, cursor_x, cursor_y, s, color[cur_fgcolor]);

	/* Save to buffer for later expose */
	int i = cursor_x / cell_w;
	int j = cursor_y / cell_h;
	int ix = i + j * terminal_width;
	int size = terminal_width * terminal_height;
	if (ix < size) {
		exposebuffer [ix] = charcode;
		exposebuffer_fg [ix] = cur_fgcolor;
		exposebuffer_bg [ix] = cur_bgcolor;
	}

	if (underline_mode) {
		fbui_draw_hline (dpy, win, cursor_x, cursor_x+cell_w-1, cursor_y+cell_h-1,
			color[cur_fgcolor]);
		fbui_flush (dpy, win);
	} 

	return 0;
}



void
HandleFBUIEvents (unsigned char *shellinput, size_t *shellinput_size)
{
	struct timeval tv;
	int i;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	Event ev;
	unsigned char event_num;

	usleep (3000);
	if (fbui_poll_event (dpy, &ev, FBUI_EVENTMASK_ALL))
		return;

	event_num = ev.type;

	if (ev.win != win) {
printf ("event id = %d, window id %d\n", ev.id, win->id);
		FATAL ("event not for fbterm window");
	}

	if (event_num == FBUI_EVENT_EXPOSE) {
		redraw();
	}
	else if (event_num == FBUI_EVENT_MOVE_RESIZE) {
		vis_w = ev.width;
		vis_h = ev.height;
		int new_width = vis_w / cell_w;
		int new_height = vis_h / cell_h;
		resize (new_width, new_height);

		// update the OS
		ws.ws_col = terminal_width;
		ws.ws_row = terminal_height;
		ws.ws_xpixel = vis_w;
		ws.ws_ypixel = vis_h;
        	if (ioctl (master_fd, TIOCSWINSZ, &ws) < 0) {
			WARNING("unable to set tty window size");
		}
	}
	else if (event_num == FBUI_EVENT_ENTER) {
		printf ("fbterm got Enter\n");
		return;
	}
	else if (event_num == FBUI_EVENT_LEAVE) {
		printf ("fbterm got Leave\n");
		return;
	}
	else if (event_num == FBUI_EVENT_ACCEL) {
		// Not using accelerators
		return;
	}
	else if (event_num == FBUI_EVENT_KEY) {
		short ch = fbui_convert_key (dpy, ev.key);
		if (!ch) 
			return;

		if (SHELLINPUT_SIZE - *shellinput_size >= (MB_CUR_MAX>3?MB_CUR_MAX:3))
		switch (ch) {
		case '\n':
			shellinput[*shellinput_size] = '\n';
			(*shellinput_size)++;
			break;

		case 8:
		case 9:
		case FBUI_DEL:
			shellinput[*shellinput_size] = 0x08;
			(*shellinput_size)++;
			break;

		default:
			fbtermPutShellChar (shellinput, shellinput_size, ch);
			break;

		case FBUI_F1:
			memcpy (shellinput+*shellinput_size, "\033OP", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F2:
			memcpy (shellinput+*shellinput_size, "\033OQ", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F3:
			memcpy (shellinput+*shellinput_size, "\033OR", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F4:
			memcpy (shellinput+*shellinput_size, "\033OS", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F5:
			memcpy (shellinput+*shellinput_size, "\033Ot", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F6:
			memcpy (shellinput+*shellinput_size, "\033Ou", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F7:
			memcpy (shellinput+*shellinput_size, "\033Ov", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F8:
			memcpy (shellinput+*shellinput_size, "\033Ol", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F9:
			memcpy (shellinput+*shellinput_size, "\033Ow", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_F10:
			memcpy (shellinput+*shellinput_size, "\033Ox", 3);
			(*shellinput_size) += 3;
			break;

		case FBUI_UP:
			memcpy (shellinput+*shellinput_size, "\033OA", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_DOWN:
			memcpy (shellinput+*shellinput_size, "\033OB", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_LEFT:
			memcpy (shellinput+*shellinput_size, "\033OD", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_RIGHT:
			memcpy (shellinput+*shellinput_size, "\033OC", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_PGUP:
			memcpy (shellinput+*shellinput_size, "\033Os", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_PGDN:
			memcpy (shellinput+*shellinput_size, "\033On", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_INS:
			memcpy (shellinput+*shellinput_size, "\033[L", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_HOME:
			memcpy (shellinput+*shellinput_size, "\033Oq", 3);
			(*shellinput_size) += 3;
			break;
		case FBUI_END:
			memcpy (shellinput+*shellinput_size, "\033Op", 3);
			(*shellinput_size) += 3;
			break;
		}
	}
}


void FBUIMapColors ()
{
	int i;
}

int
FBUIInit_graphicsmode (int vc,short cols,short rows,short xrel,short yrel)
{
	int err;
	char dummy[64];
	long size = rows * cols;

	terminal_width = cols;
	terminal_height = rows;

printf ("cols=%d rows=%d\n",cols,rows);

	exposebuffer=(wchar_t*) malloc(size * sizeof(wchar_t));
	exposebuffer_fg=(char*) malloc(size);
	exposebuffer_bg=(char*) malloc(size);

        font = font_new ();
        if (!pcf_read (font, "courR14.pcf")) {
                font_free (font);
		FATAL ("cannot load font");
        }

	cell_h = font->ascent + font->descent;

	if (!cell_h) {
		fbui_window_close (dpy, win);
		printf ("oops, invalid font data\n");
		exit(1);
	}
	
	cell_w = font->widths [' ' - font->first_char];

	if (!cell_w) {
		cell_w = font->widths ['W' - font->first_char];
		if (!cell_w) {
			fbui_window_close (dpy, win);
			printf ("oops, invalid font data\n");
			exit(1);
		}
	}

	int default_w = cell_w * cols;
	int default_h = cell_h * rows;
printf ("fbterm: trying for window size %d,%d to provide %dx%d text\n", default_w, default_h, rows,cols);

	default_fgcolor = 7;
	default_bgcolor = 0;
	fgcolor = default_fgcolor;
	bgcolor = default_bgcolor;

	int argc=1;
	char *argv[1] = {"foo"};

	if (xrel > 999) {
		xrel = 2;
		yrel = 15;
	}

        short win_w, win_h;

	long fg;
	long bg = color[default_bgcolor];

	dpy = fbui_display_open ();
        if (!dpy)
                FATAL ("cannot open display");
	
        win = fbui_window_open (dpy, default_w, default_h, 
		&win_w, &win_h,
		9999,9999,
		0, +20, 
		&fg, &bg, 
		"fbterm", "", 
		FBUI_PROGTYPE_APP, 
		false,false, 
		vc,
		true, 
		false,
		false, argc,argv);
        if (!win)
                FATAL ("cannot create window");

	vis_w = win_w;
	vis_h = win_h;

	if (win_w < 1 || win_h < 1) {
		printf ("Don't have window dimensions yet.\n");
	} else {
		printf ("Window dimensions %d,%d (%dx%d) text\n", 
			win_w,win_h,win_w/cell_w,win_h/cell_h);
	}

	cursor_x0 = 0;
	cursor_y0 = 0;

	if (fbui_set_font (dpy, win, font))
		FATAL ("cannot set font");
	
	return 0;
}


#ifdef DEBUG
void ShowGrid (void)
{
	int x, y;
	
	ggiSetGCForeground (vis, color[2]);
	for (x = 0; x < vis_w; x += cell_w)
	{
		ggiDrawVLine (vis, x, 0, vis_h);
	}
	for (y = 0; y < vis_h; y += cell_h)
	{
		ggiDrawHLine (vis, 0, y, vis_w);
	}
}
#endif /* DEBUG */

void FBUIExit (void)
{
	fbui_window_close (dpy, win);
	fbui_display_close (dpy);
}
