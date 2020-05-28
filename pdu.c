/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/
#include "ast_config.h"

#include <errno.h>			/* EINVAL ENOMEM E2BIG */

#include "pdu.h"
#include "helpers.h"			/* dial_digit_code() */
#include "char_conv.h"			/* utf8_to_hexstr_ucs2() */
#include "gsm7_luts.h"
#include "error.h"

/* SMS-SUBMIT format
	SCA		1..12 octet(s)		Service Center Address information element
	  octets
	        1		Length of Address (minimal 0)
	        2		Type of Address
	    3  12		Address

	PDU-type	1 octet			Protocol Data Unit Type
	  bits
	      1 0 MTI	Message Type Indicator Parameter describing the message type 00 means SMS-DELIVER 01 means SMS-SUBMIT
				0 0	SMS-DELIVER (SMSC ==> MS)
						or
					SMS-DELIVER REPORT (MS ==> SMSC, is generated automatically by the MOBILE, after receiving a SMS-DELIVER)

				0 1	SMS-SUBMIT (MS ==> SMSC)
						or
					SMS-SUBMIT REPORT (SMSC ==> MS)

				1 0	SMS-STATUS REPORT (SMSC ==> MS)
						or
					SMS-COMMAND (MS ==> SMSC)
				1 1	Reserved

		2 RD	Reject Duplicate
				0    Instruct the SMSC to accept an SMS-SUBMIT for an short message still
					held in the SMSC which has the same MR and
				1    Instruct the SMSC to reject an SMS-SUBMIT for an short message still
					held in the SMSC which has the same MR and DA as a previosly
					submitted short message from the same OA.

	      4 3 VPF	Validity Period Format	Parameter indicating whether or not the VP field is present
	      			0 0	VP field is not present
	      			0 1	Reserved
	      			1 0	VP field present an integer represented (relative)
	      			1 1	VP field present an semi-octet represented (absolute)

		5 SRR	Status Report Request Parameter indicating if the MS has requested a status report
				0	A status report is not requested
				1	A status report is requested

		6 UDHI	User Data Header Indicator Parameter indicating that the UD field contains a header
				0	The UD field contains only the short message
				1	The beginning of the UD field contains a header in addition of the short message
		7 RP	Reply Path Parameter indicating that Reply Path exists
				0	Reply Path parameter is not set in this PDU
				1	Reply Path parameter is set in this PDU

	MR		1 octet		Message Reference
						The MR field gives an integer (0..255) representation of a reference number of the SMSSUBMIT submitted to the SMSC by the MS.
						! notice: at the MOBILE the MR is generated automatically, -anyway you have to generate it a possible entry is for example ”00H” !
	DA		2-12 octets	Destination Address
	  octets
		1	Length of Address (of BCD digits!)
		2	Type of Address
	     3 12	Address
	PID		1 octet		Protocol Identifier
						The PID is the information element by which the Transport Layer either refers to the higher
						layer protocol being used, or indicates interworking with a certain type of telematic device.
						here are some examples of PID codings:
		00H: The PDU has to be treat as a short message
		41H: Replace Short Message Type1
		  ....
		47H: Replace Short Message Type7
					Another description:

		Bit7 bit6 (bit 7 = 0, bit 6 = 0)
		l 0 0 Assign bits 0..5, the values are defined as follows.
		l 1 0 Assign bits 0..5, the values are defined as follows.
		l 0 1 Retain
		l 1 1 Assign bits 0..5 for special use of SC
		Bit5 values:
		l 0: No interworking, but SME-to-SME protocol
		l 1: Telematic interworking (in this situation , value of bits4...0 is
		valid)
		Interface Description for HUAWEI EV-DO Data Card AT Commands
		All rights reserved Page 73 , Total 140
		Bit4...Bit0: telematic devices type identifier. If the value is 1 0 0 1 0, it
		indicates email. Other values are not supported currently.


	DCS		1 octet			Data Coding Scheme

	VP		0,1,7 octet(s)		Validity Period
	UDL		1 octet			User Data Length
	UD		0-140 octets		User Data
*/

