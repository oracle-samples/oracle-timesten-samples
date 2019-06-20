/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*
 * Time how fast we can insert data into a table in a private data store.
 *
 * This program times the various aspects of bulk insert into a table.
 * It creates a table called LOAD with COLCOUNT CHAR(COLWIDTH) columns
 * plus an integer sequence column.  It then inserts the requested number
 * of records (default 10000).  The inserts may be performed in batches
 * (default batch size is 1 record per insert).
 *
 * It then creates indexes on the first couple of columns (default is none)
 * and finally updates the statistics for the table.
 *
 * Each operation is timed.
 *
 * Parameters:
 *
 *   -r count   specifies the number of records to insert
 *   -s count   specifies the batch size (records per insert -- default 1)
 *   -i count   specifies the number of char columns to create indexes on
 *   -c count   specifies the number of records to insert before committing
 *   -b         specifies that the indexes should be created _before_
 *              the values are loaded rather than after
 *   -u         updates statistics
 *   -connStr   Optional ODBC connection string specifying DSN and attributes
 */


#ifdef WIN32
#include <windows.h>
#else
#include <sqlunix.h>
#include <unistd.h>
#endif
#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <ttTime.h>
#include "utils.h"
#include "tt_version.h"
#include "timesten.h"
#include "ttgetopt.h"

#define COLCOUNT        10              /* Number of columns */
#define COLWIDTH        20              /* Width of each column */
#define COMMIT_INTERVAL 256             /* Commit every n inserts */
#define COMMIT_INTERVAL_STR "256"
#define MAX_BATCH_SIZE      1024        /* The Maximum batch size */
#define MAX_BATCH_SIZE_STR  "1024"
#define DEFAULT_NUM_RECORDS 10000       /* Default value for -r */
#define DEFAULT_NUM_RECORDS_STR "10000"

#define MAX_INDEXCOUNT  10              /* Max value of -i option */
#define MAX_SQLBUF    4000              /* Max length of SQL buffer */

#define DSNNAME DEMODSN

SQLHENV         henv;
SQLHDBC         hdbc;
SQLHSTMT        hstmt, hstmt_ix;
SQLRETURN       rc;
SQLCHAR         szSqlState[MSG_LNG];    /* SQL state string  */
SQLINTEGER      pfNativeError;          /* Native error code */
SQLCHAR         szErrorMsg[MSG_LNG];    /* Error msg text buffer pointer */
SQLSMALLINT     pcbErrorMsg;            /* Error msg text Available bytes */

typedef         SQLCHAR *STR;

void            makeindex(int indexcount);

static char usageStr[] =
    "Usage:\t<CMD> {-h | -help | -V}\n"
    "\t<CMD> [-r <records>] [-s <size>] [-i <numix>] [-b] [-u]\n"
    "\t\t[<DSN> | -connstr <connection-string>]\n"
    "  -h              Prints this message and exits.\n"
    "  -help           Same as -h.\n"
    "  -V              Prints version number and exits.\n"
    "  -r <records>    Specifies the number of records to insert.\n"
    "                  The default is "
                       DEFAULT_NUM_RECORDS_STR " records.\n"
    "  -s <size>       Specifies the batch size (records per insert).\n"
    "                  The default (optimal) batch size is 256. The maximum is "
                       MAX_BATCH_SIZE_STR ".\n"
    "  -c <records>    Specifies the number of records to insert before commit.\n"
    "                  The default is "
                       COMMIT_INTERVAL_STR " records.\n"
    "  -i <numix>      Specifies the number of indexes on the table.\n"
    "                  The default is 0. The maximum is 10.\n"
    "  -b              Specifies that the indexes should be created \n"
    "                  _before_ the values are loaded rather than after.\n"
    "  -u              Updates statistics prior to disconnecting.\n"
    "                  Useful only for seeing how long the operation takes.\n"
    "If no DSN or connection string is given, the default is\n"
    "  \"DSN=sampledb_<instance>\".\n"
    "This program inserts rows into a table.\n";


static char* dropTable = "DROP table LOAD";


static char cmdname[80];
static char connstr[CONN_STR_LEN];
static char dsn[CONN_STR_LEN];

