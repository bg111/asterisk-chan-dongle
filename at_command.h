/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DONGLE_AT_SEND_H_INCLUDED
#define CHAN_DONGLE_AT_SEND_H_INCLUDED

#include "ast_config.h"

#include "export.h"		/* EXPORT_DECL EXPORT_DEF */
#include "dc_config.h"		/* call_waiting_t */
#include "mutils.h"		/* enum2str_def() ITEMS_OF() */

#define CCWA_CLASS_VOICE	1
#define	SMS_INDEX_MAX		256	/* exclusive */

/* AT_COMMANDS_TABLE */
#define AT_CMD_AS_ENUM(cmd, str) CMD_ ## cmd,
#define AT_CMD_AS_STRING(cmd, str) str,

#define AT_COMMANDS_TABLE(_) \
	_( USER,            "USER'S") \
	_( AT,              "AT") \
	_( AT_A,            "ATA") \
	_( AT_CCWA_STATUS,  "AT+CCWA?") \
	_( AT_CCWA_SET,     "AT+CCWA=") \
	_( AT_CFUN,         "AT+CFUN") \
\
	_( AT_CGMI,         "AT+CGMI") \
	_( AT_CGMM,         "AT+CGMM") \
	_( AT_CGMR,         "AT+CGMR") \
	_( AT_CGSN,         "AT+CGSN") \
\
	_( AT_CHUP,         "AT+CHUP") \
	_( AT_CIMI,         "AT+CIMI") \
/*	_( AT_CLIP,         "AT+CLIP") */ \
	_( AT_CLIR,         "AT+CLIR") \
\
	_( AT_CLVL,         "AT+CLVL") \
	_( AT_CMGD,         "AT+CMGD") \
	_( AT_CMGF,         "AT+CMGF") \
	_( AT_CMGR,         "AT+CMGR") \
\
	_( AT_CMGS,         "AT+CMGS") \
	_( AT_SMSTEXT,      "SMSTEXT") \
	_( AT_CNMI,         "AT+CNMI") \
	_( AT_CNUM,         "AT+CNUM") \
\
	_( AT_COPS,         "AT+COPS?") \
	_( AT_COPS_INIT,    "AT+COPS=") \
	_( AT_CPIN,         "AT+CPIN?") \
	_( AT_CPMS,         "AT+CPMS") \
\
	_( AT_CREG,         "AT+CREG?") \
	_( AT_CREG_INIT,    "AT+CREG=") \
	_( AT_CSCS,         "AT+CSCS") \
	_( AT_CSQ,          "AT+CSQ") \
\
	_( AT_CSSN,         "AT+CSSN") \
	_( AT_CUSD,         "AT+CUSD") \
	_( AT_CVOICE,       "AT^CVOICE") \
	_( AT_D,            "ATD") \
\
	_( AT_DDSETEX,      "AT^DDSETEX") \
	_( AT_DTMF,         "AT^DTMF") \
	_( AT_E,            "ATE") \
\
	_( AT_U2DIAG,       "AT^U2DIAG") \
	_( AT_Z,            "ATZ") \
	_( AT_CMEE,         "AT+CMEE") \
	_( AT_CSCA,         "AT+CSCA") \
\
	_( AT_CHLD_1x,      "AT+CHLD=1x") \
	_( AT_CHLD_2x,      "AT+CHLD=2x") \
	_( AT_CHLD_2,       "AT+CHLD=2") \
	_( AT_CHLD_3,       "AT+CHLD=3") \
	_( AT_CLCC,         "AT+CLCC") \
/* AT_COMMANDS_TABLE */

typedef enum {
	AT_COMMANDS_TABLE(AT_CMD_AS_ENUM)
} at_cmd_t;

/*!
 * \brief Get the string representation of the given AT command
 * \param cmd -- the command to process
 * \return a string describing the given command
 */

INLINE_DECL const char* at_cmd2str (at_cmd_t cmd)
{
	static const char * const cmds[] = {
		AT_COMMANDS_TABLE(AT_CMD_AS_STRING)
	};
	return enum2str_def(cmd, cmds, ITEMS_OF(cmds), "UNDEFINED");
}

struct cpvt;

EXPORT_DECL const char *at_cmd2str(at_cmd_t cmd);
EXPORT_DECL int at_enqueue_initialization(struct cpvt *cpvt, at_cmd_t from_command);
EXPORT_DECL int at_enqueue_ping(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_cops(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_sms(struct cpvt *cpvt, const char *number, const char *msg, unsigned validity_min, int report_req, const char *payload, size_t payload_len);
EXPORT_DECL int at_enqueue_ussd(struct cpvt *cpvt, const char *code);
EXPORT_DECL int at_enqueue_dtmf(struct cpvt *cpvt, char digit);
EXPORT_DECL int at_enqueue_set_ccwa(struct cpvt *cpvt, unsigned call_waiting);
EXPORT_DECL int at_enqueue_reset(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_dial(struct cpvt *cpvt, const char *number, int clir);
EXPORT_DECL int at_enqueue_answer(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_user_cmd(struct cpvt *cpvt, const char *input);
EXPORT_DECL void at_retrieve_next_sms(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_retrieve_sms(struct cpvt *cpvt, int index);
EXPORT_DECL int at_enqueue_delete_sms(struct cpvt *cpvt, int index);
EXPORT_DECL int at_enqueue_hangup(struct cpvt *cpvt, int call_idx);
EXPORT_DECL int at_enqueue_volsync(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_clcc(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_activate(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_flip_hold(struct cpvt *cpvt);
EXPORT_DECL int at_enqueue_conference(struct cpvt *cpvt);
EXPORT_DECL void at_hangup_immediality(struct cpvt *cpvt);

#endif /* CHAN_DONGLE_AT_SEND_H_INCLUDED */
