/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*

NAME
plsqlODBC.c - ODBC demo that uses PLSQL

DESCRIPTION
This ODBC program does the following:
1. Uses a PLSQL package procedure
2. Uses a PLSQL package function
3. Uses a PLSQL anonymous block with host variables
4. Opens a Ref cursor in a PLSQL procedure
   and processes the resultset in ODBC

EXPORT FUNCTION(S)
<external functions defined for use outside package - one-line descriptions>
  connectDB()    - Connect to TimesTen DB
  givePayRaise   - Use a PLSQL package procedure to randomly give one of the lowest paid workers a pay raise.
  plsqlblock()   - Use a PLSQL block to get some details on the worker who got the payraise
  getEmpName     - Use a PLSQL package function to get the name of an employee based on their empid
  getSalesPeople - Open a Ref cursor and use ODBC to iterate over the result set.

INTERNAL FUNCTION(S)
<other external functions defined - one-line descriptions>

STATIC FUNCTION(S)
<static functions defined - one-line descriptions>

NOTES
<other useful comments, qualifications, etc.>

 */

/*
  Use the TimesTen Quick Start for instructions to build and run this program.
*/


/* This source is best displayed with a tabstop of 4 */



#ifdef WIN32
#include <windows.h>
#else
#include <sqlunix.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
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

#define EMP_NAME_LEN    64
#define ERR_TEXT_LEN    255

static char usageStr[] =
  "Usage:\t<CMD> {-h | -help | -V}\n"
  "\t<CMD> [<DSN> | -connstr <connection-string>]\n\n"
  "  -h                  Prints this message and exits.\n"
  "  -help               Same as -h.\n"
  "  -V                  Prints version number and exits.\n"
  "The default if no DSN or connection-string is given is:\n"
  "  \"DSN=sampledb;UID=appuser\".\n\n";




/* Global parameters and flags (typically set by parse_args()) */

static char connstr[CONN_STR_LEN];   /* Connection string. */
static char dsn[CONN_STR_LEN];       /* DSN. */
static char cmdname[80];             /* Stripped command name. */
FILE   *statusfp;                    /* File for status messages */

SQLHENV henv;                        /* Environment handle */
SQLHDBC hdbc;

int         numEmps = 0;     /* The number of lowest paid employees to choose from */
int         empNo   = 0;     /* The employee ID of interest */
char        luckyEmp[EMP_NAME_LEN + 1];    /* The employee who got a 10% payrise */
int         errCode = 0;     
char        errText[ERR_TEXT_LEN + 1];
char        jobtype[9+1];
char        hired[20+1];
double      salary;
int         dept;
int         worked_longer;
int         higher_sal;
int         total_in_dept;


char * exec_plsql_proc =
  "begin \
    emp_pkg.givePayRaise(:numEmps, :luckyEmp, :errCode, :errText); \
  end;";

char * exec_plsql_fn =
  "begin \
     :luckyEmp := sample_pkg.getEmpName(:empNo, :errCode, :errText); \
  end;";


char * exec_OpenSalesPeopleCursor = 
  "begin \
     emp_pkg.OpenSalesPeopleCursor(:cmdRefCursor, :errCode, :errText); \
   end;"; 


void givePayRaise(    void );
void plsqlblock(     void );
void getEmpName(     void );
void getSalesPeople( void );
void connectDB(      void );
void disconnectDB(   void );


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

  ttc_getcmdname(argv[0], cmdname, sizeof(cmdname));

  ac = ttc_getoptions(argc, argv, TTC_OPT_NO_CONNSTR_OK,
                    errbuf, sizeof(errbuf),
                    "<HELP>",         usageStr,
                    "<VERSION>",      NULL,
                    "<CONNSTR>",      connstr, sizeof(connstr),
                    "<DSN>",          dsn, sizeof(dsn),
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
     */

    sprintf(connstr, "%s;%s",
            DSNNAME, UID);
  }

}



