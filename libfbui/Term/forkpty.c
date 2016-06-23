
#include "fbterm.h"

#ifndef HAVE_FORKPTY

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
/*#if GWINSZ_IN_SYS_IOCTL*/
#include <sys/ioctl.h>
/*#endif*/


int forkpty_findtty (int* master_fd, int* slave_fd, char *name)
{
	int n;
	char master_name[16];
	char slave_name[16];

	for (n=0; n<16; n++)
	{
		sprintf (master_name, "/dev/ptyp%x", n);
		*master_fd = open (master_name, O_RDWR);
		if (*master_fd >= 0) break;
	}

	if (n==16) return -1;

	sprintf (slave_name, "/dev/ttyp%x", n);
	sprintf (name, "/dev/ttyp%x", n);
	*slave_fd = open (slave_name, O_RDWR);
	if (*slave_fd < 0)
	{
		perror (slave_name);
		close (*master_fd);
		return -1;
	}
	
	return 0;
}




pid_t forkpty (int *master_fd, char *name, struct termios *termp, struct winsize *winp)
{
	int slave_fd;
	pid_t pid;
	
	debug (DEBUG_INFO, "entering custom forkpty\n");
	if (forkpty_findtty (master_fd, &slave_fd, name) < 0)
	{
		fbtermError ("Couldn't open any tty.\n");
		return -1;
	}
	
	if (ioctl (*master_fd, TIOCSWINSZ, winp) < 0)
	{
		fbtermError ("Error setting window size.\n");
	}
	
	debug (DEBUG_DETAIL, "Forking\n");
	if ((pid = fork ()) == 0)
	{
		close (*master_fd);
		
		seteuid (getuid ());
		setegid (getgid ());
		setsid ();
		
		/* If we have TIOCSCTTY we use it to set our controlling tty */
#ifdef TIOCSCTTY
		ioctl (slave_fd, TIOCSCTTY, 0);
#else
		/* If not this should do: the first tty a process opens becomes
		   its controlling tty */
		debug (DEBUG_INFO, "Child: Closing %s\n", name);
		close (slave_fd);
		debug (DEBUG_INFO, "Child: Reopening %s\n", name);
		slave_fd = open (name, O_RDWR);
		if (slave_fd < 0)
		{
			perror (name);
			fbtermError ("Child: Couldn't reopen %s\nExiting\n", name);
			/* if it didn't work, exit noisily! */
			exit (1);
		}
		else
			debug (DEBUG_INFO, "Child: Reopened %s\n", name);
#endif /* TIOCSCTTY */
		
		close (0);
		close (1);
		close (2);
		
		dup2 (slave_fd, 0);
		dup2 (slave_fd, 1);
		dup2 (slave_fd, 2);
	}
	else
	{
		close (slave_fd);
		debug (DEBUG_INFO, "leaving custom forkpty\n");
	}
	return pid;
}

#endif /* HAVE_FORKPTY */
