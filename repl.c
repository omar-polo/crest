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

#include "crest.h"

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void
do_pipe(char *cmd, char *data, size_t len)
{
	pid_t p;
	int fds[2];

	if (pipe(fds) == -1) {
		warn("pipe");
		return;
	}

	switch (p = fork()) {
	case -1:
		warn("fork");
		break;

	case 0:
		if (dup2(fds[0], 0) == -1)
			err(1, "dup2");

		close(fds[0]);
		close(fds[1]);

		execl("/bin/sh", "sh", "-c", cmd);
		warn("execl");
		break;

	default:
		write(fds[1], data, len);
		close(fds[1]);
		close(fds[0]);
		wait(NULL);
	}
}

int
repl()
{
	struct cmd cmd;

	char *res = NULL;
	size_t reslen = 0;

	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen = 0;

	while ((linelen = readline_wp(&line, &linesize, PROMPT)) != -1) {
		line[linelen - 1] = '\0';

		if (!strcmp(line, "help") || !strcmp(line, "usage")) {
			usage();
			continue;
		}

		if (*line == '|') {
			do_pipe(++line, res, reslen);
			continue;
		}

		if (!parse(line, &cmd))
			continue;

		if (verbose > 2)
			warnx("{ method=%d, path=%s, payload=%s }", cmd.method,
				cmd.path, cmd.payload);

		if (res != NULL)
			free(res);

		do_cmd(&cmd, &res, &reslen);

		free(cmd.path);
		if (cmd.payload != NULL)
			free(cmd.payload);
	}

	if (res != NULL)
		free(res);

	free(line);
	return !ferror(stdin);
}
