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

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* used to return the address in parse_set */
static long curl_http_versions[] = {
	CURL_HTTP_VERSION_1_0,
	CURL_HTTP_VERSION_1_1,
	CURL_HTTP_VERSION_2,
	CURL_HTTP_VERSION_2TLS,
	CURL_HTTP_VERSION_3,
	CURL_HTTP_VERSION_NONE,
};
static int bools[] = { 0, 1 };

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

const char *
httpver2str(long hv)
{
	switch (hv) {
	case CURL_HTTP_VERSION_1_0:
		return "HTTP/1.0";
	case CURL_HTTP_VERSION_1_1:
		return "HTTP/1.1";
	case CURL_HTTP_VERSION_2:
		return "HTTP/2";
	case CURL_HTTP_VERSION_2TLS:
		return "HTTP/2 with TLS";
	case CURL_HTTP_VERSION_3:
		return "HTTP/3";
	case CURL_HTTP_VERSION_NONE:
		return "none";
	default:
		errx(1, "httver2str: unknown http version %lu", hv);
	}
}

static const char *
eat_spaces(const char *i)
{
	while (isspace(*i))
		i++;
	return i;
}

/* string starts with - return 1 if a starts with b */
int
strsw(const char *a, const char *b)
{
	for (;;) {
		if (*b == '\0')
			return 1;
		if (*a != *b)
			return 0;
		a++, b++;
	}
}

static int
parse_setting(const char **r, const char **opt, enum imsg_type *mt)
{
	int j, k, n, v, c;

	/* here we recycle the constant IMSG_ADD instead of defining a new
	 * IMSG_SHOW_HEADERS, since the add command is used only for headers
	 */
	const char *opts[] = { "headers", "useragent", "prefix", "http",
		"http-version", "port", "peer-verification" };
	const enum imsg_type o2t[] = { IMSG_ADD, IMSG_SET_UA, IMSG_SET_PREFIX,
		IMSG_SET_HTTPVER, IMSG_SET_HTTPVER, IMSG_SET_PORT,
		IMSG_SET_PEER_VERIF };
	const char *i;

	n = sizeof(opts) / sizeof(char *);

	i = eat_spaces(*r);

	for (j = 0; *i; ++j) {
		c = tolower(*i++);

		if (isspace(c))
			break;

		for (k = 0; k < n; ++k) {
			if (opts[k] == NULL)
				continue;
			if (opts[k][j] != c)
				opts[k] = NULL;
		}
	}

	/* check if we matched an option */
	v = 0;
	for (k = 0; k < n; ++k) {
		if (opts[k] != NULL && opts[k][j] == '\0') {
			v = 1;
			*opt = opts[k];
			*mt = o2t[k];
			break;
		}
	}
	if (!v) {
		warnx("unknown option");
		return 0;
	}

	*r = i;
	return 1;
}

/* parse a string that starts with "show" */
static int
parse_show(const char *i, struct cmd *cmd)
{
	/* grammar:
	 *	show something
	 */
	const char *opt;

	/* instead of defining a bunch of IMSG_SHOW_* settings, re-use the
	 * IMSG_SET_* also for the show requests */

	assert(strsw(i, "show"));

	i += 4; /* skip the "show" */

	if (!parse_setting(&i, &opt, &cmd->show))
		return 0;

	i = eat_spaces(i);
	if (*i != '\0') {
		warnx("syntax: set <something>");
		return 0;
	}

	return 1;
}

/* parse a string that starts with "set" */
static int
parse_set(const char *i, struct cmd *cmd)
{
	/* grammar:
	 *	set something value
	 */
	const char *opt;

	assert(strsw(i, "set"));

	i += 3; /* skip the "set" */

	if (!parse_setting(&i, &opt, &cmd->opt.set))
		return 0;

	if (cmd->opt.set == IMSG_ADD) {
		warnx("cannot set headers.");
		return 0;
	}

	i = eat_spaces(i);
	if (*i == '\0') {
		warnx("missing value for set %s", opt);
		return 0;
	}

	/* i now points to the value */

	switch (cmd->opt.set) {
	case IMSG_SET_UA:
	case IMSG_SET_PREFIX:
		cmd->opt.value = (void *)i;
		cmd->opt.len = strlen(i);
		return 1;

	case IMSG_SET_HTTPVER: {
		cmd->opt.len = sizeof(long);

		if (!strcmp(i, "1.0"))
			cmd->opt.value = &curl_http_versions[0];
		else if (!strcmp(i, "1.1"))
			cmd->opt.value = &curl_http_versions[1];
		else if (!strcmp(i, "2"))
			cmd->opt.value = &curl_http_versions[2];
		else if (!strcmp(i, "2TLS"))
			cmd->opt.value = &curl_http_versions[3];
		else if (!strcmp(i, "3"))
			cmd->opt.value = &curl_http_versions[4];
		else if (!strcmp(i, "none"))
			cmd->opt.value = &curl_http_versions[5];
		else {
			warnx("unknown http version %s", i);
			return 0;
		}

		return 1;
	}

	case IMSG_SET_PORT: {
		const char *errstr;
		long *port;

		if ((port = malloc(sizeof(long))) == NULL)
			err(1, "malloc");

		*port = strtonum(i, 1, 65535, &errstr);
		if (errstr != NULL) {
			warnx("port is %s: %s", errstr, i);
			free(port);
			return 0;
		}

		cmd->opt.value = port;
		cmd->opt.len = sizeof(long);
		return 1;
	}

	case IMSG_SET_PEER_VERIF:
		if (!strcmp(i, "on") || !strcmp(i, "true"))
			cmd->opt.value = &bools[1];
		else if (!strcmp(i, "off") || !strcmp(i, "false"))
			cmd->opt.value = &bools[0];
		else {
			warnx("unknown value %s for %s", i, opt);
			return 0;
		}
		cmd->opt.len = sizeof(int);
		return 1;

	default:
		err(1, "imsg type %d shouldn't be accessible", cmd->opt.set);
	}
}

