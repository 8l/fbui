#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#define PORTNUM 110

typedef unsigned char uchar;

static int
getline (int fd, char *buf, int buflen)
{
	int got;
	char ch;
	int ix=0;

	buf[0] = 0;

	while (ix < (buflen-1)) {
		got = recv (fd, &ch, 1, 0);

		if (-1 == got && !ix) {
			perror ("recv");
			return -1;
		}

		if (!got)
			continue;

		if (ch == '\n')
			break;
		if (ch == '\r') 
			continue;

		buf [ix++] = ch;
	}

	buf [ix] = 0;
	return ix;
}


int
checkmail (char *server, char *username, char *password)
{
	int fd, result;
	char buf[256];
	char *tmp;
	int total=0;
	int tries;

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		return -1;
	}

	char *s = strchr (server, '\n');
	if (s) *s=0;
	s = strchr (server, '\r');
	if (s) *s=0;
	s = strchr (username, '\n');
	if (s) *s=0;
	s = strchr (username, '\r');
	if (s) *s=0;
	s = strchr (password, '\n');
	if (s) *s=0;
	s = strchr (password, '\r');
	if (s) *s=0;

	struct hostent* host;
	host = gethostbyname (server);
	if (!host) {
		perror ("gethostbyname");
		close(fd);
		return -1;
	}

	struct sockaddr_in sockaddr;
	memset (&sockaddr, 0, sizeof (struct sockaddr_in));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(PORTNUM);
	bcopy(host->h_addr, &sockaddr.sin_addr, host->h_length);

	result = connect (fd, (struct sockaddr*) &sockaddr, 
			sizeof(struct sockaddr));
	if (result) {
		perror ("connect");
		close(fd);
		return -1;
	}

	if (-1 == getline(fd, buf, 256)) {
		close(fd);
		return -1;
	}

	sprintf (buf, "user %s\n", username);
	if (0 > write (fd, buf, strlen(buf))) {
		perror ("write");
		close(fd);
		return -1;
	}
	
	if (-1 == getline(fd, buf, 256)) {
		close(fd);
		return -1;
	}

	sprintf (buf, "pass %s\n", password);
	if (0 > write (fd, buf, strlen(buf))) {
		perror ("write");
		close(fd);
		return -1;
	}

	if (-1 == getline(fd, buf, 256)) {
		close(fd);
		return -1;
	}

	sprintf (buf, "list\n");
	if (0 > write (fd, buf, strlen(buf))) {
		perror ("write");
		close(fd);
		return -1;
	}

	if (-1 == getline(fd, buf, 256)) {
		close(fd);
		return -1;
	}

	if (!strncmp ("+OK", buf, 3)) {

		/* the next line has a count */
		if (-1 == getline(fd, buf, 256)) {
			close(fd);
			return -1;
		}

		total = atoi(buf);
		printf ("you have %d message(s).\n", total);
	}
	else
	{
		printf ("response not +OK: (%s)\n",buf);
	}
	
	sprintf (buf, "quit\n");
	if (0 > write (fd, buf, strlen(buf))) {
		perror ("write");
		close(fd);
		return -1;
	}

	close(fd);
	return total;
}


