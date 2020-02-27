/*
    Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/

#ifndef ERROR_H_INCLUDED
#define ERROR_H_INCLUDED
#include "export.h"				/* EXPORT_DECL EXPORT_DEF */

enum error {
	E_UNKNOWN = 0,
	E_DEVICE_DISABLED,
	E_DEVICE_NOT_FOUND,
	E_DEVICE_DISCONNECTED,
	E_INVALID_USSD,
	E_INVALID_PHONE_NUMBER,
	E_PARSE_UTF8,
	E_PARSE_UCS2,
	E_ENCODE_GSM7,
	E_PACK_GSM7,
	E_DECODE_GSM7,
	E_SMSDB,
	E_QUEUE,
	E_BUILD_PDU,
	E_PARSE_CMGR_LINE,
	E_DEPRECATED_CMGR_TEXT,
	E_INVALID_TPDU_LENGTH,
	E_MALFORMED_HEXSTR,
	E_INVALID_SCA,
	E_INVALID_TPDU_TYPE,
	E_PARSE_TPDU,
	E_INVALID_TIMESTAMP,
	E_INVALID_CHARSET,
	E_BUILD_SCA,
	E_BUILD_PHONE_NUMBER,
	E_2BIG
};

INLINE_DECL const char *error2str(int err)
{
	static const char * const errors[] = {
		"Unknown error", "Device disbaled", "Device not found", "Device disconnected", "Invalid USSD", "Invalid phone number",
		"Cannot parse UTF-8", "Cannot parse UCS-2", "Cannot encode GSM7", "Cannot pack GSM7", "Cannot decode GSM7", "SMSDB error", "Queue error", "PDU building error",
		"Can't parse +CMGR response line", "Parsing messages in TEXT mode is not supported anymore; This message should never appear. Nevertheless, if this message appears, please report on GitHub.",
		"Invalid TPDU length in CMGR PDU status line", "Malformed hex string", "Invalid SCA", "Invalid TPDU type", "Cannot parse TPDU", "Invalid timestamp", "Invalid charset", "Cannot build SCA", "Cannot build phone number", "Input too large"
	};
	return enum2str(err, errors, ITEMS_OF(errors));
}

EXPORT_DECL __thread int chan_dongle_err;

#endif
