/*
 * encrypt.c
 * Copyright (C) 2015-2016 Ooma Incorporated. All rights reserved.
 */

#include "encrypt.h"
#include <openssl/rand.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#define IV_LEN 12
#define TAG_LEN 16
#define MAX_RAND_BYTES 512

void encryption_init(void)
{
	RAND_poll();
}

#ifdef STANDALONE

static void print_hex_array(GByteArray *byte_array)
{
	int i;
	guint8 *data = byte_array->data;

	for (i = 0; i < byte_array->len; i++)
		btclient_info("0x%02x ", data[i]);
}

#endif

GByteArray* byte_array_get_random(size_t size)
{
	int ret;
	guint8 *rand_bytes;
	GByteArray *byte_array;

	g_assert(size <= MAX_RAND_BYTES);

	rand_bytes = (guint8 *) g_try_malloc0(size);
	if (!rand_bytes) {
		return NULL;
	}

	ret = RAND_bytes(rand_bytes, size);
	if (ret != 1) {
		printf("RAND_bytes failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
		g_free(rand_bytes);
		return NULL;
	}

	byte_array = g_byte_array_new_take((guint8 *)rand_bytes, size);

	return byte_array;
}

GByteArray* hex_string_to_byte_array(GString *str)
{
	int i;
	guint8 a, b, c;
	GByteArray *byte_array;

	byte_array = g_byte_array_sized_new(str->len);

	for(i = 1; i < str->len; i += 2) {

		a = todigit(str->str[i - 1]);
		b = todigit(str->str[i]);
		c = (a << 4) | (b & 0x0f);
		g_byte_array_append(byte_array, &c, 1);
	}

	g_string_free(str, TRUE);
	return byte_array;
}

GString* byte_array_to_hex_string(GByteArray *byte_array)
{
	int i;
	gsize size = byte_array->len;

	g_assert(size > 0);

	GString *str = g_string_sized_new(size * 2 + 1);

	for (i = 0; i < size; i++) {
		g_string_append_printf(str, "%02x", byte_array->data[i]);
	}

	g_byte_array_free(byte_array, TRUE);
	return str;
}

static GString* string_transpose(GString *str, gsize len)
{

	GString *pad = g_string_sized_new(str->len + 1);

	g_string_append_len(pad, str->str + len, len);
	g_string_append_len(pad, str->str, len);

	g_string_free(str, TRUE);
	return pad;
}

static GString* xor_operation(GString *str1, GString *str2)
{
	int i;
	guint8 xor;
	GByteArray *byte_array1, *byte_array2;
	GByteArray *byte_array;

	byte_array1 = hex_string_to_byte_array(str1);
	byte_array2 = hex_string_to_byte_array(str2);

	byte_array = g_byte_array_sized_new(byte_array1->len);

	for (i = 0; i < byte_array1->len; i++) {
		xor = byte_array1->data[i] ^ byte_array2->data[i];
		g_byte_array_append(byte_array, &xor, 1);
	}

	g_byte_array_free(byte_array1, TRUE);
	g_byte_array_free(byte_array2, TRUE);

	return byte_array_to_hex_string(byte_array);
}

/* consumes src memory */
static GString* string_replace_odd_indexes(GString *src, GString *des)
{
	int i;
	GString *str = g_string_sized_new(src->len + 1);

	for (i = 0; i <= src->len; i++) {
		if (i % 2 != 0)
			g_string_append_c(str, src->str[i]);
		else
			g_string_append_c(str, des->str[i]);
	}

	return str;
}

GByteArray* generate_encryption_key(GByteArray *rand_bytes)
{

	GString *pad1 = g_string_new("ad945229f59f4808a38210b99be092ed");
	GString *rand, *pad;

	rand = byte_array_to_hex_string(rand_bytes);

	pad = string_replace_odd_indexes(rand, pad1);
	pad = string_transpose(pad, 16);
	pad = xor_operation(pad, rand);

	g_string_truncate(pad1, 0);
	script_runner(&pad1, "echo -n %s | sha256sum | cut -b33-64", pad->str);

	g_string_free(pad, TRUE);

	return hex_string_to_byte_array(pad1);
}

GByteArray* encrypt_data(GString *msg)
{
	GByteArray *iv;
	GByteArray *key = state.encryption_key;
	g_assert(key != NULL);

	guint8 tag[16], outbuf[512];

	int ret, outlen, cipher_len;

	/* not sure whether we need to create ctx for every message encryption
	 * or create one and use it for all the message encryptions.
	 */
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		btclient_error("ctx memory allocation failed %s", __func__);
		assert(0);
	}

	iv = byte_array_get_random(IV_LEN);

	/* pass IV and key as NULL in this call.
	 * we can pass them in subsequent calls.
	 */
	EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
	EVP_EncryptInit_ex(ctx, NULL, NULL, key->data, iv->data);

	ret = EVP_EncryptUpdate(ctx, outbuf, &outlen, (unsigned char *)msg->str, msg->len + 1);
	if (ret != 1) {
		/* highly unexpected */
		btclient_error("Error in EVP_EncryptUpdate: %s\n", ERR_error_string(ERR_get_error(), NULL));
		assert(0);
	}

	cipher_len = outlen;

	ret = EVP_EncryptFinal_ex(ctx, outbuf + outlen, &outlen);
	if (ret != 1) {
		printf("Error in EVP_EncryptFinal_ex: %s\n", ERR_error_string(ERR_get_error(), NULL));
		assert(0);
	}

	cipher_len += outlen;

	if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag)) {
		btclient_error("Error in EVP_CIPHER_CTX_ctrl: %s\n", ERR_error_string(ERR_get_error(), NULL));
		assert(0);
	}

	g_byte_array_append(iv, outbuf, cipher_len);
	g_byte_array_append(iv, tag, TAG_LEN);

	EVP_CIPHER_CTX_free(ctx);

	return iv;
}