/*********************************************************************************************/
/* Use a PLSQL package procedure to randomly give one of the lowest paid workers a payraise. */
/*********************************************************************************************/
void givePayRaise()
{
  SQLHSTMT           plsql_proc;
  SQLRETURN          rc;
  SQLLEN             luckyEmpLen;
  SQLLEN             errTextLen;


  /* Initialize the emp_pkg.givePayRaise PLSQL procedure params */
  errCode = 0;
  memset(errText,  0, sizeof(errText));
  memset(luckyEmp, 0, sizeof(luckyEmp));
  numEmps = 10;


  /* Allocate a statement for the PLSQL procedure */
  rc = SQLAllocStmt(hdbc, &plsql_proc);

  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                "allocating plsql_proc statement handle",
                __FILE__, __LINE__);

  /* prepare the PLSQL procedure emp_pkg.givePayRaise  */
  rc = SQLPrepare(plsql_proc,
                  (SQLCHAR *) exec_plsql_proc,
                  SQL_NTS);

  handle_errors(hdbc, plsql_proc, rc, ABORT_DISCONNECT_EXIT,
                "preparing plsql proc",
                __FILE__, __LINE__);


  /* Bind the procedure IN and OUT parameters */
  rc = SQLBindParameter(plsql_proc, 
                        (SQLUSMALLINT) 1,
                        SQL_PARAM_INPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &numEmps,
                        0,
                        NULL);

  handle_errors(hdbc, plsql_proc, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(plsql_proc, 
                        (SQLUSMALLINT) 2,
                        SQL_PARAM_OUTPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        EMP_NAME_LEN,
                        0,
                        luckyEmp,
                        EMP_NAME_LEN,
                        &luckyEmpLen);

  handle_errors(hdbc, plsql_proc, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(plsql_proc, 
                        (SQLUSMALLINT) 3,
                        SQL_PARAM_OUTPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &errCode,
                        0,
                        NULL);

  handle_errors(hdbc, plsql_proc, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(plsql_proc, 
                        (SQLUSMALLINT) 4,
                        SQL_PARAM_OUTPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        ERR_TEXT_LEN,
                        0,
                        errText,
                        ERR_TEXT_LEN,
                        &errTextLen);

  handle_errors(hdbc, plsql_proc, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  /* Execute the procedure */
  rc = SQLExecute(plsql_proc);


  handle_errors(hdbc, plsql_proc, rc, ABORT_DISCONNECT_EXIT,
                "Error executing proc",
                __FILE__, __LINE__);

  printf("The employee who got the 10%% pay raise was %s\n", luckyEmp);


  /* free the statement */
  rc = SQLFreeStmt(plsql_proc, SQL_DROP);
  handle_errors(hdbc, plsql_proc, rc, ABORT_DISCONNECT_EXIT,
                "dropping the statement handle",
                __FILE__, __LINE__);


}  /* givePayRaise */



/**************************************************************************************/
/* Use a PLSQL anonymous block to get some details on the worker who got the payraise */
/**************************************************************************************/
void plsqlblock()
{
  SQLHSTMT           anon_block;
  SQLRETURN          rc;
  SQLLEN             luckyEmpLen;
  SQLLEN             errTextLen;
  SQLLEN             jobTypeLen;
  SQLLEN             hiredLen;
  char               anon_plsqlblock[2048];


  /* Initialize the input and output parameters for the plsql block */
  memset(anon_plsqlblock, 0, sizeof(anon_plsqlblock));
  memset(jobtype,         0, sizeof(jobtype));
  memset(hired,           0, sizeof(hired));
  memset(errText,         0, sizeof(errText));
  errCode       = 0;
  salary        = 0.0;
  dept          = 0;
  worked_longer = 0;
  higher_sal    = 0;
  total_in_dept = 0;

  /* Need to know the length of the IN param luckyEmp */
  luckyEmpLen = strlen(luckyEmp);


  /* Create the text for the PLSQL anonymous block */
  strcpy(anon_plsqlblock, "BEGIN");

  strcat(anon_plsqlblock, "  SELECT job, hiredate, sal, deptno");
  strcat(anon_plsqlblock, "    INTO :jobtype, :hired, :salary, :dept");
  strcat(anon_plsqlblock, "  FROM emp");
  strcat(anon_plsqlblock, "  WHERE ename = UPPER(:luckyEmp);");

  strcat(anon_plsqlblock, "  SELECT count(*)");
  strcat(anon_plsqlblock, "    INTO :worked_longer");
  strcat(anon_plsqlblock, "  FROM emp");
  strcat(anon_plsqlblock, "  WHERE hiredate < :hired;");

  strcat(anon_plsqlblock, "  SELECT count(*)");
  strcat(anon_plsqlblock, "    INTO :higher_sal");
  strcat(anon_plsqlblock, "  FROM emp");
  strcat(anon_plsqlblock, "  WHERE sal > :salary;");

  strcat(anon_plsqlblock, "  SELECT count(*)");
  strcat(anon_plsqlblock, "    INTO :total_in_dept");
  strcat(anon_plsqlblock, "  FROM emp");
  strcat(anon_plsqlblock, "  WHERE deptno = :dept;");

  strcat(anon_plsqlblock, " EXCEPTION");

  strcat(anon_plsqlblock, "    WHEN OTHERS THEN");
  strcat(anon_plsqlblock, "       :errCode  := SQLCODE;");
  strcat(anon_plsqlblock, "       :errText  := SUBSTR(SQLERRM, 1, 200);");

  strcat(anon_plsqlblock, " END;");



  /* Allocate a statement for the PLSQL block */
  rc = SQLAllocStmt(hdbc, &anon_block);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  /* prepare the PLSQL statement */
  rc = SQLPrepare(anon_block,
                  (SQLCHAR *) anon_plsqlblock,
                  SQL_NTS);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "preparing anon plsql block",
                __FILE__, __LINE__);


  /* Bind the IN and OUT parameters */
  rc = SQLBindParameter(anon_block, 
                        (SQLUSMALLINT) 1,
                        SQL_PARAM_OUTPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        sizeof(jobtype),
                        0,
                        jobtype,
                        sizeof(jobtype),
                        &jobTypeLen);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter jobtype",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block, 
                        (SQLUSMALLINT) 2,
                        SQL_PARAM_OUTPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        sizeof(hired),
                        0,
                        hired,
                        sizeof(hired),
                        &hiredLen);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter hired",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block,
                        (SQLUSMALLINT) 3,
                        SQL_PARAM_OUTPUT,
                        SQL_C_DOUBLE,
                        SQL_DOUBLE,
                        9,
                        2,
                        &salary,
                        sizeof(salary),
                        NULL);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter salary",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block,
                        (SQLUSMALLINT) 4,
                        SQL_PARAM_OUTPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &dept,
                        0,
                        NULL);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter dept",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block, 
                        (SQLUSMALLINT) 5,
                        SQL_PARAM_INPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        EMP_NAME_LEN,
                        0,
                        luckyEmp,
                        EMP_NAME_LEN,
                        &luckyEmpLen);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter luckyEmp",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block,
                        (SQLUSMALLINT) 6,
                        SQL_PARAM_OUTPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &worked_longer,
                        0,
                        NULL);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter worked_longer",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block,
                        (SQLUSMALLINT) 7,
                        SQL_PARAM_OUTPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &higher_sal,
                        0,
                        NULL);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter higher_sal",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block,
                        (SQLUSMALLINT) 8,
                        SQL_PARAM_OUTPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &total_in_dept,
                        0,
                        NULL);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter total_in_dept",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block,
                        (SQLUSMALLINT) 9,
                        SQL_PARAM_OUTPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &errCode,
                        0,
                        NULL);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter errCode",
                __FILE__, __LINE__);

  rc = SQLBindParameter(anon_block,
                        (SQLUSMALLINT) 10,
                        SQL_PARAM_OUTPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        ERR_TEXT_LEN,
                        0,
                        errText,
                        ERR_TEXT_LEN,
                        &errTextLen);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter errText",
                __FILE__, __LINE__);


  /* Execute the anon PLSQL block */
  rc = SQLExecute(anon_block);

  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "Execute anaon plsql block",
                __FILE__, __LINE__);


  if (0 == errCode)
  {

      /* Display the employee's information */
      printf("\n");
      printf("Name:        %s\n", luckyEmp);
      printf("Job:         %s\n", jobtype);
      printf("Hired:       %s\t(%2d people have served longer).\n", hired, worked_longer);
      printf("Salary:      $%.2f\t\t\t(%2d people have a higher salary).\n", salary, higher_sal);
      printf("Department:  %d\t\t\t\t(%2d people in the department).\n\n", dept, total_in_dept);

  }
  else
  {
      printf("A problem with plsqlblock:");
      printf("  Error: %d\n", errCode);
      printf("  Error Text: %s\n", errText);
  }

  /* Free the statement */
  rc = SQLFreeStmt(anon_block, SQL_DROP);
  handle_errors(hdbc, anon_block, rc, ABORT_DISCONNECT_EXIT,
                "dropping the statement handle",
                __FILE__, __LINE__);

}  /* plsqlblock */



