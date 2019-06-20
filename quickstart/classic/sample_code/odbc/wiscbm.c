/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*
 * This demo program implements the Wisconsin Benchmark
 * using the TimesTen ODBC interface.
 *
 * The benchmark is implemented as described in ``The Benchmark
 * Handbook For Database and Transaction Processing Systems''
 * (edited by Jim Gray), except as noted below.
 *
 * Modifications to benchmark:
 *
 *   1. The size of the benchmark relations is NOT five times
 *      larger than the total main memory buffer space available,
 *      as required.
 *
 *   2. Results are currently not displayed on the screen or stored
 *      into a temporary result relation, as required.  Result rows
 *      are returned to the benchmark ``application,'' but not
 *      processed in any way.  This seemed like the most appropriate
 *      way to handle results in an embedded data manager.
 *
 *   3. The benchmark specifies that both clustered and non-clustered
 *      unique secondary indexes are to be built on the benchmark
 *      tables.  Clustered indexes are always built on column ``unique1''
 *      and non-clustered indexes on ``unique2''.  The order of values
 *      in the former column is random, while the order in the latter is
 *      sequential.
 *
 *      TimesTen supports only non-clustered secondary indexes, so
 *      whenever the benchmark called for a clustered index, a
 *      non-clustered one was used.  This was done (1) for completeness
 *      (i.e., to include all of the benchmark queries) and (2) to measure
 *      the impact on performance of column order (random vs. sequential).
 */


#if defined(WIN32)
  #include <windows.h>
  #include "ttRand.h"
#else
  #include <unistd.h>
  #include <sys/utsname.h>

  #include <sys/types.h>
  #include <sys/stat.h>
  #include "sqlunix.h"
  #include "sqlfunc.h"
#endif

/*
 * Include TimesTen headers for use with TimesTen provided timing routines
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <ttTime.h>
#include <time.h>

#include <sql.h>
#include <sqlext.h>
#include <string.h>
#include <ctype.h>

#include "wiscprotos.h"
#include "utils.h"
#include "tt_version.h"
#include "timesten.h"
#include "ttgetopt.h"


#if 0
/* The following are used for debugging the benchmark */
#define NO_INDEXES
#endif

#define VERBOSE_NOMSGS      0
#define VERBOSE_RESULTS     1    /* used for results (and err msgs) only */
#define VERBOSE_DFLT        2    /* the default for the cmdline demo */
#define VERBOSE_ALL         3
#define VERBOSE_FIRST       VERBOSE_NOMSGS
#define VERBOSE_LAST        VERBOSE_ALL

#define DSNNAME             "DSN=" DEMODSN

#define MAX_STR             255
#define MAXCOLS             100
#define MAXHSTMT            25

#define MAX_SCALE_FACTOR    10000  /* max value for -scale */

#define BATCH_SIZE_DFLT     256    /* default value for -batch */
#define BATCH_SIZE_MAX      1024   /* max value for -batch */

/* RPTS Should be divisible by 2 -- number repeats of exec loops */
#define RPTS                2
#define MAX_ITERS           1024
#define ARRAY_SIZE          64
#define MAXNUMQUERIES       33
#define BADQUERY            (0x0FFFFFFFFU)

/* query numbers for operations of populating tables and building indexes */

#define TblPopQNum1         40
#define TblPopQNum2         41
#define TblPopQNum3         42
#define TblPopQNum4         43
#define IxBuildQNum         44

#define NumCrTblStmts       4
#define NumCrIxStmts        6

static char usageStr[] =
  "Usage:"
  "\t<CMD> {-h | -help | -V}\n"
  "\t\t[-scale <scale>] [-iso {0|1}] [-q <query_spec>] [-nobuild] [-nocreate] [-noqueries] [-truncate] [-drop]\n"
  "\t\t[-rl {0|1}] [-tl {0|1}] [-dlpopulate] [-verify] [-v <level>]\n"
  "\t\t[<DSN> | -connstr <connection-string>]\n\n"
  "  -h                 Prints this message and exits.\n"
  "  -help              Same as -h.\n"
  "  -V                 Prints version number and exits.\n"
  "  -connstr <string>  Specifies an ODBC connection string to replace the\n"
  "                     default DSN for the program. The default is\n"
  "                     \"DSN=sampledb\".\n"
  "  -scale <scale>     Specifies a scale factor which determines the\n"
  "                     size of the small table (scale x 1000) and the large\n"
  "                     table (scale x 10000). The default scale factor is 1.\n"
  "  -iso {0|1}         Isolation level.\n"
  "                       0 = serializable\n"
  "                       1 = read-committed (default)\n"
  "  -q <query_spec>    Specifies a comma-separated list of query numbers or\n"
  "                     ranges (e.g., -q 1,3,5-7,10-14,25). If not specified,\n"
  "                     all 37 queries are selected.\n"
  "  -nobuild           Disables data store build and table population.\n"
  "  -nocreate          Disables data store build but not table population.\n"
  "  -noqueries         Disables running any DML or SELECT queries.\n"
  "  -nobatch           Disables batching of inserts during populate.\n"
  "  -truncate          Truncate tables after completion (by default tables dropped at start).\n"
  "  -drop              Drop tables after completion (by default tables dropped at start).\n"
  "  -rl {0|1}          Disable/enable row level locking.\n"
  "  -tl {0|1}          Disable/enable table level locking.\n"
  "  -dlpopulate        Lock data store when populating tables.\n"
  "  -verify            Enables verification.\n"
  "  -v <level>         Verbose level\n"
  "                         0 = errors only\n"
  "                         1 = results only\n"
  "                         2 = results and some status messages (default)\n"
  "                         3 = all messages\n"
  "  -batch <rows>      Specify number of rows to batch insert during populate.\n"
  "                     The default is 256 rows.\n"
  "  -commit <rows>     Specify number of rows to insert before committing.\n"
  "                     The default is same as batch size above.\n"
  "  -progress          Show progress indicator.\n"
  "This program runs a subset of queries from the Wisconsin benchmark.\n";


/* message macros used for all conditional non-error output */

#define wiscbm_msg0(str) \
{ \
  if (verbose >= VERBOSE_DFLT) \
  { \
    fprintf (statusfp, str); \
    fflush (statusfp); \
  } \
}

#define wiscbm_msg1(str, arg1) \
{ \
  if (verbose >= VERBOSE_DFLT) \
  { \
    fprintf (statusfp, str, arg1); \
    fflush (statusfp); \
  } \
}

#define wiscbm_msg2(str, arg1, arg2) \
{ \
  if (verbose >= VERBOSE_DFLT) \
  { \
    fprintf (statusfp, str, arg1, arg2); \
    fflush (statusfp); \
  } \
}

#define wiscbm_msg3(str, arg1, arg2, arg3) \
{ \
  if (verbose >= VERBOSE_DFLT) \
  { \
    fprintf (statusfp, str, arg1, arg2, arg3); \
    fflush (statusfp); \
  } \
}

#define InsertTmplt         "insert into %s values (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"
#define InsertTmpltNoBind   "insert into %s values (%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,'%s','%s','%s')"

/* table names */

#define SmallTblName        "small"
#define Big1TblName         "big1"
#define Big2TblName         "big2"
#define BPrimeTblName       "bprime"

/* query types */
char *QueryTypes[40] =
{
  "1% select (no index)",
  "10% select (no index)",
  "1% select using index",
  "10% select using index",
  "1% select using index",
  "10% select using index",
  "1 row select using index",
  "1% select using index",
  "Join big and big with range predicate",
  "Join big and small",
  "Join small, big, and big with range predicate",
  "Join big and big with range predicate using index",
  "Join big and small using index",
  "Join small, big, and big with range predicate using index",
  "Join big and big with range predicate using index",
  "Join big and small using index",
  "Join small, big, and big with range predicate using index",
  "1% distinct select with no duplicates",
  "100% distinct select with no duplicates",
  "Minimum aggregate with no groups",
  "Minimum aggregate with 100 groups",
  "Sum aggregate with 100 groups",
  "Minimum aggregate with no groups using index",
  "Minimum aggregate with 100 groups using index",
  "Sum aggregate with 100 groups using index",
  "Insert one row",
  "Delete one row",
  "Update one row",
  "Insert one row with index",
  "Delete one row using index",
  "Update one row using index",
  "Update one row using index",
  "Join big and big",
  "Populate small table",
  "Populate small table",
  "Populate big table",
  "Populate big table",
  "Create indexes",
};

extern SQLRETURN showPlan (SQLHDBC hDbc);


/* global variable declaration */
char            dsn[CONN_STR_LEN];          /* Data Source Name */
char            connstr[CONN_STR_LEN];      /* ODBC connection string */
char            cmdname[80];                /* stripped command name */
int             numIters[ARRAY_SIZE];

double          deltaKernelRes [MAX_ITERS];
double          deltaUserRes   [MAX_ITERS];
ttWallClockTime startT [MAX_ITERS];         /* wall-clock time */
ttWallClockTime endT   [MAX_ITERS];
double          deltaT [MAX_ITERS];
double          qTime [ARRAY_SIZE] [MAX_ITERS];

int             verify=0;      /* Flag set to 1 if "-v" is specified */
int             build=1;       /* Flag set to 0 if "-nobuild" is specified */
int             create=1;      /* Flag set to 0 if "-nocreate" is specified */
int             queries=1;     /* Flag set to 0 if "-noqueries" is specified */
int             truncat=0;     /* Flag set to 1 if "-truncate" is specified */
int             drop=0;        /* Flag set to 1 if "-drop" is specified */
int             isoLevel=-1;   /* Flag set to isolation level */
int             longerRun=1;   /* Value set if "-l" flag is set */
unsigned long   querySel=0;    /* Query specification, 1 bit per query */
int             scaleFactor;   /* Input scale factor */
int             SmlTblRowCnt=1000;    /* # of rows for the small table */
int             BigTblRowCnt=10000;   /* # of rows for the big tables */
int             verbose=VERBOSE_DFLT; /* verbose level */
FILE*           statusfp;      /* File for status messages */
int             getplan;       /* Get query plan? */
int             tryrowlocks=1; /* Consider row locks -- optimizer flag */
int             trytbllocks=1; /* Consider table locks -- optimizer flag */
int             setLocking=0;  /* Flag if we need to set locking */
int             usedbslock;    /* Use database lock in population phase */
int             nobind=0;      /* Flag set to 1 if "-nobind" is specified */
int             nobatch=0;     /* Flag set to 1 if "-nobatch" is specified */
int             grid=0;        /* Flag set to 1 if in grid mode */
int             grid_compat=0; /* Flag set to 1 if "-grid_compat" is specified */
int             tup_queue=0;   /* Flag set to 1 if "-tup_queue" is specified */
int             fiber=0;       /* Flag set to 1 if "-fiber" is specified */
int             batchSize=BATCH_SIZE_DFLT; /* Populate batch size */
int             commitSize=-1; /* Populate commit size */
int             progress=0;    /* Flag set to 1 if "-progress" is specified */
SQLHENV         henv;          /* ODBC environment handle */


#define StartTimer(_iter) \
  { \
    ttGetWallClockTime (startT + _iter); \
  }

#define StopTimer(_iter) \
  { \
    ttGetWallClockTime (endT + _iter); \
  }

#define WallClkTime(_iter)  (ttCalcElapsedWallClockTime (startT + _iter, \
                             endT + _iter, deltaT + _iter), deltaT[_iter])


/* statements to create tables */

char* crTblStmts [NumCrTblStmts] =
{
  "create table small \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null)",

  "create table big1 \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null)",

  "create table big2 \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null)",

  "create table Bprime \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null)"
};

char* crTblStmtsGrid [NumCrTblStmts] =
{
  "create table small \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null, \
      primary key (unique1)) distribute by hash",

  "create table big1 \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null, \
      primary key (unique1)) distribute by hash",

  "create table big2 \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null, \
      primary key (unique1)) distribute by hash",

  "create table Bprime \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null, \
      primary key (unique1)) distribute by hash",
};

char* crTblStmtsGridCompat [NumCrTblStmts] =
{
  "create table small \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null, \
      primary key (unique1))",

  "create table big1 \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null, \
      primary key (unique1))",

  "create table big2 \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null, \
      primary key (unique1))",

  "create table Bprime \
     (unique1 tt_integer not null, unique2 tt_integer not null, \
      two tt_integer not null, four tt_integer not null, \
      ten tt_integer not null, twenty tt_integer not null, \
      onePercent tt_integer not null, tenPercent tt_integer not null, \
      twentyPercent tt_integer not null, fiftyPercent tt_integer not null, \
      unique3 tt_integer not null, evenOnePercent tt_integer not null, \
      oddOnePercent tt_integer not null, stringu1 char(52) not null, \
      stringu2 char(52) not null, string4 char(52) not null, \
      primary key (unique1))",
};


/* statements to create indexes */

