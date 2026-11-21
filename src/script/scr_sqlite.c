/*
	SQLite builtins for GSC (part of BMX Server).

	sql_open(path)                 -> handle (int) or undefined
	sql_close(handle)
	sql_exec(handle, sql, args...) -> 1 on success, 0 on error
	sql_query(handle, sql, args...)-> array of rows (each row is an array indexed by column name), [] when empty, undefined on error
	sql_insert_id(handle)          -> rowid of the last insert
	sql_changes(handle)            -> rows changed by the last statement
	sql_error(handle)              -> last error text

	"?" in the sql text is replaced by the extra arguments (int, float, string or undefined), so values are never pasted into the sql.
	All values come back as strings (NULL = undefined). libsqlite3 is loaded at run time, nothing to link.
*/

#include "script.h"
#include <dlfcn.h>

#define SQ_MAXDB    8
#define SQ_MAXARGS  16
#define SQ_MAXROWS  2000
#define SQ_ROW      100
#define SQ_DONE     101
#define SQ_TRANSIENT ((void(*)(void*))-1)

typedef struct sq3 sq3;
typedef struct sq3_stmt sq3_stmt;

static int  (*q_open)(const char *, sq3 **);
static int  (*q_close)(sq3 *);
static int  (*q_prep)(sq3 *, const char *, int, sq3_stmt **, const char **);
static int  (*q_step)(sq3_stmt *);
static int  (*q_fin)(sq3_stmt *);
static int  (*q_bind_i)(sq3_stmt *, int, long long);
static int  (*q_bind_d)(sq3_stmt *, int, double);
static int  (*q_bind_t)(sq3_stmt *, int, const char *, int, void (*)(void *));
static int  (*q_bind_n)(sq3_stmt *, int);
static int  (*q_ncol)(sq3_stmt *);
static const char *(*q_colname)(sq3_stmt *, int);
static const unsigned char *(*q_coltext)(sq3_stmt *, int);
static int  (*q_coltype)(sq3_stmt *, int);
static long long (*q_lastid)(sq3 *);
static int  (*q_changes)(sq3 *);
static const char *(*q_errmsg)(sq3 *);
static int  (*q_busy)(sq3 *, int);
static int  (*q_exec)(sq3 *, const char *, void *, void *, char **);

static sq3 *sq_db[SQ_MAXDB + 1];
static char sq_path[SQ_MAXDB + 1][256];      /* path of each open handle: opening the same file again returns the same handle */
static char sq_err[200];
static int  sq_ready = 0;

static int sq_load(void) {
	static const char *names[] = { "libsqlite3.so.0", "libsqlite3.so", NULL };
	void *h = NULL; int i;
	if (sq_ready) return sq_ready > 0;
	sq_ready = -1;
	for (i = 0; names[i] && !h; i++) h = dlopen(names[i], RTLD_NOW);
	if (!h) { snprintf(sq_err, sizeof(sq_err), "libsqlite3 not found"); return 0; }
	*(void **)&q_open    = dlsym(h, "sqlite3_open");
	*(void **)&q_close   = dlsym(h, "sqlite3_close");
	*(void **)&q_prep    = dlsym(h, "sqlite3_prepare_v2");
	*(void **)&q_step    = dlsym(h, "sqlite3_step");
	*(void **)&q_fin     = dlsym(h, "sqlite3_finalize");
	*(void **)&q_bind_i  = dlsym(h, "sqlite3_bind_int64");
	*(void **)&q_bind_d  = dlsym(h, "sqlite3_bind_double");
	*(void **)&q_bind_t  = dlsym(h, "sqlite3_bind_text");
	*(void **)&q_bind_n  = dlsym(h, "sqlite3_bind_null");
	*(void **)&q_ncol    = dlsym(h, "sqlite3_column_count");
	*(void **)&q_colname = dlsym(h, "sqlite3_column_name");
	*(void **)&q_coltext = dlsym(h, "sqlite3_column_text");
	*(void **)&q_coltype = dlsym(h, "sqlite3_column_type");
	*(void **)&q_lastid  = dlsym(h, "sqlite3_last_insert_rowid");
	*(void **)&q_changes = dlsym(h, "sqlite3_changes");
	*(void **)&q_errmsg  = dlsym(h, "sqlite3_errmsg");
	*(void **)&q_busy    = dlsym(h, "sqlite3_busy_timeout");
	*(void **)&q_exec    = dlsym(h, "sqlite3_exec");
	if (!q_open || !q_close || !q_prep || !q_step || !q_fin || !q_bind_i || !q_bind_d || !q_bind_t || !q_bind_n ||
	    !q_ncol || !q_colname || !q_coltext || !q_coltype || !q_lastid || !q_changes || !q_errmsg || !q_busy || !q_exec) {
		snprintf(sq_err, sizeof(sq_err), "libsqlite3 is missing functions");
		return 0;
	}
	sq_ready = 1;
	return 1;
}

static sq3 *sq_get(int h) { return (h >= 1 && h <= SQ_MAXDB) ? sq_db[h] : NULL; }

static void sq_seterr(sq3 *db) { snprintf(sq_err, sizeof(sq_err), "%s", db ? q_errmsg(db) : "bad handle"); }

