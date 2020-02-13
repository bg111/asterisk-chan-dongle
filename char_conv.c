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

EXPORT_DEF ssize_t convert_string (const char* in, size_t in_length, char* out, size_t out_size, char* from, char* to)
{
	ICONV_CONST char*	in_ptr = (ICONV_CONST char*) in;
	size_t			in_bytesleft = in_length;
	char*			out_ptr = out;
	size_t			out_bytesleft = out_size - 1;
	ICONV_T			cd = (ICONV_T) -1;
	ssize_t			res;

	cd = iconv_open (to, from);
	if (cd == (ICONV_T) -1)
	{
		return -2;
	}

	res = iconv (cd, &in_ptr, &in_bytesleft, &out_ptr, &out_bytesleft);
	if (res < 0)
	{
		iconv_close (cd);
		return -3;
	}

	iconv_close (cd);

	*out_ptr = '\0';

	return (out_ptr - out);
}

#/* convert 1 hex digits of PDU to byte, return < 0 on error */
EXPORT_DEF int parse_hexdigit(int hex)
{
	if(hex >= '0' && hex <= '9')
		return hex - '0';
	if(hex >= 'a' && hex <= 'f')
		return hex - 'a' + 10;
	if(hex >= 'A' && hex <= 'F')
		return hex - 'A' + 10;
	return -1;
}

static ssize_t hexstr_to_8bitchars (const char* in, size_t in_length, char* out, size_t out_size)
{
	int d1, d2;

	/* odd number of chars check */
	if (in_length & 0x1)
		return -EINVAL;

	in_length = in_length >> 1;

	if (out_size - 1 < in_length)
	{
		return -ENOMEM;
	}
	out_size = in_length;

	for (; in_length; --in_length)
	{
		d1 = parse_hexdigit(*in++);
		if(d1 < 0)
			return -EINVAL;
		d2 = parse_hexdigit(*in++);
		if(d2 < 0)
			return -EINVAL;
		*out++ = (d1 << 4) | d2;
	}

	*out = 0;

	return out_size;
}

static ssize_t chars8bit_to_hexstr (const char* in, size_t in_length, char* out, size_t out_size)
{
	static const char hex_table[] = "0123456789ABCDEF";
	const unsigned char *in2 = (const unsigned char *)in;	/* for save time of first & 0x0F */

	if (out_size - 1 < in_length * 2)
	{
		return -1;
	}
	out_size = in_length * 2;

	for (; in_length; --in_length, ++in2)
	{
		*out++ = hex_table[*in2 >> 4];
		*out++ = hex_table[*in2 & 0xF];
	}

	*out = 0;

	return out_size;
}
static ssize_t chars16bit_to_hexstr (const uint16_t* in, size_t in_length, char* out, size_t out_size)
{
	return chars8bit_to_hexstr((const char*)in, in_length * 2, out, out_size);
}

static ssize_t hexstr_ucs2_to_utf8 (const char* in, size_t in_length, char* out, size_t out_size)
{
	char	buf[out_size];
	ssize_t	res;

	if (out_size - 1 < in_length / 2)
	{
		return -1;
	}

	res = hexstr_to_8bitchars (in, in_length, buf, out_size);
	if (res < 0)
	{
		return res;
	}

	/* Since UTF-16BE is a superset of UCS-2BE -- using unused code
	 * points from UCS-2 -- we can safely assume that UTF-16BE works
	 * here. */
	res = convert_string (buf, res, out, out_size, "UTF-16BE", "UTF-8");

	return res;
}

static ssize_t utf8_to_hexstr_ucs2 (const char* in, size_t in_length, char* out, size_t out_size)
{
	char	buf[out_size];
	ssize_t	res;

	if (out_size - 1 < in_length * 4)
	{
		return -1;
	}

	/* Since UTF-16BE is a superset of UCS-2BE -- using unused code
	 * points from UCS-2 -- we can safely assume that UTF-16BE works
	 * here. */
	res = convert_string (in, in_length, buf, out_size, "UTF-8", "UTF-16BE");
	if (res < 0)
	{
		return res;
	}

	res = chars8bit_to_hexstr (buf, res, out, out_size);

	return res;
}

