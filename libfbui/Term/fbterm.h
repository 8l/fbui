

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
/* for struct winsize and struct termios, not always included by FORKPTY_HEADER */
#include <termios.h>
/* for pid_t, strangely not included by FORKPTY_HEADER */
#include <sys/types.h>
/* for execve, read, write */
#include <unistd.h>


#ifdef MULTIBYTE

# include <locale.h>
/* MacOS X doesn't have langinfo.h, although the docs say otherwise. It's a
   bug on their part since it's a Single Unix Specification requirement */
# ifdef HAVE_NL_LANGINFO
#  include <langinfo.h>
# endif

# ifdef HAVE_ISWPRINT
#  include <wctype.h>
# else
#  define wint_t unsigned int
int iswprint (wint_t wc);
# endif

#else /* MULTIBYTE */

# define wchar_t unsigned int
# include <ctype.h> /* pour isprint */

#endif /* MULTIBYTE */

#if !STDC_HEADERS

/* if the standard C headers are not present, we need to declare the functions
   that may be declared in some other unknown header ourselves */
long int strtol(const char *nptr, char **endptr, int base);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int strcmp(const char *s1, const char *s2);

/* Then maybe the functions themselves are not present... Tough! */
# if !HAVE_MEMMOVE
# /* find a solution */
# endif
# if !HAVE_MEMCPY
# /* find a solution */
# endif
# if !HAVE_MEMSET
# /* find a solution */
# endif
# if !HAVE_STRCMP
# /* find a solution */
# endif

#else
# include <string.h>
#endif /* STDC_HEADERS */


/* signals */
#include <signal.h>

/* for forkpty() */
#include <sys/ioctl.h> /* some systems (BSD) need it even with FORKPTY_HEADER */
#ifdef FORKPTY_HEADER
# include FORKPTY_HEADER
#else
pid_t forkpty(int *amaster, char *name, struct termios *termp, struct  winsize *winp);
#endif

/* for select and associated macros */
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* for gettimeofday */
#include <sys/time.h>


/* FBUI */
#include <ggi/ggi.h>

#define fbtermError(...) { fprintf (stderr, "%s: ", __func__); fprintf (stderr, __VA_ARGS__); fprintf (stderr, "\n"); }

#ifdef DEBUG
extern int debuglevel;
#define debug(level, ...) {if (level <= debuglevel) { fprintf (stdout, "%s: ", __func__); fprintf (stdout, __VA_ARGS__); fprintf (stdout, "\n"); }}
#else
#define debug(level, ...) {}
#endif

#define DEBUG_NONE 0
#define DEBUG_INFO 1
#define DEBUG_DETAIL 2
#define DEBUG_TOTAL 3
	
#define CURSOR_AUTO 0
#define CURSOR_HIDE 1
#define CURSOR_SHOW 2

#define MAX_PARAM_NUM 9
#define MAX_PARAM_VALUE 32767
#define MAX_PARAM_SIZE 5

/* must be at least MAX_PARAM_SIZE*2+5 chars long, since do_u6() can send that much */
#define SHELLINPUT_SIZE (MAX_PARAM_SIZE*2+5+17)

/* set with care:
   - must be greater than the biggest escape sequence possible
   - input is frozen while shell output is rendered, so user experience
     commands it must not be too big either
   - when rendering of entire strings gets implemented, a small value will
     prevent taking advantage of it
   128 seems a good compromise... */
#define SHELLOUTPUT_SIZE 128

/* must be defined because even if FT support is not compiled in,
   FreetypeInit() is called with fontsize as argument */
#define DEFAULT_FONTSIZE 12

#define FBUIFLUSH_BUG
