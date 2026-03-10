/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates TimesTen JSON features using the ODBC driver.
 *
 */

#if defined(WIN32)
#include <windows.h>
#else
#include <sqlunix.h>
#include <unistd.h>
#endif
#include <sql.h>
#include <sqlext.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "timesten.h"
#include "tt_version.h"
#include "ttgetopt.h"
#include "utils.h"

#define PROGRAM_NAME        "jsonSample"
#define TABLE_NAME          "j_purchaseorder"
#define IDX_NAME            "idx_json_user"

static const char JSON_DOC1[] = "../common/jsondoc1.json";
static const char JSON_DOC1_V2[] = "../common/jsondoc1-v2.json";
static const char JSON_DOC2[] = "../common/jsondoc2.json";

#define JSON_BUFFER_SIZE    (256 * 1024)

static const char usageStr[] =
  "Usage:\t" PROGRAM_NAME " {-h | -help | -V}\n"
  "\t<CMD> [-keep] [<DSN> | -connstr <connection-string>]\n\n"
  "  -h                  Prints this message and exits.\n"
  "  -help               Same as -h.\n"
  "  -V                  Prints version number and exits.\n"
  "  -keep               Preserve table " TABLE_NAME " after the sample completes.\n"
  "The default if no DSN or connection-string is given is:\n"
  "  \"DSN=sampledb;UID=appuser\".\n\n";

static char connstr[CONN_STR_LEN];
static char dsn[CONN_STR_LEN];
static char cmdname[80];
static int keepData = 0;

SQLHENV  henv = SQL_NULL_HENV;
static SQLHDBC  hdbc = SQL_NULL_HDBC;
static SQLHSTMT hstmt = SQL_NULL_HSTMT;

/** Parses command line arguments and builds the DSN/connection string. */
static void parse_args(int argc, char *argv[]);
/** Frees allocated handles and disconnects from TimesTen. */
static void cleanup(int rollback);
/** Ensures the shared statement handle exists before executing SQL. */
static void ensure_stmt(void);
/** Drops the demo table, optionally ignoring missing-object errors. */
static void drop_table(bool reportMissing);
/** Creates the sample table populated with JSON data. */
static void create_table(void);
/** Builds the functional index used by JSON_VALUE lookups. */
static void create_index(void);
/** Fills a timestamp struct with the current local time. */
static void populate_current_timestamp(SQL_TIMESTAMP_STRUCT *timestamp);
/** Inserts a purchase order row into the sample table. */
static void insert_row(const char *id, const char *json);
/** Updates a purchase order row with new JSON content. */
static void update_row(const char *id, const char *json, const char *source);
/** Retrieves a purchase order document by identifier. */
static void select_po_by_id(const char *id);
/** Retrieves purchase orders filtered by JSON user attribute. */
static void select_po_by_user(const char *user);
/** Lists line items for a given purchase order. */
static void select_line_items(const char *id);
/** Reads a JSON file into memory. */
static void read_json(const char *path, char **buffer, size_t *length);
/** Executes the end-to-end JSON sample workflow. */
static void run_demo(void);

/**
 * Program entry point; establishes an ODBC connection and runs the demo
 * workflow before performing cleanup.
 */
int
main(int argc, char *argv[])
{
  SQLRETURN rc;

  StopRequestClear();
  if (HandleSignals() != 0) {
    err_msg0("Unable to set signal handlers\n");
    return 1;
  }

  parse_args(argc, argv);

  rc = SQLAllocEnv(&henv);
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, JUST_EXIT,
                "allocating ODBC environment", __FILE__, __LINE__);

  rc = SQLAllocConnect(henv, &hdbc);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "allocating ODBC connection", __FILE__, __LINE__);

  rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) connstr, SQL_NTS,
                        NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
  if (handle_errors(hdbc, SQL_NULL_HSTMT, rc, NO_EXIT,
                    "connecting with SQLDriverConnect", __FILE__, __LINE__) != 0) {
    cleanup(XA_ABORT);
  }

  out_msg1("Connected using connection string: %s\n", connstr);

  run_demo();

  cleanup(NO_XA_ABORT);
  return 0;
}

