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

#include <curl/curl.h>
#include <err.h>
#include <errno.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crest.h"

const char *prefix;
char *prgname;

long http_version;
long port;

int verbose;
int skip_peer_verification;

struct svec *headers;
#define HPUSH(h, v, d)                                                       \
	do {                                                                 \
		struct svec *t = NULL;                                       \
		t = svec_add(h, v, d);                                       \
		if (t == NULL)                                               \
			err(1, "svec_add");                                  \
		h = t;                                                       \
	} while (0)

void
usage()
{
	printf("USAGE: %s [-iv] [-H header] [-P port] [-V http version] "
	       "[-c jtx] [-h host] [-p prefix]\n",
		prgname);
}

int parent_main(struct imsgbuf *);
int child_main(struct imsgbuf *);

int
main(int argc, char **argv)
{
	int ch, imsg_fds[2];
	struct imsgbuf parent_ibuf, child_ibuf;

	prefix = NULL;
	prgname = *argv;

	http_version = CURL_HTTP_VERSION_2TLS;
	port = -1;

	verbose = 0;
	skip_peer_verification = 1;

	headers = NULL;

	while ((ch = getopt(argc, argv, "ivH:P:V:c:h:p:")) != -1) {
		switch (ch) {
		case 'H':
			HPUSH(headers, optarg, 0);
			break;

		case 'P': {
			char *ep;
			long lval;

			errno = 0;
			lval = strtol(optarg, &ep, 10);
			if (optarg[0] == '\0' || *ep != '\0') {
				warnx("%s is not a number", optarg);
				break;
			}
			if ((errno == ERANGE
				    && (lval == LONG_MAX || lval == LONG_MIN))
				|| (lval > 65535 || lval < 1)) {
				warnx("%s is either too large or too small "
				      "to "
				      "be a port number",
					optarg);
				break;
			}
			port = lval;
			break;
		}

		case 'V':
			switch (*optarg) {
			case '0':
				http_version = CURL_HTTP_VERSION_1_0;
				break;

			case '1':
				http_version = CURL_HTTP_VERSION_1_1;
				break;

			case '2':
				http_version = CURL_HTTP_VERSION_2;
				break;

			case 'T':
				http_version = CURL_HTTP_VERSION_2TLS;
				break;

			case '3':
				http_version = CURL_HTTP_VERSION_3;
				break;

			case 'X':
				http_version = CURL_HTTP_VERSION_NONE;
				break;

			default:
				err(1,
					"-V: accepted values are 0, 1, 2, T, "
					"3, X");
			}
			break;

		case 'c':
			switch (*optarg) {
			case 'j':
				HPUSH(headers,
					"Content-Type: application/json", 0);
				break;

			case 't':
				HPUSH(headers, "Content-Type: text/plain", 0);
				break;

			case 'x':
				HPUSH(headers,
					"Content-Type: application/xml", 0);
				break;

			default:
				err(1, "-c: valid values are j, t or x");
			}
			break;

		case 'h': {
			char *hdr = NULL;

			if (asprintf(&hdr, "Host: %s", optarg) == -1)
				err(1, "asprintf");

			HPUSH(headers, hdr, 1);
			break;
		}

		case 'i':
			skip_peer_verification = 1;
			break;

		case 'p':
			if (strlen(optarg) == 0) {
				warnx("prefix \"%s\" is too small", optarg);
				break;
			}
			prefix = optarg;
			break;

		case 'v':
			verbose++;
			break;

		default:
			usage();
			return 1;
		}
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, imsg_fds) == -1)
		err(1, "socketpair");

	switch (fork()) {
	case -1:
		err(1, "fork");

	case 0:
		if (unveil("/etc/ssl/", "r") == -1)
			err(1, "unveil");
		if (pledge("stdio rpath dns inet", NULL) == -1)
			err(1, "pledge");
		close(imsg_fds[0]);
		imsg_init(&child_ibuf, imsg_fds[1]);
		return child_main(&child_ibuf);
	}

	if (pledge("stdio proc exec", NULL) == -1)
		err(1, "pledge");

	close(imsg_fds[1]);
	imsg_init(&parent_ibuf, imsg_fds[0]);

	svec_free(headers);

	return parent_main(&parent_ibuf);
}

int
parent_main(struct imsgbuf *ibuf)
{
	ssize_t n;

	repl(ibuf);

	/* tell the child to exit */

	imsg_compose(ibuf, IMSG_EXIT, 0, 0, -1, NULL, 0);

	/* retry on errno == EAGAIN? */
	if ((n = msgbuf_write(&ibuf->w)) == -1)
		err(1, "msgbuf_write");
	if (n == 0)
		err(1, "child vanished");

	wait(NULL);

	return 0;
}

/* return 1 only on IMSG_EXIT */
int
process_messages(struct imsgbuf *ibuf, struct cmd *cmd)
{
	struct imsg imsg;
	ssize_t n, datalen;
	int done;

	poll_read(ibuf->fd);

	/* retry on errno == EAGAIN */
	if ((n = imsg_read(ibuf)) == -1)
		err(1, "imsg_read");
	if (n == 0)
		errx(1, "connection closed");
	done = 0;
	for (; !done;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			err(1, "imsg_get");

		if (n == 0) /* no more messages */
			return 0;

		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		switch (imsg.hdr.type) {
		case IMSG_EXIT:
			if (verbose > 2)
				warnx("child: exiting");
			done = 1;
			break;

		case IMSG_SET_METHOD:
			if (datalen < sizeof(cmd->method))
				err(1, "IMSG_SET_METHOD wrong size");
			memcpy(&cmd->method, imsg.data, sizeof(cmd->method));
			break;

		case IMSG_SET_URL:
			cmd->path = calloc(datalen + 1, 1);
			if (cmd->path == NULL)
				err(1, "calloc");
			memcpy(cmd->path, imsg.data, datalen);
			break;

		case IMSG_SET_PAYLOAD:
			cmd->payload = calloc(datalen + 1, 1);
			if (cmd->payload == NULL)
				err(1, "calloc");
			memcpy(cmd->payload, imsg.data, datalen);
			break;

		case IMSG_DO_REQ: {
			ssize_t n;
			char *rets = NULL;
			size_t retl = 0;

			do_cmd(cmd, &rets, &retl);

			if (retl >= UINT16_MAX)
				errx(1, "response body too big (%zu bytes)",
					retl);
			imsg_compose(ibuf, IMSG_BODY, 0, 0, -1, rets, retl);

			/* retry on errno == EAGAIN? */
			if ((n = msgbuf_write(&ibuf->w)) == -1)
				err(1, "msgbuf_write");
			if (n == 0)
				errx(1, "parent vanished");

			if (rets != NULL)
				free(rets);

			free(cmd->path);
			if (cmd->payload != NULL)
				free(cmd->payload);

			cmd->path = cmd->payload = NULL;

			break;
		}
		}

		imsg_free(&imsg);
	}

	return 1;
}

int
child_main(struct imsgbuf *ibuf)
{
	struct cmd cmd;

	memset(&cmd, 0, sizeof(struct cmd));

	curl_global_init(CURL_GLOBAL_DEFAULT);

	for (;;)
		if (process_messages(ibuf, &cmd))
			break;

	svec_free(headers);
	curl_global_cleanup();

	return 0;
}