char* crIxStmts[NumCrIxStmts] =
{
  "create unique index i1 on small (unique1)",
  "create unique index i2 on small (unique2)",
  "create unique index i3 on big1 (unique1)",
  "create unique index i4 on big1 (unique2)",
  "create unique index i5 on big2 (unique1)",
  "create unique index i6 on big2 (unique2)",
};

/* statements to drop tables */

char* dropTblStmts [NumCrTblStmts] =
{
  "drop table small",
  "drop table big1",
  "drop table big2",
  "drop table Bprime"
};

/* statements to truncate tables */

char* truncateTblStmts [NumCrTblStmts] =
{
  "truncate table small",
  "truncate table big1",
  "truncate table big2",
  "truncate table Bprime"
};

/*********************************************************************
 *
 *  FUNCTION:       setQueries
 *
 *  DESCRIPTION:    This function parses the -q query specification
 *                  (e.g., "1,2-4,30,31") and sets a bit in the
 *                  returned query specification (query#-1 bit) if
 *                  the query number is specified.
 *
 *  PARAMETERS:     char* queryString     -q "query_spec" string
 *
 *  RETURNS:        unsigned long    query specification or error(BADQUERY)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static unsigned long setQueries (char*  queryString)
{
  unsigned long querySpec = 0;

  /* Parse the string into comma separated tokens */

  char*         tokens [MAXNUMQUERIES];
  int           numTokens;
  int           tokenPtr = 0;
  char*         dash;
  int start;
  int end;
  int i;
  int           tokenValue;

  numTokens = 0;
  tokens [numTokens] = strtok (queryString, ",");
  while (tokens [numTokens] != NULL)
  {
    if (numTokens < MAXNUMQUERIES)
      tokens [++numTokens] = strtok (NULL, ",");
    else
    {
      err_msg0("Too many queries specified.\n");
      return BADQUERY;
    }
  }

  /* Check to see if you got any tokens (return if not) */
  if (numTokens == 0)
    return 0;

  /* Process each token, expanding ranges if present,
   * and adding the query numbers to the return value */

  for (tokenPtr = 0; tokenPtr < numTokens; tokenPtr++)
  {
    /* Check for a range '-' of tokens */

    dash = strchr (tokens[tokenPtr], '-');

    if (dash)
    {
      /* Get the start and end range specifiers (set the dash to null) */
      *dash = 0;
      if (sscanf (tokens[tokenPtr], "%d", &start) == -1 ||
          sscanf (dash+1, "%d", &end) == -1)
      {
        err_msg2 ("Invalid query range specified (%s-%s).\n",
                  tokens[tokenPtr], dash + 1);
        return BADQUERY;
      }

      /* Make sure that the start and end values are in range
       * (return BADQUERY to signal error in caller) */
      if ((start < 1) ||
          (start > MAXNUMQUERIES) ||
          (end < 1) ||
          (end > MAXNUMQUERIES) ||
          (start >= end))
      {
        err_msg2 ("Invalid query range specified (%d-%d).\n", start, end);
        return BADQUERY;
      }

      /* From start to end, set the bit in the querySpec */
      for (i = start; i < (end + 1); i++)
        querySpec = querySpec | ((unsigned long)1 << (i - 1));
    }
    else
    {
      /* Set  the (tokenPtr) bit in the querySpec */
      if (sscanf (tokens[tokenPtr], "%d", &tokenValue) == -1)
      {
        err_msg1("Invalid query specified (%s).\n", tokens[tokenPtr]);
        return BADQUERY;
      }

      if (tokenValue < 1 || tokenValue > MAXNUMQUERIES)
      {
        err_msg1("Invalid query specified (%d).\n", tokenValue);
        return BADQUERY;
      }

      querySpec = querySpec | ((unsigned long)1 << (tokenValue - 1));
    }
  }

  return querySpec;
}



/*********************************************************************
 *
 *  FUNCTION:       isQuery
 *
 *  DESCRIPTION:    This function checks the specified query
 *                  specification to see if the specified query is
 *                  specified by the user (in the -q flag).
 *
 *  PARAMETERS:     unsigned long querySpec  query specification
 *                  int queryNum            query number to check
 *
 *  RETURNS:        int (1 if query is specified, 0 if not)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static int isQuery (unsigned long  querySpec,
                    int            queryNum)
{
  /* Check the (queryNum-1)th bit of the querySpec */

  if (((unsigned long)1 << (queryNum - 1)) & querySpec)
    return 1;
  else
    return 0;
}

static const int QueryNonIndexed[MAXNUMQUERIES] = {
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
};

static int isQueryEnabled (int queryNum)
{
  /* Don't let certain queries run if they depend on non-indexed tables
   * but there are none available since user disabled create mode.
   */
#ifndef NO_INDEXES
  if (!create) {
    if (QueryNonIndexed[queryNum-1]) {
      wiscbm_msg1 ("Skipping query %d -- needs non-indexed table -- re-run without -nocreate or -nobuild option\n", queryNum);
      return 0;
    }
  }
#endif

  /* Don't let certain queries run if they aren't compatible
   * for grid environment.
   */
  if (grid_compat && queryNum == 32) {
    const char* reason = "grid doesn't support updating primary keys";
    wiscbm_msg2 ("Skipping query %d -- %s\n", queryNum, reason);
    return 0;
  }

  return 1;
}


/*********************************************************************
 *
 *  FUNCTION:       parse_args
 *
 *  DESCRIPTION:    This function parses the command line arguments
 *                  passed to main(), setting the appropriate global
 *                  variables and issuing a usage message for
 *                  invalid arguments.
 *
 *  PARAMETERS:     int argc          # of arguments from main()
 *                  char *argv[]      arguments  from main()
 *
 *  RETURNS:        0 - failure; 1 - success; 2 - success (caller exits)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static int parse_args (int   argc,
                       char* argv[])
{
  int ac;
  int long_mode = 0;
#define UNDEF_INT -15
  int rl_val = UNDEF_INT;
  int tl_val = UNDEF_INT;
  char q_str[80];
  char errbuf[256];

  ttc_getcmdname(argv[0], cmdname, sizeof(cmdname));

  q_str[0] = '\0';
  ac = ttc_getoptions(argc, argv, TTC_OPT_NO_CONNSTR_OK,
                    errbuf, sizeof(errbuf),
                    "<HELP>",         usageStr,
                    "<VERSION>",      NULL,
                    "<CONNSTR>",      connstr, sizeof(connstr),
                    "<DSN>",          dsn, sizeof(dsn),
                    "verify",         &verify,
                    "q=s",            q_str, sizeof(q_str),
                    "build!",         &build,
                    "create!",        &create,
                    "queries!",       &queries,
                    "truncate!",      &truncat,
                    "drop!",          &drop,
                    "iso=i",          &isoLevel,
                    "v=i",            &verbose,
                    "l",              &long_mode,
                    "scale=i",        &scaleFactor,
                    "rl=i",           &rl_val,
                    "tl=i",           &tl_val,
                    "rowlocks!",      &rl_val,
                    "tbllocks!",      &tl_val,
                    "dlpopulate",     &usedbslock,
                    "nobind",         &nobind,
                    "nobatch",        &nobatch,
                    "grid_compat",    &grid_compat,
                    "tup_queue",      &tup_queue,
                    "fiber",          &fiber,
                    "batch=i",        &batchSize,
                    "commit=i",       &commitSize,
                    "progress",       &progress,
                    NULL);

  if (ac == -1) {
    fprintf(stderr, "%s\n", errbuf);
    fprintf(stderr, "Type '%s -help' for more information.\n", cmdname);
    return 0;
  }

  if (ac != argc) {
    ttc_dump_help(stderr, cmdname, usageStr);
    return 0;
  }

  if (verify) {
    wiscbm_msg0 ("Verification enabled\n");
    /* verification means that the number of actual result
     * rows is counted for each query and compared to
     * the expected number of result rows
     */
  }

  if (q_str[0]) {
    querySel = setQueries(q_str);
  }

  if (!build) {
    wiscbm_msg0 ("Data store build disabled\n");
    /* -nobuild implies -nocreate */
    create = 0;
  } else {
    /* Disabling table create only makes sense if data store build is enabled */
    if (!create) {
      wiscbm_msg0 ("Table create disabled\n");
    }
  }

  if (!queries) {
    wiscbm_msg0 ("Queries disabled\n");
  }

  if (truncat) {
    wiscbm_msg0 ("Table truncate enabled\n");
  }

  if (drop) {
    wiscbm_msg0 ("Table drop enabled\n");
  }

  if (nobind) {
    wiscbm_msg0 ("Bind parameters disabled\n");
  }

  if (nobatch) {
    wiscbm_msg0 ("Batch populate disabled\n");
  }

  if (tup_queue) {
    wiscbm_msg0 ("Tuple queueing enabled (TupBatchOpt)\n");
  }

  if (fiber) {
    wiscbm_msg0 ("Fibers enabled (FiberOpt)\n");
  }

  if (isoLevel < 0 || isoLevel > 1) {
    if (isoLevel != -1) { /* -1 means leave it alone */
      fprintf(stderr, "%s: -iso option value should be 0 or 1.\n",
              cmdname);
      return 0;
    }
  }

  if (verbose < VERBOSE_FIRST || verbose > VERBOSE_LAST) {
    fprintf(stderr, "%s: -verbose option value should be in range [%d .. %d]\n",
            cmdname, VERBOSE_FIRST, VERBOSE_LAST);
    return 0;
  }

  if (verbose == 3) {
    getplan = 1;
  }

  if (long_mode) {
    longerRun = 5;
  }

  if (scaleFactor != 0) {
    if (scaleFactor < 1 || scaleFactor > MAX_SCALE_FACTOR) {
      fprintf(stderr, "%s: -scale option value must be in range [%d .. %d]\n",
              cmdname, 1, MAX_SCALE_FACTOR);
      return 0;
    }
    /* Calculate tuple sizes */
    SmlTblRowCnt *= scaleFactor;
    BigTblRowCnt *= scaleFactor;
  }

  if (rl_val != UNDEF_INT) {
    setLocking = 1;
    tryrowlocks = rl_val;
    if (tryrowlocks < 0 || tryrowlocks > 1) {
      fprintf(stderr, "%s: -rl option value must be 0 or 1\n", cmdname);
      return 0;
    }
  }

  if (tl_val != UNDEF_INT) {
    setLocking = 1;
    trytbllocks = tl_val;
    if (trytbllocks < 0 || trytbllocks > 1) {
      fprintf(stderr, "%s: -tl option value must be 0 or 1\n", cmdname);
      return 0;
    }
  }

  /* check batch size */
  if (batchSize < 1 || batchSize > BATCH_SIZE_MAX) {
    fprintf(stderr, "%s: -batch option value must be in range [%d .. %d]\n",
            cmdname, 1, BATCH_SIZE_MAX);
    return 0;
  }

  /* set default commit size */
  if (commitSize == -1) {
    commitSize = batchSize;
  }

  /* check commit size */
  if (commitSize < 0) {
    fprintf(stderr, "%s: -commit option value must be positive integer\n",
            cmdname);
    return 0;
  }

  /* check for connection string or DSN */
  if (dsn[0] && connstr[0]) {
    /* Got both a connection string and a DSN. */
    fprintf(stderr, "%s: Both DSN and connection string were given.\n",
            cmdname);
    return 0;
  } else if (dsn[0]) {
    /* Got a DSN, turn it into a connection string. */
    if (strlen(dsn)+5 >= sizeof(connstr)) {
      fprintf(stderr, "%s: DSN '%s' too long.\n", cmdname, dsn);
      return 0;
    }
    sprintf(connstr, "DSN=%s", dsn);
  } else if (connstr[0]) {
    /* Got a connection string. */
  } else {
    /* No connection string or DSN given, fill in value later */
  }

  return 1;
}

