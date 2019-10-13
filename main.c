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

#include <curl/curl.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *prefix;
char *prgname;

long http_version;
long port;

int verbose;
int skip_peer_verification;

struct svec *headers;
#define HPUSH(h, v, d)                                                         \
	do {                                                                   \
		struct svec *t = NULL;                                         \
		t = svec_add(h, v, d);                                         \
		if (t == NULL)                                                 \
			err(1, "svec_add");                                    \
		h = t;                                                         \
	} while (0)

void
usage()
{
	printf("USAGE: %s [-iv] [-H header] [-P port] [-V http version] "
	       "[-c jtx] [-h host] [-p prefix]\n",
		prgname);
}

int
main(int argc, char **argv)
{
	int ch;

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
				warnx("%s is either too large or too small to "
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
				err(1, "-V: valid values are 0, 1, 2, T, 3, X");
			}
			break;

		case 'c':
			switch (*optarg) {
			case 'j':
				HPUSH(headers, "Content-Type: application/json",
					0);
				break;

			case 't':
				HPUSH(headers, "Content-Type: text/plain", 0);
				break;

			case 'x':
				HPUSH(headers, "Content-Type: application/xml",
					0);
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

	curl_global_init(CURL_GLOBAL_ALL);

	repl();

	svec_free(headers);
	curl_global_cleanup();

	return 0;
}