/**
 * Parses and validates command line arguments, ensuring mutual exclusivity and
 * default values for connection configuration.
 */
static void
parse_args(int argc, char *argv[])
{
  int ac;
  char errbuf[80];

  ttc_getcmdname(argv[0], cmdname, sizeof(cmdname));

  ac = ttc_getoptions(argc, argv, TTC_OPT_NO_CONNSTR_OK,
                      errbuf, sizeof(errbuf),
                      "<HELP>",         usageStr,
                      "<VERSION>",      NULL,
                      "<CONNSTR>",      connstr, sizeof(connstr),
                      "<DSN>",          dsn, sizeof(dsn),
                      "keep",           &keepData,
                      NULL);

  if (ac == -1) {
    fprintf(stderr, "%s\n", errbuf);
    fprintf(stderr, "Type '%s -help' for more information.\n", cmdname);
    app_exit(-1);
  }
  if (ac != argc) {
    ttc_dump_help(stderr, cmdname, usageStr);
    app_exit(-1);
  }

  if (dsn[0] && connstr[0]) {
    fprintf(stderr, "%s: Both DSN and connection string were given.\n",
            cmdname);
    app_exit(-1);
  } else if (dsn[0]) {
    if (strlen(dsn) + 5 >= sizeof(connstr)) {
      fprintf(stderr, "%s: DSN '%s' too long.\n", cmdname, dsn);
      app_exit(-1);
    }
    snprintf(connstr, sizeof(connstr), "DSN=%s", dsn);
  } else if (!connstr[0]) {
    snprintf(connstr, sizeof(connstr), "DSN=sampledb;UID=appuser");
  }
}

/**
 * Releases statement handle resources and gracefully disconnects from ODBC.
 */
static void
cleanup(int rollback)
{
  if (hstmt != SQL_NULL_HSTMT) {
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    hstmt = SQL_NULL_HSTMT;
  }

  QuitTest(henv, hdbc, 0, rollback);
}

/**
 * Allocates the shared statement handle on demand.
 */
static void
ensure_stmt(void)
{
  SQLRETURN rc;
  if (hstmt != SQL_NULL_HSTMT) {
    return;
  }

  rc = SQLAllocStmt(hdbc, &hstmt);
  handle_errors(hdbc, hstmt, rc, DISCONNECT_EXIT,
                "allocating statement handle", __FILE__, __LINE__);
}

/**
 * Drops the sample table when present, optionally reporting missing objects.
 */
static void
drop_table(bool reportMissing)
{
  SQLRETURN rc;

  ensure_stmt();

  rc = SQLExecDirect(hstmt, (SQLCHAR *) ("DROP TABLE " TABLE_NAME), SQL_NTS);
  if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
    if (reportMissing) {
      handle_errors(hdbc, hstmt, rc, NO_EXIT,
                    "dropping table", __FILE__, __LINE__);
    }
    SQLFreeStmt(hstmt, SQL_CLOSE);
    return;
  }

  out_msg1("Table %s dropped\n", TABLE_NAME);
  SQLFreeStmt(hstmt, SQL_CLOSE);
}

/**
 * Creates the JSON-enabled sample table used by the demo workflow.
 */
static void
create_table(void)
{
  static const char *sql =
    "CREATE TABLE " TABLE_NAME " (id VARCHAR2(32) NOT NULL PRIMARY KEY, "
    "date_loaded TIMESTAMP, po_document JSON)";
  SQLRETURN rc;

  ensure_stmt();
  rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);

  handle_errors(hdbc, hstmt, rc, DISCONNECT_EXIT,
                "creating table", __FILE__, __LINE__);

  out_msg1("Table %s created\n", TABLE_NAME);
  SQLFreeStmt(hstmt, SQL_CLOSE);
}

/**
 * Creates a functional index that accelerates queries filtering by user name.
 */