int
main(int argc, char **argv)
{
  int             records = DEFAULT_NUM_RECORDS;
  int             commitsize = COMMIT_INTERVAL;
  SQLULEN         batchsize = COMMIT_INTERVAL;
  SQLULEN         irow;
  SQLLEN          crow;
  int             indexcount = 0;
  int             i, j;
  char            *sqlbuf;
  char            *values[COLCOUNT];
  SQLLEN          *values_len[COLCOUNT];
  SQLINTEGER      *counter;
  SQLLEN          *counter_len;
  double          loadTime, indexTime, statsTime, discTime;
  ttWallClockTime start, end;
  double          kbs;
  int             testmode = 0;
  int             indexbefore = 0;
  int             dostatsupdate = 0;
  int             grid = 0;          /* Flag set to 1 if in grid mode */
  int             grid_compat = 0;   /* Flag set to 1 if "-grid_compat" is specified */
  char            errstr[4096];
  int             ac;
  int             inserts_per_xact;

#if defined(TTCLIENTSERVER) && defined(__hppa) && !defined(__LP64__)
  /* HP requires this for C main programs that call aC++ shared libs */
  _main();
#endif /* hpux32 */


  /* Set up default signal handlers */
  StopRequestClear();
  if (HandleSignals() != 0) {
    err_msg0("Unable to set signal handlers\n");
    return 1;
  }

  ttc_getcmdname(argv[0], cmdname, sizeof(cmdname));

  ac = ttc_getoptions(argc, argv, TTC_OPT_NO_CONNSTR_OK,
                    errstr, sizeof(errstr),
                    "<HELP>",         usageStr,
                    "<VERSION>",      NULL,
                    "<CONNSTR>",      connstr, sizeof(connstr),
                    "<DSN>",          dsn, sizeof(dsn),
                    "r=i",            &records,
                    "s=i",            &batchsize,
                    "i=i",            &indexcount,
                    "t",              &testmode,
                    "u",              &dostatsupdate,
                    "b",              &indexbefore,
                    "c=i",            &commitsize,
                    "grid_compat",    &grid_compat,
                    NULL);

  if (ac == -1) {
    fprintf(stderr, "%s\n", errstr);
    fprintf(stderr, "Type '%s -help' for more information.\n", cmdname);
    exit(-1);
  }
  if (ac != argc) {
    ttc_dump_help(stderr, cmdname, usageStr);
    exit(-1);
  }

  /* records (-r option)  must be [1..INT_MAX] */
  if (records < 1) {
    fprintf(stderr, "%s: the -r option takes a postive argument\n", cmdname);
    exit(-1);
  }
  /* batchsize (-s option) must be [1 .. MAX_BATCH_SIZE] */
  if (batchsize < 1 || batchsize > MAX_BATCH_SIZE) {
    fprintf(stderr,
            "%s: the -s option takes an argument in the range [1..%d]\n",
            cmdname, MAX_BATCH_SIZE);
    exit(-1);
  }
  /* indexcount (-i option) must be [1..10] (or 0 = default) */
  if (indexcount < 0 || indexcount > MAX_INDEXCOUNT) {
    fprintf(stderr,
            "%s: the -s option takes an argument in the range [1..%d]\n",
            cmdname, MAX_INDEXCOUNT);
    exit(-1);
  }
  /* default commitsize to records if set to t0 */
  if (commitsize == 0) {
    commitsize = records;
  }
  /* commitsize (-c option) must be [0 .. INT_MAX] */
  if (commitsize < 0) {
    fprintf(stderr, "%s: the -c option takes a positive argument\n", cmdname);
    exit(-1);
  }

  /* check for connection string or DSN */
  if (dsn[0] && connstr[0]) {
    /* Got both a connection string and a DSN. */
    fprintf(stderr, "%s: Both DSN and connection string were given.\n",
            cmdname);
    exit(-1);
  } else if (dsn[0]) {
    /* Got a DSN, turn it into a connection string. */
    if (strlen(dsn)+5 >= sizeof(connstr)) {
      fprintf(stderr, "%s: DSN '%s' too long.\n", cmdname, dsn);
      exit(-1);
    }
    sprintf(connstr, "DSN=%s", dsn);
  } else if (connstr[0]) {
    /* Check connection string length. */
    if (strlen(connstr)+1 >= sizeof(connstr)) {
      fprintf(stderr, "%s: connection string '%s' too long.\n",
              cmdname, connstr);
      exit(-1);
    }
  } else {
    /* No connection string or DSN given, use the default. */
    sprintf(connstr, "DSN=%s;%s", DSNNAME, UID);
  }

  printf("Connecting to DB with connect string %s\n", connstr);

  rc = SQLAllocEnv(&henv);
  if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
    /* error occurred -- don't bother calling handle_errors, since handle
     * is not valid so SQLError won't work */
    fprintf(stderr, "*** ERROR in %s, line %d: %s\n",
            __FILE__, __LINE__, "allocating an environment handle");
    exit(1);
  }
  /* call this in case of warning */
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, NO_EXIT,
                "allocating an environment handle",
                __FILE__, __LINE__);

  rc = SQLAllocConnect(henv, &hdbc);
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "allocating a connection handle",
                __FILE__, __LINE__);

  rc = SQLDriverConnect(hdbc, NULL, (STR) connstr, SQL_NTS,
                        NULL, 0, NULL, SQL_DRIVER_COMPLETE);
  sprintf(errstr, "connecting to driver (connect string %s)\n",
          connstr);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, errstr, __FILE__, __LINE__);

  rc = SQLAllocStmt(hdbc, &hstmt);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  rc = SQLAllocStmt(hdbc, &hstmt_ix);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  rc = SQLSetStmtOption(hstmt, SQL_NOSCAN, SQL_NOSCAN_ON);
  handle_errors(hdbc, hstmt, rc, DISCONNECT_EXIT,
                "switching off the Driver scan for escape clauses",
                __FILE__, __LINE__);

  sqlbuf = (char *)malloc(MAX_SQLBUF);
  if (sqlbuf == NULL) {
    fprintf(stderr, "*** ERROR in %s, line %d: %s\n",
            __FILE__, __LINE__, "allocating an SQL buffer");
    exit(1);
  }

  grid = IsGridEnable (hdbc);
  if (grid) {
    printf("Grid enabled\n");
    grid_compat = 1;
  } else if (grid_compat) {
    printf("Grid compatibility enabled\n");
  }

  /* First DROP the LOAD table, ignore errors if it does not exist */
  rc = SQLExecDirect(hstmt, (STR) dropTable, SQL_NTS);

  sprintf(sqlbuf, "create table LOAD (counter integer not null");
  for (i = 0; i < COLCOUNT; i++) {
    char col[BUFSIZ];
    sprintf(col, ", col%d char(%d) not null", i, COLWIDTH);
    /* check the space for the last ")" character too */
    if (strlen(col) + strlen(sqlbuf) > MAX_SQLBUF-2) {
      fprintf(stderr, "*** ERROR in %s, line %d: %s\n",
              __FILE__, __LINE__, "SQL statement is too long");
      exit(1);
    }
    strcat(sqlbuf, col);
  }

  if (grid) {
    strcat(sqlbuf, ", primary key (counter)) distribute by hash");
  } else if (grid_compat) {
    strcat(sqlbuf, ", primary key (counter))");
  } else {
    strcat(sqlbuf, ")");
  }

  rc = SQLExecDirect(hstmt, (STR) sqlbuf, SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, sqlbuf,
                __FILE__, __LINE__);

  /*
   * Prepare insert statement
   */
  sprintf(sqlbuf, "insert into LOAD values(?");
  for (i = 0; i < COLCOUNT; i++) {
    strcat(sqlbuf, ", ?");
  }
  strcat(sqlbuf, ")");
  rc = SQLPrepare(hstmt, (STR) sqlbuf, SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, sqlbuf,
                __FILE__, __LINE__);

  /* allocate the counter column parameter array */
  counter = malloc(sizeof(SQLINTEGER) * batchsize);
  if (counter == NULL)
  {
    fprintf(stderr, "*** malloc of counter array failed\n");
    exit(1);
  }

  /* allocate the corresponding cbValue array */
  counter_len = malloc(sizeof(SQLLEN) * batchsize);
  if (counter_len == NULL)
  {
    fprintf(stderr, "*** malloc of counter_len array failed\n");
    exit(1);
  }

  /* bind the counter column */
  rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        (SQLULEN)10, 0,
                        counter, (SQLLEN)sizeof(SQLINTEGER), counter_len);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  for (i = 0; i < COLCOUNT; i++)
  {
    /* allocate each char column parameter array */
    values[i] = malloc(sizeof(char) * COLWIDTH * batchsize);
    if (values[i] == NULL)
    {
      fprintf(stderr, "*** malloc of values[%d] array failed\n", i);
      exit(1);
    }

    /* initialize it to spaces */
    memset(values[i], ' ', sizeof(char) * COLWIDTH * batchsize);

    /* allocate the corresponding cbValue array */
    values_len[i] = malloc(sizeof(SQLLEN) * batchsize);
    if (values_len[i] == NULL)
    {
      fprintf(stderr, "*** malloc of values_len[%d] array failed\n", i);
      exit(1);
    }

    /* bind this column */
    rc = SQLBindParameter(hstmt, (SQLUSMALLINT)(i + 2), SQL_PARAM_INPUT,
                          SQL_C_CHAR, SQL_CHAR,
                          (SQLULEN)COLWIDTH, 0,
                          (STR)values[i], (SQLLEN)COLWIDTH, values_len[i]);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "binding parameter",
                  __FILE__, __LINE__);
  }

  rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                "turning off auto commit",
                __FILE__, __LINE__);

  printf("Loading %d records with batch size %lud, commit interval %d\n", records, batchsize, commitsize);

  indexTime = 0.0;
  if (indexcount && indexbefore) {
    ttGetWallClockTime(&start);
    makeindex(indexcount);
    ttGetWallClockTime(&end);
    ttCalcElapsedWallClockTime(&start, &end, &indexTime);
    printf("Create index time: %5.2f seconds\n", indexTime/1000.0);
  }
  ttGetWallClockTime(&start);

  /* set up for batch execution */
  rc = SQLParamOptions(hstmt, batchsize, (SQLROWSETSIZE *) &irow);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "setting batch size",
                __FILE__, __LINE__);

  inserts_per_xact = commitsize;

  /* insert the records in batchsize chunks */
  for (i = 0; i < records; i += batchsize)
  {
    char stamp[20];
    char errmsg[64];
    int k;
    int cur_batchsize;

    cur_batchsize = batchsize;

    if (i + batchsize > records)
    {
      /* the final batch is of partial batchsize */
      cur_batchsize = records - i;
      rc = SQLParamOptions(hstmt, cur_batchsize, (SQLROWSETSIZE *) &irow);
      handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                    "setting partial batch size",
                    __FILE__, __LINE__);
    }

    /*
     * Set the parameter values
     */
    for (k = 0; k < cur_batchsize; k++)
    {
      /* counter gets a sequential integer */
      counter[k] = i + k;
      counter_len[k] = sizeof(SQLINTEGER);

      /* the columns get random strings of the form 'nnnnnna' */
      sprintf(stamp, "%06d", rand() % 32768);
      for (j = 0; j < COLCOUNT; j++)
      {
        memcpy(values[j] + (k * COLWIDTH), stamp, 6);
        *(values[j] + (k * COLWIDTH) + 6) = 'A' + j;
        *(values_len[j] + k) = COLWIDTH;
      }
    }

    if (!testmode)
    {
      /* insert the batch */
      rc = SQLExecute(hstmt);
      /* irow indicates the current row in the batch */
      sprintf(errmsg, "SQLExecute failed after %d inserts", i + (int)irow - 1);
      handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, errmsg,
                    __FILE__, __LINE__);
      if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
        break;

      /*
       * sanity check
       * irow and the SQLRowCount result should match the number
       * of rows inserted in the batch
       */
      rc = SQLRowCount(hstmt, &crow);
      handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                    "SQLRowCount failed",
                    __FILE__, __LINE__);
      if (irow != (SQLROWSETSIZE)cur_batchsize || crow != cur_batchsize)
      {
        fprintf(stderr,
                "Row count inconsistency.  Expected: %d, irow: %d, crow: %d\n",
                cur_batchsize, (int)irow, (int)crow);
        break;
      }

      /* commit if COMMIT_INTERVAL inserts done since last commit */
      inserts_per_xact -= cur_batchsize;
      if (inserts_per_xact <= 0) {
        rc = SQLTransact(henv, hdbc, SQL_COMMIT);
        handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                      "committing", __FILE__, __LINE__);
        inserts_per_xact = commitsize;
      }
    }
  }

  rc = SQLTransact(henv, hdbc, SQL_COMMIT);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "committing", __FILE__, __LINE__);

  ttGetWallClockTime(&end);
  ttCalcElapsedWallClockTime(&start, &end, &loadTime);
  kbs = ((double) records * (COLWIDTH * COLCOUNT + sizeof (int))) / 1024;
  if (loadTime > 1000) {
    printf("\nLoad time:        %10.1f seconds\n", loadTime/1000.0);
  }
  else {
    printf("\nLoad time:        %10.1f msec\n", loadTime);
  }
  printf("Load rate:        %10.1f records/second",
         (1000.0 * records) / loadTime);
  printf(" (%" PTRINT_FMT " KB/sec)\n",
         (tt_ptrint)((1000*kbs)/loadTime));
  if (indexcount && !indexbefore) {
    ttGetWallClockTime(&start);
    makeindex(indexcount);
    ttGetWallClockTime(&end);
    ttCalcElapsedWallClockTime(&start, &end, &indexTime);
    printf("Create index time: %5.2f seconds\n", indexTime/1000.0);
  }
  statsTime = 0.0;
  if (dostatsupdate) {
    /*
     * Since we're reusing the hstmt to update statistics,
     * set the batch size back to 1.
     */
    rc = SQLParamOptions(hstmt, 1, NULL);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "resetting batch size",
                  __FILE__, __LINE__);

    /*
     * Update statistics
     */
    ttGetWallClockTime(&start);
    rc = SQLExecDirect(hstmt, (STR) "{ CALL ttOptUpdateStats('LOAD', 0) }",
                       SQL_NTS);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "updating the statistics",
                  __FILE__, __LINE__);

    rc = SQLTransact(henv, hdbc, SQL_COMMIT);
    handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, "committing",
                  __FILE__, __LINE__);

    ttGetWallClockTime(&end);
    ttCalcElapsedWallClockTime(&start, &end, &statsTime);
    printf("Statistics time:   %5.2f seconds\n", statsTime/1000.0);
  }

  ttGetWallClockTime(&start);

  rc = SQLFreeStmt(hstmt, SQL_DROP);
  handle_errors(hdbc, hstmt, rc, DISCONNECT_EXIT,
                "freeing the statement handle",
                __FILE__, __LINE__);
  rc = SQLFreeStmt(hstmt_ix, SQL_DROP);
  handle_errors(hdbc, hstmt_ix, rc, DISCONNECT_EXIT,
                "freeing the statement handle",
                __FILE__, __LINE__);

  rc = SQLDisconnect(hdbc);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "disconnecting",
                __FILE__, __LINE__);

  rc = SQLFreeConnect(hdbc);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "freeing the connection handle",
                __FILE__, __LINE__);

  rc = SQLFreeEnv(henv);
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "freeing the environment handle",
                __FILE__, __LINE__);

  ttGetWallClockTime(&end);
  ttCalcElapsedWallClockTime(&start, &end, &discTime);

  /* free the parameter arrays */
  free(counter);
  free(counter_len);
  free(sqlbuf);
  for (i = 0; i < COLCOUNT; i++)
  {
    free(values[i]);
    free(values_len[i]);
  }

  exit(0);
}

/*
 * Build all desired indexes
 */
void
makeindex(int indexcount)
{
  int i;
  char sqlbuf[BUFSIZ];

  for (i = 0; i < indexcount; i++) {
    sprintf(sqlbuf, "create index loadix%d on load(col%d)", i, i);
    rc = SQLExecDirect(hstmt_ix, (STR) sqlbuf, SQL_NTS);
    handle_errors(hdbc, hstmt_ix, rc, ABORT_DISCONNECT_EXIT, sqlbuf,
          __FILE__, __LINE__);
  }

  rc = SQLTransact(henv, hdbc, SQL_COMMIT);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, "committing",
                __FILE__, __LINE__);
}


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
