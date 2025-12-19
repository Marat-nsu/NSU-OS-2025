#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "shell.h"

int promptline(char *prompt, char *line, int sizline)
{
	int n = 0;
	ssize_t rc;

	write(1, prompt, strlen(prompt));
	while (1)
	{
		rc = read(0, (line + n), sizline - n);
		if (rc < 0)
		{
			if (errno == EINTR)
			{
				write(1, prompt, strlen(prompt));
				n = 0;
				continue;
			}
			return -1;
		}
		if (rc == 0)
		{
			return 0;
		}
		n += rc;
		*(line + n) = '\0';
		/*
		 *  check to see if command line extends onto
		 *  next line.  If so, append next line to command line
		 */

		if (*(line + n - 2) == '\\' && *(line + n - 1) == '\n')
		{
			*(line + n) = ' ';
			*(line + n - 1) = ' ';
			*(line + n - 2) = ' ';
			continue; /*  read next line  */
		}
		return (n); /* all done */
	}
}