/*********************************************************************
 *
 *  FUNCTION:       dblCmp
 *
 *  DESCRIPTION:    This function, used by qsort, compares two
 *                  double values.
 *
 *  PARAMETERS:     const void *elt1        pointer to value 1
 *                  const void *elt2        pointer to value 2
 *
 *  RETURNS:        int (comparison result)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static int dblCmp (const void*  elt1,
                   const void*  elt2)
{
  double d1, d2;

  d1 = *(double*) elt1;
  d2 = *(double*) elt2;

  if (d1 < d2)
    return -1;
  else if (d1 > d2)
    return 1;
  else
    return 0;
}

/*********************************************************************
 *
 *  FUNCTION:       median
 *
 *  DESCRIPTION:    This function computes the median value of an
 *                  array of times.
 *
 *  PARAMETERS:     double* timeArr      pointer to array of times
 *                  int n                number of times
 *
 *  RETURNS:        double    median time value
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static double median (double* timeArr,
                      int     n)
{
  int    i;
  double sorted[MAX_ITERS];
  double med;

  if (n == 0) {
    return 0;
  }

  if (n == 1) {
    return timeArr[0];
  }

  for (i = 0; i < n; i++) {
    sorted[i] = timeArr[i];
  }

  qsort ((void*) sorted, (size_t) n, sizeof(double), dblCmp);

  if ((n & 1) == 0) {
    med = (sorted[(n-1)/2] + sorted[n/2]) / 2;
  } else {
    med = sorted[(n-1)/2];
  }
  return med;
}

/*********************************************************************
 *
 *  FUNCTION:       convert
 *
 *  DESCRIPTION:    This function converts an integer (from column
 *                  unique1 or unique2) to a string value for char
 *                  columns stringu1 and stringu2.
 *
 *  PARAMETERS:     int unique      integer value (IN)
 *                  char* buf       string value (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static void convert (int    unique,
                     char*  buf)
{
  int i;
  int rem;

  for (i = 0; i < 7; i++)
    buf [i] = 'A';

  i = 6;
  while (unique > 0)
  {
    rem = unique % 26;
    buf [i] = 'A' + rem;
    unique = unique / 26;
    i--;
  }
}

/*********************************************************************
 *
 *  FUNCTION:       randNum
 *
 *  DESCRIPTION:    This function generates a random number between
 *                  1 and the specified limit.
 *
 *  PARAMETERS:     int seed          random # seed value
 *                  int limit         maximum potential value
 *                  int generator     # of digit-specific generator
 *                  int prime         prime number for # of digits
 *
 *  RETURNS:        int (random number bewteen 1 and limit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static int randNum (int seed,
                    int limit,
                    int generator,
                    int prime)
{
  do
  {
    seed = (generator * seed) % prime;
  } while (seed > limit);

  return seed;
}


/*********************************************************************
 *
 *  FUNCTION:       populateTable
 *
 *  DESCRIPTION:    This function populates the specified table with
 *                  the specified number of rows using batching.
 *                  Column values for unique1 and unique2 are taken
 *                  from the given valRange.
 *
 *  PARAMETERS:     SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *                  SQLHSTMT hstmt  SQL Statement handle
 *                  char* tblName   Table name
 *                  int numTups     Number of rows
 *                  int valRange    Range to set values for unique
 *                  int qnum        Table population query number
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static void populateTable (SQLHDBC  hdbc,
                           SQLHSTMT hstmt,
                           char*    tblName,
                           int      numTups,
                           int      valRange,
                           int      qnum,
                           int      fBindParams,
                           int      fFreeParams)
{
  char                buf [1024];
  int                 prime = 21;
  int                 generator = 100000007;
  int                 seed;
  int                 randInt;
  static SQLINTEGER*  intVal [13];
  static SQLCHAR*     charVal [3];
  int                 charPos;
  int                 i;
  SQLRETURN           rc = SQL_SUCCESS;
  int                 tupNum;
  int                 tupBatch = batchSize;
  SQLROWSETSIZE       tupCount = 0;
  int                 tupsInserted = 0;
  int                 tupsCommitCount = 0;

  if (verbose == VERBOSE_ALL)
    wiscbm_msg2 ("Populating table %s with %d rows...\n", tblName, numTups);

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

  /* create a prepared statement */
  sprintf (buf, InsertTmplt, tblName);
  rc = SQLPrepare (hstmt, (SQLCHAR*) buf, SQL_NTS);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "preparing statement", __FILE__, __LINE__);

  /* set generator and prime values based upon value range */
  if (valRange <= 1000)
  {
    generator = 279;
    prime = 1009;
  }
  else if (valRange <= 10000)
  {
    generator = 2969;
    prime = 10007;
  }
  else if (valRange <= 100000)
  {
    generator = 21395;
    prime = 100003;
  }
  else if (valRange <= 1000000)
  {
    generator = 2107;
    prime = 1000003;
  }
  else if (valRange <= 10000000)
  {
    generator = 211;
    prime = 10000019;
  }
  else if (valRange <= 100000000)
  {
    generator = 21;
    prime = 100000007;
  }
  else
  {
    err_msg0("Too many rows in table");
  }

  seed = generator;

  /* bind the input parameters */

  if (fBindParams)
  {
    if (verbose == VERBOSE_ALL) {
      wiscbm_msg0 ("Binding parameters for table load...");
    }

    for (i = 0; i < 13; i++)
    {
      intVal [i] = malloc(sizeof(SQLINTEGER) * tupBatch);

      rc = SQLBindParameter (hstmt, (SQLUSMALLINT) (i + 1),
                             SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                             10, 0, intVal [i], sizeof(SQLINTEGER), NULL/*&cbintVal*/);
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter", __FILE__, __LINE__);
    }

    for (i = 0; i < 3; i++)
    {
      charVal [i] = malloc(sizeof(SQLCHAR) * 53 * tupBatch);

      rc = SQLBindParameter (hstmt, (SQLUSMALLINT) (i + 14),
                             SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                             sizeof(SQLCHAR)*53, 0, charVal [i], sizeof(SQLCHAR)*53,
                             NULL/*&(cValLen [i])*/);
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter", __FILE__, __LINE__);
    }

    if (verbose == VERBOSE_ALL)
      wiscbm_msg0 ("Completed\n");
  }

  rc = SQLParamOptions (hstmt, tupBatch, &tupCount);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, 
                 "Unable to set the batch size", __FILE__, __LINE__);

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  StartTimer (0);

  /* start the commit counter */
  tupsCommitCount = commitSize;

  /* insert the numTups rows */
  while (tupsInserted < numTups) {
      
    if (tupsInserted + tupBatch > numTups) {
      tupBatch = numTups - tupsInserted;
      rc = SQLParamOptions (hstmt, tupBatch, &tupCount);
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "Setting batch param option", __FILE__, __LINE__);
    }

    for (i = 0; i < tupBatch; i++)
    {
      tupNum = tupsInserted + i;
      seed = randNum (seed, valRange, generator, prime);
      randInt = (int) seed - 1;

      intVal  [0][i] = randInt;         /* unique1: range is 0 - valRange-1; order is random */
      intVal  [1][i] = tupNum;          /* unique2: range is 0 - valRange-1; order is sequential */
      intVal  [2][i] = randInt % 2;     /* two: range is 0 - 1; order is random */
      intVal  [3][i] = randInt % 4;     /* four: range is 0 - 3; order is random */
      intVal  [4][i] = randInt % 10;    /* ten: range is 0 - 9; order is random */
      intVal  [5][i] = randInt % 20;    /* twenty: range is 0 - 19; order is random */
      intVal  [6][i] = randInt % 100;   /* onePercent: range is 0 - 99; order is random */
      intVal  [7][i] = randInt % 10;    /* tenPercent: range is 0 - 9; order is random */
      intVal  [8][i] = randInt % 5;     /* twentyPercent: range is 0 - 4; order is random */
      intVal  [9][i] = randInt % 2;     /* fiftyPercent: range is 0 - 1; order is random */
      intVal [10][i] = randInt;         /* unique3: range is 0 - valRange-1; order is random */
      intVal [11][i] = (randInt%100) * 2;     /* evenOnePercent: range 0 - 198; order random */
      intVal [12][i] = (randInt%100) * 2 + 1; /* oddOnePercent: range 0 - 199; order random */

      charPos = i * 53;

      memset (&charVal [0] [charPos], 'x', 52);
      charVal [0] [charPos + 52] = '\0';
      convert (randInt, (char *)&charVal [0] [charPos]);    /* stringu1: order is random */

      memset (&charVal [1] [charPos], 'x', 52);
      charVal [1] [charPos + 52] = '\0';
      convert (tupNum, (char *)&charVal [1] [charPos]);          /* stringu2: order is sequential */

      /* string4: order is cyclic */
      switch (tupNum % 4)
      {
        case 0:
          strcpy ( (char *)&charVal [2] [charPos], "AAAAxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
          break;

        case 1:
          strcpy ( (char *)&charVal [2] [charPos], "HHHHxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
          break;

        case 2:
          strcpy ( (char *)&charVal [2] [charPos], "OOOOxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
          break;

        case 3:
          strcpy ( (char *)&charVal [2] [charPos],"VVVVxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
          break;
      }
    }

    tupCount = 0;

    /* ODBC Execute prepared statement */
    rc = SQLExecute (hstmt);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "Error in executing statement", __FILE__, __LINE__);

    tupsInserted += tupCount;
    tupsCommitCount -= tupCount;

    if (tupsCommitCount <= 0) {
      /* commit the transaction */
      rc = SQLTransact (henv, hdbc, SQL_COMMIT);
      handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction", __FILE__, __LINE__);
      /* restart the commit counter */
      tupsCommitCount = commitSize;
    }
  }

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);


  StopTimer (0);

  /* capture the elapsed, system and user times */

  qTime [qnum] [0] = WallClkTime (0);
  numIters [qnum] = 1;

  rc = SQLParamOptions (hstmt, 1, NULL);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, 
                 "Unable to reset the batch size", __FILE__, __LINE__);

  /* close the statement (but do not drop) and return */
  rc = SQLFreeStmt (hstmt, SQL_CLOSE);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "closing statement handle",
                 __FILE__, __LINE__);

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  if (fFreeParams)
  {
    for (i = 0; i < 13; i++)
    {
      if (intVal[i]) {
        free(intVal[i]);
        intVal[i] = NULL;
      }
    }
    for (i = 0; i < 3; i++)
    {
      if (charVal[i]) {
        free(charVal[i]);
        charVal[i] = NULL;
      }
    }
  }

}

/*********************************************************************
 *
 *  FUNCTION:       populateTableNoBatch
 *
 *  DESCRIPTION:    This function populates the specified table with
 *                  the specified number of rows. Column values for
 *                  unique1 and unique2 are taken from the given
 *                  valRange.
 *
 *  PARAMETERS:     SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *                  SQLHSTMT hstmt  SQL Statement handle
 *                  char* tblName   Table name
 *                  int numTups     Number of rows
 *                  int valRange    Range to set values for unique
 *                  int qnum        Table population query number
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static void populateTableNoBatch (SQLHDBC  hdbc,
                                  SQLHSTMT hstmt,
                                  char*    tblName,
                                  int      numTups,
                                  int      valRange,
                                  int      qnum,
                                  int      fBindParams)
{
  char                buf [1024];
  int                 prime = 21;
  int                 generator = 100000007;
  int                 seed;
  int                 randInt;
  /*static SQLINTEGER   intVal [13];*/
  /*static SQLLEN       cbintVal = 4;*/
  static SQLBIGINT    intVal [13];
  static SQLLEN       cbintVal = sizeof(SQLBIGINT);
  static char         charVal [3][64];
  static SQLLEN       cValLen [3];
  int                 i;
  SQLRETURN           rc = SQL_SUCCESS;

  if (verbose == VERBOSE_ALL)
    wiscbm_msg2 ("Populating table %s with %d rows...\n", tblName, numTups);

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

  /* create a prepared statement */
  sprintf (buf, InsertTmplt, tblName);
  rc = SQLPrepare (hstmt, (SQLCHAR*) buf, SQL_NTS);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "preparing statement", __FILE__, __LINE__);

  /* set generator and prime values based upon value range */
  if (valRange <= 1000)
  {
    generator = 279;
    prime = 1009;
  }
  else if (valRange <= 10000)
  {
    generator = 2969;
    prime = 10007;
  }
  else if (valRange <= 100000)
  {
    generator = 21395;
    prime = 100003;
  }
  else if (valRange <= 1000000)
  {
    generator = 2107;
    prime = 1000003;
  }
  else if (valRange <= 10000000)
  {
    generator = 211;
    prime = 10000019;
  }
  else if (valRange <= 100000000)
  {
    generator = 21;
    prime = 100000007;
  }
  else
  {
    err_msg0("Too many rows in table");
  }

  seed = generator;

  /* bind the input parameters */

  memset (charVal [0], 'x', 52);
  charVal [0] [52] = '\0';

  memset (charVal [1], 'x', 52);
  charVal [1] [52] = '\0';

  if (fBindParams)
  {
    if (verbose == VERBOSE_ALL) {
      wiscbm_msg0 ("Binding parameters for table load...");
    }


    for (i = 0; i < 13; i++)
    {
      rc = SQLBindParameter (hstmt, (SQLUSMALLINT) (i + 1),
                             SQL_PARAM_INPUT, SQL_C_BIGINT, SQL_BIGINT,
                             sizeof(SQLBIGINT), 0,
                             &(intVal [i]), sizeof(SQLBIGINT), &cbintVal);
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter", __FILE__, __LINE__);
    }

    for (i = 0; i < 3; i++)
    {
      cValLen [i] = 0;
      rc = SQLBindParameter (hstmt, (SQLUSMALLINT) (i + 14),
                             SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                             52, 0, charVal [i], 52, &(cValLen [i]));
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter", __FILE__, __LINE__);
    }

    if (verbose == VERBOSE_ALL)
      wiscbm_msg0 ("Completed\n");
  }

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  /* insert the numTups rows */

  StartTimer (0);

  for (i = 0; i < numTups; i++)
  {
    seed = randNum (seed, valRange, generator, prime);
    randInt = (int) seed - 1;

    intVal  [0] = randInt;         /* unique1: range is 0 - valRange-1; order is random */
    intVal  [1] = i;               /* unique2: range is 0 - valRange-1; order is sequential */
    intVal  [2] = randInt % 2;     /* two: range is 0 - 1; order is random */
    intVal  [3] = randInt % 4;     /* four: range is 0 - 3; order is random */
    intVal  [4] = randInt % 10;    /* ten: range is 0 - 9; order is random */
    intVal  [5] = randInt % 20;    /* twenty: range is 0 - 19; order is random */
    intVal  [6] = randInt % 100;   /* onePercent: range is 0 - 99; order is random */
    intVal  [7] = randInt % 10;    /* tenPercent: range is 0 - 9; order is random */
    intVal  [8] = randInt % 5;     /* twentyPercent: range is 0 - 4; order is random */
    intVal  [9] = randInt % 2;     /* fiftyPercent: range is 0 - 1; order is random */
    intVal [10] = randInt;         /* unique3: range is 0 - valRange-1; order is random */
    intVal [11] = (randInt%100) * 2;     /* evenOnePercent: range 0 - 198; order random */
    intVal [12] = (randInt%100) * 2 + 1; /* oddOnePercent: range 0 - 199; order random */

    convert (randInt, charVal [0]);    /* stringu1: order is random */
    cValLen [0] = strlen (charVal [0]);

    convert (i, charVal [1]);          /* stringu2: order is sequential */
    cValLen [1] = strlen (charVal [1]);

    /* string4: order is cyclic */
    switch (i % 4)
    {
      case 0:
        strcpy (charVal [2],"AAAAxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        break;

      case 1:
        strcpy (charVal [2],"HHHHxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        break;

      case 2:
        strcpy (charVal [2],"OOOOxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        break;

      case 3:
        strcpy (charVal [2],"VVVVxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        break;
    }
    cValLen [2] = strlen (charVal [2]);

    /* ODBC Execute prepared statement */
    rc = SQLExecute (hstmt);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "Error in executing statement", __FILE__, __LINE__);
  }

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  StopTimer (0);

  /* capture the elapsed, system and user times */

  qTime [qnum] [0] = WallClkTime (0);
  numIters [qnum] = 1;

  /* close the statement (but do not drop) and return */
  rc = SQLFreeStmt (hstmt, SQL_CLOSE);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "closing statement handle",
                 __FILE__, __LINE__);

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);
}



