/*
 * Copyright (c) 2019 Omar Polo <op@xglobe.in>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crest.h"

char *
sgl(FILE *in)
{
	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;

	if ((linelen = getline(&line, &linesize, in)) == -1)
		return NULL;

	/* trim the \n at the end */
	if (linelen != 0)
		line[linelen-1] = '\0';

	return line;
}

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
char *
rl(const char *prompt)
{
	char *line;

	if (!strcmp(getenv("TERM"), "dumb")) {
		printf("%s", prompt);
		fflush(stdout);
		return sgl(stdin);
	}

	line = readline(prompt);
	if (line && *line)
		add_history(line);

	return line;
}
#else
char *
rl(const char *prompt)
{
	/* if (!strcmp(getenv("TERM"), "dumb")) { */
		printf("%s", prompt);
		fflush(stdout);
		return sgl(stdin);
	/* } */
	/* return sgl(stdin); */
}
#endif

char *
rlf(const char *prompt, FILE *in)
{
	if (isatty(fileno(in)))
		return rl(prompt);
	return sgl(in);
}

int
poll_read(int fd)
{
	struct pollfd fds;

	fds.fd = fd;
	fds.events = POLLIN;

	if (poll(&fds, 1, -1) == -1)
		err(1, "poll");

	if (fds.revents & (POLLERR | POLLNVAL))
		errx(1, "bad fd");

	return 0;
}
