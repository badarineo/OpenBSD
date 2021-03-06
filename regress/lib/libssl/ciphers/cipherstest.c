/*
 * Copyright (c) 2015, 2020 Joel Sing <jsing@openbsd.org>
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

#include <openssl/ssl.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

int ssl_parse_ciphersuites(STACK_OF(SSL_CIPHER) **out_ciphers, const char *str);

static int
get_put_test(const char *name, const SSL_METHOD *method)
{
	STACK_OF(SSL_CIPHER) *ciphers;
	const SSL_CIPHER *cipher;
	unsigned char buf[2];
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int ret = 1;
	int i, len;

	if ((len = method->put_cipher_by_char(NULL, NULL)) != 2) {
		fprintf(stderr,
		    "%s: put_cipher_by_char() returned len %i (want 2)\n",
		    name, len);
		return (1);
	}

	if ((ssl_ctx = SSL_CTX_new(method)) == NULL) {
		fprintf(stderr, "%s: SSL_CTX_new() returned NULL\n", name);
		goto failure;
	}
	if ((ssl = SSL_new(ssl_ctx)) == NULL) {
		fprintf(stderr, "%s: SSL_new() returned NULL\n", name);
		goto failure;
	}

	if ((ciphers = SSL_get_ciphers(ssl)) == NULL) {
		fprintf(stderr, "%s: no ciphers\n", name);
		goto failure;
	}

	for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
		cipher = sk_SSL_CIPHER_value(ciphers, i);
		if ((len = method->put_cipher_by_char(cipher, buf)) != 2) {
			fprintf(stderr,
			    "%s: put_cipher_by_char() returned len %i for %s "
			    "(want 2)\n",
			    name, len, SSL_CIPHER_get_name(cipher));
			goto failure;
		}
		if ((cipher = method->get_cipher_by_char(buf)) == NULL) {
			fprintf(stderr,
			    "%s: get_cipher_by_char() returned NULL for %s\n",
			    name, SSL_CIPHER_get_name(cipher));
			goto failure;
		}
	}

	ret = 0;

 failure:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (ret);
}

static int
cipher_get_put_tests(void)
{
	int failed = 0;

	failed |= get_put_test("SSLv23", SSLv23_method());
	failed |= get_put_test("SSLv23_client", SSLv23_client_method());
	failed |= get_put_test("SSLv23_server", SSLv23_server_method());

	failed |= get_put_test("TLSv1", TLSv1_method());
	failed |= get_put_test("TLSv1_client", TLSv1_client_method());
	failed |= get_put_test("TLSv1_server", TLSv1_server_method());

	failed |= get_put_test("TLSv1_1", TLSv1_1_method());
	failed |= get_put_test("TLSv1_1_client", TLSv1_1_client_method());
	failed |= get_put_test("TLSv1_1_server", TLSv1_1_server_method());

	failed |= get_put_test("TLSv1_2", TLSv1_2_method());
	failed |= get_put_test("TLSv1_2_client", TLSv1_2_client_method());
	failed |= get_put_test("TLSv1_2_server", TLSv1_2_server_method());

	failed |= get_put_test("DTLSv1", DTLSv1_method());
	failed |= get_put_test("DTLSv1_client", DTLSv1_client_method());
	failed |= get_put_test("DTLSv1_server", DTLSv1_server_method());

	return failed;
}

static int
cipher_get_by_value_tests(void)
{
	STACK_OF(SSL_CIPHER) *ciphers;
	const SSL_CIPHER *cipher;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	unsigned long id;
	uint16_t value;
	int ret = 1;
	int i;

	if ((ssl_ctx = SSL_CTX_new(SSLv23_method())) == NULL) {
		fprintf(stderr, "SSL_CTX_new() returned NULL\n");
		goto failure;
	}
	if ((ssl = SSL_new(ssl_ctx)) == NULL) {
		fprintf(stderr, "SSL_new() returned NULL\n");
		goto failure;
	}

	if ((ciphers = SSL_get_ciphers(ssl)) == NULL) {
		fprintf(stderr, "no ciphers\n");
		goto failure;
	}

	for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
		cipher = sk_SSL_CIPHER_value(ciphers, i);

		id = SSL_CIPHER_get_id(cipher);
		if (SSL_CIPHER_get_by_id(id) == NULL) {
			fprintf(stderr, "SSL_CIPHER_get_by_id() failed "
			    "for %s (0x%lx)\n", SSL_CIPHER_get_name(cipher),
			    id);
			goto failure;
		}

		value = SSL_CIPHER_get_value(cipher);
		if (SSL_CIPHER_get_by_value(value) == NULL) {
			fprintf(stderr, "SSL_CIPHER_get_by_value() failed "
			    "for %s (0x%04hx)\n", SSL_CIPHER_get_name(cipher),
			    value);
			goto failure;
		}
	}

	ret = 0;

 failure:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	return (ret);
}

struct parse_ciphersuites_test {
	const char *str;
	const int want;
	const unsigned long cids[32];
};

struct parse_ciphersuites_test parse_ciphersuites_tests[] = {
	{
		/* LibreSSL names. */
		.str = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256:AEAD-AES128-GCM-SHA256",
		.want = 1,
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_3_CK_AES_128_GCM_SHA256,
		},
	},
	{
		/* OpenSSL names. */
		.str = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256",
		.want = 1,
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_3_CK_AES_128_GCM_SHA256,
		},
	},
	{
		/* Different priority order. */
		.str = "AEAD-AES128-GCM-SHA256:AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.want = 1,
		.cids = {
			TLS1_3_CK_AES_128_GCM_SHA256,
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
		},
	},
	{
		/* Known but unsupported names. */
		.str = "AEAD-AES256-GCM-SHA384:AEAD-AES128-CCM-SHA256:AEAD-AES128-CCM-8-SHA256",
		.want = 1,
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
		},
	},
	{
		/* Empty string means no TLSv1.3 ciphersuites. */
		.str = "",
		.want = 1,
		.cids = { 0 },
	},
	{
		.str = "TLS_CHACHA20_POLY1305_SHA256:TLS_NOT_A_CIPHERSUITE",
		.want = 0,
	},
	{
		.str = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256,TLS_AES_128_GCM_SHA256",
		.want = 0,
	},
};