/*********************************************************************
 *
 *  FUNCTION:       populateTableNoBind
 *
 *  DESCRIPTION:    This function populates the specified table with
 *                  the specified number of rows without using 
 *                  SQLBindParameter to bind params. Column values for
 *                  unique1 and unique2 are taken from the given
 *                  valRange.
 *
 *  PARAMETERS:     SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *                  SQLHSTMT hstmt  SQL Statement handle
 *                  char* tblName   Table name
 *                  int numTups     Number of rows
 *                  int valRange    Range to set values for unique
 *                  int qnum        Table population query number
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static void populateTableNoBind (SQLHDBC  hdbc,
                                 SQLHSTMT hstmt,
                                 char*    tblName,
                                 int      numTups,
                                 int      valRange,
                                 int      qnum)
{
  char                buf [1024];
  int                 prime = 21;
  int                 generator = 100000007;
  int                 seed;
  int                 randInt;
  static SQLINTEGER   intVal [13];
  static SQLLEN       cbintVal = 4;
  static char         charVal [3][64];
  static SQLLEN       cValLen [3];
  int                 i;
  SQLRETURN           rc = SQL_SUCCESS;

  if (verbose == VERBOSE_ALL)
    wiscbm_msg2 ("Populating table %s with %d rows...\n", tblName, numTups);

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

  /* set generator and prime values based upon value range */
  if (valRange <= 1000)
  {
    generator = 279;
    prime = 1009;
  }
  else if (valRange <= 10000)
  {
    generator = 2969;
    prime = 10007;
  }
  else if (valRange <= 100000)
  {
    generator = 21395;
    prime = 100003;
  }
  else if (valRange <= 1000000)
  {
    generator = 2107;
    prime = 1000003;
  }
  else if (valRange <= 10000000)
  {
    generator = 211;
    prime = 10000019;
  }
  else if (valRange <= 100000000)
  {
    generator = 21;
    prime = 100000007;
  }
  else
  {
    err_msg0("Too many rows in table");
  }

  seed = generator;

  /* bind the input parameters */

  memset (charVal [0], 'x', 52);
  charVal [0] [52] = '\0';

  memset (charVal [1], 'x', 52);
  charVal [1] [52] = '\0';

  /* insert the numTups rows */

  StartTimer (0);

  for (i = 0; i < numTups; i++)
  {
    seed = randNum (seed, valRange, generator, prime);
    randInt = (int) seed - 1;

    intVal  [0] = randInt;         /* unique1: range is 0 - valRange-1; order is random */
    intVal  [1] = i;               /* unique2: range is 0 - valRange-1; order is sequential */
    intVal  [2] = randInt % 2;     /* two: range is 0 - 1; order is random */
    intVal  [3] = randInt % 4;     /* four: range is 0 - 3; order is random */
    intVal  [4] = randInt % 10;    /* ten: range is 0 - 9; order is random */
    intVal  [5] = randInt % 20;    /* twenty: range is 0 - 19; order is random */
    intVal  [6] = randInt % 100;   /* onePercent: range is 0 - 99; order is random */
    intVal  [7] = randInt % 10;    /* tenPercent: range is 0 - 9; order is random */
    intVal  [8] = randInt % 5;     /* twentyPercent: range is 0 - 4; order is random */
    intVal  [9] = randInt % 2;     /* fiftyPercent: range is 0 - 1; order is random */
    intVal [10] = randInt;         /* unique3: range is 0 - valRange-1; order is random */
    intVal [11] = (randInt%100) * 2;     /* evenOnePercent: range 0 - 198; order random */
    intVal [12] = (randInt%100) * 2 + 1; /* oddOnePercent: range 0 - 199; order random */

    convert (randInt, charVal [0]);    /* stringu1: order is random */
    cValLen [0] = strlen (charVal [0]);

    convert (i, charVal [1]);          /* stringu2: order is sequential */
    cValLen [1] = strlen (charVal [1]);

    /* string4: order is cyclic */
    switch (i % 4)
    {
      case 0:
        strcpy (charVal [2],"AAAAxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        break;

      case 1:
        strcpy (charVal [2],"HHHHxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        break;

      case 2:
        strcpy (charVal [2],"OOOOxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        break;

      case 3:
        strcpy (charVal [2],"VVVVxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        break;
    }
    cValLen [2] = strlen (charVal [2]);

    /* create the insert statement */
    sprintf (buf, InsertTmpltNoBind, tblName,
             intVal[0], intVal[1], intVal[2], intVal[3], intVal[4], intVal[5],
             intVal[6], intVal[7], intVal[8], intVal[9], intVal[10], intVal[11],
             intVal[12], charVal[0], charVal[1], charVal[2]);

    /* ODBC Execute statement directly */
    rc = SQLExecDirect (hstmt, (SQLCHAR*) buf, SQL_NTS);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "Error in executing statement", __FILE__, __LINE__);
  }

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  StopTimer (0);

  /* capture the elapsed, system and user times */

  qTime [qnum] [0] = WallClkTime (0);
  numIters [qnum] = 1;

  /* close the statement (but do not drop) and return */
  rc = SQLFreeStmt (hstmt, SQL_CLOSE);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "closing statement handle",
                 __FILE__, __LINE__);

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);
}


/*********************************************************************
 *
 *  FUNCTION:       fetchSelect
 *
 *  DESCRIPTION:    This function fetches the results of the SQL
 *                  SELECT statement. It assumes no previous
 *                  knowledge of the table structure.
 *
 *  PARAMETERS:     int queryNum    Wisconsin query number
 *                  SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *                  SQLHSTMT hstmt  SQL Statement handle
 *                  int resCard     Expected number of results
 *                  int uniq        Results expected to be unique
 *                  int iter        Iteration count
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static void fetchSelect (int      queryNum,
                         SQLHENV  henv,
                         SQLHDBC  hdbc,
                         SQLHSTMT hstmt,
                         int      resCard,
                         int      uniq,
                         int      iter,
                         int      firstTime)
{
  int           i;
  SQLCHAR       colname [32];
  SQLSMALLINT   coltype;
  SQLSMALLINT   bindtype;
  SQLINTEGER    bindsize;
  SQLSMALLINT   colnamelen;
  SQLSMALLINT   nullable;
  SQLULEN       collen [MAXCOLS];
  SQLSMALLINT   scale;
  SQLLEN        outlen [MAXCOLS];
  SQLCHAR*      data [MAXCOLS];
  SQLSMALLINT   nresultcols = 0;
  SQLSMALLINT   dispcols = 0;
  SQLRETURN     rc = SQL_SUCCESS;
  int           print = 0;
  int           cnt = 0;

  for (i = 0; i < MAXCOLS; i++)
    data[i] = NULL;

  /* Instantiate the result set */

  rc = SQLNumResultCols (hstmt, &nresultcols);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "determining number of columns in result",
                 __FILE__, __LINE__);

  dispcols = 0;

  /* bind each column */

  for (i = 0; i < nresultcols; i++)
  {
    rc = SQLDescribeCol (hstmt, (SQLSMALLINT) (i + 1),
                         colname, (SQLSMALLINT) sizeof(colname),
                         &colnamelen, &coltype, &collen[i],
                         &scale, &nullable);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "describing the column",
                   __FILE__, __LINE__);

    /* Select a binding type based on the column type */

    switch (coltype)
    {
      case SQL_VARCHAR:
      case SQL_CHAR:
        bindtype = SQL_C_CHAR;
        bindsize = (SQLINTEGER) collen[i] + 1;
        break;

      case SQL_INTEGER:
        bindtype = SQL_C_SLONG;
        bindsize = sizeof (int);
        break;

      case SQL_BIGINT:
        bindtype = SQL_C_BIGINT;
        bindsize = sizeof (SQLBIGINT);
        break;

      case SQL_SMALLINT:
        bindtype = SQL_C_SHORT;
        bindsize = sizeof (short);
        break;

      case SQL_TINYINT:
        bindtype = SQL_C_TINYINT;
        bindsize = sizeof (char);
        break;

      case SQL_REAL:
        bindtype = SQL_C_FLOAT;
        bindsize = sizeof (float);
        break;

      case SQL_FLOAT:
      case SQL_DOUBLE:
        bindtype = SQL_C_DOUBLE;
        bindsize = sizeof (double);
        break;

      default:
        err_msg1 ("Unknown col type %d\n", coltype);
        bindtype = SQL_C_CHAR;
        bindsize = display_size (coltype, (SQLUINTEGER) collen[i], colname);
    }

    data[i] = malloc (bindsize);

    rc = SQLBindCol (hstmt, (SQLUSMALLINT) (i + 1), bindtype,
                     (SQLPOINTER) (data [i]), bindsize, &outlen [i]);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "binding the column", __FILE__, __LINE__);

    if (print)
      wiscbm_msg3 ("%*.*s ", (int) collen[i], (int) collen[i], colname);
  } /* end for each column */

  dispcols = i;
  dispcols = nresultcols;

  /* commit the transaction */

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, hstmt, rc, ERROR_EXIT,
                 "committing", __FILE__, __LINE__);

  /* fetch and time the operation */

  StartTimer (iter);

  /* execute the query */

  rc = SQLExecute (hstmt);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "Error while executing SQLExecute statement",
                 __FILE__, __LINE__);

  /* if specified, verify that the number of actual result
   * rows are the expected number of result rows
   */

  if (verify)
  {
    while ((rc = SQLFetch (hstmt)) != SQL_NO_DATA_FOUND)
    {
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "Error in fetching rows", __FILE__, __LINE__);
      ++cnt;

      if (uniq)        /* Guaranteed to be a single-row fetch */
        break;
    }

    if (cnt != resCard)
      err_msg3 ("Error: Query %d expected %d results, got %d\n", queryNum, resCard, cnt);
  }
  else
  {
    while ((rc = SQLFetch (hstmt)) != SQL_NO_DATA_FOUND)
    {
      if (rc != SQL_SUCCESS)
        handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                       "Error in fetching rows", __FILE__, __LINE__);

      if (print)
      {
        for (i = 0; i < dispcols; i++)
        {
          if (outlen [i] == SQL_NULL_DATA)
            strcpy ((char*) data[i], "NULL");

          wiscbm_msg3 ("%*.*s ", (int) collen[i], (int) collen[i], data[i]);
        }
        wiscbm_msg0 ("\n");
      }

      if (uniq)   /* Guaranteed to be a single-row fetch */
        break;
    }
  }

  /* commit the transaction (and close cursors) */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);

  StopTimer (iter);

  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction",
                 __FILE__, __LINE__);

  /* free locally allocated memory */

  for (i = 0; i < MAXCOLS; i++)
    if (data [i])
      free (data [i]);
}

