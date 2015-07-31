#ifndef CHAN_DONGLE_AT_LOG_H_INCLUDED
#define CHAN_DONGLE_AT_LOG_H_INCLUDED

#include <sys/time.h>			/* struct timeval */

#include <asterisk.h>
#include <asterisk/linkedlists.h>	/* AST_LIST_ENTRY */

#include "chan_dongle.h"
#include "at_log.h"
#include "export.h"			/* EXPORT_DECL EXPORT_DEF */


EXPORT_DECL void at_logv(struct pvt *pvt, const char *prefix, const struct iovec *iov, size_t iov_count);
EXPORT_DECL void at_log(struct pvt *pvt, const char *prefix, const void *buf, size_t len);

#endif