/* SMS-DELIVER format
	SCA		1..12 octet(s)		Service Center Address information element
	  octets
	        1		Length of Address (minimal 0)
	        2		Type of Address
	    3  12		Address

	PDU-type	1 octet			Protocol Data Unit Type
	  bits
	      1 0 MTI	Message Type Indicator Parameter describing the message type 00 means SMS-DELIVER 01 means SMS-SUBMIT
				0 0	SMS-DELIVER (SMSC ==> MS)
						or
					SMS-DELIVER REPORT (MS ==> SMSC, is generated automatically by the MOBILE, after receiving a SMS-DELIVER)

				0 1	SMS-SUBMIT (MS ==> SMSC)
						or
					SMS-SUBMIT REPORT (SMSC ==> MS)

				1 0	SMS-STATUS REPORT (SMSC ==> MS)
						or
					SMS-COMMAND (MS ==> SMSC)
				1 1	Reserved

		2 MMS	More Messages to Send	Parameter indicating whether or not there are more messages to send
				0 More messages are waiting for the MS in the SMSC
				1 No more messages are waiting for the MS in the SMSC
	      4 3 Reserved

		5 SRI	Status Report Indication	Parameter indicating if the SME has requested a status report
				0 A status report will not be returned to the SME
				1 A status report will be returned to the SME

		6 UDHI	User Data Header Indicator Parameter indicating that the UD field contains a header
				0	The UD field contains only the short message
				1	The beginning of the UD field contains a header in addition of the short message
		7 RP	Reply Path Parameter indicating that Reply Path exists
				0	Reply Path parameter is not set in this PDU
				1	Reply Path parameter is set in this PDU

	OA		2-12 octets	Originator Address
	  octets
		1	Length of Address (of BCD digits!)
		2	Type of Address
	     3 12	Address
	PID		1 octet		Protocol Identifier
						The PID is the information element by which the Transport Layer either refers to the higher
						layer protocol being used, or indicates interworking with a certain type of telematic device.
						here are some examples of PID codings:
		00H: The PDU has to be treat as a short message
		41H: Replace Short Message Type1
		  ....
		47H: Replace Short Message Type7
					Another description:

		Bit7 bit6 (bit 7 = 0, bit 6 = 0)
		l 0 0 Assign bits 0..5, the values are defined as follows.
		l 1 0 Assign bits 0..5, the values are defined as follows.
		l 0 1 Retain
		l 1 1 Assign bits 0..5 for special use of SC
		Bit5 values:
		l 0: No interworking, but SME-to-SME protocol
		l 1: Telematic interworking (in this situation , value of bits4...0 is
		valid)
		Interface Description for HUAWEI EV-DO Data Card AT Commands
		All rights reserved Page 73 , Total 140
		Bit4...Bit0: telematic devices type identifier. If the value is 1 0 0 1 0, it
		indicates email. Other values are not supported currently.


	DCS		1 octet			Data Coding Scheme

	SCTS		7 octets		Service Center Time Stamp
	UDL		1 octet			User Data Length
	UD		0-140 octets		User Data, may be prepended by User Data Header see UDHI flag
	    octets
	    	1 opt UDHL	Total number of Octets in UDH
	    	? IEIa
	    	? IEIDLa
	    	? IEIDa
	    	? IEIb
		  ...
*/

/* Address octets: 0=length_in_nibbles, 1=EXT/TON/NPI, 2..11=address
 * (destination address (TP-DA), originator address (TP-OA) and recipient address (TP-RA))
 * EXT: bit7: 1 "no extension"
 * TON: bit6..4: see below
 * NPI: bit3..0: see below
 * Source: https://en.wikipedia.org/wiki/GSM_03.40 */
#define TP_A_EXT		(1 << 7)
#define TP_A_EXT_NOEXT		(1 << 7)
#define TP_A_TON		(7 << 4)
#define TP_A_TON_UNKNOWN	(0 << 4)
#define TP_A_TON_INTERNATIONAL	(1 << 4)
#define TP_A_TON_NATIONAL	(2 << 4)
#define TP_A_TON_NETSPECIFIC	(3 << 4)
#define TP_A_TON_SUBSCRIBERNUM	(4 << 4)
#define TP_A_TON_ALPHANUMERIC	(5 << 4)
#define TP_A_TON_ABBREVIATEDNUM	(6 << 4)
#define TP_A_TON_RESERVED	(7 << 4)
#define TP_A_NPI		(15 << 0)
#define TP_A_NPI_UNKNOWN	(0 << 0)
#define TP_A_NPI_TEL_E164_E163	(1 << 0)
#define TP_A_NPI_TELEX		(3 << 0)
#define TP_A_NPI_SVCCENTR_SPEC1	(4 << 0)
#define TP_A_NPI_SVCCENTR_SPEC2	(5 << 0)
#define TP_A_NPI_NATIONALNUM	(8 << 0)
#define TP_A_NPI_PRIVATENUM	(9 << 0)
#define TP_A_NPI_ERMESNUM	(10 << 0)
#define TP_A_NPI_RESERVED	(15 << 0)
#define NUMBER_TYPE_INTERNATIONAL	(TP_A_EXT_NOEXT | TP_A_TON_INTERNATIONAL | TP_A_NPI_TEL_E164_E163) /* 0x91 */
#define NUMBER_TYPE_NATIONAL		(TP_A_EXT_NOEXT | TP_A_TON_SUBSCRIBERNUM | TP_A_NPI_NATIONALNUM) /* 0xC8 */
#define NUMBER_TYPE_ALPHANUMERIC	(TP_A_EXT_NOEXT | TP_A_TON_ALPHANUMERIC | TP_A_NPI_UNKNOWN) /* 0xD0 */
/* maybe NUMBER_TYPE_NETWORKSHORT should be 0xB1 ??? */
#define NUMBER_TYPE_NETWORKSHORT	(TP_A_EXT_NOEXT | TP_A_TON_NETSPECIFIC | TP_A_NPI_PRIVATENUM) /* 0xB9 */
#define NUMBER_TYPE_UNKNOWN		(TP_A_EXT_NOEXT | TP_A_TON_UNKNOWN | TP_A_NPI_TEL_E164_E163) /* 0x81 */