/*********************************************************************
 *
 *  FUNCTION:       executeSelect
 *
 *  DESCRIPTION:    This function executes the SQL command for the
 *                  the appropriate Wisconsin query.
 *
 *  PARAMETERS:     int queryNum    Wisconsin query number
 *                  SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *                  char* form1     SQL SELECT statement text (1/2)
 *                  char* form2     SQL SELECT statement text (2/2)
 *                  int tblCard     # of rows in the table
 *                  int resCard     Expected number of results
 *                  int uniq        Results expected to be unique
 *                  int repeatCnt   # of times to execute the SELECT
 *                  int numParams   # of parameters in either form
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static void executeSelect (int     queryNum,
                           SQLHENV henv,
                           SQLHDBC hdbc,
                           char*   form1,
                           char*   form2,
                           int     tblCard,
                           int     resCard,
                           int     uniq,
                           int     repeatCnt,
                           int     numParams)
{
  int       range = tblCard - resCard;
  int       val;
  char      buf [512];
  char      *buf2;
  int       i;
  int       m = 0;
  int       times;
  int       totalNumQueries;
  SQLHSTMT  hstmt [MAXHSTMT];
  SQLRETURN rc = SQL_SUCCESS;
  int       firstTime = 1;

  /* do not perform the operation if the query is not specified by the
   * user */

  if ((querySel != 0) &&
      (!isQuery (querySel, queryNum)))
    return;

  /* print the query number */

  if (verbose == VERBOSE_ALL)
    wiscbm_msg1 ("Query %d:\n", queryNum);

  /* FIXME: skip query if it's not applicable */
  if (!isQueryEnabled (queryNum)) {
    return;
  }

  /* limit the repeat count */

  if (repeatCnt > 256)
    handle_errors (hdbc, SQL_NULL_HSTMT, SQL_ERROR, JUST_DISCONNECT_EXIT,
                  "executeSelect: repeatCnt too high",
                  __FILE__, __LINE__);

  /* create array[repeatCnt*2] of statement handles */

  for (i = 0; i < (repeatCnt * 2); i++)
  {
    rc = SQLAllocStmt (hdbc, &hstmt [i]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle",
                   __FILE__, __LINE__);
  }

  /* Make sure the plan generation is turned on for the prepare
   * transaction Make use of the first hstmt, since already
   * allocated...  (TimesTen case only)
   */

  if (getplan)
    {
      rc = SQLExecDirect (hstmt [0],
                          (SQLCHAR*) "{ CALL TTOptSetFlag('GenPlan', 1) }",
                          SQL_NTS);
      handle_errors (hdbc, hstmt[0], rc, ABORT_DISCONNECT_EXIT,
                     "turning on plan generation", __FILE__, __LINE__);
    }

  if (setLocking)
    {
      sprintf (buf, "{ CALL TTOptSetFlag('rowlock', %i) }", tryrowlocks);
      rc = SQLExecDirect (hstmt[0], (SQLCHAR *) buf, SQL_NTS);
      handle_errors (hdbc, hstmt[0], rc, ABORT_DISCONNECT_EXIT,
                     "specifying row lock usage",
                     __FILE__, __LINE__);

      sprintf (buf, "{ CALL TTOptSetFlag('tbllock', %i) }", trytbllocks);
      rc = SQLExecDirect (hstmt[0], (SQLCHAR*) buf, SQL_NTS);
      handle_errors (hdbc, hstmt[0], rc, ABORT_DISCONNECT_EXIT,
                     "specifying table lock usage",
                     __FILE__, __LINE__);
    }

  if (tup_queue)
    {
      sprintf (buf, "{ CALL TTOptSetFlag('TupBatchOpt', %i) }", tup_queue);
      rc = SQLExecDirect (hstmt[0], (SQLCHAR *) buf, SQL_NTS);
      handle_errors (hdbc, hstmt[0], rc, ABORT_DISCONNECT_EXIT,
                     "specifying TupBatchOpt usage",
                     __FILE__, __LINE__);
    }

  if (fiber)
    {
      sprintf (buf, "{ CALL TTOptSetFlag('FiberOpt', %i) }", fiber);
      rc = SQLExecDirect (hstmt[0], (SQLCHAR *) buf, SQL_NTS);
      handle_errors (hdbc, hstmt[0], rc, ABORT_DISCONNECT_EXIT,
                     "specifying FiberOpt usage",
                     __FILE__, __LINE__);
    }

  /*
   * Build and compile queries that select desired number of
   * rows (alternating between the two forms).  A total of
   * ten queries are constructed, with column ranges selected
   * at random.
   */

  /* Prefix query with comment including query number */
  sprintf(buf, "/* query %d */ ", queryNum);
  buf2 = buf + strlen(buf);

  for (i = 0; i < repeatCnt; i++)
  {
    val = (int) (drand48() * range);

    switch (numParams)
    {
      case 0:
        sprintf (buf2, form1);
        break;

      case 1:
        sprintf (buf2, form1, val);
        break;

      case 2:
        sprintf (buf2, form1, val, val + resCard - 1);
        break;

      case 4:
        sprintf (buf2, form1, val, val + resCard - 1, val, val + resCard - 1);
        break;

      default:
        handle_errors (hdbc, SQL_NULL_HSTMT, SQL_ERROR, JUST_DISCONNECT_EXIT,
                       "executeSelect: unexpected numParams value",
                       __FILE__, __LINE__);
    }

    /* ODBC prepared statement 2 different statements prepared, each
     * executed repeatCnt number of times with the parameters changed
     * for each iteration */

    rc = SQLPrepare (hstmt[2 * i], (SQLCHAR*) buf, SQL_NTS);
    handle_errors (hdbc, hstmt[2 * i], rc, ABORT_DISCONNECT_EXIT,
                   "preparing statement", __FILE__, __LINE__);

    val = (int) (drand48() * range);

    switch (numParams)
    {
      case 0:
        sprintf (buf2, form2);
        break;

      case 1:
        sprintf (buf2, form2, val);
        break;

      case 2:
        sprintf (buf2, form2, val, val + resCard - 1);
        break;

      case 4:
        sprintf (buf2, form2, val, val + resCard - 1, val, val + resCard - 1);
        break;

      default:
        handle_errors (hdbc, SQL_NULL_HSTMT, SQL_ERROR, JUST_DISCONNECT_EXIT,
                       "executeSelect: unexpected numParams value",
                       __FILE__, __LINE__);
    }

    /* ODBC prepared statement #2 */
    rc = SQLPrepare (hstmt[2 * i + 1],(SQLCHAR *) buf, SQL_NTS );
    handle_errors (hdbc, hstmt[2 * i + 1], rc, ABORT_DISCONNECT_EXIT,
                  "preparing statement", __FILE__, __LINE__);
  }

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  /* display the plan */
  if (getplan)
    (void) showPlan (hdbc);

  /* execute and time queries. */
  totalNumQueries = repeatCnt * 2;

  /* ODBC version of fetchSelect call, handle freed in fetchSelect */

  if (progress) {
    fprintf (statusfp, "Fetching ");
    fflush (statusfp);
  }

  for (i = 0; i < totalNumQueries; i++)
  {
    /* execute each query many a time */
    times = longerRun * RPTS;

    for (m = 0; m < times; m++) {
      fetchSelect (queryNum, henv, hdbc, hstmt[i], resCard,
                   uniq, i * times + m, firstTime++);
      if (progress) {
        fprintf (statusfp, ".");
        fflush (statusfp);
      }
    }
  }
  if (progress) {
    fprintf (statusfp, "done\n");
    fflush (statusfp);
  }

  numIters [queryNum] = i * m;

  for (i = 0; i < numIters [queryNum]; i++)
  {
    qTime [queryNum] [i] = WallClkTime(i);
  }

  for (i = 0; i < totalNumQueries; i++)
  {
    rc = SQLFreeStmt (hstmt [i], SQL_DROP);
    handle_errors (hdbc, hstmt[i], rc, ABORT_DISCONNECT_EXIT,
                   "dropping the statement handle",
                   __FILE__, __LINE__);
  }

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);
}

/*********************************************************************
 *
 *  FUNCTION:       doInsert
 *
 *  DESCRIPTION:    This function executes and times an insert
 *                  statement 10*RPT times.
 *
 *  PARAMETERS:     int queryNum    Wisconsin query number
 *                  SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static
void doInsert (int queryNum,
               SQLHENV henv,
               SQLHDBC hdbc)
{
  char*     ins = "insert into %s values (%d, %d, 0, 2, 0, 10, 50,"
                    "688, 1950, 4950, 9950, 1, 100,"
                    "'MxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxC',"
                    "'GxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxA',"
                    "'OxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxO')";
  char      buf [512];
  int       val = BigTblRowCnt + 1;
  int       i;
  int       recordQuery = 1;
  SQLHSTMT  hstmt [MAXHSTMT];
  SQLRETURN rc = SQL_SUCCESS;

  /* perform this operation whether specified by user or not, in case
   * it affects subsequent queries; however, do not record it if not
   * specified by user */

  if ((querySel != 0) &&
      (!isQuery (querySel, queryNum)))
    recordQuery = 0;

  /* print the query number */

  if (verbose == VERBOSE_ALL && recordQuery)
    wiscbm_msg1 ("Query %d:\n", queryNum);

  /* FIXME: skip query if it's not applicable */
  if (!isQueryEnabled (queryNum)) {
    return;
  }

  /* Alloc statement handles, compile insert statements and bind parameters */

  for (i = 0; i < 5 * RPTS; i++, val++)
  {
    sprintf (buf, ins, Big1TblName, val, val);

    rc = SQLAllocStmt (hdbc, &hstmt[i * 2]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle", __FILE__, __LINE__);

    rc = SQLPrepare (hstmt[i * 2],(SQLCHAR*) buf, SQL_NTS);
    handle_errors (hdbc, hstmt[i * 2], rc, ABORT_DISCONNECT_EXIT,
                   "preparing statement", __FILE__, __LINE__);

    sprintf (buf, ins, Big2TblName, val, val);

    rc = SQLAllocStmt (hdbc, &hstmt[i * 2 + 1]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle",
                   __FILE__, __LINE__);

    rc = SQLPrepare (hstmt[i * 2 + 1],(SQLCHAR *)buf, SQL_NTS);
    handle_errors (hdbc, hstmt[i * 2 + 1], rc, ABORT_DISCONNECT_EXIT,
                   "preparing statement", __FILE__, __LINE__);
  }

  /* commit the transaction */

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  /* execute the insert statement 10*RPT times */

  for (i = 0; i < 10 * RPTS; i++)
  {
    StartTimer (i);

    rc = SQLExecute (hstmt [i]);
    handle_errors (hdbc, hstmt [i], rc, ABORT_DISCONNECT_EXIT,
                   "Error in executing statement",
                   __FILE__, __LINE__);

    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    StopTimer (i);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction", __FILE__, __LINE__);
  }

  /* gather the timing information */
  if (recordQuery)
  {
    numIters [queryNum] = i;
    for (i = 0; i < numIters [queryNum]; i++)
    {
      qTime [queryNum][i] = WallClkTime (i);
    }
  }

  /* delete the compiled commands. */
  for (i = 0; i < 10 * RPTS; i++)
  {
    rc = SQLFreeStmt (hstmt[i], SQL_DROP);
    handle_errors (hdbc, hstmt[i], rc, ABORT_DISCONNECT_EXIT,
                   "dropping the statement handle",
                   __FILE__, __LINE__);
  }

  /* commit the transaction and return */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);
}

