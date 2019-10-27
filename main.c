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

const char *prgname;
const char *prompt;

static void
usage()
{
	printf("USAGE: %s [-i] [-H header] [-P port] [-V http version] "
	       "[-c jtx] [-h host] [-p prefix] files...\n",
		prgname);
}

int
main(int argc, char **argv)
{
	int ch, imsg_fds[2], i;
	struct imsgbuf ibuf, child_ibuf;

	prgname = *argv;
	prompt = "> ";

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

	if (pledge("stdio rpath proc exec", NULL) == -1)
		err(1, "pledge");

	close(imsg_fds[1]);
	imsg_init(&ibuf, imsg_fds[0]);

	while ((ch = getopt(argc, argv, "iH:P:V:c:h:p:")) != -1) {
		switch (ch) {
		case 'H':
			csend(&ibuf, IMSG_ADD, optarg, strlen(optarg));
			break;

		case 'P': {
			const char *errstr = NULL;
			long port = 0;

			port = strtonum(optarg, 1, 65535, &errstr);
			if (errstr != NULL)
				errx(1, "port is %s: %s", errstr, optarg);
			csend(&ibuf, IMSG_SET_PORT, &port, sizeof(long));
			break;
		}

		case 'V': {
			long ver;

			switch (*optarg) {
			case '0':
				ver = CURL_HTTP_VERSION_1_0;
				break;

			case '1':
				ver = CURL_HTTP_VERSION_1_1;
				break;

			case '2':
				ver = CURL_HTTP_VERSION_2;
				break;

			case 'T':
				ver = CURL_HTTP_VERSION_2TLS;
				break;

			case '3':
				ver = CURL_HTTP_VERSION_3;
				break;

			case 'X':
				ver = CURL_HTTP_VERSION_NONE;
				break;

			default:
				errx(1, "-V: unknown value %s", optarg);
			}
			csend(&ibuf, IMSG_SET_HTTPVER, &ver, sizeof(long));
			break;
		}

		case 'c': {
			char *h;
			switch (*optarg) {
			case 'j':
				h = "Content-Type: application/json";
				break;

			case 't':
				h = "Content-Type: text/plain";
				break;

			case 'x':
				h = "Content-Type: application/xml";
				break;

			default:
				err(1, "-c: unknown value %s", optarg);
			}
			csend(&ibuf, IMSG_ADD, h, strlen(h));
			break;
		}

		case 'h': {
			char *hdr = NULL;
			int len;

			len = asprintf(&hdr, "Host: %s", optarg);
			if (len == -1)
				err(1, "asprintf");

			csend(&ibuf, IMSG_ADD, hdr, len);
			free(hdr);

			break;
		}

		case 'i':
			csend(&ibuf, IMSG_SET_PEER_VERIF, &(int) { 1 },
				sizeof(int));
			break;

		case 'p': {
			size_t len;
			if ((len = strlen(optarg)) == 0)
				errx(1, "prefix is empty");
			csend(&ibuf, IMSG_SET_PREFIX, optarg, len);
			break;
		}

		default:
			usage();
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	for (i = 0; i < argc; ++i) {
		FILE *f;

		if ((f = fopen(argv[i], "r")) == NULL)
			err(1, "%s", argv[i]);

		repl(&ibuf, f);

		fclose(f);
	}

	repl(&ibuf, stdin);
	csend(&ibuf, IMSG_EXIT, NULL, 0);
	wait(NULL);

	printf("\nbye\n");

	return 0;
}
