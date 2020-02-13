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
static ast_cond_t dbcond;
static sqlite3 *smsdb;
static pthread_t syncthread;
static int doexit;
static int dosync;

static void db_sync(void);

#define DEFINE_SQL_STATEMENT(stmt,sql) static sqlite3_stmt *stmt; \
	const char stmt##_sql[] = sql;
DEFINE_SQL_STATEMENT(get_full_message_stmt, "SELECT message FROM incoming WHERE key = ? ORDER BY seqorder")
DEFINE_SQL_STATEMENT(put_message_stmt, "INSERT OR REPLACE INTO incoming (key, seqorder, message) VALUES (?, ?, ?)")
DEFINE_SQL_STATEMENT(clear_messages_stmt, "DELETE FROM incoming WHERE key = ?")
DEFINE_SQL_STATEMENT(purge_messages_stmt, "DELETE FROM incoming WHERE arrival < CURRENT_TIMESTAMP - ?")
DEFINE_SQL_STATEMENT(get_cnt_stmt, "SELECT COUNT(seqorder) FROM incoming WHERE key = ?")
DEFINE_SQL_STATEMENT(create_incoming_stmt, "CREATE TABLE IF NOT EXISTS incoming (key VARCHAR(256), seqorder INTEGER, arrival TIMESTAMP DEFAULT CURRENT_TIMESTAMP, message VARCHAR(256), PRIMARY KEY(key, seqorder))")
DEFINE_SQL_STATEMENT(create_index_stmt, "CREATE INDEX IF NOT EXISTS incoming_key ON incoming(key)")
DEFINE_SQL_STATEMENT(create_outgoing_stmt, "CREATE TABLE IF NOT EXISTS outgoing (key VARCHAR(256), refid INTEGER, PRIMARY KEY(key))")
DEFINE_SQL_STATEMENT(inc_outgoing_stmt, "INSERT INTO outgoing (key, refid) VALUES (?, 0) ON CONFLICT(key) DO UPDATE SET refid = CASE WHEN refid < 65535 THEN refid + 1 ELSE 0 END");
DEFINE_SQL_STATEMENT(get_outgoing_stmt, "SELECT refid FROM outgoing WHERE key = ?");

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
	clean_stmt(&create_outgoing_stmt, create_outgoing_stmt_sql);
	clean_stmt(&inc_outgoing_stmt, inc_outgoing_stmt_sql);
	clean_stmt(&get_outgoing_stmt, get_outgoing_stmt_sql);
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
	|| init_stmt(&inc_outgoing_stmt, inc_outgoing_stmt_sql, sizeof(inc_outgoing_stmt_sql))
	|| init_stmt(&get_outgoing_stmt, get_outgoing_stmt_sql, sizeof(get_outgoing_stmt_sql));
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

	if (!create_outgoing_stmt) {
		init_stmt(&create_outgoing_stmt, create_outgoing_stmt_sql, sizeof(create_outgoing_stmt_sql));
	}
	ast_mutex_lock(&dblock);
	if (sqlite3_step(create_outgoing_stmt) != SQLITE_DONE) {
		ast_log(LOG_WARNING, "Couldn't create smsdb outgoing table: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}
	sqlite3_reset(create_outgoing_stmt);

	db_sync();
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
	return db_execute_sql("BEGIN TRANSACTION", NULL, NULL);
}

static int smsdb_commit_transaction(void)
{
	return db_execute_sql("COMMIT", NULL, NULL);
}

static int smsdb_rollback_transaction(void)
{
	return db_execute_sql("ROLLBACK", NULL, NULL);
}


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
	char fullkey[MAX_DB_FIELD];
	size_t fullkey_len;
	int res = 0;

	if (strlen(id) + strlen(addr) + 5 + 3 + 3 >= sizeof(fullkey)) {
		ast_log(LOG_WARNING, "Key length must be less than %zu bytes\n", sizeof(fullkey));
		return -1;
	}

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "%s/%s/%d/%d", id, addr, ref, parts);

	ast_mutex_lock(&dblock);
	if (sqlite3_bind_text(put_message_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_bind_int(put_message_stmt, 2, order) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind order to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	}  else if (sqlite3_bind_text(put_message_stmt, 3, msg, -1, SQLITE_STATIC) != SQLITE_OK) {
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
			if (!part) {
				ast_log(LOG_WARNING, "Couldn't get value\n");
				res = -1;
				break;
			}
			out = stpcpy(out, part);
		}
		sqlite3_reset(get_full_message_stmt);

		if (res != -1) {
			if (sqlite3_bind_text(clear_messages_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
				ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
				res = -1;
			} else if (sqlite3_step(clear_messages_stmt) != SQLITE_DONE) {
				ast_debug(1, "Unable to find key '%s'; Ignoring\n", fullkey);
			}
			sqlite3_reset(clear_messages_stmt);
		}
	}

	db_sync();

	ast_mutex_unlock(&dblock);

	return res;
}