static ssize_t char_to_hexstr_7bit_padded(const char* in, size_t in_length, char* out,
		size_t out_size, unsigned out_padding)
{
	size_t i;
	size_t x;
	char buf[4];
	unsigned value = 0;

	/* compute number of bytes we need for the final string, rounded up */
	x = ((out_padding + (7 * in_length) + 7) / 8);
	/* compute number of hex characters we need for the final string */
	x = (2 * x) + 1 /* terminating zero */;

	/* check that the buffer is not too small */
	if (x > out_size)
		return -1;

	for (x = i = 0; i != in_length; i++) {
		value |= (in[i] & 0x7F) << out_padding;
		out_padding += 7;
		if (out_padding < 8)
			continue;
		/* output one byte in hex */
		snprintf (buf, sizeof(buf), "%02X", value & 0xFF);
		memcpy (out + x, buf, 2);
		x = x + 2;
		value >>= 8;
		out_padding -= 8;
	}
	if (out_padding != 0) {
		snprintf (buf, sizeof(buf), "%02X", value & 0xFF);
		memcpy (out + x, buf, 2);
		x = x + 2;
	}
	/* zero terminate final string */
	out[x] = '\0';

	/* return total string length, excluding terminating zero */
	return x;
}
static ssize_t chars16bit_to_hexstr_7bit(const uint16_t* in, size_t in_length, char* out,
		size_t out_size, unsigned out_padding)
{
	size_t i;
	size_t x;
	char buf[4];
	unsigned value = 0;

	/* compute number of bytes we need for the final string, rounded up */
	x = ((out_padding + (7 * in_length) + 7) / 8);
	/* compute number of hex characters we need for the final string */
	x = (2 * x) + 1 /* terminating zero */;

	/* check that the buffer is not too small */
	if (x > out_size)
		return -1;

	for (x = i = 0; i != in_length; i++) {
		char c[] = { in[i] >> 8, in[i] & 255 };
// 		printf("%d %d-%d %s\n", in[i], c[0], c[1], LUT_GSM7_LS[0][c[1]]);

		for (int j = c[0] == 0; j < 2; ++j) {
			value |= (c[j] & 0x7F) << out_padding;
			out_padding += 7;
			if (out_padding < 8)
				continue;
			/* output one byte in hex */
			snprintf (buf, sizeof(buf), "%02X", value & 0xFF);
			memcpy (out + x, buf, 2);
			x = x + 2;
			value >>= 8;
			out_padding -= 8;
		}
	}
	if (out_padding != 0) {
		snprintf (buf, sizeof(buf), "%02X", value & 0xFF);
		memcpy (out + x, buf, 2);
		x = x + 2;
	}
	/* zero terminate final string */
	out[x] = '\0';

	/* return total string length, excluding terminating zero */
	return x;
}

static ssize_t hexstr_7bit_to_char_padded(const char* in, size_t in_length, char* out,
		size_t out_size, unsigned in_padding, uint8_t ls, uint8_t ss)
{
	if (ls > 13) ls = 0;
	if (ss > 13) ss = 0;
	size_t i;
	size_t x;
	char buf[4];
	unsigned value = 0;
	unsigned hexval;

	/* compute number of bytes */
	in_length /= 2;

	if (out_size == 0) {
		return -1;
	}

	/* check if string is empty */
	if (in_length == 0) {
		out[0] = '\0';
		return (0);
	}

#if 0
	/* compute number of characters */
	x = (((in_length * 8) - in_padding) / 7) + 1 /* terminating zero */;
#endif
	out_size -= 1; /* reserve room for terminating zero */

	/* account for the bit padding */
	in_padding = 7 - in_padding;

	/* clear temporary buffer */
	memset(buf, 0, sizeof(buf));

	/* parse the hexstring */
	int esc = 0;
	for (x = i = 0; i != in_length; i++) {
		if (x >= out_size)
			return -1;
		memcpy (buf, in + i * 2, 2);
		if (sscanf (buf, "%x", &hexval) != 1)
			return -1;
		value |= (hexval & 0xFF) << in_padding;
		in_padding += 8;

		while (in_padding >= (2 * 7)) {
			in_padding -= 7;
			value >>= 7;
			{
				const char *val = (esc ? LUT_GSM7_SS : LUT_GSM7_LS)[esc ? ss : ls][value & 0x7F];
				if (val[0] == '\x1b' && !val[1]) {
					esc = 1;
				} else {
					esc = 0;
					do {
						out[x++] = *val++;
					} while (*val && x < out_size);
				}
			}
		}
	}

	/* zero terminate final string */
	out[x] = '\0';

	/* return total string length, excluding terminating zero */
	return x;
}

