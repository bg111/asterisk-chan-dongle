/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DONGLE_PDU_H_INCLUDED
#define CHAN_DONGLE_PDU_H_INCLUDED

#include <sys/types.h>			/* size_t */
#include "export.h"			/* EXPORT_DECL EXPORT_DEF */
#include "char_conv.h"			/* str_encoding_t */

typedef struct pdu_udh {
	uint16_t ref;
	uint8_t parts, order;
	uint8_t ls, ss;
} pdu_udh_t;

EXPORT_DECL void pdu_udh_init(pdu_udh_t *udh);
EXPORT_DECL char pdu_digit2code(char digit);
EXPORT_DECL const char * pdu_parse(char ** pdu, size_t tpdu_length, char * oa, size_t oa_len, str_encoding_t * oa_enc, char ** msg, str_encoding_t * msg_enc, pdu_udh_t *udh);
EXPORT_DECL int pdu_parse_sca(char ** pdu, size_t * length);
EXPORT_DECL int pdu_build_mult(int (*cb)(const char *buf, unsigned len, void *s), const char* sca, const char* dst, const char* msg, unsigned valid_minutes, int srr, unsigned csmsref, void *s);
EXPORT_DECL int pdu_build16(char* buffer, size_t length, const char* sca, const char* dst, int dcs, const uint16_t* msg, unsigned msg_len, unsigned msg_bytes, unsigned valid_minutes, int srr, const pdu_udh_t *udh);

#endif /* CHAN_DONGLE_PDU_H_INCLUDED */
