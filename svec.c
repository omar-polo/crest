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
#include <stdlib.h>

static struct svec *
svec_new()
{
	struct svec *svec;

	svec = malloc(sizeof(struct svec));
	if (svec == NULL)
		return NULL;
	svec->len = 0;
	svec->cap = 8;

	svec->d = malloc(svec->cap * sizeof(struct str));
	if (svec->d == NULL) {
		free(svec);
		return NULL;
	}

	return svec;
}

static struct svec *
svec_grow(struct svec *svec)
{
	size_t c;
	struct str *d;

	c = svec->cap * 1.5;
	d = realloc(svec->d, c * sizeof(struct str));
	if (d == NULL)
		return NULL;

	svec->d = d;
	svec->cap = c;
	return svec;
}

static int
same_header(const char *a, const char *b)
{
	int x, y;

	for (; *a && *b; ++a, ++b) {
		x = tolower(*a);
		y = tolower(*b);

		if (x == ':' && y == ':')
			return 1;

		if (x == ':' || y == ':' || x != y)
			return 0;
	}

	return 0;
}

struct svec *
svec_add(struct svec *svec, char *str, int dirty)
{
	size_t i;

	/* allocate if necessary */
	if (svec == NULL) {
		if ((svec = svec_new()) == NULL)
			return NULL;
	}

	/* overwrite if already present */
	for (i = 0; i < svec->len; ++i) {
		if (same_header(str, svec->d[i].s)) {
			if (svec->d[i].dirty)
				free(svec->d[i].s);

			svec->d[i].s = str;
			svec->d[i].dirty = dirty;
			return svec;
		}
	}

	/* grow if needed */
	if (svec->len == svec->cap) {
		if ((svec = svec_grow(svec)) == NULL)
			return NULL;
	}

	/* append */
	svec->d[svec->len].s = str;
	svec->d[svec->len].dirty = dirty;

	svec->len++;

	return svec;
}

void
svec_free(struct svec *svec)
{
	size_t i;

	for (i = 0; i < svec->len; ++i) {
		if (svec->d[i].dirty)
			free(svec->d[i].s);
	}

	free(svec->d);
	free(svec);
}

struct curl_slist *
svec_to_curl(struct svec *svec)
{
	struct curl_slist *h, *t;
	size_t i;

	if (svec == NULL)
		return NULL;

	h = t = NULL;
	for (i = 0; i < svec->len; ++i) {
		t = curl_slist_append(h, svec->d[i].s);
		if (t == NULL) {
			if (h != NULL)
				free(h);
			return NULL;
		}
		h = t;
	}

	return h;
}
