/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>
*/
#include "ast_config.h"

#include <signal.h>				/* SIGURG */

#include <asterisk/callerid.h>			/*  AST_PRES_* */

#include "helpers.h"
#include "chan_dongle.h"			/* devices */
#include "at_command.h"
#include "error.h"
// #include "pdu.h"				/* pdu_digit2code() */

static int is_valid_ussd_string(const char* number)
{
	for (; *number; number++) {
		if (*number >= '0' && *number <= '9' || *number == '*' || *number == '#') {
			continue;
		}
		return 0;
	}
	return 1;
}

#/* */
EXPORT_DEF int is_valid_phone_number(const char *number)
{
	if (number[0] == '+') {
		number++;
	}
	for (; *number; number++) {
		if (*number >= '0' && *number <= '9') {
			continue;
		}
		return 0;
	}
	return 1;
}


#/* */
EXPORT_DEF int get_at_clir_value (struct pvt* pvt, int clir)
{
	int res = 0;

	switch (clir)
	{
		case AST_PRES_ALLOWED_NETWORK_NUMBER:
		case AST_PRES_ALLOWED_USER_NUMBER_FAILED_SCREEN:
		case AST_PRES_ALLOWED_USER_NUMBER_NOT_SCREENED:
		case AST_PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN:
		case AST_PRES_NUMBER_NOT_AVAILABLE:
			ast_debug (2, "[%s] callingpres: %s\n", PVT_ID(pvt), ast_describe_caller_presentation (clir));
			res = 2;
			break;

		case AST_PRES_PROHIB_NETWORK_NUMBER:
		case AST_PRES_PROHIB_USER_NUMBER_FAILED_SCREEN:
		case AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED:
		case AST_PRES_PROHIB_USER_NUMBER_PASSED_SCREEN:
			ast_debug (2, "[%s] callingpres: %s\n", PVT_ID(pvt), ast_describe_caller_presentation (clir));
			res = 1;
			break;

		default:
			ast_log (LOG_WARNING, "[%s] Unsupported callingpres: %d\n", PVT_ID(pvt), clir);
			if ((clir & AST_PRES_RESTRICTION) != AST_PRES_ALLOWED)
			{
				res = 0;
			}
			else
			{
				res = 2;
			}
			break;
	}

	return res;
}

typedef int (*at_cmd_f)(struct cpvt*, const char*, const char*, unsigned, int, const char*, size_t);

void free_pvt(struct pvt *pvt)
{
	ast_mutex_unlock(&pvt->lock);
}
struct pvt *get_pvt(const char *dev_name, int online)
{
	struct pvt *pvt;
	pvt = find_device_ext(dev_name);
	if (pvt) {
		if (pvt->connected && (!online || (pvt->initialized && pvt->gsm_registered))) {
			return pvt;
		}
		free_pvt(pvt);
	}
	chan_dongle_err = E_DEVICE_DISCONNECTED;
	return NULL;
}

#/* */
EXPORT_DEF int send_ussd(const char *dev_name, const char *ussd)
{
	if (!is_valid_ussd_string(ussd)) {
		chan_dongle_err = E_INVALID_USSD;
		return -1;
	}
	
	struct pvt *pvt = get_pvt(dev_name, 1);
	if (!pvt) {
		return -1;
	}
	int res = at_enqueue_ussd(&pvt->sys_chan, ussd);
	free_pvt(pvt);
	return res;
}

#/* */
EXPORT_DEF int send_sms(const char *dev_name, const char *number, const char *message, const char *validity, const char *report, const char *payload, size_t payload_len)
{
	if (!is_valid_phone_number(number)) {
		chan_dongle_err = E_INVALID_PHONE_NUMBER;
		return -1;
	}

	int val = 0;
	if (validity) {
		val = strtol(validity, NULL, 10);
		val = val <= 0 ? 0 : val;
	}

	int srr = !report ? 0 : ast_true(report);
	
	struct pvt *pvt = get_pvt(dev_name, 1);
	if (!pvt) {
		return -1;
	}
	int res = at_enqueue_sms(&pvt->sys_chan, number, message, val, srr, payload, payload_len);
	free_pvt(pvt);
	return res;
}

#/* */
EXPORT_DEF int send_reset(const char *dev_name)
{
	struct pvt *pvt = get_pvt(dev_name, 0);
	if (!pvt) {
		return -1;
	}
	int res = at_enqueue_reset(&pvt->sys_chan);
	free_pvt(pvt);
	return res;
}

#/* */
EXPORT_DEF int send_ccwa_set(const char *dev_name, call_waiting_t enable)
{
	struct pvt *pvt = get_pvt(dev_name, 1);
	if (!pvt) {
		return -1;
	}
	int res = at_enqueue_set_ccwa(&pvt->sys_chan, enable);
	free_pvt(pvt);
	return res;
}

#/* */
EXPORT_DEF int send_at_command(const char *dev_name, const char *command)
{
	struct pvt *pvt = get_pvt(dev_name, 0);
	if (!pvt) {
		return -1;
	}
	int res = at_enqueue_user_cmd(&pvt->sys_chan, command);
	free_pvt(pvt);
	return res;
}

EXPORT_DEF int schedule_restart_event(dev_state_t event, restate_time_t when, const char *dev_name)
{
	struct pvt *pvt = find_device(dev_name);

	if (pvt) {
		pvt->desired_state = event;
		pvt->restart_time = when;

		pvt_try_restate(pvt);
		ast_mutex_unlock(&pvt->lock);
	} else {
		chan_dongle_err = E_DEVICE_NOT_FOUND;
		return -1;
	}

	return 0;
}
