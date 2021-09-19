/*
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief SMSdb
 *
 * \author Max von Buelow <max@m9x.de>
 * \author Mark Spencer <markster@digium.com>
 *
 * The original code is from the astdb prat of the Asterisk project.
 */

#include "asterisk.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sqlite3.h>

#include "asterisk/app.h"
#include "asterisk/utils.h"

#include "smsdb.h"
#include "chan_dongle.h"

#define MAX_DB_FIELD 256
AST_MUTEX_DEFINE_STATIC(dblock);
static sqlite3 *smsdb;

#define DEFINE_SQL_STATEMENT(stmt,sql) static sqlite3_stmt *stmt; \
	const char stmt##_sql[] = sql;
DEFINE_SQL_STATEMENT(get_full_message_stmt, "SELECT message FROM incoming WHERE key = ? ORDER BY seqorder")
DEFINE_SQL_STATEMENT(put_message_stmt, "INSERT OR REPLACE INTO incoming (key, seqorder, expiration, message) VALUES (?, ?, datetime(julianday(CURRENT_TIMESTAMP) + ? / 86400.0), ?)")
DEFINE_SQL_STATEMENT(clear_messages_stmt, "DELETE FROM incoming WHERE key = ?")
DEFINE_SQL_STATEMENT(purge_messages_stmt, "DELETE FROM incoming WHERE expiration < CURRENT_TIMESTAMP")
DEFINE_SQL_STATEMENT(get_cnt_stmt, "SELECT COUNT(seqorder) FROM incoming WHERE key = ?")
DEFINE_SQL_STATEMENT(create_incoming_stmt, "CREATE TABLE IF NOT EXISTS incoming (key VARCHAR(256), seqorder INTEGER, expiration TIMESTAMP DEFAULT CURRENT_TIMESTAMP, message VARCHAR(256), PRIMARY KEY(key, seqorder))")
DEFINE_SQL_STATEMENT(create_index_stmt, "CREATE INDEX IF NOT EXISTS incoming_key ON incoming(key)")
DEFINE_SQL_STATEMENT(create_outgoingref_stmt, "CREATE TABLE IF NOT EXISTS outgoing_ref (key VARCHAR(256), refid INTEGER, PRIMARY KEY(key))") // key: IMSI/DEST_ADDR
DEFINE_SQL_STATEMENT(create_outgoingmsg_stmt, "CREATE TABLE IF NOT EXISTS outgoing_msg (dev VARCHAR(256), dst VARCHAR(255), cnt INTEGER, expiration TIMESTAMP, srr BOOLEAN, payload BLOB)")
DEFINE_SQL_STATEMENT(create_outgoingpart_stmt, "CREATE TABLE IF NOT EXISTS outgoing_part (key VARCHAR(256), msg INTEGER, status INTEGER, PRIMARY KEY(key))") // key: IMSI/DEST_ADDR/MR
DEFINE_SQL_STATEMENT(create_outgoingmsg_index_stmt, "CREATE INDEX IF NOT EXISTS outgoing_part_msg ON outgoing_part(msg)")
DEFINE_SQL_STATEMENT(ins_outgoingref_stmt, "INSERT INTO outgoing_ref (refid, key) VALUES (?, ?)") // This have to be the same order as set_outgoingref_stmt
DEFINE_SQL_STATEMENT(set_outgoingref_stmt, "UPDATE outgoing_ref SET refid = ? WHERE key = ?")
DEFINE_SQL_STATEMENT(get_outgoingref_stmt, "SELECT refid FROM outgoing_ref WHERE key = ?")
DEFINE_SQL_STATEMENT(put_outgoingmsg_stmt, "INSERT INTO outgoing_msg (dev, dst, cnt, expiration, srr, payload) VALUES (?, ?, ?, datetime(julianday(CURRENT_TIMESTAMP) + ? / 86400.0), ?, ?)")
DEFINE_SQL_STATEMENT(put_outgoingpart_stmt, "INSERT INTO outgoing_part (key, msg, status) VALUES (?, ?, NULL)")
DEFINE_SQL_STATEMENT(del_outgoingmsg_stmt, "DELETE FROM outgoing_msg WHERE rowid = ?")
DEFINE_SQL_STATEMENT(del_outgoingpart_stmt, "DELETE FROM outgoing_part WHERE msg = ?")
DEFINE_SQL_STATEMENT(get_outgoingmsg_stmt, "SELECT dev, dst, srr FROM outgoing_msg WHERE rowid = ?")
DEFINE_SQL_STATEMENT(set_outgoingpart_stmt, "UPDATE outgoing_part SET status = ? WHERE rowid = ?")
DEFINE_SQL_STATEMENT(get_outgoingpart_stmt, "SELECT rowid, msg FROM outgoing_part WHERE key = ?")
DEFINE_SQL_STATEMENT(cnt_outgoingpart_stmt, "SELECT m.cnt, (SELECT COUNT(p.rowid) FROM outgoing_part p WHERE p.msg = m.rowid AND (p.status & 64 != 0 OR p.status & 32 = 0)) FROM outgoing_msg m WHERE m.rowid = ?") // count all failed and completed messages; don't count messages without delivery notification and temporary failed ones
DEFINE_SQL_STATEMENT(cnt_all_outgoingpart_stmt, "SELECT m.cnt, (SELECT COUNT(p.rowid) FROM outgoing_part p WHERE p.msg = m.rowid) FROM outgoing_msg m WHERE m.rowid = ?")
DEFINE_SQL_STATEMENT(get_payload_stmt, "SELECT payload, dst FROM outgoing_msg WHERE rowid = ?")
DEFINE_SQL_STATEMENT(get_all_status_stmt, "SELECT status FROM outgoing_part WHERE msg = ? ORDER BY rowid")
DEFINE_SQL_STATEMENT(get_expired_stmt, "SELECT rowid, payload, dst FROM outgoing_msg WHERE expiration < CURRENT_TIMESTAMP LIMIT 1") // only fetch one expired row to balance the load of each transaction

