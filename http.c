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
#include <stdlib.h>
#include <string.h>

struct write_result {
	char *data;
	size_t pos;
	size_t size;
};

static size_t
write_res(void *ptr, size_t size, size_t nmemb, void *s)
{
	struct write_result *res = s;

	if (res->pos + size * nmemb >= res->size) {
		char *n;
		size_t ns;

		ns = res->size + res->size * 1.5;

		if ((n = realloc(res->data, ns)) == NULL) {
			warn("write_res: realloc");
			return 0;
		}

		res->data = n;
		res->size = ns;
	}

	memcpy(res->data + res->pos, ptr, size * nmemb);
	res->pos += size * nmemb;
	res->data[res->pos] = '\0'; /* NUL-terminate the data */
	return size * nmemb;
}

static char *
do_url(const struct cmd *cmd)
{
	char *u;
	int l;

	if (prefix == NULL)
		return strdup(cmd->path);

	/* strlen(prefix) >= 1 by main() */

	if (*cmd->path == '/' && prefix[strlen(prefix) - 1] == '/')
		l = asprintf(&u, "%s%s", prefix, cmd->path + 1);
	else if (*cmd->path == '/' || prefix[strlen(prefix) - 1] == '/')
		l = asprintf(&u, "%s%s", prefix, cmd->path);
	else
		l = asprintf(&u, "%s/%s", prefix, cmd->path);

	if (l == -1) {
		warn("asprintf");
		return NULL;
	}

	return u;
}

int
do_cmd(const struct cmd *cmd, char **rets, size_t *retl)
{
	CURL *curl;
	CURLcode code;
	char *url;
	int r;
	struct write_result res;

	*rets = NULL;
	*retl = 0;

	if ((url = do_url(cmd)) == NULL)
		return 0;

	if ((curl = curl_easy_init()) == NULL) {
		warnx("curl_easy_init failed");
		free(url);
		return 0;
	}

	switch (cmd->method) {
	case GET:
		break;

	case HEAD:
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		break;

	case POST:
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cmd->payload);
		break;

	case CONNECT:
	case DELETE:
	case OPTIONS:
	case PATCH:
	case PUT:
	case TRACE:
	default:
		warnx("method %s not (yet) supported", method2str(cmd->method));
		curl_easy_cleanup(curl);
		free(url);
		return 0;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, fwrite);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, stdout);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, http_version);

	if (port != -1) /* port is in valid range due to main(), or is -1 */
		curl_easy_setopt(curl, CURLOPT_PORT, port);

	if (skip_peer_verification)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

	if (cmd->method == HEAD) {
		memset(&res, 0, sizeof(struct write_result));
	} else {
		res.data = calloc(BUFFER_SIZE, 1);
		res.size = BUFFER_SIZE;
		res.pos = 0;

		if (res.data == NULL) {
			warn("calloc");
			curl_easy_cleanup(curl);
			free(url);
			return 0;
		}
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_res);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);

	if (headers != NULL)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	code = curl_easy_perform(curl);

	if (code != CURLE_OK) {
		warnx("curl_easy_perform(%s): %s", url,
			curl_easy_strerror(code));

		if (res.data != NULL)
			free(res.data);

		*rets = NULL;
		*retl = 0;
	} else {
		printf("%s\n", res.data);
		*rets = res.data;
		*retl = res.size;
	}

	curl_easy_cleanup(curl);
	free(url);

	return 1;
}