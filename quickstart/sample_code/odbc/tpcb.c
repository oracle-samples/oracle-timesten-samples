/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/* This source is best displayed with a tabstop of 4 */

#ifdef WIN32
#include <windows.h>
#include "ttRand.h"
#else
#include <sqlunix.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <math.h>
#include <time.h>
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <ttTime.h>
#include <tt_errCode.h>
#include "utils.h"
#include "tt_version.h"
#include "timesten.h"
#include "ttgetopt.h"

#define VERBOSE_NOMSGS 0
/* this value is used for results (and err msgs) only */
#define VERBOSE_RESULTS    1
/* this value is the default for the cmdline demo */
#define VERBOSE_DFLT   2
#define VERBOSE_ALL    3

#define DSNNAME "DSN=" DEMODSN

/* number of branches, tellers, and accounts */

#define NumBranches             1
#define TellersPerBranch        10
#define AccountsPerBranch       10000

/* number of transactions to execute */

#define NumXacts                25000

/* starting seed value for the random number generator */

#define SeedVal                 84773

static char usageStr[] =
  "Usage:\t<CMD> {-h | -help | -V}\n"
  "\t<CMD> [-xact <xacts>] [-scale <scale>] [-v <level>]\n"
  "\t\t[<DSN> | -connstr <connection-string>]\n\n"
  "  -h                  Prints this message and exits.\n"
  "  -help               Same as -h.\n"
  "  -V                  Prints version number and exits.\n"
  "  -xact <xacts>       Specifies the number of transactions to be run\n"
  "                      The default is 25000 transactions.\n"
  "  -scale <scale>      Specifies a scale factor which determines the\n"
  "                      number of branches (scale), tellers (scale x 10),\n"
  "                      accounts (scale x 10000) and non-local accounts\n"
  "                      ((scale-1) x 10000). The default scale factor is 1.\n"
  "  -v <level>          Verbose level\n"
  "                         0 = errors only\n"
  "                         1 = results only\n"
  "                         2 = results and some status messages (default)\n"
  "                         3 = all messages\n"
  "The default if no DSN or connection-string is given is:\n"
  "  \"DSN=sampledb;UID=appuser\".\n\n"
  "This program creates a data store for the TPCB benchmark and populates\n"
  "the accounts, branches and tellers tables.\n";



/*
 * Specifications of table columns.
 * Fillers of 80, 80, 84, and 24 bytes, respectively, are used
 * to ensure that rows are the width required by the benchmark.
 */

#define TuplesPerPage 256

#define AccountCrTblStmt "create table accounts \
(number tt_integer not null primary key, \
branchnum tt_integer not null, \
balance binary_float not null, \
filler char(80)) unique hash on (number) pages = %" PTRINT_FMT ";"

#define TellerCrTblStmt "create table tellers \
(number tt_integer not null primary key, \
branchnum tt_integer not null, \
balance binary_float not null, \
filler char(80)) unique hash on (number) pages = %" PTRINT_FMT ";"

#define BranchCrTblStmt "create table branches \
(number tt_integer not null primary key, \
balance binary_float not null, \
filler char(84)) unique hash on (number) pages = %" PTRINT_FMT ";"

#define HistoryCrTblStmt "create table History \
(tellernum tt_integer not null, \
branchnum tt_integer not null, \
accountnum tt_integer not null, \
delta binary_float not null, \
createtime tt_integer not null, \
filler char(24));"


#define AccountDropTblStmt "drop table accounts"
#define TellerDropTblStmt "drop table tellers"
#define BranchDropTblStmt "drop table branches"
#define HistoryDropTblStmt "drop table History"


/* Insertion statements used to populate the tables */

#define NumInsStmts 3
char* insStmt[NumInsStmts] = {
  "insert into branches values (?, 0.0, NULL);",
  "insert into tellers  values (?, ?, 0.0, NULL);",
  "insert into accounts values (?, ?, 0.0, NULL);"
};

/* Transaction statements used to update the tables */

