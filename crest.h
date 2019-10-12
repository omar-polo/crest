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

#ifndef CREST_H
#define CREST_H

#include <curl/curl.h>
#include <sys/types.h>

#define BUFFER_SIZE (256 * 1024) /* 256 kb */
#define PROMPT "> "
#define USERAGENT "cREST/0.1"

extern const char *prefix;
extern char *prgname;

extern long http_version;
extern long port; /* it's really a 16 bit */

extern int verbose;
extern int skip_peer_verification;

extern struct curl_slist *headers;

enum http_methods {
	CONNECT,
	DELETE,
	GET,
	HEAD,
	OPTIONS,
	PATCH,
	POST,
	PUT,
	TRACE,
};

struct cmd {
	enum http_methods method;
	char *path;
	char *payload;
};

void		 usage();

/* parse-related stuff */
const char	*method2str(enum http_methods);
int		 parse(const char*, struct cmd*);

/* http stuff */
int		 do_cmd(const struct cmd*, char**, size_t*);

/* print the prompt and read a line (getline(3)-style) */
ssize_t		 readline_wp(char ** restrict, size_t * restrict, 
			const char * restrict);

/* main loop */
int		 repl();

#endif
