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
#include <errno.h>
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

/* 0 on end, 1 on continue */
int
recv_into(struct imsgbuf *ibuf, struct resp *r)
{
	ssize_t n;
	struct imsg imsg;
	int rtype, ret;

	for (;;) {
		errno = 0;
		n = imsg_read(ibuf);

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			poll_read(ibuf->fd);
			continue;
		}

		if (n == -1)
			err(1, "imsg_read");
		if (n == 0)
			err(1, "child vanished");
		break;
	}

	if ((n = imsg_get(ibuf, &imsg)) == -1)
		err(1, "imsg_get");
	if (n == 0)
		err(1, "no messages");

	n = imsg.hdr.len - IMSG_HEADER_SIZE;
	rtype = imsg.hdr.type;

	switch (rtype) {
	case IMSG_HEAD:
		if (r->headers != NULL)
			errx(1, "headers already recv'd");

		r->headers = calloc(n + 1, 1);
		if (r->headers == NULL)
			err(1, "calloc");
		memcpy(r->headers, imsg.data, n);
		r->hlen = n;
		ret = 1;
		break;

	case IMSG_BODY:
		if (r->body != NULL)
			errx(1, "body already recv'd");

		r->body = calloc(n + 1, 1);
		if (r->body == NULL)
			err(1, "calloc");
		memcpy(r->body, imsg.data, n);
		r->blen = n;
		ret = 0;
		break;

	case IMSG_ERR:
		if (r->err != NULL)
			errx(1, "err already recv'd");
		r->err = calloc(n + 1, 1);
		if (r->err == NULL)
			err(1, "calloc");
		memcpy(r->err, imsg.data, n);
		r->blen = n;
		ret = 0;
		break;

	default:
		err(1, "unexpected response type %d", rtype);
	}

	imsg_free(&imsg);
	return ret;
}

int
exec_req(struct imsgbuf *ibuf, const struct cmd *cmd, struct resp *r)
{
	size_t pathlen, paylen;
	ssize_t n, datalen;
	int rtype;

	memset(r, 0, sizeof(struct resp));

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
	while (recv_into(ibuf, r))
		; /* no-op */

	write(1, r->body, r->blen);
	putchar('\n');

	return 0;
}

int
repl(struct imsgbuf *ibuf)
{
	struct cmd cmd;
	struct resp r;

	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen = 0;

	memset(&r, 0, sizeof(struct resp));

	while ((linelen = readline_wp(&line, &linesize, PROMPT)) != -1) {
		line[linelen - 1] = '\0';

		if (!strcmp(line, "help") || !strcmp(line, "usage")) {
			usage();
			continue;
		}

		if (!strcmp(line, "quit") || !strcmp(line, "exit"))
			break;

		if (*line == '|') {
			do_pipe(line + 1, r.body, r.blen);
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

		free_resp(&r);

		exec_req(ibuf, &cmd, &r);

		free(cmd.path);
		if (cmd.payload != NULL)
			free(cmd.payload);
	}

	free_resp(&r);

	if (line != NULL)
		free(line);

	return !ferror(stdin);
}
