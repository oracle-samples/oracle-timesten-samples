/*
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*
 * This program demonstrates how to query a LOB column from a table. It then updates 
 * the same column with new value and then select it after updating to confirm it has been
 * updated.
 *
 * Parameters:
 *
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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include <timesten.h>

#define BLOB_SIZE 32768

#define DSNNAME DEMODSN

SQLHENV         henv;
SQLHDBC         hdbc;
SQLHSTMT        hstmt, hstmt2, hstmt3;
SQLRETURN       rc;
SQLCHAR         szSqlState[MSG_LNG];    /* SQL state string  */
SQLINTEGER      pfNativeError;          /* Native error code */
SQLCHAR         szErrorMsg[MSG_LNG];    /* Error msg text buffer pointer */
SQLSMALLINT     pcbErrorMsg;            /* Error msg text Available bytes */

typedef         SQLCHAR *STR;
static char* selectTable = "SELECT ad_composite FROM print_media WHERE product_id = :id FOR UPDATE";

static char usageStr[] =
    "Usage:\t<CMD> {-h | -help | -V}\n"
    "\t\t[<DSN> | -connstr <connection-string>]\n"
    "  -h              Prints this message and exits.\n"
    "  -help           Same as -h.\n"
    "  -V              Prints version number and exits.\n"
    "If no DSN or connection string is given, the default is\n"
    "  \"DSN=sampledb\".\n"
    "This program queries and updates against the print_media table. \n";

static char cmdname[80];
static char connstr[CONN_STR_LEN];
static char dsn[CONN_STR_LEN];

int main(int argc, char **argv)
{

  char      LobData[BLOB_SIZE];
  SQLLEN    cbLobData = SQL_NTS;
  int       ID = 3060;
  SQLLEN    cbID = sizeof(SQLINTEGER);


  const char *my_vr = "NEW VERSION";

  SQLLEN   amount = strlen(my_vr);

  char            errstr[4096];
  char            *sqlbuf;
  int             ac; 

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

/*Allocating henv and connect to db */

  rc = SQLAllocConnect(henv, &hdbc);
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "allocating a connection handle",
                __FILE__, __LINE__);


  rc = SQLDriverConnect(hdbc, NULL, (STR) connstr, SQL_NTS,
                        NULL, 0, NULL, SQL_DRIVER_COMPLETE);
  sprintf(errstr, "connecting to driver (connect string %s)\n",
          connstr);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, errstr, __FILE__, __LINE__);



  /*** Query the lob values using SQL_LONGVARBINARY ***/

  rc = SQLAllocStmt(hdbc, &hstmt);
   handle_errors(hdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  rc = SQLAllocStmt(hdbc, &hstmt2);
    handle_errors(hdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  rc = SQLAllocStmt(hdbc, &hstmt3);
    handle_errors(hdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);
  

  /* Select the LOB value */
  rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, 
                             SQL_INTEGER, 10, 0, &ID, 0, &cbID);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "binding parameter",
                  __FILE__, __LINE__);

rc = SQLExecDirect(hstmt, (STR) selectTable, SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, errstr,
                    __FILE__, __LINE__);
  
 rc = SQLBindCol(hstmt, 1, SQL_C_BINARY, LobData, sizeof(LobData), &cbLobData);
   handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "binding column",
                  __FILE__, __LINE__);

 rc = SQLFetch(hstmt);
   handle_errors(hdbc, hstmt3, rc, ABORT_DISCONNECT_EXIT, errstr,
                    __FILE__, __LINE__);

  printf("Before: %s\n", LobData);

  /* Change the Lob / Replacement for DBMS_LOB.WRITE(lob, :amount, :offset, :my_vr); */
  memcpy((void *)((char *)(LobData)),  (unsigned char *)my_vr, amount);

  printf("New value:  %s\n", my_vr);

  /* Update the new data to the selected column of the same row  the database. */
  rc = SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                             SQL_LONGVARBINARY, BLOB_SIZE, 0, LobData, 0, &amount);
     handle_errors(hdbc, hstmt2, rc, ABORT_DISCONNECT_EXIT,
                  "binding parameter",
                  __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt2, 2, SQL_PARAM_INPUT, SQL_C_SLONG, 
                             SQL_INTEGER, 10, 0, &ID, 0, &cbID);
    handle_errors(hdbc, hstmt2, rc, ABORT_DISCONNECT_EXIT,
                  "binding parameter",
                  __FILE__, __LINE__);

  rc = SQLExecDirect(hstmt2, (SQLCHAR*)"UPDATE print_media set ad_composite = :lobData WHERE product_ID = :id", SQL_NTS);
    handle_errors(hdbc, hstmt2, rc, ABORT_DISCONNECT_EXIT, errstr,
                    __FILE__, __LINE__);

/* select the row again after update */
printf("selecting the same row after update...\n");
  rc = SQLBindParameter(hstmt3, 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                             SQL_INTEGER, 10, 0, &ID, 0, &cbID);
 handle_errors(hdbc, hstmt3, rc, ABORT_DISCONNECT_EXIT,
                  "binding parameter",
                  __FILE__, __LINE__);

 rc = SQLExecDirect(hstmt3, (SQLCHAR*)"SELECT ad_composite FROM print_media WHERE product_ID = :id", SQL_NTS);
   handle_errors(hdbc, hstmt3, rc, ABORT_DISCONNECT_EXIT, errstr,
                    __FILE__, __LINE__);



 rc = SQLBindCol(hstmt3, 1, SQL_C_BINARY, LobData, sizeof(LobData), &cbLobData);
   handle_errors(hdbc, hstmt3, rc, ABORT_DISCONNECT_EXIT,
                  "binding column",
                  __FILE__, __LINE__);

 rc = SQLFetch(hstmt3);
   handle_errors(hdbc, hstmt3, rc, ABORT_DISCONNECT_EXIT, errstr,
                    __FILE__, __LINE__);
   
  printf("After: %s\n", LobData);


 rc = SQLTransact(henv, hdbc, SQL_COMMIT);
   handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, "committing",
                  __FILE__, __LINE__);


  rc = SQLFreeStmt(hstmt, SQL_DROP);
    handle_errors(hdbc, hstmt, rc, DISCONNECT_EXIT,
                "freeing the statement handle",
                __FILE__, __LINE__);

  rc = SQLFreeStmt(hstmt2, SQL_DROP);
    handle_errors(hdbc, hstmt2, rc, DISCONNECT_EXIT,
                "freeing the statement handle",
                __FILE__, __LINE__);

rc = SQLFreeStmt(hstmt3, SQL_DROP);
    handle_errors(hdbc, hstmt3, rc, DISCONNECT_EXIT,
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
  
  exit(0);
}
