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
#include <stdio.h>
#include <unistd.h>

char *host;
char *prgname;

long http_version;

int verbose;
int skip_peer_verification;

void
usage()
{
	printf("USAGE: %s [-h host]\n", prgname);
}

int
main(int argc, char **argv)
{
	int ch;

	host = NULL;
	prgname = *argv;

	http_version = CURL_HTTP_VERSION_2TLS;

	verbose = 0;
	skip_peer_verification = 1;

	while ((ch = getopt(argc, argv, "ivh:")) != -1) {
		switch (ch) {
		case 'h':
			host = optarg;
			break;

		case 'i':
			skip_peer_verification = 1;
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

	curl_global_cleanup();

	return 0;
}
