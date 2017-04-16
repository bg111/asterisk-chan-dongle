/*
    Copyright (C) 2009 - 2010
    Artem Makhutov <artem@makhutov.org>
    http://www.makhutov.org

    Dmitry Vagin <dmitry2004@yandex.ru>

    Copyright (C) 2010 - 2011
    bg <bg_one@mail.ru>
*/
#include "ast_config.h"

#include <sys/types.h>

#include <iconv.h>			/* iconv_t iconv() */
#include <string.h>			/* memcpy() */
#include <stdio.h>			/* sscanf() snprintf() */
#include <errno.h>			/* EINVAL */

#include "char_conv.h"
#include "mutils.h"			/* ITEMS_OF() */

static ssize_t convert_string (const char* in, size_t in_length, char* out, size_t out_size, char* from, char* to)
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

static ssize_t char_to_hexstr_7bit_pad_0(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return char_to_hexstr_7bit_padded(in, in_length, out, out_size, 0);
}

static ssize_t char_to_hexstr_7bit_pad_1(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return char_to_hexstr_7bit_padded(in, in_length, out, out_size, 1);
}

static ssize_t char_to_hexstr_7bit_pad_2(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return char_to_hexstr_7bit_padded(in, in_length, out, out_size, 2);
}

static ssize_t char_to_hexstr_7bit_pad_3(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return char_to_hexstr_7bit_padded(in, in_length, out, out_size, 3);
}

static ssize_t char_to_hexstr_7bit_pad_4(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return char_to_hexstr_7bit_padded(in, in_length, out, out_size, 4);
}

static ssize_t char_to_hexstr_7bit_pad_5(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return char_to_hexstr_7bit_padded(in, in_length, out, out_size, 5);
}

static ssize_t char_to_hexstr_7bit_pad_6(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return char_to_hexstr_7bit_padded(in, in_length, out, out_size, 6);
}

/* GSM 03.38 7bit alphabet */
static const char *const alphabet_7bit[128] = {
	"@", "£", "$", "¥", "è", "é", "ù", "ì", "ò", "Ç", "\n", "Ø",
	"ø", "\r", "Å", "å", "∆", "_", "Φ", "Γ", "Λ", "Ω", "Π", "Ψ",
	"Σ", "Θ", "Ξ", "\x1b" /* ESC */, "Æ", "æ", "ß", "É", " ", "!",
	"\"", "#", "¤", "%", "&", "'", "(", ")", "*", "+", ",", "-",
	".", "/", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	":", ";", "<", "=", ">", "?", "¡", "A", "B", "C", "D", "E",
	"F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q",
	"R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Ä", "Ö", "Ñ",
	"Ü", "§", "¿", "a", "b", "c", "d", "e", "f", "g", "h", "i",
	"j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u",
	"v", "w", "x", "y", "z", "ä", "ö", "ñ", "ü", "à"
	/* TODO: ESC could unlock the basic charset extension,
	 * interpeting the following char differently. */
};

static ssize_t hexstr_7bit_to_char_padded(const char* in, size_t in_length, char* out,
		size_t out_size, unsigned in_padding)
{
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
				const char *val = alphabet_7bit[value & 0x7F];
				do {
					out[x++] = *val++;
				} while (*val && x < out_size);
			}
		}
	}

	/* zero terminate final string */
	out[x] = '\0';

	/* return total string length, excluding terminating zero */
	return x;
}

static ssize_t hexstr_7bit_to_char_pad_0(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return hexstr_7bit_to_char_padded(in, in_length, out, out_size, 0);
}

static ssize_t hexstr_7bit_to_char_pad_1(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return hexstr_7bit_to_char_padded(in, in_length, out, out_size, 1);
}

static ssize_t hexstr_7bit_to_char_pad_2(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return hexstr_7bit_to_char_padded(in, in_length, out, out_size, 2);
}

static ssize_t hexstr_7bit_to_char_pad_3(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return hexstr_7bit_to_char_padded(in, in_length, out, out_size, 3);
}

static ssize_t hexstr_7bit_to_char_pad_4(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return hexstr_7bit_to_char_padded(in, in_length, out, out_size, 4);
}

static ssize_t hexstr_7bit_to_char_pad_5(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return hexstr_7bit_to_char_padded(in, in_length, out, out_size, 5);
}

static ssize_t hexstr_7bit_to_char_pad_6(const char* in, size_t in_length, char* out,
		size_t out_size)
{
	return hexstr_7bit_to_char_padded(in, in_length, out, out_size, 6);
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

typedef ssize_t (*coder) (const char* in, size_t in_length, char* out, size_t out_size);

/* array in order of values RECODE_* */
static const coder recoders[STR_ENCODING_UNKNOWN][2] =
{
	[STR_ENCODING_7BIT_HEX_PAD_0] = { hexstr_7bit_to_char_pad_0, char_to_hexstr_7bit_pad_0 },
	[STR_ENCODING_8BIT_HEX] = { hexstr_to_8bitchars, chars8bit_to_hexstr },
	[STR_ENCODING_UCS2_HEX] = { hexstr_ucs2_to_utf8, utf8_to_hexstr_ucs2 },
	[STR_ENCODING_7BIT] = { just_copy, just_copy },
	[STR_ENCODING_7BIT_HEX_PAD_1] = { hexstr_7bit_to_char_pad_1, char_to_hexstr_7bit_pad_1 },
	[STR_ENCODING_7BIT_HEX_PAD_2] = { hexstr_7bit_to_char_pad_2, char_to_hexstr_7bit_pad_2 },
	[STR_ENCODING_7BIT_HEX_PAD_3] = { hexstr_7bit_to_char_pad_3, char_to_hexstr_7bit_pad_3 },
	[STR_ENCODING_7BIT_HEX_PAD_4] = { hexstr_7bit_to_char_pad_4, char_to_hexstr_7bit_pad_4 },
	[STR_ENCODING_7BIT_HEX_PAD_5] = { hexstr_7bit_to_char_pad_5, char_to_hexstr_7bit_pad_5 },
	[STR_ENCODING_7BIT_HEX_PAD_6] = { hexstr_7bit_to_char_pad_6, char_to_hexstr_7bit_pad_6 },
};

#/* */
EXPORT_DEF ssize_t str_recode(recode_direction_t dir, str_encoding_t encoding, const char* in, size_t in_length, char* out, size_t out_size)
{
	unsigned idx = encoding;
	if((dir == RECODE_DECODE || dir == RECODE_ENCODE) && idx < ITEMS_OF(recoders))
		return (recoders[idx][dir])(in, in_length, out, out_size);
	return -EINVAL;
}

#/* */
EXPORT_DEF str_encoding_t get_encoding(recode_direction_t hint, const char* in, size_t length)
{
	if(hint == RECODE_ENCODE)
	{
		for(; length; --length, ++in)
			if(*in & 0x80)
				return STR_ENCODING_UCS2_HEX;
		return STR_ENCODING_7BIT_HEX_PAD_0;
	}
	else
	{
		size_t x;
		for(x = 0; x < length; ++x)
		{
			if(parse_hexdigit(in[x]) < 0) {
				return STR_ENCODING_7BIT;
			}
		}
		// TODO: STR_ENCODING_7BIT_HEX_PAD_X or STR_ENCODING_8BIT_HEX or STR_ENCODING_UCS2_HEX
	}

	return STR_ENCODING_UNKNOWN;
}
