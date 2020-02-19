/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DONGLE_CHAR_CONV_H_INCLUDED
#define CHAN_DONGLE_CHAR_CONV_H_INCLUDED

#include <sys/types.h>			/* ssize_t size_t */
#include "export.h"			/* EXPORT_DECL EXPORT_DEF */
#include <stdint.h>

/* encoding types of strings to/from device */
/* for simplefy first 3 values same as in PDU DCS bits 3..2 */
/* NOTE: order is magic see definition of recoders in char_conv.c */
typedef enum {
	STR_ENCODING_GSM7_HEX_PAD_0 = 0,	/* 7bit encoding, 0 bits of padding */
	STR_ENCODING_GSM7_HEX_PAD_1,		/* 7bit encoding, 1 bit of padding */
	STR_ENCODING_GSM7_HEX_PAD_2,		/* 7bit encoding, 2 bits of padding */
	STR_ENCODING_GSM7_HEX_PAD_3,		/* 7bit encoding, 3 bits padding */
	STR_ENCODING_GSM7_HEX_PAD_4,		/* 7bit encoding, 4 bits padding */
	STR_ENCODING_GSM7_HEX_PAD_5,		/* 7bit encoding, 5 bits padding */
	STR_ENCODING_GSM7_HEX_PAD_6,		/* 7bit encoding, 6 bits padding */
	STR_ENCODING_8BIT_HEX,			/* 8bit encoding */
	STR_ENCODING_UCS2_HEX,			/* UCS-2 in hex like PDU */
/* TODO: check its really 7bit input from device */
	STR_ENCODING_ASCII,			/* 7bit ASCII  no need recode to utf-8 */
//	STR_ENCODING_8BIT,			/* 8bit */
//	STR_ENCODING_UCS2,			/* UCS2 */
	STR_ENCODING_UNKNOWN,			/* still unknown */
} str_encoding_t;

EXPORT_DECL ssize_t convert_string (const char* in, size_t in_length, char* out, size_t out_size, char* from, char* to);

/* recode in both directions */
EXPORT_DECL ssize_t str_encode(str_encoding_t encoding, const char* in, size_t in_length, char* out, size_t out_size);
EXPORT_DECL ssize_t str_encode16(str_encoding_t encoding, const uint16_t* in, size_t in_length, char* out, size_t out_size);
EXPORT_DECL ssize_t str_decode(str_encoding_t encoding, const char* in, size_t in_length, char* out, size_t out_size, uint8_t ls, uint8_t ss);

EXPORT_DECL int parse_hexdigit(int hex);
EXPORT_DECL str_encoding_t get_encoding(const char * in, size_t in_length);

EXPORT_DECL const uint8_t *get_char_gsm7_encoding(uint16_t c);

#endif /* CHAN_DONGLE_CHAR_CONV_H_INCLUDED */
