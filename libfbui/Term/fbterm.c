
#define VERSION "0.1"
#include "fbterm.h"

/* External functions and variables */

int HandleEscapeSequences (unsigned char **, size_t *, unsigned char*, size_t*);
void MoveCursor (int, int);

void fbtermNextPos ();
int FBUIWriteChar (wchar_t);
int FBUIInit_graphicsmode (int,short,short,short,short);
void HandleFBUIEvents (unsigned char *, size_t *);
void FBUIExit (void);
void fbtermBlinkCursor (int);
#ifdef DEBUG
void ShowGrid (void);
#endif /* DEBUG */


extern int vis_w, vis_h;
extern int cursor_x, cursor_y, cursor_x0, cursor_y0, cell_w, cell_h;
extern int altcharset_mode;
extern int region_top, region_bottom;

extern char **environ;

/* Global variables */
int terminal_width, terminal_height;
int quit = 0;
#ifdef DEBUG
int debuglevel = 0;
#endif /* DEBUG */
wchar_t last_char;
char result[SHELLOUTPUT_SIZE*4+1];
char *tabstops;


int (*fbtermWriteChar) (wchar_t charcode);
int (*fbtermGetShellChar) (wchar_t *charcode, unsigned char **inputbuf,
			   size_t *inputsize);
int (*fbtermPutShellChar) (unsigned char *buffer,
			   size_t *index, wchar_t charcode);


int
DefaultGetShellChar (wchar_t *charcode, unsigned char **inputbuf,
		    size_t *inputsize)
{
#ifdef MULTIBYTE
	size_t bytes;
	
	if (altcharset_mode)
	{
		/* we bypass the multibyte handling */
		*charcode = (*inputbuf)[0];
		(*inputsize)--;
		(*inputbuf)++;
		return 0;
	}
	bytes = mbtowc (charcode, (char*) *inputbuf, *inputsize);
	switch (bytes)
	{
		case -1:
			/* invalid character encountered */
			debug (DEBUG_INFO,
				       "Invalid byte, skipping it");
			(*inputsize)--;
			(*inputbuf)++;
			return 1;
		case -2:
			/* incomplete input */
			debug (DEBUG_INFO,
			       "Incomplete input");
			return 2;
		default:
			(*inputsize) -= bytes;
			(*inputbuf) += bytes;
			return 0;
	}
#else
	*charcode = (*inputbuf)[0];
	(*inputsize)--;
	(*inputbuf)++;
	return 0;
#endif
}

int
DefaultPutShellChar (unsigned char *buffer, size_t *index, wchar_t charcode)
{
# ifdef MULTIBYTE
	(*index) += wctomb ((char*) buffer, charcode);
# else
	buffer[*index] = (unsigned char)(charcode & 0xFF);
	(*index)++;
# endif
	return 0;
}

void
fbtermUsage (void)
{
	fprintf (stderr,
		 "\n"
		 "fbterm v%s by Z. Smith\n"
		 "Based on ggiterm v0.6.2 by A. Reynaud\n"
		 "\n"
		 "Usage: fbterm [OPTIONS...]\n"
		 "Where option is one or more of:\n"
		 "	--help\n", VERSION);
	fprintf (stderr,
		"	--debuglevel n           (%d:none, %d:info, %d:detailed, %d:total, default:%d)\n",
		DEBUG_NONE, DEBUG_INFO, DEBUG_DETAIL, DEBUG_TOTAL, DEBUG_NONE);
	fprintf (stderr,
		"	--font <font file>\n"
		"	--font-size n            (default: %d)\n\n", DEFAULT_FONTSIZE);
}

void
child_died ()
{
	quit = 2;
}


#ifdef DEBUG
char* BinaryToPrintable (unsigned char* buffer, size_t size)
{
	int i, index=0;
	
	for (i=0; i<size; i++)
	{
		if (buffer[i]>=32 && buffer[i]<=126)
		{
			sprintf (result+index, "%c", buffer[i]);
			index++;
		}
		else
		{
			sprintf (result+index, "\\%03o", buffer[i]);
			index += 4;
		}
	}
	result[index] = '\0';
	return result;
}
#endif /* DEBUG */

