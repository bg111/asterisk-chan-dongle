/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DONGLE_AT_RESPONSE_H_INCLUDED
#define CHAN_DONGLE_AT_RESPONSE_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

struct pvt;
struct iovec;

/* AT_RESPONSES_TABLE */
#define AT_RES_AS_ENUM(res, desc, str) RES_ ## res,
#define AT_RES_AS_STRUCTLIST(res, desc, str) {RES_ ## res, desc, str, (sizeof(str)-1)},

#define AT_RESPONSES_TABLE(_) \
	_( PARSE_ERROR, "PARSE ERROR",  "") \
	_( UNKNOWN,     "UNKNOWN",      "") \
\
	_( BOOT,        "^BOOT",        "^BOOT:") \
	_( BUSY,        "BUSY",         "BUSY\r") \
	_( CEND,        "^CEND",        "^CEND:") \
\
	_( CMGR,        "+CMGR",        "+CMGR:") \
	_( CMS_ERROR,   "+CMS ERROR",   "+CMS ERROR:") \
	_( CMTI,        "+CMTI",        "+CMTI:") \
	_( CDSI,        "+CDSI",        "+CDSI:") \
\
	_( CNUM,        "+CNUM",        "+CNUM:") \
		/* and "ERROR+CNUM:", hacked later on */ \
\
	_( CONF,        "^CONF",        "^CONF:") \
	_( CONN,        "^CONN",        "^CONN:") \
	_( COPS,        "+COPS",        "+COPS:") \
	_( CPIN,        "+CPIN",        "+CPIN:") \
\
	_( CREG,        "+CREG",        "+CREG:") \
	_( CSQ,         "+CSQ",         "+CSQ:") \
	_( CSSI,        "+CSSI",        "+CSSI:") \
	_( CSSU,        "+CSSU",        "+CSSU:") \
\
	_( CUSD,        "+CUSD",        "+CUSD:") \
	_( ERROR,       "ERROR",        "ERROR\r") \
		/* and "COMMAND NOT SUPPORT\r", hacked later on */ \
\
	_( MODE,        "^MODE",        "^MODE:") \
	_( NO_CARRIER,  "NO CARRIER",   "NO CARRIER\r") \
\
	_( NO_DIALTONE, "NO DIALTONE",  "NO DIALTONE\r") \
	_( OK,          "OK",           "OK\r") \
	_( ORIG,        "^ORIG",        "^ORIG:") \
	_( RING,        "RING",         "RING\r") \
\
	_( RSSI,        "^RSSI",        "^RSSI:") \
	_( SMMEMFULL,   "^SMMEMFULL",   "^SMMEMFULL:") \
	_( SMS_PROMPT,  "> ",           "> ") \
	_( SRVST,       "^SRVST",       "^SRVST:") \
\
	_( CVOICE,      "^CVOICE",      "^CVOICE:") \
	_( CMGS,        "+CMGS",        "+CMGS:") \
	_( CPMS,        "+CPMS",        "+CPMS:") \
	_( CSCA,        "+CSCA",        "+CSCA:") \
\
	_( CLCC,        "+CLCC",        "+CLCC:") \
	_( CCWA,        "+CCWA",        "+CCWA:") \
/* AT_RESPONSES_TABLE */


typedef enum {

	/* Hackish way to force RES_PARSE_ERROR = -1 for compatibility */
	COMPATIBILITY_RES_START_AT_MINUSONE = -2,

	AT_RESPONSES_TABLE(AT_RES_AS_ENUM)

	/* Hackish way to maintain MAX and MIN responses for compatibility */
	RES_MIN = RES_PARSE_ERROR,
	RES_MAX = RES_CCWA,
} at_res_t;

/*! response description */
typedef struct at_response_t
{
	at_res_t		res;
	const char*		name;
	const char*		id;
	unsigned		idlen;
} at_response_t;

/*! responses control */
typedef struct at_responses_t
{
	const at_response_t*	responses;
	unsigned		ids_first;		/*!< index of first id */
	unsigned		ids;			/*!< number of ids */
	int			name_first;		/*!< value of enum for first name */
	int			name_last;		/*!< value of enum for last name */
} at_responses_t;

/*! responses description */
EXPORT_DECL const at_responses_t at_responses;
EXPORT_DECL const char* at_res2str (at_res_t res);
EXPORT_DECL int at_response (struct pvt* pvt, const struct iovec * iov, int iovcnt, at_res_t at_res);
EXPORT_DECL int at_poll_sms (struct pvt* pvt);

#endif /* CHAN_DONGLE_AT_RESPONSE_H_INCLUDED */
