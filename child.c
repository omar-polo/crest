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

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "crest.h"

struct svec *headers;
#define HPUSH(h, v, d)                                                       \
	do {                                                                 \
		struct svec *t = NULL;                                       \
		t = svec_add(h, v, d);                                       \
		if (t == NULL)                                               \
			err(1, "svec_add");                                  \
		h = t;                                                       \
	} while (0)

struct settings settings;

/* wrapper around imsg_compose to send a message to the child.  It will
 * implicitly write */
void
csend(struct imsgbuf *ibuf, int type, void *ptr, size_t len)
{
	ssize_t n;

	imsg_compose(ibuf, type, 0, 0, -1, ptr, len);

	/* retry on errno == EAGAIN? */
	if ((n = msgbuf_write(&ibuf->w)) == -1)
		err(1, "msgbuf_write");
	if (n == 0)
		errx(1, "child vanished");
}

static void
child_do_req(struct imsgbuf *ibuf, struct req *req)
{
	ssize_t n;
	struct resp r;

	if (!do_req(req, &r, headers)) {
		const char *err = "failed";
		n = strlen(err);
		imsg_compose(ibuf, IMSG_ERR, 0, 0, -1, err, n);
	} else {
		if (r.hlen >= UINT16_MAX || r.blen >= UINT16_MAX)
			errx(1, "response headers or body too big.");
		imsg_compose(ibuf, IMSG_STATUS, 0, 0, -1, &r.http_code,
			sizeof(r.http_code));
		imsg_compose(ibuf, IMSG_HEAD, 0, 0, -1, r.headers, r.hlen);
		imsg_compose(ibuf, IMSG_BODY, 0, 0, -1, r.body, r.blen);
	}

	/* retry on errno == EAGAIN? */
	if ((n = msgbuf_write(&ibuf->w)) == -1)
		err(1, "msgbuf_write");
	if (n == 0)
		errx(1, "parent vanished");

	if (r.headers != NULL)
		free(r.headers);
	if (r.body != NULL)
		free(r.body);
	if (r.err != NULL)
		free(r.body);
}

static void
show(enum imsg_type t)
{
	switch (t) {
	case IMSG_SET_HEADER: {
		size_t i;

		if (headers == NULL)
			break;

		for (i = 0; i < headers->len; ++i)
			printf("%s\n", headers->d[i].s);
		break;
	}

	case IMSG_SET_UA:
		printf("%s\n", settings.useragent.s);
		break;

	case IMSG_SET_PREFIX:
		printf("%s\n", settings.prefix.s);
		break;

	case IMSG_SET_HTTPVER:
		printf("%s\n", httpver2str(settings.http_version));
		break;

	case IMSG_SET_PORT:
		if (settings.port != -1)
			printf("%ld\n", settings.port);
		break;

	case IMSG_SET_PEER_VERIF:
		if (settings.skip_peer_verification)
			puts("false");
		else
			puts("true");
		break;

	default:
		errx(1, "unknown show %d", t);
	}
}

/* return 1 only on IMSG_EXIT */
static int
process_messages(struct imsgbuf *ibuf, struct req *req)
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
			done = 1;
			break;

		case IMSG_SET_METHOD:
			if (datalen < sizeof(req->method))
				err(1, "IMSG_SET_METHOD wrong size");
			memcpy(&req->method, imsg.data, sizeof(req->method));
			break;

		case IMSG_SET_URL:
			req->path = calloc(datalen + 1, 1);
			if (req->path == NULL)
				err(1, "calloc");
			memcpy(req->path, imsg.data, datalen);
			break;

		case IMSG_SET_PAYLOAD:
			if (datalen == 0) {
				req->payload = NULL;
				break;
			}

			req->payload = calloc(datalen + 1, 1);
			if (req->payload == NULL)
				err(1, "calloc");
			memcpy(req->payload, imsg.data, datalen);
			break;

		case IMSG_DO_REQ:
			child_do_req(ibuf, req);

			free(req->path);
			if (req->payload != NULL)
				free(req->payload);
			req->path = req->payload = NULL;

			break;

		case IMSG_SET_HEADER: {
			char *h;
			if ((h = calloc(datalen + 1, 1)) == NULL)
				err(1, "calloc");
			memcpy(h, imsg.data, datalen);
			HPUSH(headers, h, 1);
			break;
		}

		case IMSG_SET_UA: {
			char *h;
			if ((h = calloc(datalen + 1, 1)) == NULL)
				err(1, "calloc");
			memcpy(h, imsg.data, datalen);
			UPDATE_STR(settings.useragent, h, 1);
			break;
		}

		case IMSG_SET_PREFIX: {
			char *h;
			if (datalen == 0)
				errx(1, "prefix cannot be empty");
			if ((h = calloc(datalen + 1, 1)) == NULL)
				err(1, "calloc");
			memcpy(h, imsg.data, datalen);
			UPDATE_STR(settings.prefix, h, 1);
			break;
		}

		case IMSG_SET_HTTPVER: {
			if (datalen != sizeof(settings.http_version))
				errx(1, "http_version: size mismatch");
			memcpy(&settings.http_version, imsg.data, datalen);
			break;
		}

		case IMSG_SET_PORT: {
			if (datalen != sizeof(settings.port))
				errx(1, "port: size mismatch");
			memcpy(&settings.port, imsg.data, datalen);
			if (settings.port == -1 && settings.port < 0
				&& settings.port > 65535)
				errx(1, "invalid port number %ld",
					settings.port);
			break;
		}

		case IMSG_SET_PEER_VERIF: {
			if (datalen
				!= sizeof(settings.skip_peer_verification))
				errx(1,
					"skip_peer_verification: size "
					"mismatch");
			memcpy(&settings.skip_peer_verification, imsg.data,
				datalen);
			break;
		}

		case IMSG_SHOW:
			show(*(enum imsg_type *)imsg.data);
			csend(ibuf, IMSG_DONE, NULL, 0);
			break;

		default:
			errx(1, "Unknown message type %d", imsg.hdr.type);
		}

		imsg_free(&imsg);
	}

	return 1;
}

int
child_main(struct imsgbuf *ibuf)
{
	struct req req;

	memset(&req, 0, sizeof(struct req));

	memset(&settings, 0, sizeof(struct settings));
	settings.bufsize = 256 * 1024; /* 256 kb */
	settings.useragent = LITERAL_STR("cREST/0.1");
	settings.http_version = CURL_HTTP_VERSION_2TLS;
	settings.port = -1;

	headers = NULL;

	curl_global_init(CURL_GLOBAL_DEFAULT);

	for (;;)
		if (process_messages(ibuf, &req))
			break;

	svec_free(headers);
	curl_global_cleanup();

	FREE_STR(settings.useragent);
	FREE_STR(settings.prefix);

	return 0;
}