/*********************************************************************
 *
 *  FUNCTION:       doDelete
 *
 *  DESCRIPTION:    This function executes and times a delete
 *                  statement 10*RPT times.
 *
 *  PARAMETERS:     int queryNum    Wisconsin query number
 *                  SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static
void doDelete (int queryNum,
              SQLHENV henv,
              SQLHDBC hdbc)
{
  char*     ins = "delete from %s where unique1=%d";
  char      buf [512];
  int       val = BigTblRowCnt + 1;
  int       i;
  int       recordQuery = 1;
  SQLHSTMT  hstmt [MAXHSTMT];
  SQLRETURN rc = SQL_SUCCESS;

  /* perform this operation whether specified by user or not, in case
   * it affects subsequent queries; however, do not record it if not
   * specified by user */

  if ((querySel != 0) &&
      (!isQuery (querySel, queryNum)))
    recordQuery = 0;

  /* print the query number */
  if (verbose == VERBOSE_ALL && recordQuery)
    wiscbm_msg1 ("Query %d:\n", queryNum);

  /* FIXME: skip query if it's not applicable */
  if (!isQueryEnabled (queryNum)) {
    return;
  }

  /* make sure plan generation is turned on for the prepare transaction */

  if (getplan)
  {
    rc = SQLAllocStmt (hdbc, hstmt);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle" ,
                   __FILE__, __LINE__);

    rc = SQLExecDirect (hstmt[0],
                        (SQLCHAR*) "{ CALL TTOptSetFlag('GenPlan', 1) }",
                        SQL_NTS);
    handle_errors (hdbc, hstmt[0], rc,  ABORT_DISCONNECT_EXIT,
                   "turning on plan generation",
                   __FILE__, __LINE__);

    rc = SQLFreeStmt (hstmt[0], SQL_DROP);
    handle_errors (hdbc, hstmt[0] , rc, ABORT_DISCONNECT_EXIT,
                   "dropping the statement handle",
                   __FILE__, __LINE__);
  }

  /* prepare delete statements */

  for (i = 0; i < 5 * RPTS; i++, val++)
  {
    sprintf (buf, ins, Big1TblName, val);
    rc = SQLAllocStmt (hdbc, &hstmt [i*2]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle",
                   __FILE__, __LINE__);

    rc = SQLPrepare (hstmt [i*2], (SQLCHAR*)buf, SQL_NTS );
    handle_errors (hdbc, hstmt [i*2], rc, ABORT_DISCONNECT_EXIT,
                   "preparing statement", __FILE__, __LINE__);

    sprintf(buf, ins, Big2TblName, val);
    rc = SQLAllocStmt (hdbc, &hstmt [i*2+1]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle" ,
                   __FILE__, __LINE__);

    rc= SQLPrepare (hstmt [i*2+1], (SQLCHAR*) buf, SQL_NTS );
    handle_errors(hdbc, hstmt[i*2+1], rc, ABORT_DISCONNECT_EXIT,
                  "preparing statement",
                  __FILE__, __LINE__);
  }

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  /* execute and time deletes. */
  for (i = 0; i < 10 * RPTS; i++)
  {
    StartTimer (i);

    rc = SQLExecute (hstmt[i]);
    handle_errors (hdbc, hstmt[i], rc, ABORT_DISCONNECT_EXIT,
                   "Error in executing statement" , __FILE__, __LINE__);

    /* commit the transaction */
    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    StopTimer (i);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction",
                   __FILE__, __LINE__);

  }

  /* gather the timing information */
  if (recordQuery)
  {
    numIters [queryNum] = i;
    for (i = 0; i < numIters [queryNum]; i++)
    {
      qTime [queryNum][i] = WallClkTime (i);
    }
  }

  /* delete compiled commands.*/

  for (i = 0; i < 10 * RPTS; i++)
  {
    rc = SQLFreeStmt (hstmt[i], SQL_DROP);
    handle_errors (hdbc, hstmt[i], rc, ABORT_DISCONNECT_EXIT,
                   "dropping the statement handle",
                   __FILE__, __LINE__);
  }

  /* commit the transaction and return */

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);
}

/*********************************************************************
 *
 *  FUNCTION:       doUpdate
 *
 *  DESCRIPTION:    This function executes and times an update
 *                  statement 10*RPT times.
 *
 *  PARAMETERS:     int queryNum    Wisconsin query number
 *                  SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *                  char* col       Column to update
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static
void doUpdate (int queryNum,
               SQLHENV henv,
               SQLHDBC hdbc,
               char* col)
{
  char*     upd = "update %s set %s = %d where %s = %d";
  char      buf [512];
  int       val1 = BigTblRowCnt + 1;
  int       val2 = BigTblRowCnt / 2;
  int       i;
  int       recordQuery = 1;
  SQLHSTMT  hstmt [MAXHSTMT];
  SQLRETURN rc = SQL_SUCCESS;

  /* perform this operation whether specified by user or not, in case
   * it affects subsequent queries; however, do not record it if not
   * specified by user */

  if ((querySel != 0) &&
      (!isQuery (querySel, queryNum)))
    recordQuery = 0;

  /* print the query number */
  if (verbose == VERBOSE_ALL && recordQuery)
    wiscbm_msg1 ("Query %d:\n", queryNum);

  /* FIXME: skip query if it's not applicable */
  if (!isQueryEnabled (queryNum)) {
    return;
  }

  /* make sure plan generation is turned on for the prepare
   * transaction */

  if (getplan)
  {
    rc = SQLAllocStmt (hdbc, hstmt);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle" ,
                   __FILE__, __LINE__);

    rc = SQLExecDirect (hstmt[0],
                        (SQLCHAR*) "{ CALL TTOptSetFlag('GenPlan', 1) }",
                        SQL_NTS);
    handle_errors (hdbc, hstmt[0], rc,  ABORT_DISCONNECT_EXIT,
                   "turning on plan generation", __FILE__, __LINE__);

    rc = SQLFreeStmt (hstmt[0], SQL_DROP);
    handle_errors (hdbc, hstmt[0] , rc, ABORT_DISCONNECT_EXIT,
                   "dropping the statement handle",
                  __FILE__, __LINE__);
  }

  /* prepare the update statements */
  for (i = 0; i < ((10 * RPTS) / 4); i++, val1++, val2++)
  {
    sprintf (buf, upd, Big1TblName, col, val1, col, val2);

    /* allocate and prepare statement #1 */
    rc = SQLAllocStmt (hdbc, &hstmt[i * 4]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle" ,
                   __FILE__, __LINE__);

    rc = SQLPrepare (hstmt[i * 4],(SQLCHAR*) buf, SQL_NTS);
    handle_errors (hdbc, hstmt[i * 4], rc, ABORT_DISCONNECT_EXIT,
                   "preparing statement", __FILE__, __LINE__);

    /* allocate and prepare statement #2 */
    sprintf (buf, upd, Big2TblName, col, val1, col, val2);

    rc = SQLAllocStmt (hdbc, &hstmt[i * 4 + 1]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle" ,
                   __FILE__, __LINE__);

    rc = SQLPrepare (hstmt[i * 4 + 1],(SQLCHAR*) buf, SQL_NTS);
    handle_errors (hdbc, hstmt[i * 4 + 1], rc, ABORT_DISCONNECT_EXIT,
                   "preparing statement", __FILE__, __LINE__);

    /* allocate and prepare statement #3 */
    sprintf (buf, upd, Big1TblName, col, val2, col, val1);

    rc = SQLAllocStmt (hdbc, &hstmt[i * 4 + 2]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle",
                   __FILE__, __LINE__);

    rc = SQLPrepare (hstmt[i * 4 + 2],(SQLCHAR*) buf, SQL_NTS);
    handle_errors (hdbc, hstmt[i * 4 + 2], rc, ABORT_DISCONNECT_EXIT,
                   "preparing statement", __FILE__, __LINE__);

    /* allocate and prepare statement #4 */
    sprintf (buf, upd, Big2TblName, col, val2, col, val1);

    rc = SQLAllocStmt (hdbc, &hstmt[i * 4 + 3]);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating statement handle" ,
                   __FILE__, __LINE__);

    rc = SQLPrepare (hstmt[i * 4 + 3],(SQLCHAR*) buf, SQL_NTS );
    handle_errors (hdbc, hstmt[i * 4 + 3], rc, ABORT_DISCONNECT_EXIT,
                   "preparing statement", __FILE__, __LINE__);
  }

  /* commit the transaction */

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  /* Execute and time updates. */
  for (i = 0; i < 10 * RPTS; i++)
  {
    StartTimer (i);

    rc = SQLExecute (hstmt[i]);
    handle_errors (hdbc, hstmt[i], rc, ABORT_DISCONNECT_EXIT,
                   "Error in executing statement", __FILE__, __LINE__);

    rc = SQLTransact (henv, hdbc, SQL_COMMIT);

    StopTimer(i);

    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction", __FILE__, __LINE__);
  }

  /* gather the timing information */
  if (recordQuery)
  {
    numIters [queryNum] = i;

    for (i = 0; i < numIters [queryNum]; i++)
    {
      qTime [queryNum][i] = WallClkTime (i);
    }
  }

  /* Delete compiled commands. */

  for (i = 0; i < 10 * RPTS; i++)
  {
    rc = SQLFreeStmt (hstmt[i], SQL_DROP);
    handle_errors (hdbc, hstmt[i] , rc, ABORT_DISCONNECT_EXIT,
                   "dropping the statement handle",
                   __FILE__, __LINE__);
  }

  /* commit the transaction and return */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);
}



static void AllocAndConnectToDb (SQLHENV*   phenv,
                                 SQLHDBC*   phdbc,
                                 SQLHSTMT*  phstmt)
{
  SQLRETURN rc;
  char      errstr [1024];


  if (! connstr[0])
  {
    /* Running the benchmark with a scale factor creates a (scale x 1000)
     * small table and 3 (scale x 10000) large tables.  The size of the
     * table row is 208.
     */
    sprintf (connstr, "%s;%s", DSNNAME, UID);
  }

  printf("Connecting to DB with connect string %s\n", connstr);

  /* connect to data store */
  rc = SQLAllocEnv (phenv);
  handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, JUST_EXIT,
                 "allocating an environment handle",
                 __FILE__, __LINE__);

  rc = SQLAllocConnect (*phenv, phdbc);
  handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "allocating connection handle",
                 __FILE__, __LINE__);

  rc = SQLSetConnectOption (*phdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  handle_errors (*phdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "setting connection option", __FILE__, __LINE__);

  rc = SQLSetConnectOption (*phdbc, SQL_NOSCAN, SQL_NOSCAN_ON);
  handle_errors (*phdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "setting connection option", __FILE__, __LINE__);

  if (verbose == VERBOSE_ALL)
    wiscbm_msg0 ("Connecting to the data source...\n");

  rc = SQLDriverConnect (*phdbc, NULL, (SQLCHAR*) connstr, SQL_NTS,
                         NULL, 0, NULL, SQL_DRIVER_COMPLETE);
  sprintf (errstr, "connecting to driver (connect string %s)\n", connstr);
  handle_errors (*phdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, errstr,
                 __FILE__, __LINE__);

  /* allocate statement handle for use with table creation and population */

  rc = SQLAllocStmt (*phdbc, phstmt);
  handle_errors (*phdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                 "allocating statement handle", __FILE__, __LINE__);

}

static int ConfigGrid (SQLHDBC   hdbc,
                       SQLHSTMT  hstmt)
{
  SQLRETURN rc;
  char      crTblBuf [2048];

  if (!IsGridEnable (hdbc)) {
    return 0;
  }

#if 0
  /* create tables. */
  if (verbose == VERBOSE_ALL)
    wiscbm_msg0("\nConfiguring grid settings...\n");

  /* configure grid params */
  rc = SQLExecDirect (hstmt, (SQLCHAR*) "call ttSystemConfig('sbGridRouteBufferSize', '65536');", SQL_NTS);
  handle_errors (hdbc, hstmt, rc,  ABORT_DISCONNECT_EXIT,
                 "specifying sbGridRouteBufferSize", __FILE__, __LINE__);
  while ((rc = SQLFetch (hstmt)) != SQL_NO_DATA_FOUND)
  {
    if (rc != SQL_SUCCESS)
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "Error in fetching rows", __FILE__, __LINE__);
  }

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  rc = SQLExecDirect (hstmt, (SQLCHAR*) "call ttSystemConfig('sbGridRouteBufferCount', '4');", SQL_NTS);
  handle_errors (hdbc, hstmt, rc,  ABORT_DISCONNECT_EXIT,
                 "specifying sbGridRouteBufferCount", __FILE__, __LINE__);
  while ((rc = SQLFetch (hstmt)) != SQL_NO_DATA_FOUND)
  {
    if (rc != SQL_SUCCESS)
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "Error in fetching rows", __FILE__, __LINE__);
  }

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  rc = SQLExecDirect (hstmt, (SQLCHAR*) "call ttSystemConfig('sbGridRouteBatchCount', '1');", SQL_NTS);
  handle_errors (hdbc, hstmt, rc,  ABORT_DISCONNECT_EXIT,
                 "specifying sbGridRouteBatchCount", __FILE__, __LINE__);
  while ((rc = SQLFetch (hstmt)) != SQL_NO_DATA_FOUND)
  {
    if (rc != SQL_SUCCESS)
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "Error in fetching rows", __FILE__, __LINE__);
  }

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  rc = SQLExecDirect (hstmt, (SQLCHAR*) "call ttSystemConfig('sbGridRouteQueueCapacity', '256');", SQL_NTS);
  handle_errors (hdbc, hstmt, rc,  ABORT_DISCONNECT_EXIT,
                 "specifying sbGridRouteQueueCapacity", __FILE__, __LINE__);
  while ((rc = SQLFetch (hstmt)) != SQL_NO_DATA_FOUND)
  {
    if (rc != SQL_SUCCESS)
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "Error in fetching rows", __FILE__, __LINE__);
  }

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  rc = SQLFreeStmt (hstmt, SQL_CLOSE);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "closing statement handle", __FILE__, __LINE__);
#endif

  return 1;
}