/* Reject Duplicate */
#define PDUTYPE_RD_SHIFT			2
#define PDUTYPE_RD_ACCEPT			(0x00 << PDUTYPE_RD_SHIFT)
#define PDUTYPE_RD_REJECT			(0x01 << PDUTYPE_RD_SHIFT)

/* Validity Period Format */
#define PDUTYPE_VPF_SHIFT			3
#define PDUTYPE_VPF_NOT_PRESENT			(0x00 << PDUTYPE_VPF_SHIFT)
#define PDUTYPE_VPF_RESERVED			(0x01 << PDUTYPE_VPF_SHIFT)
#define PDUTYPE_VPF_RELATIVE			(0x02 << PDUTYPE_VPF_SHIFT)
#define PDUTYPE_VPF_ABSOLUTE			(0x03 << PDUTYPE_VPF_SHIFT)

/* Status Report Request */
#define PDUTYPE_SRR_SHIFT			5
#define PDUTYPE_SRR_NOT_REQUESTED		(0x00 << PDUTYPE_SRR_SHIFT)
#define PDUTYPE_SRR_REQUESTED			(0x01 << PDUTYPE_SRR_SHIFT)

/* User Data Header Indicator */
#define PDUTYPE_UDHI_SHIFT			6
#define PDUTYPE_UDHI_NO_HEADER			(0x00 << PDUTYPE_UDHI_SHIFT)
#define PDUTYPE_UDHI_HAS_HEADER			(0x01 << PDUTYPE_UDHI_SHIFT)
#define PDUTYPE_UDHI_MASK			(0x01 << PDUTYPE_UDHI_SHIFT)
#define PDUTYPE_UDHI(pdutype)			((pdutype) & PDUTYPE_UDHI_MASK)

/* eply Path Parameter */
#define PDUTYPE_RP_SHIFT			7
#define PDUTYPE_RP_IS_NOT_SET			(0x00 << PDUTYPE_RP_SHIFT)
#define PDUTYPE_RP_IS_SET			(0x01 << PDUTYPE_RP_SHIFT)

#define PDU_MESSAGE_REFERENCE			0x00		/* assigned by MS */

#define PDU_PID_SMS				0x00		/* bit5 No interworking, but SME-to-SME protocol = SMS */
#define PDU_PID_EMAIL				0x32		/* bit5 Telematic interworking, bits 4..0 0x 12  = email */
#define PDU_PID_SMS_REPLACE_MASK		0x40		/* bit7 Replace Short Message function activated (TP-PID = 0x41 to 0x47) */

/*   bits 3..2  */
#define PDU_DCS_ALPHABET_SHIFT			2
#define PDU_DCS_ALPHABET_7BIT			(0x00 << PDU_DCS_ALPHABET_SHIFT)
#define PDU_DCS_ALPHABET_8BIT			(0x01 << PDU_DCS_ALPHABET_SHIFT)
#define PDU_DCS_ALPHABET_UCS2			(0x02 << PDU_DCS_ALPHABET_SHIFT)
#define PDU_DCS_ALPHABET_MASK			(0x03 << PDU_DCS_ALPHABET_SHIFT)
#define PDU_DCS_ALPHABET(dcs)			((dcs) & PDU_DCS_ALPHABET_MASK)

#define ROUND_UP2(x)				(((x) + 1) & (0xFFFFFFFF << 1))
#define DIV2UP(x)			(((x) + 1)/2)

#define CSMS_GSM7_MAX_LEN 153
#define SMS_GSM7_MAX_LEN 160
#define CSMS_UCS2_MAX_LEN 67
#define SMS_UCS2_MAX_LEN 70