#define N_PARSE_CIPHERSUITES_TESTS \
    (sizeof(parse_ciphersuites_tests) / sizeof(*parse_ciphersuites_tests))

static int
parse_ciphersuites_test()
{
	struct parse_ciphersuites_test *pct;
	STACK_OF(SSL_CIPHER) *ciphers = NULL;
	SSL_CIPHER *cipher;
	int failed = 1;
	int j, ret;
	size_t i;

	for (i = 0; i < N_PARSE_CIPHERSUITES_TESTS; i++) {
		pct = &parse_ciphersuites_tests[i];

		ret = ssl_parse_ciphersuites(&ciphers, pct->str);
		if (ret != pct->want) {
			fprintf(stderr, "FAIL: test %zu - "
			    "ssl_parse_ciphersuites returned %d, want %d\n",
			    i, ret, pct->want);
			goto failed;
		}
		if (ret == 0)
			continue;

		for (j = 0; j < sk_SSL_CIPHER_num(ciphers); j++) {
			cipher = sk_SSL_CIPHER_value(ciphers, j);
			if (SSL_CIPHER_get_id(cipher) == pct->cids[j])
				continue;
			fprintf(stderr, "FAIL: test %zu - got cipher %d with "
			    "id %lx, want %lx\n", i, j,
			    SSL_CIPHER_get_id(cipher), pct->cids[j]);
			goto failed;
		}
		if (pct->cids[j] != 0) {
			fprintf(stderr, "FAIL: test %zu - got %d ciphers, "
			    "expected more", i, sk_SSL_CIPHER_num(ciphers));
			goto failed;
		}
	}

	failed = 0;

 failed:
	sk_SSL_CIPHER_free(ciphers);

	return failed;
}

struct cipher_set_test {
	int ctx_ciphersuites_first;
	const char *ctx_ciphersuites;
	const char *ctx_rulestr;
	int ssl_ciphersuites_first;
	const char *ssl_ciphersuites;
	const char *ssl_rulestr;
	const unsigned long cids[32];
};