static void
create_index(void)
{
  static const char *sql =
    "CREATE INDEX " IDX_NAME " ON " TABLE_NAME
    " (JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128) ERROR ON ERROR))";
  SQLRETURN rc;

  ensure_stmt();
  rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);

  handle_errors(hdbc, hstmt, rc, DISCONNECT_EXIT,
                "creating JSON index", __FILE__, __LINE__);

  out_msg1("JSON index %s created\n", IDX_NAME);
  SQLFreeStmt(hstmt, SQL_CLOSE);
}

/**
 * Helper to populate a timestamp with the current local time.
 */
static void
populate_current_timestamp(SQL_TIMESTAMP_STRUCT *timestamp)
{
  time_t now = time(NULL);
  struct tm *tmnow = localtime(&now);

  if (tmnow == NULL) {
    err_msg1("Unable to compute local time: %s\n", strerror(errno));
    cleanup(XA_ABORT);
  }

  timestamp->year = (SQLSMALLINT) (tmnow->tm_year + 1900);
  timestamp->month = (SQLUSMALLINT) (tmnow->tm_mon + 1);
  timestamp->day = (SQLUSMALLINT) tmnow->tm_mday;
  timestamp->hour = (SQLUSMALLINT) tmnow->tm_hour;
  timestamp->minute = (SQLUSMALLINT) tmnow->tm_min;
  timestamp->second = (SQLUSMALLINT) tmnow->tm_sec;
  timestamp->fraction = 0;
}

/**
 * Inserts a purchase order row populated with the current timestamp and JSON.
 */
static void
insert_row(const char *id, const char *json)
{
  static const char *sql =
    "INSERT INTO " TABLE_NAME " (id, date_loaded, po_document) VALUES (?, ?, ?)";
  SQLHSTMT stmt = SQL_NULL_HSTMT;
  SQLRETURN rc;
  SQL_TIMESTAMP_STRUCT ts;

  rc = SQLAllocStmt(hdbc, &stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "allocating insert statement", __FILE__, __LINE__);

  populate_current_timestamp(&ts);

  rc = SQLPrepare(stmt, (SQLCHAR *) sql, SQL_NTS);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "preparing insert", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                        32, 0, (SQLPOINTER) id, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding id", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_TIMESTAMP,
                        SQL_TIMESTAMP, 0, 0, &ts, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding timestamp", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR,
                        0, 0, (SQLPOINTER) json, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding JSON document", __FILE__, __LINE__);

  rc = SQLExecute(stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "executing insert", __FILE__, __LINE__);

  out_msg1("Inserted purchase order %s\n", id);

  SQLFreeStmt(stmt, SQL_DROP);
}

/**
 * Replaces a purchase order document with a new payload and timestamp.
 */
static void
update_row(const char *id, const char *json, const char *source)
{
  static const char *sql =
    "UPDATE " TABLE_NAME " SET date_loaded = ?, po_document = ? WHERE id = ?";
  SQLHSTMT stmt = SQL_NULL_HSTMT;
  SQLRETURN rc;
  SQL_TIMESTAMP_STRUCT ts;

  rc = SQLAllocStmt(hdbc, &stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "allocating update statement", __FILE__, __LINE__);

  populate_current_timestamp(&ts);

  rc = SQLPrepare(stmt, (SQLCHAR *) sql, SQL_NTS);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "preparing update", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TIMESTAMP,
                        SQL_TIMESTAMP, 0, 0, &ts, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding timestamp", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR,
                        0, 0, (SQLPOINTER) json, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding JSON document", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                        32, 0, (SQLPOINTER) id, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding id", __FILE__, __LINE__);

  rc = SQLExecute(stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "executing update", __FILE__, __LINE__);

  out_msg2("Updated purchase order %s using %s\n", id, source);

  SQLFreeStmt(stmt, SQL_DROP);
}

/**
 * Retrieves and prints a formatted purchase order by identifier.
 */