EXPORT_DEF void pdu_udh_init(pdu_udh_t *udh)
{
	udh->ref = 0;
	udh->parts = 1;
	udh->order = 0;
	udh->ss = 0;
	udh->ls = 0;
}

#/* get digit code, 0 if invalid  */
static uint8_t pdu_digit2code(char digit)
{
	switch (digit) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return digit - '0';
	case '*':
		return 0xa;
	case '#':
		return 0xb;
	case 'a':
	case 'A':
		return 0xc;
		break;
	case 'b':
	case 'B':
		return 0xd;
	case 'c':
	case 'C':
		return 0xe;
	default:
		return 255;
	}
}

#/* */
static char pdu_code2digit(uint8_t code)
{
	switch (code) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
		return code + '0';
	case 0xa:
		return '*';
	case 0xb:
		return '#';
	case 0xc:
		return 'A';
	case 0xd:
		return 'B';
	case 0xe:
		return 'C';
	default:
		return 0;
	}
}

#/* convert minutes to relative VP value */
static int pdu_relative_validity(unsigned minutes)
{
#define DIV_UP(x,y)	(((x)+(y)-1)/(y))
/*
	0 ... 143  (vp + 1) * 5 minutes				   5  ...   720		m = (vp + 1) * 5		m / 5 - 1 = vp
	144...167  12 hours + (vp - 143) * 30 minutes		 750  ...  1440		m = 720 + (vp - 143) * 30	(m - 720) / 30 + 143 = m / 30 + 119
	168...196  (vp - 166) * 1 day				2880  ... 43200		m = (vp - 166) * 1440		(m / 1440) + 166
	197...255  (vp - 192) * 1 week			       50400  ...635040		m = (vp - 192) * 10080		(m / 10080) + 192
*/
	int validity;
	if(minutes <= 720)
		validity = DIV_UP(minutes, 5) - 1;
	else if(minutes <= 1440)
		validity = DIV_UP(minutes, 30) + 119;
	else if(minutes <= 43200)
		validity = DIV_UP(minutes, 1440) + 166;
	else if(minutes <= 635040)
		validity = DIV_UP(minutes, 10080) + 192;
	else
		validity = 0xFF;
	return validity;
#undef DIV_UP
}

/*!
 * \brief Store number in PDU
 * \param buffer -- pointer to place where number will be stored, CALLER MUST be provide length + 2 bytes of buffer
 * \param number -- phone number w/o leading '+'
 * \param length -- length of number
 * \return number of bytes written to buffer
 */
static int pdu_store_number(uint8_t* buffer, int toa, const char *number, unsigned length)
{
	int i = 0;
	buffer[i++] = toa;

	int j;
	for (j = 0; j + 1 < length; j += 2) {
		uint8_t a = pdu_digit2code(number[j]);
		uint8_t b = pdu_digit2code(number[j + 1]);
		if (a == 255 || b == 255) {
			return -1;
		}
		buffer[i++] = a | b << 4;
	}

	if (j != length) {
		uint8_t a = pdu_digit2code(number[j]);
		if (a == 255) {
			return -1;
		}
		buffer[i++] = a | 0xf0;
	}
	return i;
}

#/* reverse of pdu_store_number() */
static int pdu_parse_number(uint8_t *pdu, size_t pdu_length, unsigned digits, char *number, size_t num_len)
{
	if (num_len < digits + 2) {
		return -ENOMEM;
	}
	int toa;

	int i = 0, res;
	toa = pdu[i++];
	unsigned syms = ROUND_UP2(digits);
	if (syms > pdu_length - i) {
		return -EINVAL;
	}

	if ((toa & TP_A_TON) == TP_A_TON_ALPHANUMERIC) {
		uint16_t number16tmp[num_len];
		res = gsm7_unpack_decode(pdu + i, syms, number16tmp, num_len, 0, 0, 0);
		if (res < 0) return -EINVAL;
		res = ucs2_to_utf8(number16tmp, res, number, num_len);
		i += syms / 2;
		number += res;
	} else {
		if ((toa & TP_A_TON) == TP_A_TON_INTERNATIONAL) {
			*number++ = '+';
		}
		for (int j = 0; j < syms / 2; ++j) {
			int c = pdu[i];
			*number++ = pdu_code2digit(c & 0xf);
			char o = c >> 4;
			if (o != 0xf) *number++ = pdu_code2digit(o);
			++i;
		}
	}
	*number = '\0';

	return i;
}

