/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>
*/

/*
   Copyright (C) 2009 - 2010 Artem Makhutov
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/

#include "ast_config.h"

#include <asterisk/utils.h>

#include "at_command.h"
//#include ".h"
#include "at_queue.h"
#include "char_conv.h"			/* char_to_hexstr_7bit() */
#include "chan_dongle.h"		/* struct pvt */
#include "pdu.h"			/* build_pdu() */
#include "smsdb.h"
#include "error.h"

static const char cmd_at[] 	 = "AT\r";
static const char cmd_chld1x[]   = "AT+CHLD=1%d\r";
static const char cmd_chld2[]    = "AT+CHLD=2\r";
static const char cmd_clcc[]     = "AT+CLCC\r";
static const char cmd_ddsetex2[] = "AT^DDSETEX=2\r";
static const char cmd_qpcmv10[]  = "AT+QPCMV=1,0\r";

/*!
 * \brief Format and fill generic command
 * \param cmd -- the command structure
 * \param format -- printf format string
 * \param ap -- list of arguments
 * \return 0 on success
 */

static int at_fill_generic_cmd_va (at_queue_cmd_t * cmd, const char * format, va_list ap)
{
	char buf[4096];

	cmd->length = vsnprintf (buf, sizeof(buf)-1, format, ap);

	buf[cmd->length] = 0;
	cmd->data = ast_strdup(buf);
	if(!cmd->data)
		return -1;

	cmd->flags &= ~ATQ_CMD_FLAG_STATIC;
	return 0;

}

/*!
 * \brief Format and fill generic command
 * \param cmd -- the command structure
 * \param format -- printf format string
 * \return 0 on success
 */

static int __attribute__ ((format(printf, 2, 3))) at_fill_generic_cmd (at_queue_cmd_t * cmd, const char * format, ...)
{
	va_list ap;
	int rv;

	va_start(ap, format);
	rv = at_fill_generic_cmd_va(cmd, format, ap);
	va_end(ap);

	return rv;
}

/*!
 * \brief Enqueue generic command
 * \param pvt -- pvt structure
 * \param cmd -- at command
 * \param prio -- queue priority of this command
 * \param format -- printf format string including AT command text
 * \return 0 on success
 */

static int __attribute__ ((format(printf, 4, 5))) at_enqueue_generic(struct cpvt *cpvt, at_cmd_t cmd, int prio, const char *format, ...)
{
	va_list ap;
	int rv;
	at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_DYN(cmd);

	va_start(ap, format);
	rv = at_fill_generic_cmd_va(&at_cmd, format, ap);
	va_end(ap);

	if(!rv)
		rv = at_queue_insert(cpvt, &at_cmd, 1, prio);
	return rv;
}