/************************************************************************************/
/* Use a PLSQL package function to get the name of an employee based on their empid */
/************************************************************************************/
void getEmpName()
{
  SQLHSTMT           plsql_func;
  SQLRETURN          rc;
  SQLLEN             luckyEmpLen;
  SQLLEN             errTextLen;


  /* Initialize the PLSQL proc params */
  memset(luckyEmp, 0, sizeof(luckyEmp));
  memset(errText,  0, sizeof(errText));
  errCode =    0;
  empNo   = 7839;


  /* Allocate a statement for the PLSQL function */
  rc = SQLAllocStmt(hdbc, &plsql_func);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  /* Prepare the PLSQL function sample_pkg.getEmpName */
  rc = SQLPrepare(plsql_func,
                  (SQLCHAR *) exec_plsql_fn, 
                  SQL_NTS);

  handle_errors(hdbc, plsql_func, rc, ABORT_DISCONNECT_EXIT,
                "preparing plsql func",
                __FILE__, __LINE__);


  /* Bind the IN and OUT parameters */
  rc = SQLBindParameter(plsql_func, 
                        (SQLUSMALLINT) 1,
                        SQL_PARAM_OUTPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        EMP_NAME_LEN,
                        0,
                        luckyEmp,
                        EMP_NAME_LEN,
                        &luckyEmpLen);

  handle_errors(hdbc, plsql_func, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(plsql_func, 
                        (SQLUSMALLINT) 2,
                        SQL_PARAM_INPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &empNo,
                        0,
                        NULL);

  handle_errors(hdbc, plsql_func, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(plsql_func, 
                        (SQLUSMALLINT) 3,
                        SQL_PARAM_OUTPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &errCode,
                        0,
                        NULL);

  handle_errors(hdbc, plsql_func, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(plsql_func, 
                        (SQLUSMALLINT) 4,
                        SQL_PARAM_OUTPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        ERR_TEXT_LEN,
                        0,
                        errText,
                        ERR_TEXT_LEN,
                        &errTextLen);

  handle_errors(hdbc, plsql_func, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);


  /* Execute the PLSQ function */
  rc = SQLExecute(plsql_func);

  handle_errors(hdbc, plsql_func, rc, ABORT_DISCONNECT_EXIT,
                "Error executing func",
                __FILE__, __LINE__);


  printf("The employee with empid %d was %s\n", empNo, luckyEmp);

  /* Free the statement */
  rc = SQLFreeStmt(plsql_func, SQL_DROP);
  handle_errors(hdbc, plsql_func, rc, ABORT_DISCONNECT_EXIT,
                "dropping the statement handle",
                __FILE__, __LINE__);

}  /* getEmpName */




/**************************************************************************/
/* Use a PLSQL Packge procedure to open a ref cursor for all sales people */
/* Use ODBC to iterate over of the resultset of the ref cursor            */
/**************************************************************************/
void getSalesPeople()
{
  SQLHSTMT           plsql_OpenSalesPeopleCursor;  /* call the PLSQL proc to open the ref cursor */
  SQLHSTMT           ref_cursor;       /* Ref Cursor returned by PL/SQL */  
  SQLHSTMT           real_ref_cursor;  /* Use the returned ref cursor in ODBC */

  SQLRETURN          rc;

  SQLLEN             luckyEmpLen;
  SQLLEN             errTextLen;


  /* Allocate the statement for the ODBC Ref Cursor */
  rc = SQLAllocStmt(hdbc, &ref_cursor);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  /* Allocate the statement for the PLSQL procedure */
  rc = SQLAllocStmt(hdbc, &plsql_OpenSalesPeopleCursor);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  /* Prepare the PLSQL statement */
  rc = SQLPrepare(plsql_OpenSalesPeopleCursor,
                  (SQLCHAR *) exec_OpenSalesPeopleCursor,
                  SQL_NTS);

  handle_errors(hdbc, plsql_OpenSalesPeopleCursor, rc, ABORT_DISCONNECT_EXIT,
                "preparing procedure OpenSalesPeopleCursor",
                __FILE__, __LINE__);

  /*
  ** A Driver Manager may wrap a hStmt into its own structure. 
  ** Call SQLGetInfo() to get a handle to the real underlying hStmt (the one 
  ** used by the actual ODBC driver) and use that in the SQLBindParameter() 
  ** call.
  ** If there is no driver manager, then SQLGetInfo() just returns a pointer 
  ** to the current hStmt.
  */
  real_ref_cursor = ref_cursor;
  rc = SQLGetInfo(hdbc, SQL_DRIVER_HSTMT, &real_ref_cursor, sizeof(real_ref_cursor), 0);

  handle_errors(hdbc, plsql_OpenSalesPeopleCursor, rc, ABORT_DISCONNECT_EXIT,
                "Calling SQLGetInfo()",
                __FILE__, __LINE__);

  /* Bind the PLSQL OUT parameters */
  rc = SQLBindParameter(plsql_OpenSalesPeopleCursor,
                        (SQLUSMALLINT) 1,
                        SQL_PARAM_OUTPUT,
                        SQL_C_REFCURSOR,
                        SQL_REFCURSOR,
                        0,
                        0,
                        real_ref_cursor,
                        0,
                        NULL);

  handle_errors(hdbc, plsql_OpenSalesPeopleCursor, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter ref cursor",
                __FILE__, __LINE__);

  rc = SQLBindParameter(plsql_OpenSalesPeopleCursor,
                        (SQLUSMALLINT) 2,
                        SQL_PARAM_OUTPUT,
                        SQL_C_SLONG,
                        SQL_INTEGER,
                        0,
                        0,
                        &errCode,
                        0,
                        NULL);

  handle_errors(hdbc, plsql_OpenSalesPeopleCursor, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter errCode",
                __FILE__, __LINE__);

  rc = SQLBindParameter(plsql_OpenSalesPeopleCursor,
                        (SQLUSMALLINT) 3,
                        SQL_PARAM_OUTPUT,
                        SQL_C_CHAR,
                        SQL_VARCHAR,
                        ERR_TEXT_LEN,
                        0,
                        errText,
                        ERR_TEXT_LEN,
                        &errTextLen);


  handle_errors(hdbc, plsql_OpenSalesPeopleCursor, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter errText",
                __FILE__, __LINE__);


  /* Initialize the ref cursor params */
  errCode = 0;
  memset(errText, 0, sizeof(errText));


  /* Execute the PLSQL procedure OpenSalesPeopleCursor */
  rc = SQLExecute(plsql_OpenSalesPeopleCursor);

  handle_errors(hdbc, plsql_OpenSalesPeopleCursor, rc, ABORT_DISCONNECT_EXIT,
                "Error executing plsql_OpenSalesPeopleCursor",
                __FILE__, __LINE__);

  if (0 == errCode) {


    /* We know the number and type of the Ref Cursor columns so just bind them */
    rc = SQLBindCol(ref_cursor, 1, SQL_C_SLONG, &empNo, 0, NULL);

    handle_errors(hdbc, ref_cursor, rc, ABORT_DISCONNECT_EXIT,
                  "Error in SQLBindCol for empNo",
                  __FILE__, __LINE__);

    rc = SQLBindCol(ref_cursor, 2, SQL_C_CHAR, luckyEmp, sizeof(luckyEmp), &luckyEmpLen);

    handle_errors(hdbc, ref_cursor, rc, ABORT_DISCONNECT_EXIT,
                  "Error in SQLBindCol for luckyEmp",
                  __FILE__, __LINE__);

    rc = SQLBindCol(ref_cursor, 3, SQL_C_DOUBLE, &salary, 0, NULL);

    handle_errors(hdbc, ref_cursor, rc, ABORT_DISCONNECT_EXIT,
                  "Error in SQLBindCol for salary",
                  __FILE__, __LINE__);


    /* Iterate over the ref cursors result set */
    printf("\nThe sales people are:\n\n");
    printf("  EMPNO ENAME      SALARY\n");
    printf("  ===== ========== ==========\n");

    while((rc = SQLFetch(ref_cursor)) != SQL_NO_DATA_FOUND){

      printf("  %5.0d %-10s %10.2f\n", empNo, luckyEmp, salary);
    }


    /* close the ref cursor */
    rc = SQLFreeStmt(ref_cursor, SQL_CLOSE);


  } else {

    printf("Did not call ref cursor OK\n");
  }

  /* Drop the ref Cursor ODBC statment */
  rc = SQLFreeStmt(ref_cursor, SQL_DROP);
  handle_errors(hdbc, ref_cursor, rc, ABORT_DISCONNECT_EXIT,
                "dropping the statement handle",
                __FILE__, __LINE__);

  /* Drop the PLSQL statment */
  rc = SQLFreeStmt(plsql_OpenSalesPeopleCursor, SQL_DROP);
  handle_errors(hdbc, plsql_OpenSalesPeopleCursor, rc, ABORT_DISCONNECT_EXIT,
                "dropping the statement handle",
                __FILE__, __LINE__);

}  /* getSalesPeople */



/*****************************************/
/* Connect to the TimesTen DB using ODBC */
/*****************************************/
void connectDB( void )
{
  /* variables for ODBC */
  SQLRETURN          rc;

  /* variables used for setting up the tables */
  char               errstr[4096];


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

  printf("\nConnecting with connect string %s\n", connstr);

  rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) connstr, SQL_NTS,
                        NULL, 0, NULL,
                        SQL_DRIVER_COMPLETE);

  sprintf(errstr, "connecting to driver (connect string %s)\n",
          connstr);

  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                errstr, __FILE__, __LINE__);

  printf("Connected\n\n");

  /* Turn auto-commit off */
  rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                "switching off the AUTO_COMMIT option",
                __FILE__, __LINE__);


  /* commit transaction */
  rc = SQLTransact(henv, hdbc, SQL_COMMIT);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "committing transaction",
                __FILE__, __LINE__);

}  /* connectDB */