struct cipher_set_test cipher_set_tests[] = {
	{
		.ctx_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_3_CK_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_3_CK_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ctx_ciphersuites_first = 1,
		.ctx_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ctx_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ssl_ciphersuites_first = 1,
		.ssl_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ctx_ciphersuites_first = 0,
		.ctx_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ctx_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ssl_ciphersuites_first = 0,
		.ssl_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ctx_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.ssl_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
	{
		.ctx_rulestr = "TLSv1.2+ECDHE+AEAD+AES",
		.ssl_ciphersuites = "AEAD-AES256-GCM-SHA384:AEAD-CHACHA20-POLY1305-SHA256",
		.cids = {
			TLS1_3_CK_AES_256_GCM_SHA384,
			TLS1_3_CK_CHACHA20_POLY1305_SHA256,
			TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		},
	},
};

#define N_CIPHER_SET_TESTS \
    (sizeof(cipher_set_tests) / sizeof(*cipher_set_tests))

static int
cipher_set_test()
{
	struct cipher_set_test *cst;
	STACK_OF(SSL_CIPHER) *ciphers = NULL;
	SSL_CIPHER *cipher;
	SSL_CTX *ctx = NULL;
	SSL *ssl = NULL;
	int failed = 1;
	size_t i;
	int j;

	for (i = 0; i < N_CIPHER_SET_TESTS; i++) {
		cst = &cipher_set_tests[i];

		if ((ctx = SSL_CTX_new(TLS_method())) == NULL)
			errx(1, "SSL_CTX_new");

		if (cst->ctx_ciphersuites_first && cst->ctx_ciphersuites != NULL) {
			if (!SSL_CTX_set_ciphersuites(ctx, cst->ctx_ciphersuites))
				errx(1, "SSL_CTX_set_ciphersuites");
		}
		if (cst->ctx_rulestr != NULL) {
			if (!SSL_CTX_set_cipher_list(ctx, cst->ctx_rulestr))
				errx(1, "SSL_CTX_set_cipher_list");
		}
		if (!cst->ctx_ciphersuites_first && cst->ctx_ciphersuites != NULL) {
			if (!SSL_CTX_set_ciphersuites(ctx, cst->ctx_ciphersuites))
				errx(1, "SSL_CTX_set_ciphersuites");
		}

		/* XXX - check SSL_CTX_get_ciphers(ctx) */

		if ((ssl = SSL_new(ctx)) == NULL)
			errx(1, "SSL_new");

		if (cst->ssl_ciphersuites_first && cst->ssl_ciphersuites != NULL) {
			if (!SSL_set_ciphersuites(ssl, cst->ssl_ciphersuites))
				errx(1, "SSL_set_ciphersuites");
		}
		if (cst->ssl_rulestr != NULL) {
			if (!SSL_set_cipher_list(ssl, cst->ssl_rulestr))
				errx(1, "SSL_set_cipher_list");
		}
		if (!cst->ssl_ciphersuites_first && cst->ssl_ciphersuites != NULL) {
			if (!SSL_set_ciphersuites(ssl, cst->ssl_ciphersuites))
				errx(1, "SSL_set_ciphersuites");
		}

		ciphers = SSL_get_ciphers(ssl);

		for (j = 0; j < sk_SSL_CIPHER_num(ciphers); j++) {
			cipher = sk_SSL_CIPHER_value(ciphers, j);
			if (SSL_CIPHER_get_id(cipher) == cst->cids[j])
				continue;
			fprintf(stderr, "FAIL: test %zu - got cipher %d with "
			    "id %lx, want %lx\n", i, j,
			    SSL_CIPHER_get_id(cipher), cst->cids[j]);
			goto failed;
		}
		if (cst->cids[j] != 0) {
			fprintf(stderr, "FAIL: test %zu - got %d ciphers, "
			    "expected more", i, sk_SSL_CIPHER_num(ciphers));
			goto failed;
		}

		SSL_CTX_free(ctx);
		ctx = NULL;
		SSL_free(ssl);
		ssl = NULL;
	}

	failed = 0;

 failed:
	SSL_CTX_free(ctx);
	SSL_free(ssl);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= cipher_get_put_tests();
	failed |= cipher_get_by_value_tests();

	failed |= parse_ciphersuites_test();
	failed |= cipher_set_test();

	return (failed);
}