#define NumXactStmts 5
char* tpcbXactStmt[NumXactStmts] = {
  "update accounts \
set    balance = balance + ? \
where  number = ?;",

  "select balance \
from   accounts \
where  number = ?;",

  "update tellers \
set    balance = balance + ? \
where  number = ?;",

  "update branches \
set    balance = balance + ? \
where  number = ?;",

  "insert into History \
values (?, ?, ?, ?, ?, NULL);"
};

/* Global parameters and flags (typically set by parse_args()) */

static char connstr[CONN_STR_LEN];   /* Connection string. */
static char dsn[CONN_STR_LEN];       /* DSN. */
static char cmdname[80];             /* Stripped command name. */
int     scaleFactor = 1;             /* Default TPS scale factor */
tt_ptrint numBranchTups;             /* Number of branches */
tt_ptrint numTellerTups;             /* Number of tellers */
tt_ptrint numAccountTups;            /* Number of accounts */
tt_ptrint numNonLocalAccountTups;    /* Number of local accounts */
tt_ptrint numXacts = NumXacts;       /* Default number of transactions */
int     verbose = VERBOSE_DFLT;      /* Verbose level */
FILE   *statusfp;                    /* File for status messages */

SQLHENV henv;                        /* Environment handle */



/*********************************************************************
 *
 *  FUNCTION:       parse_args
 *
 *  DESCRIPTION:    This function parses the command line arguments
 *                  passed to main(), setting the appropriate global
 *                  variables and issuing a usage message for
 *                  invalid arguments.
 *
 *  PARAMETERS:     int argc         # of arguments from main()
 *                  char *argv[]     arguments  from main()
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
parse_args(int argc, char *argv[])
{
  int ac;
  char errbuf[80];
  int n_xacts = NumXacts;

  ttc_getcmdname(argv[0], cmdname, sizeof(cmdname));

  ac = ttc_getoptions(argc, argv, TTC_OPT_NO_CONNSTR_OK,
                    errbuf, sizeof(errbuf),
                    "<HELP>",         usageStr,
                    "<VERSION>",      NULL,
                    "<CONNSTR>",      connstr, sizeof(connstr),
                    "<DSN>",          dsn, sizeof(dsn),
                    "v=i",            &verbose,
                    "xact=i",         &n_xacts,
                    "scale=i",        &scaleFactor,
                    NULL);

  if (ac == -1) {
    fprintf(stderr, "%s\n", errbuf);
    fprintf(stderr, "Type '%s -help' for more information.\n", cmdname);
    exit(-1);
  }
  if (ac != argc) {
    ttc_dump_help(stderr, cmdname, usageStr);
    exit(-1);
  }

  if (verbose < 0 || verbose > 3) {
    fprintf(stderr, "%s: -v option value should be in range [0..3]\n",
            cmdname);
    app_exit(-1);
  }
  numXacts = (tt_ptrint)n_xacts;
  if (numXacts < 0) {
    fprintf(stderr, "%s: -xact option value should be >= 0\n",
            cmdname);
    app_exit(-1);
  }
  if (scaleFactor < 0) {
    fprintf(stderr, "%s: -scale option value should be >= 0\n",
            cmdname);
    app_exit(-1);
  }
  /* Calculate tuple sizes based on -scale value */
  numBranchTups = NumBranches * scaleFactor;
  numTellerTups = TellersPerBranch * scaleFactor;
  numAccountTups = AccountsPerBranch * scaleFactor;
  numNonLocalAccountTups = AccountsPerBranch * (scaleFactor-1);

  /* check for connection string or DSN */
  if (dsn[0] && connstr[0]) {
    /* Got both a connection string and a DSN. */
    fprintf(stderr, "%s: Both DSN and connection string were given.\n",
            cmdname);
    app_exit(-1);

  } else if (dsn[0]) {
    /* Got a DSN, turn it into a connection string. */
    if (strlen(dsn)+5 >= sizeof(connstr)) {
      fprintf(stderr, "%s: DSN '%s' too long.\n", cmdname, dsn);
      app_exit(-1);
    }
    sprintf(connstr, "DSN=%s", dsn);

  } else if (connstr[0]) {
    /* Got a connection string, append some attributes. */
    if (strlen(connstr)+1 >= sizeof(connstr)) {
      fprintf(stderr, "%s: connection string '%s' too long.\n",
              cmdname, connstr);
      app_exit(-1);
    }

  } else {
    /*
     * No connection string or DSN given, use the default.
     *
     * Running the benchmark with a scale factor creates (scale) branches,
     * (scale x 10) tellers, (scale x 10000) accounts and ((scale-1) x 10000)
     * non-local accounts. The size of the table rows are branches (141)
     * tellers (141) and accounts (141). 
     */
    sprintf(connstr, "%s;%s", DSNNAME, UID );
  }

}

