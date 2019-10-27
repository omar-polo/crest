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

#include <curl/curl.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "crest.h"

struct write_result {
	char *data;
	size_t pos;
	size_t size;
};

static int
init_res(struct write_result *res)
{
	memset(res, 0, sizeof(struct write_result));

	res->data = calloc(settings.bufsize, 1);
	res->size = settings.bufsize;
	res->pos = 0;

	if (res->data == NULL)
		err(1, "calloc");
	return 1;
}

static size_t
resize_res(struct write_result *res)
{
	char *n;
	size_t ns;

	ns = res->size + res->size * 1.5;

	if ((n = realloc(res->data, ns)) == NULL) {
		warn("write_res: realloc");
		return 0;
	}

	res->data = n;
	res->size = ns;

	return ns;
}

static size_t
write_res_header(void *ptr, size_t size, size_t nmemb, void *s)
{
	size_t i;
	char *p;
	struct write_result *res = s;

	p = ptr;

	if (res->pos + size * nmemb >= res->size) {
		if (resize_res(res) == 0)
			return 0;
	}

	for (i = 0; i < size * nmemb; ++i) {
		if (p[i] == '\r')
			continue;

		res->data[res->pos] = p[i];
		res->pos++;
	}

	res->data[res->pos] = '\0'; /* NUL-terminate the data */
	return size * nmemb;
}

static size_t
write_res(void *ptr, size_t size, size_t nmemb, void *s)
{
	struct write_result *res = s;

	if (res->pos + size * nmemb >= res->size) {
		if (resize_res(res) == 0)
			return 0;
	}

	memcpy(res->data + res->pos, ptr, size * nmemb);
	res->pos += size * nmemb;
	res->data[res->pos] = '\0'; /* NUL-terminate the data */
	return size * nmemb;
}

static char *
do_url(const struct req *req)
{
	char *u, *prefix;
	int l;

	prefix = settings.prefix.s;

	if (prefix == NULL)
		return strdup(req->path);

	/* strlen(prefix) >= 1 by main() */

	if (*req->path == '/' && prefix[strlen(prefix) - 1] == '/')
		l = asprintf(&u, "%s%s", prefix, req->path + 1);
	else if (*req->path == '/' || prefix[strlen(prefix) - 1] == '/')
		l = asprintf(&u, "%s%s", prefix, req->path);
	else
		l = asprintf(&u, "%s/%s", prefix, req->path);

	if (l == -1) {
		warn("asprintf");
		return NULL;
	}

	return u;
}

int
do_req(const struct req *req, struct resp *resp, struct svec *headers)
{
	CURL *curl;
	CURLcode code;
	char *url;
	int ret;
	struct write_result hdr, res;
	struct curl_slist *hdrs;

	curl = NULL;
	url = NULL;
	hdrs = NULL;
	ret = 0;

	memset(resp, 0, sizeof(struct resp));

	if ((url = do_url(req)) == NULL)
		return 0;

	if ((curl = curl_easy_init()) == NULL) {
		warnx("curl_easy_init failed");
		goto fail;
	}

	switch (req->method) {
	case DELETE:
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

		if (req->payload != NULL)
			curl_easy_setopt(
				curl, CURLOPT_POSTFIELDS, req->payload);
		break;

	case GET:
		break;

	case HEAD:
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		break;

	case OPTIONS:
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");

		/* by RFC 7231 if a payload is present, we MUST send a
		 * Content-Type.  We don't have still a way to define
		 * the Content-Type of the request, so... */
		if (req->payload != NULL)
			warnx("ignoring payload for OPTIONS\n");
		break;

	case POST:
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->payload);
		break;

	case CONNECT:
	case PATCH:
	case PUT:
	case TRACE:
	default:
		warnx("method %s not (yet) supported",
			method2str(req->method));
		goto fail;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, settings.useragent);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, settings.http_version);

	/* port is in valid range due to main(), or is -1 */
	if (settings.port != -1)
		curl_easy_setopt(curl, CURLOPT_PORT, settings.port);

	if (settings.skip_peer_verification)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

	init_res(&hdr);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &write_res_header);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hdr);

	if (req->method == HEAD)
		memset(&res, 0, sizeof(struct write_result));
	else
		init_res(&res);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_res);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);

	hdrs = NULL;
	if (headers != NULL) {
		hdrs = svec_to_curl(headers);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	}

	code = curl_easy_perform(curl);

	if (code != CURLE_OK) {
		warnx("curl_easy_perform(%s): %s", url,
			curl_easy_strerror(code));

		if (res.data != NULL)
			free(res.data);
		if (hdr.data != NULL)
			free(hdr.data);
		hdr.data = res.data = NULL;

		goto fail;
	} else {
		curl_easy_getinfo(
			curl, CURLINFO_RESPONSE_CODE, &resp->http_code);
		resp->hlen = hdr.pos;
		resp->headers = hdr.data;
		resp->blen = res.pos;
		resp->body = res.data;
	}

	ret = 1;

fail:
	if (url != NULL)
		free(url);
	if (curl != NULL)
		curl_easy_cleanup(curl);
	if (hdrs != NULL)
		curl_slist_free_all(hdrs);

	return ret;
}

void
free_resp(struct resp *r)
{
	if (r->headers != NULL)
		free(r->headers);
	if (r->body != NULL)
		free(r->body);
	if (r->err != NULL)
		free(r->err);
}