/*!
 * \brief Enqueue initialization commands
 * \param cpvt -- cpvt structure
 * \param from_command -- begin initialization from this command in list
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_initialization(struct cpvt *cpvt, at_cmd_t from_command)
{
	static const at_queue_cmd_t st_cmds[] = {
		/* AT */
		ATQ_CMD_DECLARE_ST(CMD_AT, cmd_at),
		/* optional,  reload configuration */
		ATQ_CMD_DECLARE_ST(CMD_AT_Z, "ATZ\r"),
		/* disable echo */
		ATQ_CMD_DECLARE_ST(CMD_AT_E, "ATE0\r"),
		/* optional, Enable or disable some devices */
		ATQ_CMD_DECLARE_DYN(CMD_AT_U2DIAG),
		/* Get manufacturer info */
		ATQ_CMD_DECLARE_ST(CMD_AT_CGMI, "AT+CGMI\r"),

		/* Get Product name */
		ATQ_CMD_DECLARE_ST(CMD_AT_CGMM, "AT+CGMM\r"),
		/* Get software version */
		ATQ_CMD_DECLARE_ST(CMD_AT_CGMR, "AT+CGMR\r"),
		/* set MS Error Report to 'ERROR' only.
		 * TODO: change to 1 or 2 and add support in response handlers */
		ATQ_CMD_DECLARE_ST(CMD_AT_CMEE, "AT+CMEE=0\r"),

		/* IMEI Read */
		ATQ_CMD_DECLARE_ST(CMD_AT_CGSN, "AT+CGSN\r"),
		/* IMSI Read */
		ATQ_CMD_DECLARE_ST(CMD_AT_CIMI, "AT+CIMI\r"),
		/* check is password authentication requirement and the
		 * remainder validation times */
		ATQ_CMD_DECLARE_ST(CMD_AT_CPIN, "AT+CPIN?\r"),
		/* Read operator name */
		ATQ_CMD_DECLARE_ST(CMD_AT_COPS_INIT, "AT+COPS=0,0\r"),

		/* GSM registration status setting */
		ATQ_CMD_DECLARE_STI(CMD_AT_CREG_INIT, "AT+CREG=2\r"),
		/* GSM registration status */
		ATQ_CMD_DECLARE_ST(CMD_AT_CREG, "AT+CREG?\r"),
		/* Get Subscriber number */
		ATQ_CMD_DECLARE_STI(CMD_AT_CNUM, "AT+CNUM\r"),
		/* read the current voice mode, and return sampling
		 * rate, data bit, frame period */
		ATQ_CMD_DECLARE_STI(CMD_AT_CVOICE, "AT^CVOICE?\r"),
		ATQ_CMD_DECLARE_STI(CMD_AT_QPCMV, "AT+QPCMV?\r"), /* for Quectel */

		/* Get SMS Service center address */
		ATQ_CMD_DECLARE_ST(CMD_AT_CSCA, "AT+CSCA?\r"),
		/* activate Supplementary Service Notification with CSSI and CSSU */
		ATQ_CMD_DECLARE_ST(CMD_AT_CSSN, "AT+CSSN=1,1\r"),
		/* Set Message Format */
		ATQ_CMD_DECLARE_ST(CMD_AT_CMGF, "AT+CMGF=0\r"),

		/* SMS Storage Selection */
		ATQ_CMD_DECLARE_ST(CMD_AT_CPMS, "AT+CPMS=\"SM\",\"SM\",\"SM\"\r"),
		/* New SMS Notification Setting +CNMI=[<mode>[,<mt>[,<bm>[,<ds>[,<bfr>]]]]] */
		/* pvt->initialized = 1 after successful of CMD_AT_CNMI */
		ATQ_CMD_DECLARE_ST(CMD_AT_CNMI, "AT+CNMI=2,1,0,2,0\r"),
		/* Query Signal quality */
		ATQ_CMD_DECLARE_ST(CMD_AT_CSQ, "AT+CSQ\r"),
	};

	unsigned in, out;
	int begin = -1;
	int err;
	char * ptmp1 = NULL;
	pvt_t * pvt = cpvt->pvt;
	at_queue_cmd_t cmds[ITEMS_OF(st_cmds)];

	/* customize list */
	for(in = out = 0; in < ITEMS_OF(st_cmds); in++)
	{
		if(begin == -1)
		{
			if(st_cmds[in].cmd == from_command)
				begin = in;
			else
				continue;
		}

		if(st_cmds[in].cmd == CMD_AT_Z && !CONF_SHARED(pvt, resetdongle))
			continue;
		if(st_cmds[in].cmd == CMD_AT_U2DIAG && CONF_SHARED(pvt, u2diag) == -1)
			continue;

		memcpy(&cmds[out], &st_cmds[in], sizeof(st_cmds[in]));

		if(cmds[out].cmd == CMD_AT_U2DIAG)
		{
			err = at_fill_generic_cmd(&cmds[out], "AT^U2DIAG=%d\r", CONF_SHARED(pvt, u2diag));
			if(err)
				goto failure;
			ptmp1 = cmds[out].data;
		}
		if(cmds[out].cmd == from_command)
			begin = out;
		out++;
	}

	if(out > 0)
		return at_queue_insert(cpvt, cmds, out, 0);
	return 0;
failure:
	if(ptmp1)
		ast_free(ptmp1);
	return err;
}

