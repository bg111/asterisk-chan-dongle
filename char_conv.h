/*
   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/
#ifndef CHAN_DONGLE_CHAR_CONV_H_INCLUDED
#define CHAN_DONGLE_CHAR_CONV_H_INCLUDED

#include <sys/types.h>			/* ssize_t size_t */
#include "export.h"			/* EXPORT_DECL EXPORT_DEF */
#include <stdint.h>

EXPORT_DECL ssize_t utf8_to_ucs2(const char *in, size_t in_length, uint16_t *out, size_t out_size);
EXPORT_DECL ssize_t ucs2_to_utf8(const uint16_t *in, size_t in_length, char *out, size_t out_size);
EXPORT_DECL int unhex(const char *in, uint8_t *out);
EXPORT_DECL void hexify(const uint8_t *in, size_t in_length, char *out);
EXPORT_DECL ssize_t gsm7_encode(const uint16_t *in, size_t in_length, uint16_t *out);
EXPORT_DECL ssize_t gsm7_pack(const uint16_t *in, size_t in_length, char *out, size_t out_size, unsigned out_padding);
EXPORT_DECL ssize_t gsm7_unpack_decode(const char *in, size_t in_length, uint16_t *out, size_t out_size, unsigned in_padding, uint8_t ls, uint8_t ss);

#endif /* CHAN_DONGLE_CHAR_CONV_H_INCLUDED */