/*********************************************************************
 *
 *  FUNCTION:       doImmed
 *
 *  DESCRIPTION:    This function executes and frees the specified
 *                  statement. It is used as a direct means to
 *                  create the tables used by this benchmark,
 *
 *  PARAMETERS:     SQLHDBC hdbc    SQL Connection handle
 *                  SQLHSTMT hs     SQL Statement handle
 *                  char* cmd       SQL Statement text
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
doImmed(SQLHDBC hdbc, SQLHSTMT hs, char* cmd)
{
  SQLRETURN rc;

  /* Execute the command */

  rc = SQLExecDirect(hs, (SQLCHAR *) cmd, SQL_NTS);
  handle_errors(hdbc, hs, rc, ABORT_DISCONNECT_EXIT,
                "Error executing statement", __FILE__, __LINE__);

  /* Close associated cursor and drop pending results */

  rc = SQLFreeStmt(hs, SQL_CLOSE);
  handle_errors(hdbc, hs, rc, ABORT_DISCONNECT_EXIT,
                "closing statement handle",
                __FILE__, __LINE__);

}


/*********************************************************************
 *
 *  FUNCTION:       main
 *
 *  DESCRIPTION:    This is the main function of the tpcb benchmark.
 *                  It connects to an ODBC data source, creates and
 *                  populates tables, updates the tables in a user-
 *                  specified number of transactions and reports on
 *                  on the transaction times.
 *
 *  PARAMETERS:     int argc        # of command line arguments
 *                  char *argv[]    command line arguments
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int
main(int argc, char *argv[])
{

  /* variables used for setting up the tables */

  char               cmdStr[1024];
  char               errstr[4096];

  /* variables used during transactions */

  int                accountNum;
  int                tellerNum;
  int                branchNum;
  int                timeStamp;
  double             delta;
  tt_uptrint         lrand;
  unsigned short     srand, localLimit;

  /* variables used for timing and statistics */

  ttWallClockTime    startT, endT;
  ttWallClockTime**  rtStart;
  ttWallClockTime**  rtEnd;
  double**           resTime;
  double             totTime;
  int                i;
  int                j;
  tt_ptrint          numLocalXacts=0, numRemoteXacts=0;

  /* variables for ODBC */

  SQLHDBC            hdbc;
  SQLHSTMT           hstmt;
  SQLHSTMT           txstmt[NumXactStmts];
  SQLRETURN          rc;
  char               buf [1024];

  /* Set up default signal handlers */
  StopRequestClear();
  if (HandleSignals() != 0) {
    err_msg0("Unable to set signal handlers\n");
    return 1;
  }

  /* set IO mode for demo */
  /* set_io_mode(); */

  /* initialize the file for status messages */
  statusfp = stdout;

  /* set the default tuple sizes */

  numBranchTups = NumBranches * scaleFactor;
  numTellerTups = TellersPerBranch * scaleFactor;
  numAccountTups = AccountsPerBranch * scaleFactor;
  numNonLocalAccountTups = AccountsPerBranch * (scaleFactor-1);

  /* parse the command arguments */
  parse_args(argc, argv);

  /* allocate the transaction-based variables */

  rtStart = (ttWallClockTime**) malloc(numXacts * sizeof(ttWallClockTime*));
  if (!rtStart) {
    err_msg0("Cannot allocate the transaction timing structures");
    app_exit(1);
  }
  for (i = 0; i < numXacts; i++) {
    rtStart[i] = (ttWallClockTime*) malloc(sizeof(ttWallClockTime));
    if (!rtStart[i]) {
      err_msg0("Cannot allocate the transaction timing structures");
      app_exit(1);
    }
  }

  rtEnd = (ttWallClockTime**) malloc(numXacts * sizeof(ttWallClockTime*));
  if (!rtEnd) {
    err_msg0("Cannot allocate the transaction timing structures");
    app_exit(1);
  }
  for (i = 0; i < numXacts; i++) {
    rtEnd[i] = (ttWallClockTime*) malloc(sizeof(ttWallClockTime));
    if (!rtEnd[i]) {
      err_msg0("Cannot allocate the transaction timing structures");
      app_exit(1);
    }
  }

  resTime = (double**) malloc(numXacts * sizeof(double*));
  if (!resTime) {
    err_msg0("Cannot allocate the transaction timing structures");
    app_exit(1);
  }
  for (i = 0; i < numXacts; i++) {
    resTime[i] = (double*) malloc(sizeof(double));
    if (!resTime[i]) {
      err_msg0("Cannot allocate the transaction timing structures");
      app_exit(1);
    }
  }

  /* ODBC initialization */

  rc = SQLAllocEnv(&henv);
  if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
    /* error occurred -- don't bother calling handle_errors, since handle
     * is not valid so SQLError won't work */
    err_msg3("ERROR in %s, line %d: %s\n",
             __FILE__, __LINE__, "allocating an environment handle");
    app_exit(1);
  }

  /* call this in case of warning */
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, NO_EXIT,
                "allocating execution environment",
                __FILE__, __LINE__);

  rc = SQLAllocConnect(henv, &hdbc);
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "allocating connection handle",
                __FILE__, __LINE__);

  /* Connect to data store */

  printf("Connecting to the database with connect string %s\n", connstr);

  rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) connstr, SQL_NTS,
                        NULL, 0, NULL,
                        SQL_DRIVER_COMPLETE);
  sprintf(errstr, "connecting to driver (connect string %s)\n",
          connstr);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                errstr, __FILE__, __LINE__);

  /* Turn auto-commit off */

  rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                "switching off the AUTO_COMMIT option",
                __FILE__, __LINE__);

  /* Allocate a statement handle */

  rc = SQLAllocStmt(hdbc, &hstmt);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  /* (Implicit) Transaction begin */

  status_msg0("Creating tables\n");

  /* create branches table */
  rc = SQLExecDirect(hstmt, (SQLCHAR *) BranchDropTblStmt, SQL_NTS);
  sprintf(cmdStr, BranchCrTblStmt, numBranchTups/TuplesPerPage + 1);
  doImmed(hdbc, hstmt, cmdStr);

  /* create tellers table */
  rc = SQLExecDirect(hstmt, (SQLCHAR *) TellerDropTblStmt, SQL_NTS);
  sprintf(cmdStr, TellerCrTblStmt, numTellerTups/TuplesPerPage + 1);
  doImmed(hdbc, hstmt, cmdStr);

  /* create accounts table */
  rc = SQLExecDirect(hstmt, (SQLCHAR *) AccountDropTblStmt, SQL_NTS);
  sprintf(cmdStr, AccountCrTblStmt, numAccountTups/TuplesPerPage + 1);
  doImmed(hdbc, hstmt, cmdStr);

  /* create History table */
  rc = SQLExecDirect(hstmt, (SQLCHAR *) HistoryDropTblStmt, SQL_NTS);
  doImmed(hdbc, hstmt, HistoryCrTblStmt);

  /* Commit */
  rc = SQLTransact(henv,hdbc,SQL_COMMIT);
  if ( rc != SQL_SUCCESS) {
    handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                  "committing transaction",
                  __FILE__, __LINE__);
  }

  /* Turn off row level locking */
  sprintf (buf, "{ CALL TTOptSetFlag('rowlock', 0) }");
  rc = SQLExecDirect (hstmt, (SQLCHAR *) buf, SQL_NTS);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "specifying row lock usage", __FILE__, __LINE__);

  /* Turn on row level locking */
  sprintf (buf, "{ CALL TTOptSetFlag('tbllock', 1) }");
  rc = SQLExecDirect (hstmt, (SQLCHAR*) buf, SQL_NTS);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "specifying table lock usage", __FILE__, __LINE__);


  /* populate branches table */

  rc = SQLPrepare(hstmt, (SQLCHAR *) insStmt[0], SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "preparing statement",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &branchNum, sizeof branchNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  status_msg1("Populating branches table (%" PTRINT_FMT " rows)\n",
              numBranchTups);
  for (i=0; i<numBranchTups; i++) {
    branchNum = i;
    rc = SQLExecute(hstmt);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Error executing statement",
                  __FILE__, __LINE__);
  }


  /* Reset all bind-parameters for the statement handle. */
  rc = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "resetting parms on statement handle",
                __FILE__, __LINE__);

  /* populate tellers table */

  status_msg1("Populating tellers table (%" PTRINT_FMT " rows)\n",
              numTellerTups);
  rc = SQLPrepare(hstmt, (SQLCHAR *) insStmt[1], SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "preparing statement",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &tellerNum, sizeof tellerNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &branchNum, sizeof branchNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  for (i=0; i<numTellerTups; i++) {
    tellerNum = i;
    branchNum = i/TellersPerBranch;
    rc = SQLExecute(hstmt);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Error executing statement",
                  __FILE__, __LINE__);
  }

  /* Reset all bind-parameters for the statement handle. */

  rc = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "resetting parms on statement handle",
                __FILE__, __LINE__);

  /* populate accounts table */

  status_msg1("Populating accounts table (%" PTRINT_FMT " rows)\n",
              numAccountTups);
  rc = SQLPrepare(hstmt, (SQLCHAR *) insStmt[2], SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "preparing statement",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &accountNum, sizeof accountNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &branchNum, sizeof branchNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  for (i=0; i<numAccountTups; i++) {
    accountNum = i;
    branchNum = i/AccountsPerBranch;
    rc = SQLExecute(hstmt);

    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Error executing statement",
                  __FILE__, __LINE__);

  }

  /* prepare SQL statements of transaction */

  status_msg0("Preparing statements\n");
  for (i=0; i<NumXactStmts; i++) {
    rc = SQLAllocStmt(hdbc, &txstmt[i]);
    handle_errors(hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                  "allocating a statement handle",
                  __FILE__, __LINE__);

    rc = SQLPrepare(txstmt[i], (SQLCHAR *) tpcbXactStmt[i], SQL_NTS);
    handle_errors(hdbc, txstmt[i], rc, ABORT_DISCONNECT_EXIT,
                  "preparing statement",
                  __FILE__, __LINE__);
  }

  /*  unuse dbs lock */
  rc = SQLExecDirect(hstmt, (SQLCHAR *)"call ttlocklevel('Row')", SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "specifying row lock usage",
                __FILE__, __LINE__);

  /* commit transaction */

  rc = SQLTransact(henv, hdbc, SQL_COMMIT);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "committing transaction",
                __FILE__, __LINE__);

  /* Initialize random seed and timers */

  srand48(SeedVal);
  localLimit = (unsigned short)((1<<16) * 0.85);

  /* Initialize parameter lists for each of the transactions */

  rc = SQLBindParameter(txstmt[0], 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 15, 0, &delta, sizeof delta, NULL);
  handle_errors(hdbc, txstmt[0], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[0], 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &accountNum, sizeof accountNum,
                        NULL);
  handle_errors(hdbc, txstmt[0], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[1], 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &accountNum, sizeof accountNum,
                        NULL);
  handle_errors(hdbc, txstmt[1], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[2], 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 15, 0, &delta, sizeof delta, NULL);
  handle_errors(hdbc, txstmt[2], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[2], 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &tellerNum, sizeof tellerNum,
                        NULL);
  handle_errors(hdbc, txstmt[2], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[3], 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 15, 0, &delta, sizeof delta, NULL);
  handle_errors(hdbc, txstmt[3], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[3], 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &branchNum, sizeof branchNum,
                        NULL);
  handle_errors(hdbc, txstmt[3], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &tellerNum, sizeof tellerNum,
                        NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &branchNum, sizeof branchNum,
                        NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 3, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &accountNum, sizeof accountNum,
                        NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 4, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 15, 0, &delta, sizeof delta, NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 5, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &timeStamp, sizeof timeStamp,
                        NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  /* Execute transaction loop. */
  /* Execute tpcb transaction numXacts times.*/

  status_msg1("Executing and timing %" PTRINT_FMT " tpcb transactions\n",
              numXacts);

  ttGetWallClockTime(&startT);

  for (i = 0; i < numXacts; i++) {

    lrand = lrand48();
    srand = (unsigned short) (lrand % 0x10000);

    /* randomly choose a teller */

    tellerNum = lrand % numTellerTups;

    /* compute branch */

    branchNum = (tellerNum / TellersPerBranch);

    /* randomly choose an account */

    if (srand < localLimit || numBranchTups == 1) {

      /* choose account local to selected branch */

      accountNum = branchNum * AccountsPerBranch +
        (lrand48() % AccountsPerBranch);

      ++numLocalXacts;

    }
    else {
      /* choose account not local to selected branch */

      /* first select account in range [0,numNonLocalAccountTups) */

      accountNum = lrand48() % numNonLocalAccountTups;

      /* if branch number of selected account is at least as big
       * as local branch number, then increment account number
       * by AccountsPerBranch to skip over local accounts
       */

      if ((accountNum/AccountsPerBranch) >= branchNum)
        accountNum += AccountsPerBranch;

      ++numRemoteXacts;
    }

    /* select delta amount, -999,999 to +999,999 */

    delta = ((lrand48() % 1999999) - 999999);


    /* begin timing the "residence time" */

    ttGetWallClockTime(rtStart[i]);

    for (j = 0; j < NumXactStmts - 1; j++) {

      rc = SQLExecute(txstmt[j]);
      handle_errors(hdbc, txstmt[j], rc, ABORT_DISCONNECT_EXIT,
                    "Error executing statement",
                    __FILE__, __LINE__);
    }

    /* note that time must be taken within the transaction */

    timeStamp = time(NULL);

    rc = SQLExecute(txstmt[NumXactStmts - 1]);
    handle_errors(hdbc, txstmt[NumXactStmts - 1], rc,
                  ABORT_DISCONNECT_EXIT, "Error executing statement",
                  __FILE__, __LINE__);

    rc = SQLTransact(henv, hdbc, SQL_COMMIT);
    handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                  "Error committing transaction",
                  __FILE__, __LINE__);

    ttGetWallClockTime(rtEnd[i]);

  } /* end fortransaction loop */

  ttGetWallClockTime(&endT);

  /* Compute and report timing statistics */

  totTime = 0.0;

  for (i = 0; i < numXacts; i++) {
    ttCalcElapsedWallClockTime(rtStart[i], rtEnd[i], resTime[i]);
    totTime += *(resTime[i]);
  }

  if (verbose) {
    out_msg2("\nTransaction rate: %10.1f transactions/second (%d ops/transaction)\n",
             (numXacts*1000.0)/totTime, NumXactStmts);
  }

  /* Disconnect and return */

  rc = SQLFreeStmt(hstmt, SQL_DROP);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "dropping the statement handle",
                __FILE__, __LINE__);

  for (i=0; i<NumXactStmts; i++) {
    rc = SQLFreeStmt(txstmt[i], SQL_DROP);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "dropping the statement handle",
                  __FILE__, __LINE__);
  }

  rc = SQLDisconnect(hdbc);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "disconnecting",
                __FILE__, __LINE__);

  rc = SQLFreeConnect(hdbc);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "freeing connection handle",
                __FILE__, __LINE__);

  rc = SQLFreeEnv(henv);
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "freeing environment handle",
                __FILE__, __LINE__);

  app_exit(0);
  return 0;
}


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