#/* */
ssize_t just_copy (const char* in, size_t in_length, char* out, size_t out_size)
{
	// FIXME: or copy out_size-1 bytes only ?
	if (in_length <= out_size - 1)
	{
		memcpy(out, in, in_length);
		out[in_length] = 0;
		return in_length;
	}
	return -ENOMEM;
}

#/* */
EXPORT_DEF ssize_t str_encode(str_encoding_t encoding, const char* in, size_t in_length, char* out, size_t out_size)
{
	if (encoding >= STR_ENCODING_GSM7_HEX_PAD_0 && encoding <= STR_ENCODING_GSM7_HEX_PAD_6) {
		return char_to_hexstr_7bit_padded(in, in_length, out, out_size, encoding - STR_ENCODING_GSM7_HEX_PAD_0);
	}
	switch (encoding) {
	case STR_ENCODING_ASCII:
		return just_copy(in, in_length, out, out_size);
	case STR_ENCODING_UCS2_HEX:
		return utf8_to_hexstr_ucs2(in, in_length, out, out_size);
	default:
		return -EINVAL;
	}
}

#/* */
EXPORT_DEF ssize_t str_encode16(str_encoding_t encoding, const uint16_t* in, size_t in_length, char* out, size_t out_size)
{
	if (encoding == STR_ENCODING_UCS2_HEX) {
		return chars16bit_to_hexstr(in, in_length, out, out_size);
	} else if (encoding >= STR_ENCODING_GSM7_HEX_PAD_0 && encoding <= STR_ENCODING_GSM7_HEX_PAD_6) {
		return chars16bit_to_hexstr_7bit(in, in_length, out, out_size, encoding - STR_ENCODING_GSM7_HEX_PAD_0);
	}
	return -EINVAL;
}

EXPORT_DEF ssize_t str_decode(str_encoding_t encoding, const char* in, size_t in_length, char* out, size_t out_size, uint8_t ls, uint8_t ss)
{
	if (encoding >= STR_ENCODING_GSM7_HEX_PAD_0 && encoding <= STR_ENCODING_GSM7_HEX_PAD_6) {
		return hexstr_7bit_to_char_padded(in, in_length, out, out_size, encoding - STR_ENCODING_GSM7_HEX_PAD_0, ls, ss);
	}
	switch (encoding) {
	case STR_ENCODING_ASCII:
		return just_copy(in, in_length, out, out_size);
	case STR_ENCODING_8BIT_HEX:
		return hexstr_to_8bitchars(in, in_length, out, out_size);
	case STR_ENCODING_UCS2_HEX:
		return hexstr_ucs2_to_utf8(in, in_length, out, out_size);
	default:
		return -EINVAL;
	}
}

#/* */
EXPORT_DEF const uint8_t *get_char_gsm7_encoding(uint16_t c)
{
	int minor = c >> 8, major = c & 255;
	int subtab = LUT_GSM7_REV1[major];
	if (subtab == -1) return LUT_GSM7_REV2_INV;
	return LUT_GSM7_REV2[subtab][minor];
}
EXPORT_DEF str_encoding_t get_encoding(const char* in, size_t length)
{
	size_t x;
	for(x = 0; x < length; ++x)
	{
		if(parse_hexdigit(in[x]) < 0) {
			return STR_ENCODING_ASCII;
		}
	}
	// TODO: STR_ENCODING_GSM7_HEX_PAD_X or STR_ENCODING_8BIT_HEX or STR_ENCODING_UCS2_HEX

	return STR_ENCODING_UNKNOWN;
}
