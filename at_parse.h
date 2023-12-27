/*
    Copyright (C) 2010 bg <bg_one@mail.ru>
    Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/
#ifndef CHAN_DONGLE_AT_PARSE_H_INCLUDED
#define CHAN_DONGLE_AT_PARSE_H_INCLUDED

#include <sys/types.h>			/* size_t */

#include "export.h"		/* EXPORT_DECL EXPORT_DECL */
#include "char_conv.h"		/* str_encoding_t */
#include "pdu.h"
struct pvt;

EXPORT_DECL char* at_parse_cnum (char* str);
EXPORT_DECL char* at_parse_cops (char* str);
EXPORT_DECL int at_parse_creg (char* str, unsigned len, int* gsm_reg, int* gsm_reg_status, char** lac, char** ci);
EXPORT_DECL int at_parse_cmti (const char* str);
EXPORT_DECL int at_parse_cdsi (const char* str);
EXPORT_DECL int at_parse_cmgr(char *str, size_t len, int *tpdu_type, char *sca, size_t sca_len, char *oa, size_t oa_len, char *scts, int *mr, int *st, char *dt, char *msg, size_t *msg_len, pdu_udh_t *udh);
EXPORT_DECL int at_parse_cmgs (const char* str);
EXPORT_DECL int at_parse_cusd (char* str, int * type, char ** cusd, int * dcs);
EXPORT_DECL int at_parse_cpin (char* str, size_t len);
EXPORT_DECL int at_parse_csq (const char* str, int* rssi);
EXPORT_DECL int at_parse_rssi (const char* str);
EXPORT_DECL int at_parse_mode (char* str, int * mode, int * submode);
EXPORT_DECL int at_parse_csca (char* str, char ** csca);
EXPORT_DECL int at_parse_clcc (char* str, unsigned * call_idx, unsigned * dir, unsigned * state, unsigned * mode, unsigned * mpty, char ** number, unsigned * toa);
EXPORT_DECL int at_parse_ccwa(char* str, unsigned * class);



#endif /* CHAN_DONGLE_AT_PARSE_H_INCLUDED */