static void
select_po_by_id(const char *id)
{
  static const char *sql =
    "SELECT JSON_SERIALIZE(po_document PRETTY) FROM " TABLE_NAME " WHERE id = ?";
  SQLHSTMT stmt = SQL_NULL_HSTMT;
  SQLRETURN rc;
  SQLCHAR buffer[JSON_BUFFER_SIZE];
  SQLLEN len;

  rc = SQLAllocStmt(hdbc, &stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "allocating select statement", __FILE__, __LINE__);

  rc = SQLPrepare(stmt, (SQLCHAR *) sql, SQL_NTS);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "preparing select", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                        32, 0, (SQLPOINTER) id, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding id", __FILE__, __LINE__);

  rc = SQLExecute(stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "executing select", __FILE__, __LINE__);

  rc = SQLFetch(stmt);
  if (rc == SQL_NO_DATA) {
    out_msg1("No purchase order found for id %s\n", id);
    SQLFreeStmt(stmt, SQL_DROP);
    return;
  }
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "fetching purchase order", __FILE__, __LINE__);

  rc = SQLGetData(stmt, 1, SQL_C_CHAR, buffer, sizeof(buffer), &len);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "retrieving purchase order", __FILE__, __LINE__);

  out_msg1("Purchase order for id %s:\n", id);
  printf("%s\n", buffer);

  SQLFreeStmt(stmt, SQL_DROP);
}

/**
 * Retrieves all purchase orders submitted by a given user.
 */
static void
select_po_by_user(const char *user)
{
  static const char *sql =
    "SELECT JSON_SERIALIZE(po_document PRETTY) "
    "FROM " TABLE_NAME " WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = ?";
  SQLHSTMT stmt = SQL_NULL_HSTMT;
  SQLRETURN rc;
  SQLCHAR buffer[JSON_BUFFER_SIZE];
  SQLLEN len;
  int printed = 0;

  rc = SQLAllocStmt(hdbc, &stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "allocating select by user", __FILE__, __LINE__);

  rc = SQLPrepare(stmt, (SQLCHAR *) sql, SQL_NTS);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "preparing select by user", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                        128, 0, (SQLPOINTER) user, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding user", __FILE__, __LINE__);

  rc = SQLExecute(stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "executing select by user", __FILE__, __LINE__);

  while ((rc = SQLFetch(stmt)) != SQL_NO_DATA) {
    handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                  "fetching purchase orders", __FILE__, __LINE__);

    rc = SQLGetData(stmt, 1, SQL_C_CHAR, buffer, sizeof(buffer), &len);
    handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                  "retrieving purchase order", __FILE__, __LINE__);

    if (!printed) {
      out_msg1("Purchase orders for user %s:\n", user);
      printed = 1;
    }
    printf("%s\n", buffer);
  }

  if (!printed) {
    out_msg1("No purchase orders found for user %s\n", user);
  }

  SQLFreeStmt(stmt, SQL_DROP);
}

/**
 * Lists individual line items for a purchase order using JSON_TABLE.
 */