#/* */
static int pdu_parse_timestamp(uint8_t *pdu, size_t length, char *out)
{
	int d, m, y, h, i, s, o, os;
	if (length >= 7) {
		y = (10 * (pdu[0] & 15) + (pdu[0] >> 4)) + 2000;
		m = 10 * (pdu[1] & 15) + (pdu[1] >> 4);
		d = 10 * (pdu[2] & 15) + (pdu[2] >> 4);
		h = 10 * (pdu[3] & 15) + (pdu[3] >> 4);
		i = 10 * (pdu[4] & 15) + (pdu[4] >> 4);
		s = 10 * (pdu[5] & 15) + (pdu[5] >> 4);
		o = (pdu[6] >> 4) + 10 * (pdu[6] & 7);
		os = pdu[6] & 0x8;

		sprintf(out, "%02d-%02d-%02d %02d:%02d:%02d %c%02d:%02d", y, m, d, h, i, s, os ? '-' : '+', o / 4, (o % 4) * 15);

		return 7;
	}
	return -1;
}

EXPORT_DEF int pdu_build_mult(pdu_part_t *pdus, const char *sca, const char *dst, const uint16_t* msg, size_t msg_len, unsigned valid_minutes, int srr, uint8_t csmsref)
{
	uint16_t msg_gsm7[msg_len];
	int gsm7_len = gsm7_encode(msg, msg_len, msg_gsm7);

	unsigned split = 0;
	unsigned cnt = 0, i = 0, off = 0;
	if (gsm7_len >= 0) {
		if (gsm7_len > SMS_GSM7_MAX_LEN) {
			split = CSMS_GSM7_MAX_LEN;
		} else {
			split = SMS_GSM7_MAX_LEN;
		}
		while (off < msg_len) {
			unsigned septets = 0, n;
			for (n = 0; off + n < msg_len; ++n) {
				unsigned req = msg_gsm7[off + n] > 255 ? 2 : 1;
				if (septets + req >= split) {
					break;
				}
				septets += req;
			}
			++cnt;
			off += n;
		}
		if (cnt > 255) {
			chan_dongle_err = E_2BIG;
			return -1;
		}
		off = 0;
		while (off < msg_len) {
			unsigned septets = 0, n;
			for (n = 0; off + n < msg_len; ++n) {
				unsigned req = msg_gsm7[off + n] > 255 ? 2 : 1;
				if (septets + req >= split) {
					break;
				}
				septets += req;
			}
			pdu_udh_t udh;
			udh.ref = csmsref;
			udh.order = i + 1;
			udh.parts = cnt;
			ssize_t curlen = pdu_build(pdus[i].buffer, PDU_LENGTH, &pdus[i].tpdu_length, sca, dst, PDU_DCS_ALPHABET_7BIT, msg_gsm7 + off, n, septets, valid_minutes, srr, &udh);
			if (curlen < 0) {
				/* pdu_build sets chan_dongle_err */
				return -1;
			}
			pdus[i].length = curlen;
			off += n;
			++i;
		}
	} else {
		if (msg_len > SMS_UCS2_MAX_LEN) {
			split = CSMS_UCS2_MAX_LEN;
		} else {
			split = SMS_UCS2_MAX_LEN;
		}
		cnt = (msg_len + split - 1) / split;
		if (cnt > 255) {
			chan_dongle_err = E_2BIG;
			return -1;
		}
		while (off < msg_len) {
			unsigned r = msg_len - off;
			unsigned n = r < split ? r : split;
			pdu_udh_t udh;
			udh.ref = csmsref;
			udh.order = i + 1;
			udh.parts = cnt;
			ssize_t curlen = pdu_build(pdus[i].buffer, PDU_LENGTH, &pdus[i].tpdu_length, sca, dst, PDU_DCS_ALPHABET_UCS2, msg + off, n, n * 2, valid_minutes, srr, &udh);
			if (curlen < 0) {
				/* pdu_build sets chan_dongle_err */
				return -1;
			}
			pdus[i].length = curlen;
			off += n;
			++i;
		}
	}

	return i;
}


