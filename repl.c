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

#include <sys/wait.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crest.h"

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

		execl("/bin/sh", "sh", "-c", cmd, NULL);
		err(1, "execl");

	default:
		write(fds[1], data, len);
		close(fds[1]);
		close(fds[0]);
		wait(NULL);
	}
}

int
exec_req(struct imsgbuf *ibuf, const struct cmd *cmd, char **rets,
	size_t *retl)
{
	struct imsg imsg;
	size_t pathlen, paylen;
	ssize_t n, datalen;

	pathlen = paylen = 0;

	pathlen = strlen(cmd->path);
	if (cmd->payload != NULL)
		paylen = strlen(cmd->payload);

	if (pathlen >= UINT16_MAX || paylen >= UINT16_MAX)
		err(1, "url or payload too big");

	imsg_compose(ibuf, IMSG_SET_METHOD, 0, 0, -1, &cmd->method,
		sizeof(enum http_methods));
	imsg_compose(ibuf, IMSG_SET_URL, 0, 0, -1, cmd->path, pathlen);
	imsg_compose(ibuf, IMSG_SET_PAYLOAD, 0, 0, -1, cmd->payload, paylen);
	imsg_compose(ibuf, IMSG_DO_REQ, 0, 0, -1, NULL, 0);

	/* retry on errno == EAGAIN? */
	if ((n = msgbuf_write(&ibuf->w)) == -1)
		err(1, "msgbuf_write");
	if (n == 0)
		err(1, "child vanished");

	/* read the response */

	poll_read(ibuf->fd);
	if ((n = imsg_read(ibuf)) == -1)
		err(1, "imsg_read");
	if (n == 0)
		errx(1, "child vanished");

	if ((n = imsg_get(ibuf, &imsg)) == -1)
		err(1, "imsg_get");
	if (n == 0)
		errx(1, "no messages?");

	datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

	if (imsg.hdr.type != IMSG_BODY)
		err(1, "unexpected response type");

	if (datalen == 0) {
		*rets = NULL;
		*retl = 0;
	} else {
		*retl = datalen;
		*rets = calloc(datalen + 1, 1);
		if (*rets == NULL)
			err(1, "calloc");
		memcpy(*rets, imsg.data, datalen);
	}

	imsg_free(&imsg);
}

int
repl(struct imsgbuf *ibuf)
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

		if (!strcmp(line, "quit") || !strcmp(line, "exit"))
			break;

		if (*line == '|') {
			do_pipe(line + 1, res, reslen);
			continue;
		}

		cmd.path = NULL;
		cmd.payload = NULL;
		if (!parse(line, &cmd)) {
			if (cmd.path != NULL)
				free(cmd.path);
			if (cmd.payload != NULL)
				free(cmd.payload);
			continue;
		}

		if (verbose > 2)
			warnx("{ method=%d, path=%s, payload=%s }",
				cmd.method, cmd.path, cmd.payload);

		if (res != NULL)
			free(res);

		exec_req(ibuf, &cmd, &res, &reslen);

		free(cmd.path);
		if (cmd.payload != NULL)
			free(cmd.payload);
	}

	if (res != NULL)
		free(res);

	if (line != NULL)
		free(line);

	return !ferror(stdin);
}