/*****************************************/
/* Disconnect to the TimesTen DB using ODBC */
/*****************************************/
void disconnectDB( void )
{
  /* variables for ODBC */
  SQLRETURN          rc;


  printf("\nDisconnecting\n");
  /* Disconnect and return */

  /* commit transaction */
  rc = SQLTransact(henv, hdbc, SQL_COMMIT);
  handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                "committing transaction",
                __FILE__, __LINE__);

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

  printf("Disconnected\n\n");


}  /* disconnectDB */


/*********************************************************************
 *
 *  FUNCTION:       main
 *
 *  DESCRIPTION:    This is the main function of the odbcPLSQL program.
 *                  It connects to an ODBC database, and
 *                  calls the PLSQL procedures, functions, 
 *                  anon blocks and Ref cursors
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

  /* initialize the file for status messages */
  statusfp = stdout;

  /* parse the command arguments */
  parse_args(argc, argv);

  /* Connect to the DB */
  connectDB();


  /* Use a PLSQL package procedure to randomly give one of the lowest paid workers a pay raise. */
  givePayRaise();

  /* Use a PLSQL block to get some details on the worker who got the payraise */
  plsqlblock();

  /* Use a PLSQL package function to get the name of an employee based on their empid */
  getEmpName();

  /* Use a PLSQL Packge procedure to open a ref cursor for all sales people */
  /* Use ODBC to iterate over of the resultset of the ref cursor */
  getSalesPeople();


  /* Disconnect from the DB */
  disconnectDB();

  app_exit(0);
  return 0;
}


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