/*
 * 96 bit IV + encrypted JSON ciphertext + 128 bit authentication tag
 */
GByteArray* decrypt_data(GByteArray *cipher)
{
	guint8 text[512];
	int ret, len, outlen;
	int cipher_len = cipher->len - IV_LEN - TAG_LEN;
	GByteArray *msg = NULL;
	GByteArray *key = state.encryption_key;

	/* coding error if length is less than zero.
	 * length check should be performed before calling this method.
	 */
	g_assert(key != NULL && cipher_len > 0);

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		btclient_error("ctx memory allocation failed %s", __func__);
		assert(0);
	}

	/* pass IV and key as NULL in this call.
	 * we can pass them in subsequent calls.
	 */
	EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);

	/* first 12 bytes of cipher is initialization vector */
	EVP_DecryptInit_ex(ctx, NULL, NULL, key->data, cipher->data);

	ret = EVP_DecryptUpdate(ctx, text, &len, cipher->data + IV_LEN, cipher_len);
	if (ret != 1) {
		btclient_error("Error in EVP_DecryptUpdate: %s\n", ERR_error_string(ERR_get_error(), NULL));
		assert(0);
	}
	outlen = len;

	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, cipher->data + IV_LEN + cipher_len);

	ret = EVP_DecryptFinal_ex(ctx, text, &len);
	if (ret <= 0) {
		btclient_error("TAG match failed\n");
		goto cleanup;
	}

	outlen += len;

	msg = g_byte_array_sized_new(outlen);
	g_byte_array_append(msg, (const guint8 *)text, outlen);

	EVP_CIPHER_CTX_free(ctx);

cleanup:
	g_byte_array_free(cipher, TRUE);
	return msg;
}

#ifdef STANDALONE

int main(void)
{
	encryption_init();
	GByteArray *pad = byte_array_get_random(16);
	key = generate_encryption_key(pad);
	print_hex_array(key);
	GString *msg = g_string_new("{ \"req\" : 1, \"tid\" : 2 \"resp\" : { } }");
	GByteArray *cipher = encrypt_data(msg);
	print_hex_array(cipher);
	pad = decrypted_data(cipher);
	printf("decrypted string:\n");
	print_hex_array(pad);
	g_string_free(msg, TRUE);
}

#endif
