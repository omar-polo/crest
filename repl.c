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

#include "compat.h"
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

void
safe_println(const char *text, size_t len)
{
	char *s;
	size_t slen;

	if (text == NULL) {
		putchar('\n');
		return;
	}

	slen = len * 4 + 1;

	if ((s = calloc(slen, 1)) == NULL)
		err(1, "calloc");

	if (strnvis(s, text, slen, VIS_CSTYLE) == -1)
		err(1, "strnvis");

	/* we don't care about the NUL-terminator, since we'll be explicit and
	 * use write(2) instead of a printf.  So we recycle that byte as a
	 * newline. */
	s[slen - 1] = '\n';

	write(1, s, slen);

	free(s);
}

/* 0 on end, 1 on continue */
int
recv_into(struct imsgbuf *ibuf, struct resp *r)
{
	ssize_t n;
	struct imsg imsg;
	int rtype, ret;

	ret = 1;

	if ((n = imsg_get(ibuf, &imsg)) == -1)
		err(1, "imsg_get");
	if (n == 0)
		errx(1, "no messages");

	n = imsg.hdr.len - IMSG_HEADER_SIZE;
	rtype = imsg.hdr.type;

	switch (rtype) {
	case IMSG_STATUS:
		if (n != sizeof(r->http_code))
			errx(1, "http_code: wrong size");
		memcpy(&r->http_code, imsg.data, n);
		break;

	case IMSG_HEAD:
		if (r->headers != NULL)
			errx(1, "headers already recv'd");

		r->headers = calloc(n + 1, 1);
		if (r->headers == NULL)
			err(1, "calloc");
		memcpy(r->headers, imsg.data, n);
		r->hlen = n;
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
exec_req(struct imsgbuf *ibuf, const struct req *req, struct resp *r)
{
	size_t pathlen, paylen;
	ssize_t n;

	memset(r, 0, sizeof(struct resp));

	pathlen = paylen = 0;

	pathlen = strlen(req->path);
	if (req->payload != NULL)
		paylen = strlen(req->payload);

	if (pathlen >= UINT16_MAX || paylen >= UINT16_MAX)
		err(1, "url or payload too big");

	imsg_compose(ibuf, IMSG_SET_METHOD, 0, 0, -1, &req->method,
		sizeof(enum http_methods));
	imsg_compose(ibuf, IMSG_SET_URL, 0, 0, -1, req->path, pathlen);
	imsg_compose(ibuf, IMSG_SET_PAYLOAD, 0, 0, -1, req->payload, paylen);
	imsg_compose(ibuf, IMSG_DO_REQ, 0, 0, -1, NULL, 0);

	/* retry on errno == EAGAIN? */
	if ((n = msgbuf_write(&ibuf->w)) == -1)
		err(1, "msgbuf_write");
	if (n == 0)
		err(1, "child vanished");

	/* read the response */
	poll_read(ibuf->fd);
	for (;;) {
		errno = 0;
		n = imsg_read(ibuf);
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			continue;
		if (n == -1)
			err(1, "imsg_read");
		if (n == 0)
			errx(1, "child vanished");
		break;
	}
	while (recv_into(ibuf, r))
		; /* no-op */

	safe_println(r->headers, r->hlen);
	safe_println(r->body, r->blen);

	return 0;
}

static void
wait_for_done(struct imsgbuf *ibuf)
{
	ssize_t n;
	struct imsg imsg;

	poll_read(ibuf->fd);
	for (;;) {
		errno = 0;
		n = imsg_read(ibuf);
		if (errno == EAGAIN)
			continue;
		if (n == -1)
			err(1, "imsg_read");
		if (n == 0)
			errx(1, "child vanished");
		break;
	}

	if ((n = imsg_get(ibuf, &imsg)) == -1)
		err(1, "imsg_get");
	if (n == 0)
		errx(1, "no messages");
	if (imsg.hdr.type != IMSG_DONE)
		errx(1, "unexpected message %d", imsg.hdr.type);
}

int
repl(struct imsgbuf *ibuf, FILE *in)
{
	struct cmd cmd;
	struct resp r;

	char *line = NULL;
	size_t linesize = 0;
	ssize_t len = 0;

	memset(&r, 0, sizeof(struct resp));

	while ((len = rlp(&line, &linesize, prompt, in)) != -1) {
		line[len - 1] = '\0';

		if (*line == '#') /* ignore comments */
			continue;

		if (len == 1) /* ignore empty lines (i.e. only a \n) */
			continue;

		if (*line == '|') {
			do_pipe(line + 1, r.body, r.blen);
			continue;
		}

		memset(&cmd, 0, sizeof(struct cmd));
		if (!parse(line, &cmd)) {
			if (cmd.type == CMD_REQ && cmd.req.path != NULL)
				free(cmd.req.path);
			if (cmd.type == CMD_REQ && cmd.req.payload != NULL)
				free(cmd.req.payload);
			continue;
		}

		switch (cmd.type) {
		case CMD_REQ:
			free_resp(&r);
			exec_req(ibuf, &cmd.req, &r);

			free(cmd.req.path);
			if (cmd.req.payload != NULL)
				free(cmd.req.payload);
			break;

		case CMD_SET:
			csend(ibuf, cmd.opt.set, cmd.opt.value, cmd.opt.len);

			if (cmd.opt.set == IMSG_SET_PORT)
				free(cmd.opt.value);
			break;

		case CMD_SHOW:
			csend(ibuf, IMSG_SHOW, &cmd.show, sizeof(cmd.show));
			wait_for_done(ibuf);
			break;

		case CMD_ADD:
			csend(ibuf, IMSG_ADD, cmd.hdrname,
				strlen(cmd.hdrname));
			break;

		case CMD_DEL:
			/* copy also the NUL-terminator. */
			csend(ibuf, IMSG_DEL, cmd.hdrname,
				strlen(cmd.hdrname) + 1);
			wait_for_done(ibuf);
			break;

		case CMD_SPECIAL:
			switch (cmd.sp) {
			case SC_HELP:
				usage();
				break;
			case SC_QUIT:
				goto end;
				break;
			case SC_VERSION:
				warnx("version ?");
				break;
			}
			break;

		default:
			err(1, "invalid cmd.type %d", cmd.type);
		}
	}
end:

	free_resp(&r);

	if (line != NULL)
		free(line);

	return !ferror(stdin);
}