/* binds the extra script arguments (index first..) to the ? of the statement */
static int sq_bind_all(sq3_stmt *st, int first, int argc) {
	int i;
	if (argc - first > SQ_MAXARGS) return 0;
	for (i = first; i < argc; i++) {
		int t = Scr_GetType(i), idx = i - first + 1, rc;
		if (t == VT_INT)         rc = q_bind_i(st, idx, (long long)Scr_GetInt(i));
		else if (t == VT_FLOAT)  rc = q_bind_d(st, idx, (double)Scr_GetFloat(i));
		else if (t == VT_STRING) { char *s = Scr_GetString(i); rc = q_bind_t(st, idx, s ? s : "", -1, SQ_TRANSIENT); }
		else                     rc = q_bind_n(st, idx);
		if (rc != 0) return 0;
	}
	return 1;
}

void GScr_sql_open(int a1) {
	char *path; int i;
	if (Scr_GetNumParam() < 1 || Scr_GetType(0) != VT_STRING) { Scr_AddUndefined(); return; }
	if (!sq_load()) { Scr_AddUndefined(); return; }
	path = Scr_GetString(0);
	for (i = 1; i <= SQ_MAXDB; i++) if (sq_db[i] && !strcmp(sq_path[i], path)) { Scr_AddInt(i); return; }      /* already open (a script restarts on every map) */
	for (i = 1; i <= SQ_MAXDB; i++) {
		if (!sq_db[i]) {
			sq3 *db = NULL;
			if (q_open(path, &db) != 0 || !db) { sq_seterr(db); if (db) q_close(db); Scr_AddUndefined(); return; }
			q_busy(db, 3000);
			q_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
			q_exec(db, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
			sq_db[i] = db; snprintf(sq_path[i], sizeof(sq_path[i]), "%s", path);
			Scr_AddInt(i);
			return;
		}
	}
	snprintf(sq_err, sizeof(sq_err), "too many open databases");
	Scr_AddUndefined();
}

void GScr_sql_close(int a1) {
	int h;
	if (Scr_GetNumParam() < 1 || !sq_ready || sq_ready < 0) return;
	h = Scr_GetInt(0);
	if (sq_get(h)) { q_close(sq_db[h]); sq_db[h] = NULL; }
}

void GScr_sql_exec(int a1) {
	int argc = Scr_GetNumParam(); sq3 *db; sq3_stmt *st = NULL; int ok = 0;
	if (argc < 2 || Scr_GetType(0) != VT_INT || Scr_GetType(1) != VT_STRING || !sq_load()) { Scr_AddInt(0); return; }
	db = sq_get(Scr_GetInt(0));
	if (!db) { sq_seterr(NULL); Scr_AddInt(0); return; }
	if (q_prep(db, Scr_GetString(1), -1, &st, NULL) != 0 || !st) { sq_seterr(db); Scr_AddInt(0); return; }
	if (sq_bind_all(st, 2, argc)) {
		int rc;
		while ((rc = q_step(st)) == SQ_ROW) { }
		if (rc == SQ_DONE) ok = 1; else sq_seterr(db);
	} else snprintf(sq_err, sizeof(sq_err), "bad arguments");
	q_fin(st);
	Scr_AddInt(ok);
}

void GScr_sql_query(int a1) {
	int argc = Scr_GetNumParam(), ncol, i, rows = 0, rc; sq3 *db; sq3_stmt *st = NULL; short names[64];
	if (argc < 2 || Scr_GetType(0) != VT_INT || Scr_GetType(1) != VT_STRING || !sq_load()) { Scr_AddUndefined(); return; }
	db = sq_get(Scr_GetInt(0));
	if (!db) { sq_seterr(NULL); Scr_AddUndefined(); return; }
	if (q_prep(db, Scr_GetString(1), -1, &st, NULL) != 0 || !st) { sq_seterr(db); Scr_AddUndefined(); return; }
	if (!sq_bind_all(st, 2, argc)) { snprintf(sq_err, sizeof(sq_err), "bad arguments"); q_fin(st); Scr_AddUndefined(); return; }
	ncol = q_ncol(st); if (ncol > 64) ncol = 64;
	for (i = 0; i < ncol; i++) names[i] = Scr_AllocString(q_colname(st, i), 1);
	Scr_MakeArray();
	while ((rc = q_step(st)) == SQ_ROW && rows < SQ_MAXROWS) {
		Scr_MakeArray();
		for (i = 0; i < ncol; i++) {
			if (q_coltype(st, i) == 5) Scr_AddUndefined();          /* SQLITE_NULL */
			else Scr_AddString((char *)q_coltext(st, i));
			Scr_AddArrayStringIndexed(names[i]);
		}
		Scr_AddArray();
		rows++;
	}
	if (rc != SQ_ROW && rc != SQ_DONE) sq_seterr(db);
	q_fin(st);
}

void GScr_sql_insert_id(int a1) {
	sq3 *db = (sq_ready > 0 && Scr_GetNumParam() > 0) ? sq_get(Scr_GetInt(0)) : NULL;
	if (!db) { Scr_AddUndefined(); return; }
	Scr_AddInt((int)q_lastid(db));
}

void GScr_sql_changes(int a1) {
	sq3 *db = (sq_ready > 0 && Scr_GetNumParam() > 0) ? sq_get(Scr_GetInt(0)) : NULL;
	if (!db) { Scr_AddUndefined(); return; }
	Scr_AddInt(q_changes(db));
}

void GScr_sql_error(int a1) {
	Scr_AddString(sq_err);
}