static int
parse_unset(const char *i, struct cmd *cmd)
{
	/* grammar:
	 *	unset opt1
	 */
	const char *opt;

	assert(strsw(i, "unset"));

	i += 5; /* skip the "unset" */

	if (!parse_setting(&i, &opt, &cmd->opt.set))
		return 0;

	if (cmd->opt.set == IMSG_ADD) {
		warnx("cannot unset headers.");
		return 0;
	}

	switch (cmd->opt.set) {
	case IMSG_SET_UA:
	case IMSG_SET_PREFIX:
		cmd->opt.value = NULL;
		cmd->opt.len = 0;
		return 1;

	case IMSG_SET_HTTPVER:
		cmd->opt.len = sizeof(long);
		cmd->opt.value = &curl_http_versions[5]; /* none */
		return 1;

	case IMSG_SET_PORT: {
		long *port;

		if ((port = malloc(sizeof(long))) == NULL)
			err(1, "malloc");

		*port = -1;

		cmd->opt.value = port;
		cmd->opt.len = sizeof(long);
		return 1;
	}

	case IMSG_SET_PEER_VERIF:
		cmd->opt.value = &bools[1];
		cmd->opt.len = sizeof(int);
		return 1;

	default:
		err(1, "imsg type %d shouldn't be accessible", cmd->opt.set);
	}
}

static int
parse_add(const char *i, struct cmd *cmd)
{
	assert(strsw(i, "add"));

	i += 3; /* skip the add */
	i = eat_spaces(i);

	if (*i == '\0') {
		warnx("missing header to add");
		return 0;
	}

	cmd->hdrname = i;
	return 1;
}

static int
parse_del(const char *i, struct cmd *cmd)
{
	assert(strsw(i, "del"));

	i += 3; /* skip the del */
	i = eat_spaces(i);

	if (*i == '\0') {
		warnx("missing header to delete");
		return 0;
	}

	cmd->hdrname = i;
	return 1;
}

static int
parse_req(const char *i, struct cmd *cmd)
{
	/* grammar:
	 *	{GET|POST|...} url payload?
	 */

	int c, j, v;
	size_t k, l;
	const char *t;
	const char *methods[] = { "connect", "delete", "get", "head",
		"options", "patch", "post", "put", "trace" };
	enum http_methods m2m[] = { CONNECT, DELETE, GET, HEAD, OPTIONS,
		PATCH, POST, PUT, TRACE };
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
			cmd->req.method = m2m[k];
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

	cmd->req.path = strndup(t, l);
	if (cmd->req.path == NULL) {
		warn("strndup");
		return 0;
	}

	/* no payload case */
	if (!*i) {
		cmd->req.payload = NULL;
		return 1;
	}

	cmd->req.payload = strdup(i);
	if (cmd->req.payload == NULL) {
		warn("strdup");
		return 0;
	}

	return 1;
}

int
parse(const char *i, struct cmd *cmd)
{
	if (!strcmp(i, "help") || !strcmp(i, "usage")) {
		cmd->type = CMD_SPECIAL;
		cmd->sp = SC_HELP;
		return 1;
	}

	if (!strcmp(i, "quit") || !strcmp(i, "exit")) {
		cmd->type = CMD_SPECIAL;
		cmd->sp = SC_QUIT;
		return 1;
	}

	if (!strcmp(i, "version")) {
		cmd->type = CMD_SPECIAL;
		cmd->sp = SC_VERSION;
		return 1;
	}

	if (strsw(i, "set")) {
		cmd->type = CMD_SET;
		return parse_set(i, cmd);
	}

	if (strsw(i, "unset")) {
		cmd->type = CMD_SET;
		return parse_unset(i, cmd);
	}

	if (strsw(i, "show")) {
		cmd->type = CMD_SHOW;
		return parse_show(i, cmd);
	}

	if (strsw(i, "add")) {
		cmd->type = CMD_ADD;
		return parse_add(i, cmd);
	}

	if (strsw(i, "del")) {
		cmd->type = CMD_DEL;
		return parse_del(i, cmd);
	}

	cmd->type = CMD_REQ;
	return parse_req(i, cmd);
}