static int init_stmt(sqlite3_stmt **stmt, const char *sql, size_t len)
{
	ast_mutex_lock(&dblock);
	if (sqlite3_prepare(smsdb, sql, len, stmt, NULL) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't prepare statement '%s': %s\n", sql, sqlite3_errmsg(smsdb));
		ast_mutex_unlock(&dblock);
		return -1;
	}
	ast_mutex_unlock(&dblock);

	return 0;
}

/*! \internal
 * \brief Clean up the prepared SQLite3 statement
 * \note dblock should already be locked prior to calling this method
 */
static int clean_stmt(sqlite3_stmt **stmt, const char *sql)
{
	if (sqlite3_finalize(*stmt) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't finalize statement '%s': %s\n", sql, sqlite3_errmsg(smsdb));
		*stmt = NULL;
		return -1;
	}
	*stmt = NULL;
	return 0;
}

/*! \internal
 * \brief Clean up all prepared SQLite3 statements
 * \note dblock should already be locked prior to calling this method
 */
static void clean_statements(void)
{
	clean_stmt(&get_full_message_stmt, get_full_message_stmt_sql);
	clean_stmt(&put_message_stmt, put_message_stmt_sql);
	clean_stmt(&clear_messages_stmt, clear_messages_stmt_sql);
	clean_stmt(&purge_messages_stmt, purge_messages_stmt_sql);
	clean_stmt(&get_cnt_stmt, get_cnt_stmt_sql);
	clean_stmt(&create_incoming_stmt, create_incoming_stmt_sql);
	clean_stmt(&create_index_stmt, create_index_stmt_sql);
	clean_stmt(&create_outgoingref_stmt, create_outgoingref_stmt_sql);
	clean_stmt(&create_outgoingmsg_stmt, create_outgoingmsg_stmt_sql);
	clean_stmt(&create_outgoingpart_stmt, create_outgoingpart_stmt_sql);
	clean_stmt(&create_outgoingmsg_index_stmt, create_outgoingmsg_index_stmt_sql);
	clean_stmt(&ins_outgoingref_stmt, ins_outgoingref_stmt_sql);
	clean_stmt(&set_outgoingref_stmt, set_outgoingref_stmt_sql);
	clean_stmt(&get_outgoingref_stmt, get_outgoingref_stmt_sql);
	clean_stmt(&put_outgoingmsg_stmt, put_outgoingmsg_stmt_sql);
	clean_stmt(&put_outgoingpart_stmt, put_outgoingpart_stmt_sql);
	clean_stmt(&del_outgoingmsg_stmt, del_outgoingmsg_stmt_sql);
	clean_stmt(&del_outgoingpart_stmt, del_outgoingpart_stmt_sql);
	clean_stmt(&get_outgoingmsg_stmt, get_outgoingmsg_stmt_sql);
	clean_stmt(&set_outgoingpart_stmt, set_outgoingpart_stmt_sql);
	clean_stmt(&get_outgoingpart_stmt, get_outgoingpart_stmt_sql);
	clean_stmt(&cnt_outgoingpart_stmt, cnt_outgoingpart_stmt_sql);
	clean_stmt(&cnt_all_outgoingpart_stmt, cnt_all_outgoingpart_stmt_sql);
	clean_stmt(&get_payload_stmt, get_payload_stmt_sql);
	clean_stmt(&get_all_status_stmt, get_all_status_stmt_sql);
	clean_stmt(&get_expired_stmt, get_expired_stmt_sql);
}

