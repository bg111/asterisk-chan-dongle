#ifndef CHAN_DONGLE_AST_COMPAT_H_INCLUDED
#define CHAN_DONGLE_AST_COMPAT_H_INCLUDED

#ifndef ASTERISK_VERSION_NUM
#error ASTERISK_VERSION_NUM is not set. Please re-run configure, \
	prefixed like this: CPPFLAGS=-DASTERISK_VERSION_NUM=100501 \
	./configure (where 100501 is asterisk version 10.5.1).
#endif

#include <asterisk/channel.h>

/* Asterisk 11+ channel opaqification */
#if ASTERISK_VERSION_NUM < 110000 /* 11- */

/* channel->name */
static inline const char *ast_channel_name(const struct ast_channel *chan) { return chan->name; }

/* channel->_state / channel->state */
static inline enum ast_channel_state ast_channel_state(const struct ast_channel *chan) { return chan->_state; }

/* channel->fdno */
static inline int ast_channel_fdno(const struct ast_channel *chan) { return chan->fdno; }

/* channel->tech */
static inline const struct ast_channel_tech *ast_channel_tech(const struct ast_channel *chan) { return chan->tech; }
static inline void ast_channel_tech_set(struct ast_channel *chan, const struct ast_channel_tech *value) { chan->tech = value; }

static inline void *ast_channel_tech_pvt(const struct ast_channel *chan) { return chan->tech_pvt; }
static inline void ast_channel_tech_pvt_set(struct ast_channel *chan, void *value) { chan->tech_pvt = value; }

/* channel->rings */
static inline int ast_channel_rings(const struct ast_channel *chan) { return chan->rings; }
static inline void ast_channel_rings_set(struct ast_channel *chan, int value) { chan->rings = value; }

/* ast_string_field_set(channel, language, ...) */
static inline void ast_channel_language_set(struct ast_channel *chan, const char *value) { ast_string_field_set(chan, language, value); }

/* channel->connected */
static inline struct ast_party_connected_line *ast_channel_connected(struct ast_channel *chan) { return &chan->connected; }

/* channel->linkedid */
static inline const char *ast_channel_linkedid(const struct ast_channel *chan) { return chan->linkedid; }

/* channel->hangupcause */
static inline void ast_channel_hangupcause_set(struct ast_channel *chan, int value) { chan->hangupcause = value; }

#if ASTERISK_VERSION_NUM >= 100000 /* 10+ */
/* channel->*format* */
static inline struct ast_format_cap *ast_channel_nativeformats(const struct ast_channel *chan) { return chan->nativeformats; }
static inline struct ast_format ast_channel_readformat(const struct ast_channel *chan) { return chan->readformat; }
static inline struct ast_format ast_channel_writeformat(const struct ast_channel *chan) { return chan->writeformat; }
#endif /* ^10+ */

#endif /* ^11- */

#endif /* CHAN_DONGLE_AST_COMPAT_H_INCLUDED */
