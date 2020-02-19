/*
   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/

#ifndef CHAN_DONGLE_SMSDB_H_INCLUDED
#define CHAN_DONGLE_SMSDB_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

EXPORT_DECL int smsdb_init();
EXPORT_DECL int smsdb_put(const char *id, const char *addr, int ref, int parts, int order, const char *msg, char *out);
EXPORT_DECL int smsdb_outgoing_get(const char *id);

#endif
