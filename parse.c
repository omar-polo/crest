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

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <string.h>

/* grammar is:
 *	{GET|POST|...} url payload? '\n'
 */

const char *
method2str(enum http_methods m)
{
	switch (m) {
	case CONNECT:
		return "CONNECT";
	case DELETE:
		return "DELETE";
	case GET:
		return "GET";
	case HEAD:
		return "HEAD";
	case OPTIONS:
		return "OPTIONS";
	case PATCH:
		return "PATCH";
	case POST:
		return "POST";
	case PUT:
		return "PUT";
	case TRACE:
		return "TRACE";
	default:
		return NULL;
	}
}

int
parse(const char *i, struct cmd *cmd)
{
	int c, j, k, v;
	size_t l;
	const char *t;
	const char *methods[] = { "connect", "delete", "get", "head", "options",
		"patch", "post", "put", "trace" };
	enum http_methods m2m[] = { CONNECT, DELETE, GET, HEAD, OPTIONS, PATCH,
		POST, PUT, TRACE };
#define N (sizeof(methods) / sizeof(char *))

	/* parse method */
	for (j = 0; *i; ++j) {
		c = tolower(*i++);

		if (isspace(c))
			break;

		for (k = 0; k < N; ++k) {
			if (methods[k] == NULL)
				continue;

			if (methods[k][j] != c)
				methods[k] = NULL;
		}
	}

	/* check if we matched a method */
	v = 0;
	for (k = 0; k < N; ++k) {
		if (methods[k] != NULL && methods[k][j] == '\0') {
			v = 1;
			cmd->method = m2m[k];
			break;
		}
	}
	if (!v) {
		fprintf(stderr, "cannot understand the HTTP method\n");
		return 0;
	}

	/* i points to the url */
	t = i;
	for (l = 0; *i; ++l) {
		c = *i++;
		if (isspace(c))
			break;
	}

	cmd->path = strndup(t, l);
	if (cmd->path == NULL) {
		warn("strndup");
		return 0;
	}

	/* no payload case */
	if (!*i) {
		cmd->payload = NULL;
		return 1;
	}

	cmd->payload = strdup(i);
	if (cmd->payload == NULL) {
		warn("strdup");
		return 0;
	}

	return 1;
}