static void TruncateTables (SQLHDBC   hdbc,
                            SQLHSTMT  hstmt)
{
  int       i;
  SQLRETURN rc;

  /* truncate tables. */
  if (verbose == VERBOSE_ALL)
    wiscbm_msg0("\nTruncating tables...");

  for (i = 0; i < NumCrTblStmts; i++)
  {
    /* Truncate wiscbm tables */
    /*   Ignore errors if they did not exist */
    rc = SQLExecDirect (hstmt, (SQLCHAR*) (truncateTblStmts [i]), SQL_NTS);
  }

  /*
   * Free statement but don't drop it - we'll use it to populate the
   * the tables saves us having to take the cost of freeing and reallocating
   * the resources behind the statement handle
   */

  rc = SQLFreeStmt (hstmt, SQL_CLOSE);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "closing statement handle", __FILE__, __LINE__);

  if (verbose == VERBOSE_ALL) {
    wiscbm_msg0("Completed\n");
  }
  
  /* commit work so far */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);
}


static void BuildDatabase (SQLHDBC   hdbc,
                           SQLHSTMT  hstmt)
{
  int       i;
  SQLULEN   tIso = SQL_TXN_READ_COMMITTED;
  SQLRETURN rc;
  char      crTblBuf [2048];

  /* Only create tables if -nocreate wasn't specified */

  if (!create) {

    /* Make sure tables are empty before populating again. */
    TruncateTables (hdbc, hstmt);

  } else {

    /* create tables. */
    if (verbose == VERBOSE_ALL) {
      wiscbm_msg0("\nCreating tables...");
    }
  
    for (i = 0; i < NumCrTblStmts; i++)
    {
      /* Drop wiscbm tables */
      /*   Ignore errors if they did not exist */
      rc = SQLExecDirect (hstmt, (SQLCHAR*) (dropTblStmts [i]), SQL_NTS);
  
      /* Now create the tables */
      if (grid) {
        sprintf(crTblBuf, "%s", crTblStmtsGrid [i]);
      } else if (grid_compat) {
        sprintf(crTblBuf, "%s", crTblStmtsGridCompat [i]);
      } else {
        sprintf(crTblBuf, "%s", crTblStmts [i]);
      }
      rc = SQLExecDirect (hstmt, (SQLCHAR*) crTblBuf, SQL_NTS);
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "Error in executing statement", __FILE__, __LINE__);
    }
  
    /*
     * Free statement but don't drop it - we'll use it to populate the
     * the tables saves us having to take the cost of freeing and reallocating
     * the resources behind the statement handle
     */
  
    rc = SQLFreeStmt (hstmt, SQL_CLOSE);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "closing statement handle", __FILE__, __LINE__);
  
    if (verbose == VERBOSE_ALL) {
      wiscbm_msg0("Completed\n");
    }
  
    /* commit work so far */
    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction", __FILE__, __LINE__);
  
  }

  if (isoLevel != -1)
  {
    /* explicitly set isolation level */
    switch (isoLevel)
    {
      case 0: tIso = SQL_TXN_SERIALIZABLE;
              break;

      case 1: tIso = SQL_TXN_READ_COMMITTED;
              break;
        /* value was error checked above */
    }

    rc = SQLSetConnectOption (hdbc, SQL_TXN_ISOLATION, tIso);
    handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "setting connection option", __FILE__, __LINE__);
  }

  /* Don't bother populating tables if -truncate is specified */

  if (!truncat) {

    /* populate tables. */
    if (verbose == VERBOSE_ALL) {
      wiscbm_msg0 ("\nPopulating tables...\n");
    }

    /* use dbs lock  ? */
    if (usedbslock)
    {
      rc = SQLExecDirect (hstmt, (SQLCHAR*) "call ttlocklevel('DS')", SQL_NTS);
      handle_errors (hdbc, hstmt, rc,  ABORT_DISCONNECT_EXIT,
                     "specifying dbs lock usage", __FILE__, __LINE__);

      /* make sure dbs lock take effect in next transaction */
      rc = SQLTransact (henv, hdbc, SQL_COMMIT);
      handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction", __FILE__, __LINE__);
    }

    if (nobind)
    {
      populateTableNoBind (hdbc, hstmt, SmallTblName, SmlTblRowCnt,
                           SmlTblRowCnt, TblPopQNum1);
    
      populateTableNoBind (hdbc, hstmt, BPrimeTblName, BigTblRowCnt/10,
                           BigTblRowCnt, TblPopQNum2);
    
      populateTableNoBind (hdbc, hstmt, Big1TblName, BigTblRowCnt,
                           BigTblRowCnt, TblPopQNum3);
    
      populateTableNoBind (hdbc, hstmt, Big2TblName, BigTblRowCnt,
                           BigTblRowCnt, TblPopQNum4);
    } else if (nobatch) {

      populateTableNoBatch (hdbc, hstmt, SmallTblName, SmlTblRowCnt,
                            SmlTblRowCnt, TblPopQNum1, 1 /* bind params */);
    
      populateTableNoBatch (hdbc, hstmt, BPrimeTblName, BigTblRowCnt/10,
                            BigTblRowCnt, TblPopQNum2, 0);
    
      populateTableNoBatch (hdbc, hstmt, Big1TblName, BigTblRowCnt,
                            BigTblRowCnt, TblPopQNum3, 0);
    
      populateTableNoBatch (hdbc, hstmt, Big2TblName, BigTblRowCnt,
                            BigTblRowCnt, TblPopQNum4, 0);
    } else {

      populateTable (hdbc, hstmt, SmallTblName, SmlTblRowCnt,
                     SmlTblRowCnt, TblPopQNum1, 1 /* bind params */, 0);
    
      populateTable (hdbc, hstmt, BPrimeTblName, BigTblRowCnt/10,
                     BigTblRowCnt, TblPopQNum2, 0, 0);
    
      populateTable (hdbc, hstmt, Big1TblName, BigTblRowCnt,
                     BigTblRowCnt, TblPopQNum3, 0, 0);
    
      populateTable (hdbc, hstmt, Big2TblName, BigTblRowCnt,
                     BigTblRowCnt, TblPopQNum4, 0, 1 /* free params */);
    }

  } /* if (!truncat) */

  rc = SQLFreeStmt (hstmt, SQL_RESET_PARAMS);
  handle_errors (hdbc, hstmt, rc, DISCONNECT_EXIT,
                 "resetting parms for the statement handle",
                 __FILE__, __LINE__);

  rc = SQLFreeStmt (hstmt, SQL_CLOSE);
  handle_errors (hdbc, hstmt, rc, DISCONNECT_EXIT,
                 "closing the statement handle",
                 __FILE__, __LINE__);

  if (!truncat) {

    if (verbose == VERBOSE_ALL)
      wiscbm_msg0("\nCalculating table statistics...\n");
    wiscUpdateStats (hdbc, hstmt, SmallTblName);

    if (verbose == VERBOSE_ALL)
      wiscbm_msg1 ("Updating stats for table: %s ...\n", BPrimeTblName);
    wiscUpdateStats (hdbc, hstmt, BPrimeTblName);

    if (verbose == VERBOSE_ALL)
      wiscbm_msg1 ("  Updating stats for table: %s ...\n", Big1TblName);
    wiscUpdateStats (hdbc, hstmt, Big1TblName);

    if (verbose == VERBOSE_ALL)
      wiscbm_msg1 ("  Updating stats for table: %s ...\n", Big2TblName);
    wiscUpdateStats (hdbc, hstmt, Big2TblName);

    if (verbose == VERBOSE_ALL)
    {
      wiscbm_msg1 ("\nSmall table cardinality: %d rows\n",   SmlTblRowCnt);
      wiscbm_msg1 ("Large table cardinalities: %d rows\n\n", BigTblRowCnt);
    }

  } /* if (!truncat) */

  if (usedbslock)
  {
    rc = SQLExecDirect (hstmt, (SQLCHAR*) "call ttlocklevel('Row')", SQL_NTS);
    handle_errors (hdbc, hstmt, rc,  ABORT_DISCONNECT_EXIT,
                   "specifying  row lock usage", __FILE__, __LINE__);

    /* make sure dbs lock take effect in next transaction */
    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "committing transaction", __FILE__, __LINE__);
  }
}

static void DropTables (SQLHDBC   hdbc,
                        SQLHSTMT  hstmt)
{
  int       i;
  SQLRETURN rc;

  /* drop tables. */
  if (verbose == VERBOSE_ALL)
    wiscbm_msg0("\nDropping tables...\n");

  for (i = 0; i < NumCrTblStmts; i++)
  {
    /* Drop wiscbm tables */
    /*   Ignore errors if they did not exist */
    rc = SQLExecDirect (hstmt, (SQLCHAR*) (dropTblStmts [i]), SQL_NTS);
  }

  /*
   * Free statement but don't drop it - we'll use it to populate the
   * the tables saves us having to take the cost of freeing and reallocating
   * the resources behind the statement handle
   */

  rc = SQLFreeStmt (hstmt, SQL_CLOSE);
  handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                 "closing statement handle", __FILE__, __LINE__);

  /* commit work so far */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);
}

static void printVerboseSummary (void)
{
  double  qTimeMedian;
  int     i, j;

  out_msg0("\nQuery\t Time (msec)\tDescription\n");
  out_msg0("-----\t -----------\t-----------\n");

  for (i = 1; i <= MAXNUMQUERIES; i++)
  {
    if ((querySel == 0) || (querySel == BADQUERY) ||
        isQuery (querySel, i))
    {
      qTimeMedian = median (qTime[i], numIters[i]);
      out_msg3 ("%4d\t%12.6f\t%s\n", i, qTimeMedian, QueryTypes[i-1]);
    }
  }

  if (build && (querySel == 0))
  {
    int     tblSizes[4];

    tblSizes[0] = SmlTblRowCnt;
    tblSizes[1] = BigTblRowCnt/10;
    tblSizes[2] = tblSizes[3] = BigTblRowCnt;

    for (i = TblPopQNum1, j=MAXNUMQUERIES+1; i < IxBuildQNum; i++, j++)
    {
      qTimeMedian = median (qTime[i], numIters[i]);
      out_msg4 ("%4d\t%12.6f\t%s (%d rows)\n",
                j, qTimeMedian, QueryTypes[j-1], tblSizes[j-(MAXNUMQUERIES+1)]);
    }
    qTimeMedian = median (qTime[i], numIters[i]);
    out_msg3 ("%4d\t%12.6f\t%s\n", j, qTimeMedian, QueryTypes[j-1]);
  }
}


/*********************************************************************
 *
 *  FUNCTION:       main
 *
 *  DESCRIPTION:    This is the main function of the Wisconsin
 *                  benchmark. It connects to an ODBC data source,
 *                  creates and populates 4 tables, performs the
 *                  selected operations (QUERIES) and reports on the
 *                  results.
 *
 *  PARAMETERS:     int argc        # of command line arguments
 *                  char *argv[]    command line arguments
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int main (int argc, char *argv[])
{
  int       i;
  char      queryBuf [512];
  char*     disableMergeJoin = "";

  SQLHDBC   hdbc;
  SQLHSTMT  hstmt1;
  SQLRETURN rc;


#if defined(TTCLIENTSERVER) && defined(__hppa) && !defined(__LP64__)
  /* hp requires this for c main programs that call c++ shared libs */
  _main();