static int init_statements(void)
{
	/* Don't initialize create_smsdb_statement here as the smsdb table needs to exist
	 * brefore these statements can be initialized */
	return init_stmt(&get_full_message_stmt, get_full_message_stmt_sql, sizeof(get_full_message_stmt_sql))
	|| init_stmt(&put_message_stmt, put_message_stmt_sql, sizeof(put_message_stmt_sql))
	|| init_stmt(&clear_messages_stmt, clear_messages_stmt_sql, sizeof(clear_messages_stmt_sql))
	|| init_stmt(&purge_messages_stmt, purge_messages_stmt_sql, sizeof(purge_messages_stmt_sql))
	|| init_stmt(&get_cnt_stmt, get_cnt_stmt_sql, sizeof(get_cnt_stmt_sql))
	|| init_stmt(&ins_outgoingref_stmt, ins_outgoingref_stmt_sql, sizeof(ins_outgoingref_stmt_sql))
	|| init_stmt(&set_outgoingref_stmt, set_outgoingref_stmt_sql, sizeof(set_outgoingref_stmt_sql))
	|| init_stmt(&get_outgoingref_stmt, get_outgoingref_stmt_sql, sizeof(get_outgoingref_stmt_sql))
	|| init_stmt(&put_outgoingmsg_stmt, put_outgoingmsg_stmt_sql, sizeof(put_outgoingmsg_stmt_sql))
	|| init_stmt(&put_outgoingpart_stmt, put_outgoingpart_stmt_sql, sizeof(put_outgoingpart_stmt_sql))
	|| init_stmt(&del_outgoingmsg_stmt, del_outgoingmsg_stmt_sql, sizeof(del_outgoingmsg_stmt_sql))
	|| init_stmt(&del_outgoingpart_stmt, del_outgoingpart_stmt_sql, sizeof(del_outgoingpart_stmt_sql))
	|| init_stmt(&get_outgoingmsg_stmt, get_outgoingmsg_stmt_sql, sizeof(get_outgoingmsg_stmt_sql))
	|| init_stmt(&get_outgoingpart_stmt, get_outgoingpart_stmt_sql, sizeof(get_outgoingpart_stmt_sql))
	|| init_stmt(&set_outgoingpart_stmt, set_outgoingpart_stmt_sql, sizeof(set_outgoingpart_stmt_sql))
	|| init_stmt(&cnt_outgoingpart_stmt, cnt_outgoingpart_stmt_sql, sizeof(cnt_outgoingpart_stmt_sql))
	|| init_stmt(&cnt_all_outgoingpart_stmt, cnt_all_outgoingpart_stmt_sql, sizeof(cnt_all_outgoingpart_stmt_sql))
	|| init_stmt(&get_payload_stmt, get_payload_stmt_sql, sizeof(get_payload_stmt_sql))
	|| init_stmt(&get_all_status_stmt, get_all_status_stmt_sql, sizeof(get_all_status_stmt_sql))
	|| init_stmt(&get_expired_stmt, get_expired_stmt_sql, sizeof(get_expired_stmt_sql));
}

