/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DONGLE_PDU_H_INCLUDED
#define CHAN_DONGLE_PDU_H_INCLUDED

#include <sys/types.h>			/* size_t */
#include "export.h"			/* EXPORT_DECL EXPORT_DEF */
#include "char_conv.h"			/* str_encoding_t */

/* Message Type Indicator Parameter */
#define PDUTYPE_MTI_SHIFT			0
#define PDUTYPE_MTI_SMS_DELIVER			(0x00 << PDUTYPE_MTI_SHIFT)
#define PDUTYPE_MTI_SMS_DELIVER_REPORT		(0x00 << PDUTYPE_MTI_SHIFT)
#define PDUTYPE_MTI_SMS_SUBMIT			(0x01 << PDUTYPE_MTI_SHIFT)
#define PDUTYPE_MTI_SMS_SUBMIT_REPORT		(0x01 << PDUTYPE_MTI_SHIFT)
#define PDUTYPE_MTI_SMS_STATUS_REPORT		(0x02 << PDUTYPE_MTI_SHIFT)
#define PDUTYPE_MTI_SMS_COMMAND			(0x02 << PDUTYPE_MTI_SHIFT)
#define PDUTYPE_MTI_RESERVED			(0x03 << PDUTYPE_MTI_SHIFT)

#define PDUTYPE_MTI_MASK			(0x03 << PDUTYPE_MTI_SHIFT)
#define PDUTYPE_MTI(pdutype)			((pdutype) & PDUTYPE_MTI_MASK)

#define TPDU_LENGTH 176
#define PDU_LENGTH 256

typedef struct pdu_udh
{
	uint8_t ref;
	uint8_t parts, order;
	uint8_t ls, ss;
} pdu_udh_t;

typedef struct pdu_part
{
	uint8_t buffer[PDU_LENGTH];
	size_t tpdu_length, length;
} pdu_part_t;

EXPORT_DECL void pdu_udh_init(pdu_udh_t *udh);
EXPORT_DECL int pdu_build_mult(pdu_part_t *pdus, const char* sca, const char* dst, const uint16_t* msg, size_t msg_len, unsigned valid_minutes, int srr, uint8_t csmsref);
EXPORT_DECL ssize_t pdu_build(uint8_t* buffer, size_t length, size_t *tpdulen, const char* sca, const char* dst, int dcs, const uint16_t* msg, unsigned msg_len, unsigned msg_bytes, unsigned valid_minutes, int srr, const pdu_udh_t *udh);
EXPORT_DECL int pdu_parse_sca(uint8_t *pdu, size_t pdu_length, char *sca, size_t sca_len);
EXPORT_DECL int tpdu_parse_type(uint8_t *pdu, size_t pdu_length, int *type);
EXPORT_DECL int tpdu_parse_status_report(uint8_t *pdu, size_t pdu_length, int *mr, char *ra, size_t ra_len, char *scts, char *dt, int *st);
EXPORT_DECL int tpdu_parse_deliver(uint8_t *pdu, size_t pdu_length, int tpdu_type, char *oa, size_t oa_len, char *scts, uint16_t *msg, pdu_udh_t *udh);

#endif /* CHAN_DONGLE_PDU_H_INCLUDED */