static int smsdb_purge(int ttl)
{
	int res = 0;

	if (sqlite3_bind_int(purge_messages_stmt, 1, ttl) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(purge_messages_stmt) != SQLITE_DONE) {
		res = -1;
	}
	sqlite3_reset(purge_messages_stmt);

	return res;
}

EXPORT_DEF int smsdb_outgoing_get(const char *id)
{
	int res = 0;

	ast_mutex_lock(&dblock);

	if (sqlite3_bind_text(inc_outgoing_stmt, 1, id, strlen(id), SQLITE_STATIC) != SQLITE_OK) {
		ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
		res = -1;
	} else if (sqlite3_step(inc_outgoing_stmt) != SQLITE_DONE) {
		res = -1;
	}
	sqlite3_reset(inc_outgoing_stmt);
	db_sync();

	if (res != -1) {
		if (sqlite3_bind_text(get_outgoing_stmt, 1, id, strlen(id), SQLITE_STATIC) != SQLITE_OK) {
			ast_log(LOG_WARNING, "Couldn't bind key to stmt: %s\n", sqlite3_errmsg(smsdb));
			res = -1;
		} else if (sqlite3_step(get_outgoing_stmt) != SQLITE_ROW) {
			ast_debug(1, "Unable to find key '%s'\n", id);
			res = -1;
		}
		res = sqlite3_column_int(get_outgoing_stmt, 0);
		sqlite3_reset(get_outgoing_stmt);
	}

	ast_mutex_unlock(&dblock);

	return res;
}

/*!
 * \internal
 * \brief Signal the smsdb sync thread to do its thing.
 *
 * \note dblock is assumed to be held when calling this function.
 */
static void db_sync(void)
{
	dosync = 1;
	ast_cond_signal(&dbcond);
}

/*!
 * \internal
 * \brief smsdb sync thread
 *
 * This thread is in charge of syncing smsdb to disk after a change.
 * By pushing it off to this thread to take care of, this I/O bound operation
 * will not block other threads from performing other critical processing.
 * If changes happen rapidly, this thread will also ensure that the sync
 * operations are rate limited.
 */
static void *db_sync_thread(void *data)
{
	(void)(data);
	ast_mutex_lock(&dblock);
	smsdb_begin_transaction();
	for (;;) {
		/* If dosync is set, db_sync() was called during sleep(1),
		 * and the pending transaction should be committed.
		 * Otherwise, block until db_sync() is called.
		 */
		while (!dosync) {
			ast_cond_wait(&dbcond, &dblock);
		}
		dosync = 0;
		smsdb_purge(CONF_GLOBAL(csms_ttl));
		if (smsdb_commit_transaction()) {
			smsdb_rollback_transaction();
		}
		if (doexit) {
			ast_mutex_unlock(&dblock);
			break;
		}
		smsdb_begin_transaction();
		ast_mutex_unlock(&dblock);
		sleep(1);
		ast_mutex_lock(&dblock);
	}

	return NULL;
}

/*!
 * \internal
 * \brief Clean up resources on Asterisk shutdown
 */
static void smsdb_atexit(void)
{
	/* Set doexit to 1 to kill thread. db_sync must be called with
	 * mutex held. */
	ast_mutex_lock(&dblock);
	doexit = 1;
	db_sync();
	ast_mutex_unlock(&dblock);

	pthread_join(syncthread, NULL);
	ast_mutex_lock(&dblock);
	clean_statements();
	if (sqlite3_close(smsdb) == SQLITE_OK) {
		smsdb = NULL;
	}
	ast_mutex_unlock(&dblock);
}

int smsdb_init()
{
	ast_cond_init(&dbcond, NULL);

	if (db_init()) {
		return -1;
	}

	if (ast_pthread_create_background(&syncthread, NULL, db_sync_thread, NULL)) {
		return -1;
	}

	ast_register_atexit(smsdb_atexit);
	return 0;
}