static void
select_line_items(const char *id)
{
  static const char *sql =
    "SELECT jt.line_number, jt.sku, jt.description, jt.quantity, jt.unit_price "
    "FROM " TABLE_NAME " po, "
    "JSON_TABLE(po.po_document, '$.LineItems[*]' COLUMNS ("
    "line_number FOR ORDINALITY, "
    "sku VARCHAR2(40) PATH '$.Part.UPCCode', "
    "description VARCHAR2(256) PATH '$.Part.Description', "
    "quantity NUMBER PATH '$.Quantity', "
    "unit_price NUMBER PATH '$.Part.UnitPrice')) jt WHERE po.id = ?";
  SQLHSTMT stmt = SQL_NULL_HSTMT;
  SQLRETURN rc;
  SQLINTEGER lineNumber;
  SQLCHAR sku[41];
  SQLCHAR description[257];
  double quantity;
  double unitPrice;
  int printed = 0;

  rc = SQLAllocStmt(hdbc, &stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "allocating JSON_TABLE select", __FILE__, __LINE__);

  rc = SQLPrepare(stmt, (SQLCHAR *) sql, SQL_NTS);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "preparing JSON_TABLE select", __FILE__, __LINE__);

  rc = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                        32, 0, (SQLPOINTER) id, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding id", __FILE__, __LINE__);

  rc = SQLExecute(stmt);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "executing JSON_TABLE select", __FILE__, __LINE__);

  rc = SQLBindCol(stmt, 1, SQL_C_SLONG, &lineNumber, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding line_number", __FILE__, __LINE__);
  rc = SQLBindCol(stmt, 2, SQL_C_CHAR, sku, sizeof(sku), NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding sku", __FILE__, __LINE__);
  rc = SQLBindCol(stmt, 3, SQL_C_CHAR, description, sizeof(description), NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding description", __FILE__, __LINE__);
  rc = SQLBindCol(stmt, 4, SQL_C_DOUBLE, &quantity, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding quantity", __FILE__, __LINE__);
  rc = SQLBindCol(stmt, 5, SQL_C_DOUBLE, &unitPrice, 0, NULL);
  handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                "binding unit price", __FILE__, __LINE__);

  while ((rc = SQLFetch(stmt)) != SQL_NO_DATA) {
    handle_errors(hdbc, stmt, rc, DISCONNECT_EXIT,
                  "fetching line item", __FILE__, __LINE__);

    if (!printed) {
      out_msg1("Line items for purchase order %s:\n", id);
      out_msg0("Line  SKU           Description                     Qty  Unit Price  Extended\n");
      printed = 1;
    }

    printf("%4d  %-12s %-30s %4.0f  %10.2f  %8.2f\n",
           (int) lineNumber, sku, description, quantity, unitPrice, quantity * unitPrice);
  }

  if (!printed) {
    out_msg1("No line items found for purchase order %s\n", id);
  }

  SQLFreeStmt(stmt, SQL_DROP);
}

/**
 * Loads a JSON document from disk into a NUL-terminated buffer.
 */
static void
read_json(const char *path, char **buffer, size_t *length)
{
  FILE *fp = NULL;
  char *data;
  size_t size;

  fp = fopen(path, "rb");
  if (!fp) {
    err_msg1("Unable to open %s\n", path);
    cleanup(XA_ABORT);
  }

  fseek(fp, 0, SEEK_END);
  size = (size_t) ftell(fp);
  fseek(fp, 0, SEEK_SET);

  data = (char *) malloc(size + 1);
  if (!data) {
    fclose(fp);
    err_msg0("Out of memory reading JSON file\n");
    cleanup(XA_ABORT);
  }

  if (fread(data, 1, size, fp) != size) {
    fclose(fp);
    free(data);
    err_msg1("Unable to read %s\n", path);
    cleanup(XA_ABORT);
  }

  data[size] = '\0';
  fclose(fp);

  *buffer = data;
  *length = size;
}

/**
 * Executes the full sample workflow, including setup, CRUD operations, and
 * optional teardown based on command line options.
 */
static void
run_demo(void)
{
  char *json1 = NULL;
  char *json1v2 = NULL;
  char *json2 = NULL;
  size_t len;

  drop_table(0);
  create_table();

  read_json(JSON_DOC1, &json1, &len);
  read_json(JSON_DOC1_V2, &json1v2, &len);
  read_json(JSON_DOC2, &json2, &len);

  insert_row("1600", json1);
  insert_row("1721", json2);

  create_index();

  update_row("1600", json1v2, JSON_DOC1_V2);

  select_po_by_id("1600");
  select_po_by_id("1721");

  select_po_by_user("ABULL");
  select_po_by_user("CGIRAFFE");
  select_line_items("1600");

  free(json1);
  free(json1v2);
  free(json2);

  if (!keepData) {
    drop_table(1);
  } else {
    out_msg1("Leaving table %s in place (-keep specified)\n", TABLE_NAME);
  }
}