/*!
 * \brief Enqueue the AT+COPS? command
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

EXPORT_DEF int at_enqueue_cops(struct cpvt *cpvt)
{
	static const char cmd[] = "AT+COPS?\r";
	static at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_ST(CMD_AT_COPS, cmd);

	if (at_queue_insert_const(cpvt, &at_cmd, 1, 0) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}


/* SMS sending */
static int at_enqueue_pdu(struct cpvt *cpvt, const char *pdu, size_t length, size_t tpdulen, int uid)
{
	char buf[8+25+1];
	at_queue_cmd_t at_cmd[] = {
		{ CMD_AT_CMGS,    RES_SMS_PROMPT, ATQ_CMD_FLAG_DEFAULT, { ATQ_CMD_TIMEOUT_MEDIUM, 0}, NULL, 0 },
		{ CMD_AT_SMSTEXT, RES_OK,         ATQ_CMD_FLAG_DEFAULT, { ATQ_CMD_TIMEOUT_LONG, 0},   NULL, 0 }
		};

	at_cmd[1].data = ast_malloc(length + 2);
	if(!at_cmd[1].data)
	{
		return -ENOMEM;
	}

	at_cmd[1].length = length + 1;

	memcpy(at_cmd[1].data, pdu, length);
	at_cmd[1].data[length] = 0x1A;
	at_cmd[1].data[length + 1] = 0x0;

	at_cmd[0].length = snprintf(buf, sizeof(buf), "AT+CMGS=%d\r", (int)tpdulen);
	at_cmd[0].data = ast_strdup(buf);
	if(!at_cmd[0].data)
	{
		ast_free(at_cmd[1].data);
		return -ENOMEM;
	}

	if (at_queue_insert_uid(cpvt, at_cmd, ITEMS_OF(at_cmd), 0, uid) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue a SMS message
 * \param cpvt -- cpvt structure
 * \param number -- the destination of the message
 * \param msg -- utf-8 encoded message
 */
EXPORT_DEF int at_enqueue_sms(struct cpvt *cpvt, const char *destination, const char *msg, unsigned validity_minutes, int report_req, const char *payload, size_t payload_len)
{
	ssize_t res;
	pvt_t* pvt = cpvt->pvt;

	/* set default validity period */
	if (validity_minutes <= 0)
		validity_minutes = 3 * 24 * 60;

	int msg_len = strlen(msg);
	uint16_t msg_ucs2[msg_len * 2];
	res = utf8_to_ucs2(msg, msg_len, msg_ucs2, sizeof(msg_ucs2));
	if (res < 0) {
		chan_dongle_err = E_PARSE_UTF8;
		return -1;
	}

	char hexbuf[PDU_LENGTH * 2 + 1];

	pdu_part_t pdus[255];
	int csmsref = smsdb_get_refid(pvt->imsi, destination);
	if (csmsref < 0) {
		chan_dongle_err = E_SMSDB;
		return -1;
	}
	res = pdu_build_mult(pdus, "" /* pvt->sms_scenter */, destination, msg_ucs2, res, validity_minutes, !!report_req, csmsref);
	if (res < 0) {
		/* pdu_build_mult sets chan_dongle_err */
		return -1;
	}
	int uid = smsdb_outgoing_add(pvt->imsi, destination, res, validity_minutes * 60, report_req, payload, payload_len);
	if (uid < 0) {
		chan_dongle_err = E_SMSDB;
		return -1;
	}
	for (int i = 0; i < res; ++i) {
		hexify(pdus[i].buffer, pdus[i].length, hexbuf);
		if (at_enqueue_pdu(cpvt, hexbuf, pdus[i].length * 2, pdus[i].tpdu_length, uid) < 0) {
			return -1;
		}
	}

	return 0;
}

/*!
 * \brief Enqueue AT+CUSD.
 * \param cpvt -- cpvt structure
 * \param code the CUSD code to send
 */

EXPORT_DEF int at_enqueue_ussd(struct cpvt *cpvt, const char *code)
{
	static const char cmd[] = "AT+CUSD=1,\"";
	static const char cmd_end[] = "\",15\r";
	at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_CUSD);
	ssize_t res;
	int length;
	char buf[4096];

	memcpy (buf, cmd, STRLEN(cmd));
	length = STRLEN(cmd);
	int code_len = strlen(code);

	// use 7 bit encoding. 15 is 00001111 in binary and means 'Language using the GSM 7 bit default alphabet; Language unspecified' accodring to GSM 23.038
	uint16_t code16[code_len * 2];
	uint8_t code_packed[4069];
	res = utf8_to_ucs2(code, code_len, code16, sizeof(code16));
	if (res < 0) {
		chan_dongle_err = E_PARSE_UTF8;
		return -1;
	}
	res = gsm7_encode(code16, res, code16);
	if (res < 0) {
		chan_dongle_err = E_ENCODE_GSM7;
		return -1;
	}
	res = gsm7_pack(code16, res, (char*)code_packed, sizeof(code_packed), 0);
	if (res < 0) {
		chan_dongle_err = E_PACK_GSM7;
		return -1;
	}
	res = (res + 1) / 2;
	hexify(code_packed, res, buf + STRLEN(cmd));
	length += res * 2;

	memcpy(buf + length, cmd_end, STRLEN(cmd_end)+1);
	length += STRLEN(cmd_end);

	at_cmd.length = length;
	at_cmd.data = ast_strdup (buf);
	if (!at_cmd.data) {
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}

	if (at_queue_insert(cpvt, &at_cmd, 1, 0) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}


/*!
 * \brief Enqueue a DTMF command
 * \param cpvt -- cpvt structure
 * \param digit -- the dtmf digit to send
 * \return -2 if digis is invalid, 0 on success
 */

EXPORT_DEF int at_enqueue_dtmf(struct cpvt *cpvt, char digit)
{
	switch (digit)
	{
/* unsupported, but AT^DTMF=1,22 OK and "2" sent
*/
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
			return -1974; // TODO: ???
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

		case '*':
		case '#':
			return at_enqueue_generic(cpvt, CMD_AT_DTMF, 1, "AT^DTMF=%d,%c\r", cpvt->call_idx, digit);
	}
	return -1;
}

/*!
 * \brief Enqueue the AT+CCWA command (disable call waiting)
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

EXPORT_DEF int at_enqueue_set_ccwa(struct cpvt *cpvt, unsigned call_waiting)
{
	static const char cmd_ccwa_get[] = "AT+CCWA=1,2,1\r";
	static const char cmd_ccwa_set[] = "AT+CCWA=%d,%d,%d\r";
	int err;
	call_waiting_t value;
	at_queue_cmd_t cmds[] = {
		/* Set Call-Waiting On/Off */
		ATQ_CMD_DECLARE_DYNIT(CMD_AT_CCWA_SET, ATQ_CMD_TIMEOUT_MEDIUM, 0),
		/* Query CCWA Status for Voice Call  */
		ATQ_CMD_DECLARE_STIT(CMD_AT_CCWA_STATUS, cmd_ccwa_get, ATQ_CMD_TIMEOUT_MEDIUM, 0),
	};
	at_queue_cmd_t * pcmd = cmds;
	unsigned count = ITEMS_OF(cmds);

	if(call_waiting == CALL_WAITING_DISALLOWED || call_waiting == CALL_WAITING_ALLOWED)
	{
		value = call_waiting;
		err = call_waiting == CALL_WAITING_ALLOWED ? 1 : 0;
		err = at_fill_generic_cmd(&cmds[0], cmd_ccwa_set, err, err, CCWA_CLASS_VOICE);
		if (err) {
			chan_dongle_err = E_UNKNOWN;
		    return -1;
		}
	}
	else
	{
		value = CALL_WAITING_AUTO;
		pcmd++;
		count--;
	}
	CONF_SHARED(cpvt->pvt, callwaiting) = value;

	if (at_queue_insert(cpvt, pcmd, count, 0) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue the device reset command (AT+CFUN Operation Mode Setting)
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

EXPORT_DEF int at_enqueue_reset(struct cpvt *cpvt)
{
	static const char cmd[] = "AT+CFUN=1,1\r";
	static const at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CFUN, cmd);

	if (at_queue_insert_const(cpvt, &at_cmd, 1, 0) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}


/*!
 * \brief Enqueue a dial commands
 * \param cpvt -- cpvt structure
 * \param number -- the called number
 * \param clir -- value of clir
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_dial(struct cpvt *cpvt, const char *number, int clir)
{
	struct pvt *pvt = cpvt->pvt;
	int err;
	int cmdsno = 0;
	char * tmp = NULL;
	at_queue_cmd_t cmds[6];

	if(PVT_STATE(pvt, chan_count[CALL_STATE_ACTIVE]) > 0 && CPVT_TEST_FLAG(cpvt, CALL_FLAG_HOLD_OTHER))
	{
		ATQ_CMD_INIT_ST(cmds[0], CMD_AT_CHLD_2, cmd_chld2);
/*  enable this cause response_clcc() see all calls are held and insert 'AT+CHLD=2'
		ATQ_CMD_INIT_ST(cmds[1], CMD_AT_CLCC, cmd_clcc);
*/
		cmdsno = 1;
	}

	if(clir != -1)
	{
		err = at_fill_generic_cmd(&cmds[cmdsno], "AT+CLIR=%d\r", clir);
		if (err) {
			chan_dongle_err = E_UNKNOWN;
			return -1;
		}
		tmp = cmds[cmdsno].data;
		ATQ_CMD_INIT_DYNI(cmds[cmdsno], CMD_AT_CLIR);
		cmdsno++;
	}

	err = at_fill_generic_cmd(&cmds[cmdsno], "ATD%s;\r", number);
	if(err)
	{
		ast_free(tmp);
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}

	ATQ_CMD_INIT_DYNI(cmds[cmdsno], CMD_AT_D);
	cmdsno++;

/* on failed ATD this up held call */
	ATQ_CMD_INIT_ST(cmds[cmdsno], CMD_AT_CLCC, cmd_clcc);
	cmdsno++;

	if (pvt->has_voice_quectel) {
		ATQ_CMD_INIT_ST(cmds[cmdsno], CMD_AT_DDSETEX, cmd_qpcmv10);
	} else {
		ATQ_CMD_INIT_ST(cmds[cmdsno], CMD_AT_DDSETEX, cmd_ddsetex2);
	}
	cmdsno++;

	if (at_queue_insert(cpvt, cmds, cmdsno, 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	/* set CALL_FLAG_NEED_HANGUP early because ATD may be still in queue while local hangup called */
	CPVT_SET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
	return 0;
}

/*!
 * \brief Enqueue a answer commands
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_answer(struct cpvt *cpvt)
{
	pvt_t* pvt = cpvt->pvt;
	at_queue_cmd_t cmds[2];
	unsigned count = 2; /* AT_A + setup-voice */
	const char * cmd1;

	ATQ_CMD_INIT_DYN(cmds[0], CMD_AT_A);
	if (pvt->has_voice_quectel) {
		ATQ_CMD_INIT_ST(cmds[1], CMD_AT_DDSETEX, cmd_qpcmv10);
	} else {
		ATQ_CMD_INIT_ST(cmds[1], CMD_AT_DDSETEX, cmd_ddsetex2);
	}

	if(cpvt->state == CALL_STATE_INCOMING)
	{
/* FIXME: channel number? */
		cmd1 = "ATA\r";
	}
	else if(cpvt->state == CALL_STATE_WAITING)
	{
		cmds[0].cmd = CMD_AT_CHLD_2x;
		cmd1 = "AT+CHLD=2%d\r";
/* no need CMD_AT_DDSETEX in this case? */
		count--;
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Request answer for call idx %d with state '%s'\n", PVT_ID(cpvt->pvt), cpvt->call_idx, call_state2str(cpvt->state));
		return -1;
	}

	if (at_fill_generic_cmd(&cmds[0], cmd1, cpvt->call_idx) != 0) {
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}
	if (at_queue_insert(cpvt, cmds, count, 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue an activate commands 'Put active calls on hold and activate call x.'
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_activate(struct cpvt *cpvt)
{
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_DYN(CMD_AT_CHLD_2x),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc),
		};

	if (cpvt->state == CALL_STATE_ACTIVE)
		return 0;

	if (cpvt->state != CALL_STATE_ONHOLD && cpvt->state != CALL_STATE_WAITING)
	{
		ast_log (LOG_ERROR, "[%s] Imposible activate call idx %d from state '%s'\n",
				PVT_ID(cpvt->pvt), cpvt->call_idx, call_state2str(cpvt->state));
		return -1;
	}

	if (at_fill_generic_cmd(&cmds[0], "AT+CHLD=2%d\r", cpvt->call_idx) != 0) {
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}
	if (at_queue_insert(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue an commands for 'Put active calls on hold and activate the waiting or held call.'
 * \param pvt -- pvt structure
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_flip_hold(struct cpvt *cpvt)
{
	static const at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT_CHLD_2, cmd_chld2),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc),
		};

	if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue ping command
 * \param pvt -- pvt structure
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_ping(struct cpvt *cpvt)
{
	static const at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_STIT(CMD_AT, cmd_at, ATQ_CMD_TIMEOUT_SHORT, 0),
		};

	if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue user-specified command
 * \param cpvt -- cpvt structure
 * \param input -- user's command
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_user_cmd(struct cpvt *cpvt, const char *input)
{
	if (at_enqueue_generic(cpvt, CMD_USER, 1, "%s\r", input) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Start reading next SMS, if any
 * \param cpvt -- cpvt structure
 */
EXPORT_DEF void at_retrieve_next_sms(struct cpvt *cpvt, at_cmd_suppress_error_t suppress_error)
{
	pvt_t *pvt = cpvt->pvt;
	unsigned int i;

	if (pvt->incoming_sms_index != -1U)
	{
		/* clear SMS index */
		i = pvt->incoming_sms_index;
		pvt->incoming_sms_index = -1U;

		/* clear this message index from inbox */
		sms_inbox_clear(pvt, i);
	}

	/* get next message to fetch from inbox */
	for (i = 0; i != SMS_INDEX_MAX; i++)
	{
		if (is_sms_inbox_set(pvt, i))
			break;
	}

	if (i == SMS_INDEX_MAX ||
	    at_enqueue_retrieve_sms(cpvt, i, suppress_error) != 0)
	{
		pvt_try_restate(pvt);
	}
}

/*!
 * \brief Enqueue commands for reading SMS
 * \param cpvt -- cpvt structure
 * \param index -- index of message in store
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_retrieve_sms(struct cpvt *cpvt, int index, at_cmd_suppress_error_t suppress_error)
{
	pvt_t *pvt = cpvt->pvt;
	int err;
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_DYN2(CMD_AT_CMGR, RES_CMGR),
	};
	unsigned cmdsno = ITEMS_OF(cmds);

	if (suppress_error == SUPPRESS_ERROR_ENABLED) {
		cmds[0].flags |= ATQ_CMD_FLAG_SUPPRESS_ERROR;
	}

	/* set that we want to receive this message */
	if (!sms_inbox_set(pvt, index)) {
		chan_dongle_err = E_UNKNOWN;
		return -1;
	}

	/* check if message is already being received */
	if (pvt->incoming_sms_index != -1U) {
		ast_debug (4, "[%s] SMS retrieve of [%d] already in progress\n",
		    PVT_ID(pvt), pvt->incoming_sms_index);
		return 0;
	}

	pvt->incoming_sms_index = index;

	err = at_fill_generic_cmd (&cmds[0], "AT+CMGR=%d\r", index);
	if (err)
		goto error;

	err = at_queue_insert (cpvt, cmds, cmdsno, 0);
	if (err)
		goto error;
	return 0;
error:
	ast_log (LOG_WARNING, "[%s] SMS command error %d\n", PVT_ID(pvt), err);
	pvt->incoming_sms_index = -1U;
	chan_dongle_err = E_UNKNOWN;
	return -1;
}

/*!
 * \brief Enqueue commands for deleting SMS
 * \param cpvt -- cpvt structure
 * \param index -- index of message in store
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_delete_sms(struct cpvt *cpvt, int index)
{
	int err;
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_DYN(CMD_AT_CMGD)
	};
	unsigned cmdsno = ITEMS_OF(cmds);

	err = at_fill_generic_cmd (&cmds[0], "AT+CMGD=%d\r", index);
	if (err) {
		chan_dongle_err = E_UNKNOWN;
		return err;
	}

	if (at_queue_insert(cpvt, cmds, cmdsno, 0) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue AT+CHLD1x or AT+CHUP hangup command
 * \param cpvt -- channel_pvt structure
 * \param call_idx -- call id
 * \return 0 on success
 */

EXPORT_DEF int at_enqueue_hangup(struct cpvt *cpvt, int call_idx)
{

/*
	this try of hangup non-active (held) channel as workaround for HW BUG 2

	int err;
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT_CHLD_2, cmd_chld2),
		ATQ_CMD_DECLARE_DYN(CMD_AT_CHLD_1x),
		};
	at_queue_cmd_t * pcmds = cmds;
	unsigned count = ITEMS_OF(cmds);

	err = at_fill_generic_cmd(&cmds[1], "AT+CHLD=1%d\r", call_idx);
	if(err)
		return err;

	if(cpvt->state != CALL_STATE_ACTIVE)
	{
		pcmds++;
		count--;
	}
	return at_queue_insert(cpvt, pcmds, count, 1);
*/

/*
	HW BUG 1:
	    Sequence
		ATDnum;
		    OK
		    ^ORIG:1,0
		AT+CHLD=11		if this command write to modem E1550 before ^CONF: for ATD device no more write responses to any entered command at all
		    ^CONF:1
	Workaround
		a) send AT+CHUP if possible (single call)
		b) insert fake empty command after ATD expected ^CONF: response if CONF not received yet
	HW BUG 2:
	    Sequence
		ATDnum1;
		    OK
		    ^ORIG:1,0
		    ^CONF:1
		    ^CONN:1,0
		AT+CHLD=2
		    OK
		ATDnum2;
		    OK
		    ^ORIG:2,0
		    ^CONF:2
		    ^CONN:2,0
		AT+CHLD=11		after this command call 1 terminated, but call 2 no voice data and any other new calls created
		    OK
		    ^CEND:1,...
					same result if active call terminated with AT+CHLD=12
					same result if active call terminated by peer side1
	Workaround
		not found yes
*/
/*
	static const struct
	{
		at_cmd_t	cmd;
		const char	*data;
	} commands[] =
	{
		{ CMD_AT_CHUP, "AT+CHUP\r" },
		{ CMD_AT_CHLD_1x, "AT+CHLD=1%d\r" }
	};
	int idx = 0;
	if(cpvt == &cpvt->pvt->sys_chan || CPVT_TEST_FLAGS(cpvt, CALL_FLAG_CONF_DONE|CALL_FLAG_IDX_VALID))
	{
		if(cpvt->pvt->chansno > 1)
			idx = 1;
	}

	return at_enqueue_generic(cpvt, commands[idx].cmd, 1, commands[idx].data, call_idx);
*/
	static const char cmd_chup[] = "AT+CHUP\r";

	struct pvt* pvt = cpvt->pvt;
	int err;
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT_CHUP, cmd_chup),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc),
		};

	if(cpvt == &pvt->sys_chan || cpvt->dir == CALL_DIR_INCOMING || (cpvt->state != CALL_STATE_INIT && cpvt->state != CALL_STATE_DIALING))
	{
		/* FIXME: other channels may be in RELEASED or INIT state */
		if(PVT_STATE(pvt, chansno) > 1)
		{
			cmds[0].cmd = CMD_AT_CHLD_1x;
			err = at_fill_generic_cmd(&cmds[0], cmd_chld1x, call_idx);
			if (err) {
				chan_dongle_err = E_UNKNOWN;
				return -1;
			}
		}
	}

	/* early AT+CHUP before ^ORIG for outgoing call may not get ^CEND in future */
	if(cpvt->state == CALL_STATE_INIT)
		pvt->last_dialed_cpvt = 0;

	if (at_queue_insert(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue AT+CLVL commands for volume synchronization
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

EXPORT_DEF int at_enqueue_volsync(struct cpvt *cpvt)
{
	static const char cmd1[] = "AT+CLVL=1\r";
	static const char cmd2[] = "AT+CLVL=5\r";
	static const at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT_CLVL, cmd1),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLVL, cmd2),
		};

	if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue AT+CLCC command
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_clcc(struct cpvt *cpvt)
{
	static const at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc);

	if (at_queue_insert_const(cpvt, &at_cmd, 1, 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue AT+CHLD=3 command
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
EXPORT_DEF int at_enqueue_conference(struct cpvt *cpvt)
{
	static const char cmd_chld3[] = "AT+CHLD=3\r";
	static const at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT_CHLD_3, cmd_chld3),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc),
		};

	if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_dongle_err = E_QUEUE;
		return -1;
	}
	return 0;
}


/*!
 * \brief SEND AT+CHUP command to device IMMEDIALITY
 * \param cpvt -- cpvt structure
 */
EXPORT_DEF void at_hangup_immediality(struct cpvt* cpvt)
{
	char buf[20];
	int length = snprintf(buf, sizeof(buf), cmd_chld1x, cpvt->call_idx);

	if(length > 0)
		at_write(cpvt->pvt, buf, length);
}