EXPORT_DEF ssize_t pdu_build(uint8_t *buffer, size_t length, size_t *tpdulen, const char *sca, const char *dst, int dcs, const uint16_t *msg, unsigned msg_reallen, unsigned msg_len, unsigned valid_minutes, int srr, const pdu_udh_t *udh)
{
	int len = 0;

	int sca_toa = NUMBER_TYPE_INTERNATIONAL;
	int dst_toa;
	int pdutype = PDUTYPE_MTI_SMS_SUBMIT | PDUTYPE_RD_ACCEPT | PDUTYPE_VPF_RELATIVE | PDUTYPE_SRR_NOT_REQUESTED | PDUTYPE_UDHI_NO_HEADER | PDUTYPE_RP_IS_NOT_SET;
	int use_udh = udh->parts > 1;
	int res;
	if (use_udh) pdutype |= PDUTYPE_UDHI_HAS_HEADER;

	unsigned dst_len;
	unsigned sca_len;

	if (sca[0] == '+') {
		++sca;
	}

	if (dst[0] == '+') {
		dst_toa = NUMBER_TYPE_INTERNATIONAL;
		++dst;
	} else {
		if (strlen(dst) < 6) {
			dst_toa = NUMBER_TYPE_NETWORKSHORT;
		} else {
			dst_toa = NUMBER_TYPE_UNKNOWN;
		}
	}

	/* count length of strings */
	sca_len = strlen(sca);
	dst_len = strlen(dst);

	/* SCA Length */
	/* Type-of-address of the SMSC */
	/* Address of SMSC */
	if (sca_len) {
		buffer[len++] = 1 + DIV2UP(sca_len);
		res = pdu_store_number(buffer + len, sca_toa, sca, sca_len);
		if (res < 0) {
			chan_dongle_err = E_BUILD_SCA;
			return -1;
		}
		len += res;
	} else {
		buffer[len++] = 0;
	}
	sca_len = len;

	if(srr)
		pdutype |= PDUTYPE_SRR_REQUESTED;

	/* PDU-type */
	/* TP-Message-Reference. Value will be ignored. The phone will set the number itself. */
	/* Address-Length */
	/* Type-of-address of the sender number */
	buffer[len++] = pdutype;
	buffer[len++] = PDU_MESSAGE_REFERENCE;
	buffer[len++] = dst_len;

	/*  Destination address */
	res = pdu_store_number(buffer + len, dst_toa, dst, dst_len);
	if (res < 0) {
		chan_dongle_err = E_BUILD_PHONE_NUMBER;
		return -1;
	}
	len += res;

	/* TP-PID. Protocol identifier  */
	/* TP-DCS. Data coding scheme */
	/* TP-Validity-Period */
	/* TP-User-Data-Length */
	buffer[len++] = PDU_PID_SMS;
	buffer[len++] = dcs;
	buffer[len++] = pdu_relative_validity(valid_minutes);
	buffer[len++] = msg_len + (!use_udh ? 0 : dcs == PDU_DCS_ALPHABET_UCS2 ? 6 : 7);

	/* encode UDH */
	int msg_padding = 0;
	if (use_udh) {
		buffer[len++] = 5;
		buffer[len++] = 0;
		buffer[len++] = 3;
		buffer[len++] = udh->ref;
		buffer[len++] = udh->parts;
		buffer[len++] = udh->order;
		msg_padding = 1;
	}

	/* TP-User-Data */
	if (dcs == PDU_DCS_ALPHABET_UCS2) {
		memcpy(buffer + len, (const char*)msg, msg_len);
		len += msg_len;
	} else {
		len += (gsm7_pack(msg, msg_reallen, buffer + len, length - len - 1, msg_padding) + 1) / 2;
	}

	/* also check message limit in 176 octets of TPDU */
	*tpdulen = len - sca_len;
	if (*tpdulen > TPDU_LENGTH) {
		chan_dongle_err = E_2BIG;
		return -1;
	}
	if (len > PDU_LENGTH) {
		chan_dongle_err = E_2BIG;
		return -1;
	}

	return len;
}

/*!
 * \brief Parse PDU
 * \param pdu -- SCA + TPDU
 * \param tpdu_length -- length of TPDU in octets
 * \param timestamp -- 25 bytes for timestamp string
 * \return 0 on success
 */
