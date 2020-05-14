/*
    Copyright (C) 2009 - 2010
    Artem Makhutov <artem@makhutov.org>
    http://www.makhutov.org

    Dmitry Vagin <dmitry2004@yandex.ru>

    Copyright (C) 2010 - 2011
    bg <bg_one@mail.ru>

    Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/
#include "ast_config.h"

#include <sys/types.h>

#include <iconv.h>			/* iconv_t iconv() */
#include <string.h>			/* memcpy() */
#include <stdio.h>			/* sscanf() snprintf() */
#include <errno.h>			/* EINVAL */

#include "char_conv.h"
#include "mutils.h"			/* ITEMS_OF() */
#include "gsm7_luts.h"

static char lut_hex2val[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, 
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
static const char *lut_val2hex = "0123456789ABCDEF";

static ssize_t convert_string(const char *in, size_t in_length, char *out, size_t out_size, char *from, char *to)
{
	ICONV_CONST char *in_ptr = (ICONV_CONST char*)in;
	size_t in_bytesleft = in_length;
	char *out_ptr = out;
	size_t out_bytesleft = out_size - 1;
	ICONV_T cd = (ICONV_T)-1;
	ssize_t res;

	cd = iconv_open(to, from);
	if (cd == (ICONV_T)-1) {
		return -1;
	}

	res = iconv(cd, &in_ptr, &in_bytesleft, &out_ptr, &out_bytesleft);
	if (res < 0) {
		iconv_close(cd);
		return -1;
	}

	iconv_close(cd);

	return out_ptr - out;
}

EXPORT_DEF ssize_t utf8_to_ucs2(const char *in, size_t in_length, uint16_t *out, size_t out_size)
{
	ssize_t res = convert_string(in, in_length, (char*)out, out_size * 2, "UTF-8", "UTF-16BE");
	if (res < 0) return res;
	return res / 2;
}
EXPORT_DEF ssize_t ucs2_to_utf8(const uint16_t *in, size_t in_length, char *out, size_t out_size)
{
	return convert_string((const char*)in, in_length * 2, out, out_size, "UTF-16BE", "UTF-8");
}


static char hexchar2val(char h)
{
	return lut_hex2val[h];
}
static char val2hexchar(char h)
{
	return lut_val2hex[h];
}
EXPORT_DEF int unhex(const char *in, uint8_t *out)
{
	int len = 0, nibbles = 0;
	while (in[0]) {
		nibbles += 1 + !!in[1];
		char p0 = hexchar2val(*in++);
		char p1 = *in ? hexchar2val(*in++) : 0;
		if (p0 == -1 || p1 == -1) {
			return -1;
		}
		out[len++] = p0 << 4 | p1;
	}
	return nibbles;
}
EXPORT_DEF void hexify(const uint8_t *in, size_t in_length, char *out)
{
	// code from end of string to allow in-place encoding
	for (int i = in_length - 1; i >= 0; --i) {
		char c0 = val2hexchar(in[i] >> 4), c1 = val2hexchar(in[i] & 15);
		out[i * 2] = c0;
		out[i * 2 + 1] = c1;
	}
	out[in_length * 2] = '\0';
}

#/* */
static const uint8_t *get_char_gsm7_encoding(uint16_t c)
{
	int minor = c >> 8, major = c & 255;
	int subtab = LUT_GSM7_REV1[major];
	if (subtab == -1) return LUT_GSM7_REV2_INV;
	return LUT_GSM7_REV2[subtab][minor];
}

EXPORT_DEF ssize_t gsm7_encode(const uint16_t *in, size_t in_length, uint16_t *out)
{
	// TODO: Should we check for other tables or just use UCS-2?
	unsigned bytes = 0;
	const uint8_t *escenc = get_char_gsm7_encoding(0x1B00);
	for (unsigned i = 0; i < in_length; ++i) {
		const uint8_t *enc = get_char_gsm7_encoding(in[i]);
		uint8_t c = enc[0];
		if (c == GSM7_INVALID) {
			return -1;
		}
		if (c > 127) {
			bytes += 2;
			out[i] = (escenc[0] << 8) | (c - 128);
		} else {
			++bytes;
			out[i] = c;
		}
	}
	return bytes;
}
EXPORT_DEF ssize_t gsm7_pack(const uint16_t *in, size_t in_length, char *out, size_t out_size, unsigned out_padding)
{
	size_t i, x;
	unsigned value = 0;

	/* compute number of bytes we need for the final string, rounded up */
	x = ((out_padding + (7 * in_length) + 7) / 8) + 1;

	/* check that the buffer is not too small */
	if (x > out_size)
		return -1;

	for (x = i = 0; i != in_length; i++) {
		char c[] = { in[i] >> 8, in[i] & 255 };

		for (int j = c[0] == 0; j < 2; ++j) {
			value |= (c[j] & 0x7F) << out_padding;
			out_padding += 7;
			if (out_padding < 8)
				continue;
			/* output one byte */
			out[x++] = value & 0xff;
			value >>= 8;
			out_padding -= 8;
		}
	}
	if (out_padding != 0) {
		out[x++] = value & 0xff;
	}

	/* return total string length in nibbles, excluding terminating zero */
	return x * 2 - (out_padding == 1 || out_padding == 2 || out_padding == 3 ? 1 : 0);
}
EXPORT_DEF ssize_t gsm7_unpack_decode(const char *in, size_t in_nibbles, uint16_t *out, size_t out_size, unsigned in_padding, uint8_t ls, uint8_t ss)
{
	if (ls > 13) ls = 0;
	if (ss > 13) ss = 0;
	size_t i;
	size_t x;
	unsigned value = 0;
	unsigned c;

	if (out_size == 0) {
		return -1;
	}

	/* check if string is empty */
	if (in_nibbles < 2) {
		out[0] = '\0';
		return 0;
	}

	/* account for the bit padding */
	in_padding = 7 - in_padding;

	/* parse the hexstring */
	int esc = 0;
	for (x = i = 0; i < in_nibbles; ++i) {
		if (x >= out_size)
			return -1;
		c = in[i / 2];
		if (i & 1) c >>= 4;
		uint8_t n = c & 0xf;
		value |= n << in_padding;
		in_padding += 4;

		while (in_padding >= 7 * 2) {
			in_padding -= 7;
			value >>= 7;
			{
				uint16_t val = (esc ? LUT_GSM7_SS16 : LUT_GSM7_LS16)[esc ? ss : ls][value & 0x7f];
				if (val == 0x1b) {
					esc = 1;
				} else {
					esc = 0;
					out[x++] = ((val & 0xff) << 8) | (val >> 8);
				}
			}
		}
	}

	return x;
}