#endif

  /* Set up default signal handlers */
  if (HandleSignals() != 0) {
    err_msg0("Unable to set signal handlers\n");
    return 1;
  }
  StopRequestClear();

  /* initialize the file for status messages */
  statusfp  = stdout;

  srand48 (12345678);

  connstr[0] = '\0';

  /* parse the command line arguments */
  i = parse_args (argc, argv);

  switch (i) {

    case 0: return 1;

    case 1: break;

    case 2: return 0;

    default: return 2;
  }

  AllocAndConnectToDb (&henv, &hdbc, &hstmt1);

  /* Determine if we're running with grid-enabled connection */
  grid = ConfigGrid (hdbc, hstmt1);

  if (grid) {
    wiscbm_msg0 ("Grid enabled\n");
    grid_compat = 1;
  } else if (grid_compat) {
    wiscbm_msg0 ("Grid compatibility enabled\n");
  }

  /* Optionally create a new database, then create and populate tables
   * (if the -nobuild flag is NOT specified) */

  if (build) {
    BuildDatabase (hdbc, hstmt1);
  }

  /* Run queries unless -noqueries specified. */

  if (queries) {

    if (grid_compat) {
      /* Merge join not yet fully supported on grid */
      /* See bug http://rg1-6.us.oracle.com:3000/issues/267 */
      disableMergeJoin = "/*+ tt_mergejoin(0) */ ";
    }

    /* execute the non-indexed queries */
    wiscbm_msg0("Executing non-indexed queries...\n");
  
    /* query 1 (1% selection on big table, no index) */
  
    executeSelect (1, henv, hdbc,
                   "select * from big1 where unique2 between %d and %d",
                   "select * from big2 where unique2 between %d and %d",
                   BigTblRowCnt, BigTblRowCnt/100, 0, 5, 2);
  
    /* query 2 (10% selection on big table, no index) */
  
    executeSelect (2, henv, hdbc,
                   "select * from big1 where unique2 between %d and %d",
                   "select * from big2 where unique2 between %d and %d",
                   BigTblRowCnt, BigTblRowCnt/10, 0, 5, 2);
  
    /* query 9 (one join with filtered input, no index) */
  
    sprintf (queryBuf, "select * from big1, big2 where "
                       "big1.unique2 = big2.unique2 and "
                       "big2.unique2 < %d", BigTblRowCnt/10);
    executeSelect (9, henv, hdbc, queryBuf, queryBuf, BigTblRowCnt,
                   BigTblRowCnt/10, 0, 2, 0);

    /* query 10 (one join, no index) */

    executeSelect (10, henv, hdbc,  "select * from big1, bprime "
                   "where big1.unique2 = bprime.unique2",
                   "select * from big1, bprime "
                   "where big1.unique2 = bprime.unique2",
                   BigTblRowCnt, BigTblRowCnt/10, 0, 2, 0);

    /* query 11 (two joins with filtered input, no index) */

    sprintf (queryBuf, "select %s* from small, big1, big2 "
                       "where  big1.unique2 = big2.unique2 and "
                       "small.unique2 = big1.unique2 and "
                       "big1.unique2 < %d", disableMergeJoin, BigTblRowCnt/10);
    executeSelect (11, henv, hdbc, queryBuf, queryBuf, BigTblRowCnt,
                   BigTblRowCnt/10, 0, 2, 0);
  
    /* query 18 (projection, 1%) */
  
    executeSelect (18, henv, hdbc,
                   "select distinct two, four, "
                   "ten, twenty, onePercent, string4 from big1",
                   "select distinct two, four, ten, twenty, onePercent, "
                   "string4 from big2",
                    BigTblRowCnt, 400, 0, 2, 0);
  
    /* query 19 (projection, 100%) */
  
    executeSelect (19, henv, hdbc,
                   "select distinct two, four, ten, twenty, onePercent, "
                   "tenPercent, twentyPercent, fiftyPercent, unique3, "
                   "evenOnePercent, oddOnePercent, stringu1, stringu2, "
                   "string4 from big1",
                   "select distinct two, four, ten, twenty, onePercent, "
                   "tenPercent, twentyPercent, fiftyPercent, unique3, "
                   "evenOnePercent, oddOnePercent, stringu1, stringu2, "
                   "string4 from big2",
                    BigTblRowCnt, BigTblRowCnt, 0, 2, 0);
  
    /* query 20 (min. agg., no grouping) */
  
    executeSelect (20, henv, hdbc,  "select min(unique2) from big1",
                   "select min(unique2) from big2",
                   BigTblRowCnt, 1, 0, 2, 0);
  
    /* query 21 (min. agg., 100 groups) */
  
    executeSelect (21, henv, hdbc,
                   "select min(unique2) from big1 group by onePercent",
                   "select min(unique2) from big1 group by onePercent",
                   BigTblRowCnt, 100, 0, 2, 0);
  
    /* query 22 (sum agg., 100 groups) */
  
    executeSelect (22, henv, hdbc,
                   "select sum(unique2) from big1 group by onePercent",
                   "select sum(unique2) from big1 group by onePercent",
                   BigTblRowCnt, 100, 0, 2, 0);
  
    /* query 26 (insert one row) */
  
    doInsert(26, henv, hdbc);
  
    /* query 27 (delete one row) */
  
    doDelete(27, henv, hdbc);
  
    /* query 28 (update key attribute) */
  
    doUpdate(28, henv, hdbc, "unique2");

  } /* if (queries) */

  /*
   * Use statement handle allocated to create tables
   * as handle for creating indices
   */

  if (create) {

#ifndef NO_INDEXES

    /* create indexes */

    if (verbose == VERBOSE_ALL) {
      wiscbm_msg0 ("\nCreating indexes...");
    }

    /* lock database ? */
    if (usedbslock) {
      rc = SQLExecDirect (hstmt1, (SQLCHAR*) "call ttlocklevel('DS')", SQL_NTS);
      handle_errors (hdbc, hstmt1, rc,  ABORT_DISCONNECT_EXIT,
                     "specifying dbs lock usage", __FILE__, __LINE__);
      /* make sure dbs lock take effect in next transaction */
      rc = SQLTransact (henv, hdbc, SQL_COMMIT);
      if (rc != SQL_SUCCESS) {
        handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                       "committing transaction", __FILE__, __LINE__);
      }
    }

    StartTimer (0);

    for (i = 0; i < NumCrIxStmts; i++) {

      if ((i % 2) == 0 && grid_compat) {
        /* skip indexes on unique1 column since distributed tables already have primary key */
        continue;
      }
      rc = SQLExecDirect (hstmt1, (SQLCHAR*) crIxStmts[i], SQL_NTS);
      handle_errors (hdbc, hstmt1, rc, ABORT_DISCONNECT_EXIT,
                     "Error in executing statement",
                     __FILE__, __LINE__);
    }

    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction", __FILE__, __LINE__);

    StopTimer (0);

    if (usedbslock) {
      rc = SQLExecDirect (hstmt1, (SQLCHAR*) "call ttlocklevel('Row')", SQL_NTS);
      handle_errors (hdbc, hstmt1, rc, ABORT_DISCONNECT_EXIT,
                     "specifying row lock usage",
                     __FILE__, __LINE__);

      /* make sure dbs lock take effect in next transaction */
      rc = SQLTransact (henv, hdbc, SQL_COMMIT);
      handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction", __FILE__, __LINE__);
    }

    if (verbose == VERBOSE_ALL) {
      wiscbm_msg0 ("Completed\n");
    }

#endif

  }

  numIters [IxBuildQNum]    = 1;
  qTime   [IxBuildQNum][0] = WallClkTime (0);

  /*
   * Close statement handle (don't drop as we'll use it to clean up
   * the data store when we exit)
   */

  rc = SQLFreeStmt (hstmt1, SQL_CLOSE);
  handle_errors (hdbc, hstmt1, rc, ABORT_DISCONNECT_EXIT,
                 "closing statement handle", __FILE__, __LINE__);

  /* Run queries unless -noqueries specified. */

  if (queries) {

    /* execute indexed queries */
    wiscbm_msg0 ("Executing indexed queries...\n");

    /* query 3 (1% selection on big table, index) */

    executeSelect (3, henv, hdbc,
                   "select * from big1 where unique2 between %d and %d",
                   "select * from big2 where unique2 between %d and %d",
                   BigTblRowCnt, BigTblRowCnt/100, 0, 5, 2);

    /* query 4 (10% selection on big table, index) */

    executeSelect (4, henv, hdbc,
                   "select * from big1 where unique2 between %d and %d",
                   "select * from big2 where unique2 between %d and %d",
                   BigTblRowCnt, BigTblRowCnt/10, 0, 5, 2);

    /* query 5 (1% selection on big table, index) */

    executeSelect (5, henv, hdbc,
                   "select * from big1 where unique1 between %d and %d",
                   "select * from big2 where unique1 between %d and %d",
                   BigTblRowCnt, BigTblRowCnt/100, 0, 5, 2);

    /* query 6 (10% selection on big table, index) */

    executeSelect (6, henv, hdbc,
                   "select * from big1 where unique1 between %d and %d",
                   "select * from big2 where unique1 between %d and %d",
                   BigTblRowCnt, BigTblRowCnt/10, 0, 5, 2);

    /* query 7 (1 row selection on big table, index) */

    executeSelect (7, henv, hdbc,
                   "select * from big1 where unique2 = %d",
                   "select * from big2 where unique2 = %d",
                   BigTblRowCnt, 1, 1, 5, 1);

    /* query 8 (1% selection on big table, index) */

    executeSelect (8, henv, hdbc,
                   "select * from big1 where unique2 between %d and %d",
                   "select * from big2 where unique2 between %d and %d",
                   BigTblRowCnt, BigTblRowCnt/100, 0, 5, 2);

    /* query 12 (one join with filtered input, no index) */

    sprintf (queryBuf, "select * from big1, big2 "
             "where big1.unique2 = big2.unique2 and "
             "big2.unique2 < %d", BigTblRowCnt/10);
    executeSelect (12, henv, hdbc, queryBuf, queryBuf, BigTblRowCnt,
                   BigTblRowCnt/10, 0, 2, 0);

    /* query 13 (one join, index) */

    sprintf (queryBuf, "select %s* from big1, bprime "
                       "where big1.unique2 = bprime.unique2",
                       disableMergeJoin);
    executeSelect (13, henv, hdbc, queryBuf, queryBuf,
                   BigTblRowCnt, BigTblRowCnt/10, 0, 2, 0);

    /* query 14 (two joins with filtered input, index) */

    sprintf (queryBuf, "select * from small, big1, big2 "
             "where  big1.unique2 = big2.unique2 and "
             "small.unique2 = big1.unique2 and "
             "big1.unique2 < %d", BigTblRowCnt/10);
    executeSelect (14, henv, hdbc, queryBuf, queryBuf, BigTblRowCnt,
                   BigTblRowCnt/10, 0, 2, 0);

    /* query 15 (one join with filtered input, no index) */

    sprintf (queryBuf, "select * from big1, big2 "
             "where big1.unique1 = big2.unique1 and "
             "big2.unique1 < %d", BigTblRowCnt/10);
    executeSelect (15, henv, hdbc, queryBuf, queryBuf, BigTblRowCnt,
                   BigTblRowCnt/10, 0, 2, 0);

    /* query 33 (one join with filter matching nothing, no index) */

    sprintf (queryBuf, "select * from big1, big2 "
             "where big1.unique1 = big2.unique1 and "
             "big2.stringu1 like 'NONE%%%%'");
    executeSelect (33, henv, hdbc, queryBuf, queryBuf, BigTblRowCnt,
                   0, 0, 2, 0);

    /* query 16 (one join, index) */

    executeSelect (16, henv, hdbc,
                   "select * from big1, bprime "
                   "where big1.unique1 = bprime.unique1",
                   "select * from big1, bprime "
                   "where big1.unique1 = bprime.unique1",
                   BigTblRowCnt, BigTblRowCnt/10, 0, 2, 0);

    /* query 17 (two joins with filtered input, index) */

    sprintf (queryBuf, "select * from small, big1, big2 "
             "where  big1.unique1 = big2.unique1 and "
             "small.unique1 = big1.unique1 and "
             "big1.unique1 < %d", BigTblRowCnt/10);
    executeSelect (17, henv, hdbc, queryBuf, queryBuf, BigTblRowCnt,
                   BigTblRowCnt/10, 0, 2, 0);

    /* query 23 (min. agg., no grouping) */

    executeSelect (23, henv, hdbc, "select min(unique2) from big1",
                   "select min(unique2) from big2",
                   BigTblRowCnt, 1, 0, 2, 0);

    /* query 24 (min. agg., 100 groups) */

    executeSelect (24, henv, hdbc,
                   "select min(unique2) from big1 group by onePercent",
                   "select min(unique2) from big1 group by onePercent",
                   BigTblRowCnt, 100, 0, 2, 0);

    /* query 25 (sum agg., 100 groups) */

    executeSelect (25, henv, hdbc,
                   "select sum(unique2) from big1 group by onePercent",
                   "select sum(unique2) from big1 group by onePercent",
                   BigTblRowCnt, 100, 0, 2, 0);

    /* query 29 (insert one row) */
      
    doInsert (29,henv, hdbc);
      
    /* query 30 (delete one row) */
      
    doDelete (30,henv, hdbc);
      
    /* query 31 (update key attribute) */
      
    doUpdate (31,henv, hdbc, "unique2");

    /* query 32 (update key attribute) */

    doUpdate (32,henv, hdbc, "unique1");

    /* Print per query statistics */

    if (verbose) {
      printVerboseSummary ();
    }

  } /* if (queries) */

  /* This is the end of the wiscbm */
  if (verbose == VERBOSE_ALL) {
    wiscbm_msg0 ("\nExecution completed...\n");
  }

  if (truncat) {
    TruncateTables (hdbc, hstmt1);
  }

  if (drop) {
    DropTables (hdbc, hstmt1);
  }

  QuitTest (henv, hdbc, 0, NO_XA_ABORT);

  return 0;
}


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