void
HandleShellOutput (unsigned char *shelloutput, size_t *shelloutput_size, unsigned char *shellinput, size_t *shellinput_size)
{
	int err;
	size_t buffer_size;
	unsigned char *buffer;
	wchar_t charcode;

	buffer = shelloutput;
	buffer_size = *shelloutput_size;
	do
	{
		debug (DEBUG_DETAIL, "Parsing shell output: %s", BinaryToPrintable (buffer, buffer_size));
		do
		{
			err = HandleEscapeSequences (&buffer, &buffer_size, shellinput, shellinput_size);
			debug (DEBUG_DETAIL, "HandleEscapeSequences returned %d", err);
		}
		while (!err && buffer_size > 0);
		if (err == 1 && buffer_size > 0)
		{
			err = fbtermGetShellChar (&charcode, &buffer, &buffer_size);
			if (!err)
			{
#ifdef MULTIBYTE
				if (iswprint ((wint_t)charcode))
				{
					debug (DEBUG_DETAIL,
					       "Character code = 0x%08lX (%09ld)\t\"%lc\"",
					       (unsigned long)charcode, (unsigned long)charcode, (wint_t)charcode);
#else
				if (isprint (charcode))
				{
					debug (DEBUG_DETAIL,
					       "Character code = 0x%08lX (%09ld)\t\"%c\"",
					       (unsigned long)charcode, (unsigned long)charcode, (int)charcode);
#endif
				}
				else
				{
					debug (DEBUG_DETAIL,
					       "Character code = 0x%08lX (%09ld)\t\"?\" (unprintable)",
					       (unsigned long)charcode, (unsigned long)charcode);
				}
				fbtermWriteChar (charcode);
				last_char = charcode;
				fbtermNextPos ();
			}
		}
	}
	while (buffer_size > 0 && err != 2);
	// ggiFlush (vis);
	memmove (shelloutput, buffer, buffer_size);
	*shelloutput_size = buffer_size;
}


extern int fbui_parse_geom (char *s1, short *w, short *h, short *xr, short *yr);

int master_fd;
struct winsize ws;

int
/*main (int argc, char **argv, char **envp)*/
main (int argc, char **argv)
{
	int err, using_ft=0;
	pid_t shell_pid;
	unsigned char shelloutput[SHELLOUTPUT_SIZE];
	size_t shelloutput_size = 0;
	unsigned char shellinput[SHELLINPUT_SIZE];
	size_t shellinput_size = 0;
	char buf[128];
	struct timeval tv;
	fd_set rfds, wfds;
	char *shell = "/bin/sh";

	int vc = -1;
	short cols = 80; 
	short rows = 33;
	short xrel = 0;
	short yrel = 10;

	argv++;
	argc--;
	int i=0;
	while (i < argc) {
		if (!strncmp (argv[i], "-c",2)) {
			vc = atoi (argv[i]+2);
		}
		else if (!strncmp (argv[i], "-geo",4)) {
			short n1,n2,n3,n4;
			if (fbui_parse_geom (argv[i]+4,&n1,&n2,&n3,&n4)) {
				cols = n1;
				rows = n2;
				xrel = n3;
				yrel = n4;
			}
		}
		else if (!strcmp (argv[i], "--help")) {
			fbtermUsage ();
			return 0;
		}
		else if (!strcmp (argv[i], "--encoding")) {
			fbtermError ("The --encoding option is obsolete. Codesets are now handled automatically.");
			argc -= 2;
			argv += 2;
			continue;
		}
		else
		if (!strcmp (argv[i], "--debuglevel"))
		{
#ifdef DEBUG
			debuglevel = strtol (argv[1], NULL, 10);
			if (errno == ERANGE)
			{
				perror ("fbterm: strtol");
				return 1;
			}
			argc -= 2;
			argv += 2;
			continue;
#else
			fbtermError ("Debug support not compiled in\n");
			return 1;
#endif /* DEBUG */
		} else {
			fbtermError ("Unknown option %s\n", argv[i]);
			return 1;
		}
		i++;
	}
	
	/* Get current locale */
#ifdef MULTIBYTE
	setlocale (LC_CTYPE, "");
# ifdef HAVE_NL_LANGINFO
	debug (DEBUG_INFO, "Current codeset is %s", nl_langinfo (CODESET));
# else
	debug (DEBUG_INFO, "Current codeset is unknown - nl_langinfo() undefined");
# endif
	debug (DEBUG_INFO, "Maximum character length is %d", MB_CUR_MAX);
#endif /* MULTIBYTE */

	debug (DEBUG_INFO, "Setting up default handlers:");
	fbtermGetShellChar = &DefaultGetShellChar;
	debug (DEBUG_INFO, "       fbtermGetShellChar -> DefaultGetShellChar");
	fbtermPutShellChar = &DefaultPutShellChar;
	debug (DEBUG_INFO, "       fbtermPutShellChar -> DefaultPutShellChar");
	fbtermWriteChar = &FBUIWriteChar;
	debug (DEBUG_INFO, "       fbtermWriteChar   -> FBUIWriteChar");
	
	err = FBUIInit_graphicsmode (vc,cols,rows,xrel,yrel);
	if (!err)
	{
		debug (DEBUG_INFO, "Using FBUI in graphics mode.");
	}
	else
	{
		fbtermError ("Cannot complete FBUI setup. Aborting.");
		FBUIExit ();
		return 1;
	}
	
	region_top = 0;
	region_bottom = terminal_height;
	MoveCursor (cursor_x0, cursor_y0);
	debug (DEBUG_INFO, "Terminal is %d x %d characters",
	       terminal_width, terminal_height);
	debug (DEBUG_INFO, "Characters are %d x %d pixels", cell_w,
	       cell_h);
	/* Allocate and populate the array for tab stops */
	tabstops = malloc (terminal_width * sizeof(char));
	for (err=0; err<terminal_width; err++)
	{
		tabstops[err] = (err%8 ? 0 : 1);
	}

	if (getenv ("SHELL") != NULL)
	{
		shell = getenv ("SHELL");
	}
	signal (SIGCHLD, child_died);

	ws.ws_col = terminal_width;
	ws.ws_row = terminal_height;
	ws.ws_xpixel = vis_w;
	ws.ws_ypixel = vis_h;

	/* in the following, buf should be set to NULL because of buffer overflow
	 * risk, but is left so for debugging purposes */
	shell_pid = forkpty (&master_fd, buf, NULL, &ws);
	if (shell_pid < 0)
	{
		perror ("fbterm: Couldn't fork. Exiting.\n");
		exit (1);
	}
	if (shell_pid == 0)
	{
		char *argv[] = { shell, NULL };
#ifdef HAVE_PUTENV
		putenv ("TERM=fbterm");
#else
		setenv ("TERM", "fbterm", 1);
#endif /* HAVE_PUTENV */
		debug (DEBUG_DETAIL,
			 "This is the forked process (PID %d, child of PID %d)\n",
			 (int)getpid (), (int)getppid ());
		debug (DEBUG_DETAIL,
			 "Sample UTF-8 output: CaractÃ¨res franÃ§ais");
		debug (DEBUG_DETAIL,
			 "Sample LATIN1 output: Caractères français");
		debug (DEBUG_DETAIL, "Now exec-ing %s", shell);
		execve (argv[0], argv, environ);
		exit (1);
	}
	debug (DEBUG_INFO, "Child forked (PID %d), pseudo tty is %s",
	       (int)shell_pid, buf);
	debug (DEBUG_INFO, "Waiting for its death...");

	while (!quit)
	{
		fbtermBlinkCursor (CURSOR_AUTO);
		
		/* do we have pending FBUI events?
		 * (non blocking)
		 */
		HandleFBUIEvents (shellinput, &shellinput_size);

		tv.tv_sec = 0;
		tv.tv_usec = 20000;
		FD_ZERO (&rfds);
		FD_ZERO (&wfds);
		FD_SET (master_fd, &rfds);
		if (shellinput_size)
			FD_SET (master_fd, &wfds);

		err = select (master_fd + 1, &rfds, &wfds, NULL, &tv);
		if (err < 0 && errno != EINTR)
		{
			perror ("fbterm: select");
			continue;
		}
		if (quit) break;
		if (err) debug (DEBUG_DETAIL, "%d file descriptors updated", err);

		if (FD_ISSET (master_fd, &wfds))
		{
			debug (DEBUG_DETAIL, "master_fd is ready for writing");
			/* if we have anything pending, then write it */
			if (shellinput_size)
			{
				do err = write (master_fd, shellinput, shellinput_size);
				while (err < 0 && errno == EINTR && !quit);
				if (err < 0)
				{
					perror ("fbterm: write");
					continue;
				}
				if (quit) break;
				debug (DEBUG_DETAIL,
				       "Written %d bytes out of %d to master_fd", err,
				       (int)shellinput_size);
				memmove (shellinput, shellinput+err, shellinput_size - err);
				shellinput_size -= err;
			}
		}

		if (FD_ISSET (master_fd, &rfds))
		{
			debug (DEBUG_DETAIL, "master_fd is set\nmain: Reading:");
			debug (DEBUG_DETAIL, "Buffer index is %d", (int)shelloutput_size);
			do err = read (master_fd, shelloutput + shelloutput_size, SHELLOUTPUT_SIZE - shelloutput_size);
			while (err < 0 && errno == EINTR && !quit);
			if (err < 0)
			{
				perror ("fbterm: read");
				continue;
			}
			if (err == 0) quit = 1;
			if (quit) break;
			if (err > 0)
			{
				debug (DEBUG_DETAIL, "Read %d bytes from master_fd", err);
				shelloutput_size += err;
				debug (DEBUG_DETAIL,
				       "Buffer index is now %d",
				       (int)shelloutput_size);
				fbtermBlinkCursor (CURSOR_HIDE);
				HandleShellOutput (shelloutput, &shelloutput_size, shellinput, &shellinput_size);
				fbtermBlinkCursor (CURSOR_SHOW);
			}
			debug (DEBUG_DETAIL, "Finished reading");
		}
	}

	switch (quit)
	{
	case 1:
		debug (DEBUG_INFO, "Exiting: EOF condition");
		break;
	case 2:
		debug (DEBUG_INFO, "Exiting: Child process has died");
		break;
	case 3:
		debug (DEBUG_INFO, "Exiting: Pipe error");
		break;
	}
	fbtermBlinkCursor (1);
	FBUIExit ();
	free (tabstops);
	close (master_fd);
	
	return (0);
}