EXPORT_DEF int pdu_parse_sca(uint8_t *pdu, size_t pdu_length, char *sca, size_t sca_len)
{
	int i = 0;
	int sca_digits = (pdu[i++] - 1) * 2;
	int field_len = pdu_parse_number(pdu + i, pdu_length - i, sca_digits, sca, sca_len);
	if (field_len <= 0) {
		chan_dongle_err = E_INVALID_SCA;
		return -1;
	}
	i += field_len;
	return i;
}
EXPORT_DEF int tpdu_parse_type(uint8_t *pdu, size_t pdu_length, int *type)
{
	if (pdu_length < 1) {
		chan_dongle_err = E_INVALID_TPDU_TYPE;
		return -1;
	}
	*type = *pdu;
	return 1;
}
EXPORT_DEF int tpdu_parse_status_report(uint8_t *pdu, size_t pdu_length, int *mr, char *ra, size_t ra_len, char *scts, char *dt, int *st)
{
	int i = 0, field_len;
	if (i + 2 > pdu_length) {
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}
	*mr = pdu[i++];
	int ra_digits = pdu[i++];
	field_len = pdu_parse_number(pdu + i, pdu_length - i, ra_digits, ra, ra_len);
	if (field_len < 0) {
		chan_dongle_err = E_INVALID_PHONE_NUMBER;
		return -1;
	}
	i += field_len;

	if (i + 14 + 1 > pdu_length) {
		chan_dongle_err = E_INVALID_TIMESTAMP;
		return -1;
	}
	i += pdu_parse_timestamp(pdu + i, pdu_length - i, scts);
	i += pdu_parse_timestamp(pdu + i, pdu_length - i, dt);
	*st = pdu[i++];
	return 0;
}
EXPORT_DEF int tpdu_parse_deliver(uint8_t *pdu, size_t pdu_length, int tpdu_type, char *oa, size_t oa_len, char *scts, uint16_t *msg, pdu_udh_t *udh)
{
	int i = 0, field_len, oa_digits, pid, dcs, alphabet, udl, udhl, msg_padding = 0;

	if (i + 1 > pdu_length) {
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}
	oa_digits = pdu[i++];

	field_len = pdu_parse_number(pdu + i, pdu_length - i, oa_digits, oa, oa_len);
	if (field_len < 0) {
		chan_dongle_err = E_INVALID_PHONE_NUMBER;
		return -1;
	}
	i += field_len;

	if (i + 2 + 7 + 1 > pdu_length) {
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}

	pid = pdu[i++];
	dcs = pdu[i++];
	i += pdu_parse_timestamp(pdu + i, pdu_length - i, scts);
	udl = pdu[i++];

	if (pid != PDU_PID_SMS && !(0x41 <= pid && pid <= 0x47) /* PDU_PID_SMS_REPLACE_MASK */) {
		/* 3GPP TSS 23.040 v14.0.0 (2017-013) */
		/* """The MS (Mobile Station, _we_) shall interpret
		 * reserved, obsolete, or unsupported values as the
		 * value 00000000 but shall store them exactly as
		 * received.""" */
		/* Lots of small variations in interpretations, but none
		 * really useful to us. We'll just go with accepting
		 * everything. */
		/* A handfull from the list: */
		/* 0x20..0x3E: different "telematic" devices */
		/* 0x32: e-mail */
		/* 0x38..0x3E: various Service Center specific codes */
		/* 0x3F: gsm/umts station */
		/* 0x40: silent sms; ME should ack but not tell the user */
		/* 0x41..0x47: sms updates (replacing the previous sms #N) */
		ast_log(LOG_NOTICE, "Treating TP-PID value 0x%hhx as regular SMS\n",
			(unsigned char)pid);
	}

	/* http://www.etsi.org/deliver/etsi_gts/03/0338/05.00.00_60/gsmts_0338v050000p.pdf */
	/* The TP-Data-Coding-Scheme field, defined in GSM 03.40,
	 * indicates the data coding scheme of the TP-UD field, and may
	 * indicate a message class. The octet is used according to a
	 * coding group which is indicated in bits 7..4. The octet is
	 * then coded as follows: */
	{
		int dcs_hi = dcs >> 4;
		int dcs_lo = dcs & 0xF;
		int reserved = 0;
		alphabet = -1; /* 7bit, 8bit, ucs2 */

		switch (dcs_hi) {
		case 0x0: /* HIGH 0000: Regular message */
		case 0x1: /* HIGH 0001: Regular message with class */
		case 0x4: /* HIGH 0100: Marked for self-destruct */
		case 0x5: /* HIGH 0101: Marked for self-destruct with class */
		case 0xF: /* HIGH 1111: Data coding/message class */
			/* Apparently bits 0..3 are not reserved anymore:
			 * bits 3..2: {7bit, 8bit, ucs2, undef} */
			alphabet = PDU_DCS_ALPHABET(dcs);
			/* Bits 3..2 set to 11 is reserved, but
			 * according to 3GPP TS 23.038 v14.0.0 (2017-03)
			 * for HIGH 1111 bit 3 (regardless of bit 2) is
			 * reserved. */
			if (alphabet == PDU_DCS_ALPHABET_MASK) {
				reserved = 1;
			}
			/* if 0x1 || 0xF then (dsc_lo & 3): {
			 *     class0, class1-ME-specific,
			 *     class2-SIM-specific,
			 *     class3-TE-specific (3GPP TS 27.005)} */
			break;
		case 0x2: /* HIGH 0010: Compressed regular message */
		case 0x3: /* HIGH 0011: Compressed regular with class */
		case 0x6: /* HIGH 0110: Compressed, marked for self-destruct */
		case 0x7: /* HIGH 0111: Compressed, marked for self-destruct with class */
			chan_dongle_err = E_UNKNOWN;
			return -1;
		case 0xC: /* HIGH 1100: "Discard" MWI */
		case 0xD: /* HIGH 1101: "Store" MWI */
			/* if 0xC then the recipient may discard message
			 * contents, and only show notification */
			/*inactive_active = (dcs_lo & 8);*/
			reserved = (dcs_lo & 4); /* bit 2 reserved */
			/* (dsc_lo & 3): {VM, Fax, E-mail, Other} */
			break;
		default:
			chan_dongle_err = E_UNKNOWN;
			reserved = 1;
			break;
		}
		if (reserved) {
			chan_dongle_err = E_UNKNOWN;
			return -1;
		}
		if (alphabet == -1) {
			chan_dongle_err = E_UNKNOWN;
			return -1;
		}
	}
	if (alphabet == PDU_DCS_ALPHABET_8BIT) {
		// TODO: What to do with binary messages? Are there any?
		// Return an error as it is dangerous to forward the raw binary data as text
		chan_dongle_err = E_INVALID_CHARSET;
		return -1;
	}

	/* calculate number of octets in UD */
	int udl_nibbles;
	int udl_bytes = udl;
	if (alphabet == PDU_DCS_ALPHABET_7BIT) {
		udl_nibbles = (udl * 7 + 3) / 4;
		udl_bytes = (udl_nibbles + 1) / 2;
	}
	if (udl_bytes != pdu_length - i) {
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}

	if (PDUTYPE_UDHI(tpdu_type) == PDUTYPE_UDHI_HAS_HEADER) {
		if (i + 1 > pdu_length) {
			chan_dongle_err = E_UNKNOWN;
			return -1;
		}
		udhl = pdu[i++];

		/* adjust 7-bit padding */
		if (alphabet == PDU_DCS_ALPHABET_7BIT) {
			msg_padding = 6 - (udhl % 7);
			udl_nibbles -= (udhl + 1) * 2;
		}

		/* NOTE: UDHL count octets no need calculation */
		if (pdu_length - i < (size_t)udhl) {
			chan_dongle_err = E_UNKNOWN;
			return -1;
		}

		while (udhl >= 2) {
			int iei_type, iei_len;

			/* get type byte */
			iei_type = pdu[i++];

			/* get length byte */
			iei_len = pdu[i++];

			/* subtract bytes */
			udhl -= 2;

			if (iei_len >= 0 && iei_len <= udhl) {
				switch (iei_type) {
				case 0x00: /* Concatenated */
					if (iei_len != 3) {
						chan_dongle_err = E_UNKNOWN;
						return -1;
					}
					udh->ref = pdu[i++];
					udh->parts = pdu[i++];
					udh->order = pdu[i++];
					udhl -= 3;
					break;
				case 0x08: /* Concatenated, 16 bit ref */
					if (iei_len != 4) {
						chan_dongle_err = E_UNKNOWN;
						return -1;
					}
					udh->ref = (pdu[i++] << 8);
					udh->ref |= pdu[i++];
					udh->parts = pdu[i++];
					udh->order = pdu[i++];
					udhl -= 4;
					break;
				case 0x24: /* National Language Single Shift */
					if (iei_len != 1) {
						chan_dongle_err = E_UNKNOWN;
						return -1;
					}
					udh->ss = pdu[i++];
					break;
				case 0x25: /* National Language Single Shift */
					if (iei_len != 1) {
						chan_dongle_err = E_UNKNOWN;
						return -1;
					}
					udh->ls = pdu[i++];
					break;
				default:
					/* skip rest of IEI */
					i += iei_len;
					udhl -= iei_len;
				}
			} else {
				chan_dongle_err = E_UNKNOWN;
				return -1;
			}
		}

		/* skip rest of UDH, if any */
		i += udhl;
	}

	int msg_len = pdu_length - i, out_len;
	if (alphabet == PDU_DCS_ALPHABET_7BIT) {
		out_len = gsm7_unpack_decode(pdu + i, udl_nibbles, msg, 1024 /* assume enough memory, as SMS messages are limited in size */, msg_padding, udh->ls, udh->ss);
		if (out_len < 0) {
			chan_dongle_err = E_DECODE_GSM7;
			return -1;
		}
	} else {
		out_len = msg_len / 2;
		memcpy((char*)msg, pdu + i, msg_len);
		msg[out_len] = '\0';
	}
	msg[out_len] = '\0';

	return out_len;
}