static int db_create_smsdb(void)
{
	int res = 0;

	if (!create_incoming_stmt) {
		init_stmt(&create_incoming_stmt, create_incoming_stmt_sql, sizeof(create_incoming_stmt_sql));
	}
	ast_mutex_lock(&dblock);
	if (sqlite3_step(create_incoming_stmt) != SQLITE_DONE) {
		ast_log(LOG_WARNING, "Couldn't create smsdb table: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}
	sqlite3_reset(create_incoming_stmt);
	ast_mutex_unlock(&dblock);

	if (!create_index_stmt) {
		init_stmt(&create_index_stmt, create_index_stmt_sql, sizeof(create_index_stmt_sql));
	}
	ast_mutex_lock(&dblock);
	if (sqlite3_step(create_index_stmt) != SQLITE_DONE) {
		ast_log(LOG_WARNING, "Couldn't create smsdb index: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}
	sqlite3_reset(create_index_stmt);
	ast_mutex_unlock(&dblock);

	if (!create_outgoingref_stmt) {
		init_stmt(&create_outgoingref_stmt, create_outgoingref_stmt_sql, sizeof(create_outgoingref_stmt_sql));
	}
	ast_mutex_lock(&dblock);
	if (sqlite3_step(create_outgoingref_stmt) != SQLITE_DONE) {
		ast_log(LOG_WARNING, "Couldn't create smsdb outgoing table: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}
	sqlite3_reset(create_outgoingref_stmt);
	ast_mutex_unlock(&dblock);

	if (!create_outgoingmsg_stmt) {
		init_stmt(&create_outgoingmsg_stmt, create_outgoingmsg_stmt_sql, sizeof(create_outgoingmsg_stmt_sql));
	}
	ast_mutex_lock(&dblock);
	if (sqlite3_step(create_outgoingmsg_stmt) != SQLITE_DONE) {
		ast_log(LOG_WARNING, "Couldn't create smsdb outgoing table: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}
	sqlite3_reset(create_outgoingmsg_stmt);
	ast_mutex_unlock(&dblock);

	if (!create_outgoingpart_stmt) {
		init_stmt(&create_outgoingpart_stmt, create_outgoingpart_stmt_sql, sizeof(create_outgoingpart_stmt_sql));
	}
	ast_mutex_lock(&dblock);
	if (sqlite3_step(create_outgoingpart_stmt) != SQLITE_DONE) {
		ast_log(LOG_WARNING, "Couldn't create smsdb outgoing table: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}
	sqlite3_reset(create_outgoingpart_stmt);
	ast_mutex_unlock(&dblock);

	if (!create_outgoingmsg_index_stmt) {
		init_stmt(&create_outgoingmsg_index_stmt, create_outgoingmsg_index_stmt_sql, sizeof(create_outgoingmsg_index_stmt_sql));
	}
	ast_mutex_lock(&dblock);
	if (sqlite3_step(create_outgoingmsg_index_stmt) != SQLITE_DONE) {
		ast_log(LOG_WARNING, "Couldn't create smsdb outgoing index: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}
	sqlite3_reset(create_outgoingmsg_index_stmt);
	ast_mutex_unlock(&dblock);
	return res;
}

static int db_open(void)
{
	char *dbname;
	if (!(dbname = ast_alloca(strlen(CONF_GLOBAL(sms_db)) + sizeof(".sqlite3")))) {
		return -1;
	}
	strcpy(dbname, CONF_GLOBAL(sms_db));
	strcat(dbname, ".sqlite3");

	ast_mutex_lock(&dblock);
	if (sqlite3_open(dbname, &smsdb) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Unable to open Asterisk database '%s': %s\n", dbname, sqlite3_errmsg(smsdb));
		sqlite3_close(smsdb);
		ast_mutex_unlock(&dblock);
		return -1;
	}

	ast_mutex_unlock(&dblock);

	return 0;
}

static int db_init()
{
	if (smsdb) {
		return 0;
	}

	if (db_open() || db_create_smsdb() || init_statements()) {
		return -1;
	}

	return 0;
}

/* We purposely don't lock around the sqlite3 call because the transaction
 * calls will be called with the database lock held. For any other use, make
 * sure to take the dblock yourself. */
static int db_execute_sql(const char *sql, int (*callback)(void *, int, char **, char **), void *arg)
{
	char *errmsg = NULL;
	int res =0;

	if (sqlite3_exec(smsdb, sql, callback, arg, &errmsg) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Error executing SQL (%s): %s\n", sql, errmsg);
		sqlite3_free(errmsg);
		res = -1;
	}

	return res;
}

static int smsdb_begin_transaction(void)
{
	ast_mutex_lock(&dblock);
	int res = db_execute_sql("BEGIN TRANSACTION", NULL, NULL);
	return res;
}

static int smsdb_commit_transaction(void)
{
	int res = db_execute_sql("COMMIT", NULL, NULL);
	ast_mutex_unlock(&dblock);
	return res;
}

#if 0
static int smsdb_rollback_transaction(void)
{
	int res = db_execute_sql("ROLLBACK", NULL, NULL);
	ast_mutex_unlock(&dblock);
	return res;
}
#endif


/*!
 * \brief Adds a message part into the DB and returns the whole message into 'out' when the message is complete.
 * \param id -- Some ID for the device or so, e.g. the IMSI
 * \param addr -- The sender address
 * \param ref -- The reference ID
 * \param parts -- The total number of messages
 * \param order -- The current message number
 * \param msg -- The current message part
 * \param out -- Output: Only written if parts == cnt
 * \retval <=0 Error
 * \retval >0 Current number of messages in the DB
 */
EXPORT_DEF int smsdb_put(const char *id, const char *addr, int ref, int parts, int order, const char *msg, char *out)
{
	const char *part;
	char fullkey[MAX_DB_FIELD + 1];
	int fullkey_len;
	int res = 0;
	int ttl = CONF_GLOBAL(csms_ttl);

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "%s/%s/%d/%d", id, addr, ref, parts);
	if (fullkey_len < 0) {
		ast_log(LOG_ERROR, "Key length must be less than %zu bytes\n", sizeof(fullkey));
		return -1;
	}

	smsdb_begin_transaction();
	if (sqlite3_bind_text(put_message_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_int(put_message_stmt, 2, order) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind order to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_int(put_message_stmt, 3, ttl) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind TTL to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_text(put_message_stmt, 4, msg, -1, SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind msg to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(put_message_stmt) != SQLITE_DONE) {
		ast_log(LOG_WARNING, "Couldn't execute statement: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}

	sqlite3_reset(put_message_stmt);

	if (sqlite3_bind_text(get_cnt_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(get_cnt_stmt) != SQLITE_ROW) {
		ast_debug(1, "Unable to find key '%s'\n", fullkey);
		res = -1;
	}
	res = sqlite3_column_int(get_cnt_stmt, 0);

	sqlite3_reset(get_cnt_stmt);

	if (res != -1 && res == parts) {
		if (sqlite3_bind_text(get_full_message_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else while (sqlite3_step(get_full_message_stmt) == SQLITE_ROW) {
			part = (const char*)sqlite3_column_text(get_full_message_stmt, 0);
			int partlen = sqlite3_column_bytes(get_full_message_stmt, 0);
			if (!part) {
				ast_log(LOG_WARNING, "Couldn't get value\n");
				res = -1;
				break;
			}
			out = stpncpy(out, part, partlen);
		}
		out[0] = '\0';
		sqlite3_reset(get_full_message_stmt);

		if (res >= 0) {
			if (sqlite3_bind_text(clear_messages_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
				ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
				res = -1;
			} else if (sqlite3_step(clear_messages_stmt) != SQLITE_DONE) {
				ast_debug(1, "Unable to find key '%s'; Ignoring\n", fullkey);
			}
			sqlite3_reset(clear_messages_stmt);
		}
	}

	smsdb_commit_transaction();

	return res;
}

#if 0
static int smsdb_purge()
{
	int res = 0;

	if (sqlite3_step(purge_messages_stmt) != SQLITE_DONE) {
		res = -1;
	}
	sqlite3_reset(purge_messages_stmt);

	return res;
}
#endif

EXPORT_DEF int smsdb_get_refid(const char *id, const char *addr)
{
	int res = 0;

	smsdb_begin_transaction();

	char fullkey[MAX_DB_FIELD + 1];
	int fullkey_len;

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "%s/%s", id, addr);
	if (fullkey_len < 0) {
		ast_log(LOG_ERROR, "Key length must be less than %zu bytes\n", sizeof(fullkey));
		return -1;
	}

	int use_insert = 0;
	if (sqlite3_bind_text(get_outgoingref_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(get_outgoingref_stmt) != SQLITE_ROW) {
		res = 255;
		use_insert = 1;
	} else {
		res = sqlite3_column_int(get_outgoingref_stmt, 0);
	}
	sqlite3_reset(get_outgoingref_stmt);

	if (res >= 0) {
		++res;
		if (res >= 256) res = 0;
		sqlite3_stmt *stmt = use_insert ? ins_outgoingref_stmt : set_outgoingref_stmt;
		if (sqlite3_bind_int(stmt, 1, res) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind refid to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_bind_text(stmt, 2, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_step(stmt) != SQLITE_DONE) {
			res = -1;
		}
		sqlite3_reset(stmt);
	}

	smsdb_commit_transaction();

	return res;
}
EXPORT_DEF int smsdb_outgoing_add(const char *id, const char *addr, int cnt, int ttl, int srr, const char *payload, size_t len)
{
	int res = 0;

	smsdb_begin_transaction();

	if (sqlite3_bind_text(put_outgoingmsg_stmt, 1, id, strlen(id), SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind dev to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_text(put_outgoingmsg_stmt, 2, addr, strlen(addr), SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind destination address to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_int(put_outgoingmsg_stmt, 3, cnt) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind count to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_int(put_outgoingmsg_stmt, 4, ttl) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind TTL to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_int(put_outgoingmsg_stmt, 5, srr) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind SRR to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_blob(put_outgoingmsg_stmt, 6, payload, len > SMSDB_PAYLOAD_MAX_LEN ? SMSDB_PAYLOAD_MAX_LEN : len, SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind payload to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(put_outgoingmsg_stmt) != SQLITE_DONE) {
		res = -1;
	} else {
		res = sqlite3_last_insert_rowid(smsdb);
	}
	sqlite3_reset(put_outgoingmsg_stmt);

	smsdb_commit_transaction();

	return res;
}

static int smsdb_outgoing_clear_nolock(int uid)
{
	int res = 0;

	if (sqlite3_bind_int(del_outgoingmsg_stmt, 1, uid) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind UID to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(del_outgoingmsg_stmt) != SQLITE_DONE) {
		res = -1;
	}
	sqlite3_reset(del_outgoingmsg_stmt);

	if (sqlite3_bind_int(del_outgoingpart_stmt, 1, uid) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind UID to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(del_outgoingpart_stmt) != SQLITE_DONE) {
		res = -1;
	}
	sqlite3_reset(del_outgoingpart_stmt);
	
	return res;
}
EXPORT_DEF ssize_t smsdb_outgoing_clear(int uid, char *dst, char *payload)
{
	int res = 0;
	smsdb_begin_transaction();

	if (sqlite3_bind_int(get_payload_stmt, 1, uid) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(get_payload_stmt) != SQLITE_ROW) {
		res = -1;
	} else {
		strcpy(dst, (const char*)sqlite3_column_text(get_payload_stmt, 1));
		res = sqlite3_column_bytes(get_payload_stmt, 0);
		res = res > SMSDB_PAYLOAD_MAX_LEN ? SMSDB_PAYLOAD_MAX_LEN : res;
		memcpy(payload, sqlite3_column_blob(get_payload_stmt, 0), res);
	}
	sqlite3_reset(get_payload_stmt);

	if (res != -1 && smsdb_outgoing_clear_nolock(uid) < 0) {
		res = -1;
	}

	smsdb_commit_transaction();

	return res;
}
EXPORT_DEF ssize_t smsdb_outgoing_part_put(int uid, int refid, char *dst, char *payload)
{
	int res = 0;
	char fullkey[MAX_DB_FIELD + 1];
	int fullkey_len;
	int srr = 0, cnt, cur;

	smsdb_begin_transaction();

	if (sqlite3_bind_int(get_outgoingmsg_stmt, 1, uid) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind UID to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(get_outgoingmsg_stmt) != SQLITE_ROW) {
		res = -2;
	} else {
		const char *dev = (const char*)sqlite3_column_text(get_outgoingmsg_stmt, 0);
		const char *dst = (const char*)sqlite3_column_text(get_outgoingmsg_stmt, 1);
		srr = sqlite3_column_int(get_outgoingmsg_stmt, 2);

		fullkey_len = snprintf(fullkey, sizeof(fullkey), "%s/%s/%d", dev, dst, refid);
		if (fullkey_len < 0) {
			ast_log(LOG_ERROR, "Key length must be less than %zu bytes\n", sizeof(fullkey));
			return -1;
		}
	}
	sqlite3_reset(get_outgoingmsg_stmt);

	if (res >= 0) {
		if (sqlite3_bind_text(put_outgoingpart_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_bind_int(put_outgoingpart_stmt, 2, uid) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind UID to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_step(put_outgoingpart_stmt) != SQLITE_DONE) {
			res = -1;
		}
		sqlite3_reset(put_outgoingpart_stmt);
	}

	if (srr) {
		res = -2;
	}

	// if no status report is requested, just count successfully inserted parts and return payload if the counter reached the number of parts
	if (res >= 0) {
		if (sqlite3_bind_int(cnt_all_outgoingpart_stmt, 1, uid) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_step(cnt_all_outgoingpart_stmt) != SQLITE_ROW) {
			res = -1;
		} else {
			cur = sqlite3_column_int(cnt_all_outgoingpart_stmt, 0);
			cnt = sqlite3_column_int(cnt_all_outgoingpart_stmt, 1);
		}
		sqlite3_reset(cnt_all_outgoingpart_stmt);
	}

	if (res >= 0 && cur != cnt) {
		res = -2;
	}

	// get payload
	if (res >= 0) {
		if (sqlite3_bind_int(get_payload_stmt, 1, uid) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_step(get_payload_stmt) != SQLITE_ROW) {
			res = -1;
		} else {
			strcpy(dst, (const char*)sqlite3_column_text(get_payload_stmt, 1));
			res = sqlite3_column_bytes(get_payload_stmt, 0);
			res = res > SMSDB_PAYLOAD_MAX_LEN ? SMSDB_PAYLOAD_MAX_LEN : res;
			memcpy(payload, sqlite3_column_blob(get_payload_stmt, 0), res);
		}
		sqlite3_reset(get_payload_stmt);
	}

	// clear if everything is finished
	if (res >= 0 && smsdb_outgoing_clear_nolock(uid) < 0) {
		res = -1;
	}


	smsdb_commit_transaction();

	return res;
}

EXPORT_DEF ssize_t smsdb_outgoing_part_status(const char *id, const char *addr, int mr, int st, int *status_all, char *payload)
{
	char fullkey[MAX_DB_FIELD + 1];
	int fullkey_len;
	int res = 0, partid, uid, cur, cnt;

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "%s/%s/%d", id, addr, mr);
	if (fullkey_len < 0) {
		ast_log(LOG_ERROR, "Key length must be less than %zu bytes\n", sizeof(fullkey));
		return -1;
	}

	smsdb_begin_transaction();

	if (sqlite3_bind_text(get_outgoingpart_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(get_outgoingpart_stmt) != SQLITE_ROW) {
		res = -1;
	} else {
		partid = sqlite3_column_int(get_outgoingpart_stmt, 0);
		uid = sqlite3_column_int(get_outgoingpart_stmt, 1);
	}
	sqlite3_reset(get_outgoingpart_stmt);

	// set status
	if (res >= 0) {
		if (sqlite3_bind_int(set_outgoingpart_stmt, 1, st) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind status to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_bind_int(set_outgoingpart_stmt, 2, partid) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind ID to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_step(set_outgoingpart_stmt) != SQLITE_DONE) {
			res = -1;
		}
		sqlite3_reset(set_outgoingpart_stmt);
	}

	// get count
	if (res >= 0) {
		if (sqlite3_bind_int(cnt_outgoingpart_stmt, 1, uid) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_step(cnt_outgoingpart_stmt) != SQLITE_ROW) {
			res = -1;
		} else {
			cur = sqlite3_column_int(cnt_outgoingpart_stmt, 0);
			cnt = sqlite3_column_int(cnt_outgoingpart_stmt, 1);
		}
		sqlite3_reset(cnt_outgoingpart_stmt);
	}

	if (res != -1 && cur != cnt) {
		res = -2;
	}

	// get status array
	if (res >= 0) {
		int i = 0;
		if (sqlite3_bind_int(get_all_status_stmt, 1, uid) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else while (sqlite3_step(get_all_status_stmt) == SQLITE_ROW) {
			status_all[i++] = sqlite3_column_int(get_all_status_stmt, 0);
		}
		status_all[i] = -1;
		sqlite3_reset(get_all_status_stmt);
	}

	// get payload
	if (res >= 0) {
		if (sqlite3_bind_int(get_payload_stmt, 1, uid) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_step(get_payload_stmt) != SQLITE_ROW) {
			res = -1;
		} else {
			res = sqlite3_column_bytes(get_payload_stmt, 0);
			res = res > SMSDB_PAYLOAD_MAX_LEN ? SMSDB_PAYLOAD_MAX_LEN : res;
			memcpy(payload, sqlite3_column_blob(get_payload_stmt, 0), res);
		}
		sqlite3_reset(get_payload_stmt);
	}

	// clear if everything is finished
	if (res >= 0 && smsdb_outgoing_clear_nolock(uid) < 0) {
		res = -1;
	}

	smsdb_commit_transaction();

	return res;
}

EXPORT_DEF ssize_t smsdb_outgoing_purge_one(char *dst, char *payload)
{
	int res = -1, uid;

	smsdb_begin_transaction();

	if (sqlite3_step(get_expired_stmt) != SQLITE_ROW) {
		res = -1;
	} else {
		uid = sqlite3_column_int(get_expired_stmt, 0);

		strcpy(dst, (const char*)sqlite3_column_text(get_expired_stmt, 2));
		res = sqlite3_column_bytes(get_expired_stmt, 1);
		res = res > SMSDB_PAYLOAD_MAX_LEN ? SMSDB_PAYLOAD_MAX_LEN : res;
		memcpy(payload, sqlite3_column_blob(get_expired_stmt, 1), res);
	}
	sqlite3_reset(get_expired_stmt);

	if (res != -1 && smsdb_outgoing_clear_nolock(uid) < 0) {
		res = -1;
	}

	smsdb_commit_transaction();

	return res;
}

/*!
 * \internal
 * \brief Clean up resources on Asterisk shutdown
 */
EXPORT_DEF void smsdb_atexit()
{
	ast_mutex_lock(&dblock);
	clean_statements();
	if (sqlite3_close(smsdb) == SQLITE_OK) {
		smsdb = NULL;
	}
	ast_mutex_unlock(&dblock);
}

EXPORT_DEF int smsdb_init()
{
	if (db_init()) {
		return -1;
	}

	return 0;
}
