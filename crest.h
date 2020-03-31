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

#include "compat/compat.h"
#include <imsg.h>

enum imsg_type {

	/* parent -> child
	 * tell the child to exit */
	IMSG_EXIT,

	/* parent -> child
	 * tell the child the method for the next req */
	IMSG_SET_METHOD,

	/* parent -> child
	 * tell the child the url for the next request */
	IMSG_SET_URL,

	/* parent -> child
	 * tell the child the payload for the next request */
	IMSG_SET_PAYLOAD,

	/* parent -> child
	 * tell the child to perform the request */
	IMSG_DO_REQ,

	/* parent <- child
	 * curl failed */
	IMSG_ERR,

	/* parent <- child
	 * return the http status */
	IMSG_STATUS,

	/* parent <- child
	 * return the headers */
	IMSG_HEAD,

	/* parent <- child
	 * return the body */
	IMSG_BODY,

	/* parent -> child */
	IMSG_SET_UA,

	/* parent -> child */
	IMSG_SET_PREFIX,

	/* parent -> child */
	IMSG_SET_HTTPVER,

	/* parent -> child */
	IMSG_SET_PORT,

	/* parent -> child */
	IMSG_SET_PEER_VERIF,

	/* parent -> child */
	IMSG_SHOW,

	/* parent -> child
	 * add header */
	IMSG_ADD,

	/* parent -> child
	 * delete header */
	IMSG_DEL,

	/* parent <- child */
	IMSG_DONE,
};

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

struct req {
	enum http_methods method;
	char *path;
	char *payload;
};

struct setopt {
	enum	 imsg_type set;
	void	*value;
	size_t	 len;
};

enum special_cmd_type {
	SC_HELP,
	SC_QUIT,
	SC_VERSION,
};

struct cmd {
	enum {
		CMD_REQ,
		CMD_SET,
		CMD_SHOW,
		CMD_ADD,
		CMD_DEL,
		CMD_SPECIAL,
	} type;
	union {
		struct req req;
		struct setopt opt;
		enum imsg_type show;
		const char *hdrname;
		enum special_cmd_type sp;
	};
};

struct resp {
	long	http_code;

	size_t	 hlen;
	char	*headers;

	size_t	 blen;
	char	*body;

	size_t	 errlen;
	char	*err;
};

struct str {
	char *s;
	int dirty;
};

#define LITERAL_STR(a) ((struct str){ .s=(a), .dirty=0})
#define FREE_STR(a)			\
	do {				\
		if ((a).dirty)		\
			free(a.s);	\
	} while(0)
#define UPDATE_STR(_a, _s, _v)		\
	do {				\
		FREE_STR(_a);		\
		(_a).s = _s;		\
		(_a).dirty = _v;	\
	} while(0)

struct svec {
	size_t len;
	size_t cap;
	struct str *d;
};

struct settings {
	size_t bufsize;
	struct str useragent;
	struct str prefix;
	long http_version;
	long port; /* it's -1 or uint16_t in reality */
	int skip_peer_verification;
};

extern struct settings settings;
extern const char *prgname;
extern const char *prompt;

/* parse-related stuff */
const char	*method2str(enum http_methods);
const char	*httpver2str(long);
int		 strsw(const char*, const char*);
int		 parse(const char*, struct cmd*);

/* http stuff */
int		 do_req(const struct req*, struct resp*, struct svec*);
void		 free_resp(struct resp*);

/* print the prompt and read a line (getline(3)-style) */
char		*rlf(const char*, FILE*);
/* wait until fd becomes ready to read */
int		 poll_read(int);

/* main loop */
int		 repl(struct imsgbuf*, FILE*);

/* svec related */
struct svec	*svec_add(struct svec*, char*, int);
int		 svec_del(struct svec*, const char*);
void		 svec_free(struct svec*);
struct curl_slist *svec_to_curl(struct svec*);

/* child related */
int	child_main(struct imsgbuf*);
void	csend(struct imsgbuf*, int, const void*, size_t);

#endif
