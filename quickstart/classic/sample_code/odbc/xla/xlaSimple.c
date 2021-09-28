/************************************************************************** 
*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * xlaSimple.c: demonstrating usage of TimesTen persistent XLA (roughly 
 *              equivalent to the TTClasses example program xlaSimple.cpp)
 *
 **************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <timesten.h>        /* TimesTen ODBC include file */
#include <tt_errCode.h>      /* TimesTen Native Error codes */
#include <tt_xla.h>          /* TimesTen XLA include file */
#include "tt_version.h"

#ifndef WIN32
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <signal.h>
#include <stdio.h>

#include "utils.h"
#include "ttgetopt.h"


#define FETCH_WAIT_SECS 5
#define TABLE_OWNER UIDNAME
#define TABLE_NAME "MYDATA"
#define MAX_XLA_COLUMNS 128
#define DSNNAME DEMODSN

static char usageStr[] =
  "Usage:\t<CMD> {-h | -help | -V}\n"
  "\t{<DSN> | -connstr <connection-string>}\n"
  "  -h, -help           Prints this message and exits.\n"
  "  -V                  Prints TimesTen version number.\n"
  "  <DSN>               Specifies the Data Source Name.\n"
  "  -connstr <connstr>  Specifies the ODBC connection string.\n"
  "Either <DSN> or '-connstr <connstr>' must be specified.\n\n"
  "This demo program illustrates a simple program that utilizes\n"
  "transaction log (XLA) API to monitor changes made to the table\n"
  "'appuser.mydata'.\n";



int NAME_COLUMN = -1;
int VALUE_COLUMN = -1;

/* Made global */

SQLINTEGER ncols;

SQLHENV henv = SQL_NULL_HENV;
SQLHDBC hdbc = SQL_NULL_HDBC;
SQLHSTMT hstmt = SQL_NULL_HSTMT;
ttXlaHandle_h xla_handle = NULL;
SQLUBIGINT SYSTEM_TABLE_ID = 0;      /* how TimesTen identifies each table */
ttXlaColDesc_t xla_column_defs[MAX_XLA_COLUMNS];

#define ERR_BUF_LEN 10240
SQLCHAR       err_buf [ERR_BUF_LEN] ;      /* better safe than sorry */

static char cmdname[80];   /* stripped command name */

static char connstr[CONN_STR_LEN];   /* connection string from user */
static char dsn[CONN_STR_LEN];       /* DSN */

/*----------------------------------------------------------------------------*/
/* signal handling stuff                                                      */
/*----------------------------------------------------------------------------*/

static int sigReceived;
static int sigReported;

/*
 * Signal handling
 */
void
RequestStop(int sig)
{
  sigReceived = sig;
  sigReported = 0;
}

void
StopRequestClear(void)
{
  sigReceived = 0;
  sigReported = 0;
}

int
StopRequested(void)
{
  if (sigReceived != 0) {
    if (!sigReported) {
      fprintf(stderr, 
              "\n*** Stop request received (signal %d).  Terminating. ***\n",
              sigReceived);
      sigReported = 1; /* only print this message once */
    }
    return 1;
  }
  return 0;
}

int
SignalReceived(void)
{
  return (sigReceived);
}

#ifdef WIN32
/*
 * WIN32's signal is not the BSD signal(), it's the old SYSV signal().
 *
 * Since SYSV signal() is not really robust, and since signal() is not
 * really a native WIN32 API, we use SetConsoleCtrlHandler instead to do
 * things the Windows way.
 */
static BOOL WINAPI
ConCtrlHandler(DWORD ctrlType)
{
  switch(ctrlType) {
  case CTRL_C_EVENT :
    RequestStop (SIGINT) ; 
    return TRUE; /* This "signal" is handled. */

  case CTRL_BREAK_EVENT :
    RequestStop (SIGBREAK) ; 
    return TRUE; /* This "signal" is also handled. */
     
  case CTRL_CLOSE_EVENT :
  case CTRL_LOGOFF_EVENT :
  case CTRL_SHUTDOWN_EVENT :
  default:
    return FALSE; /* Let someone else handle this */
  }
  return FALSE;
}

int HandleSignals(void)
{
  int rc;
  rc = SetConsoleCtrlHandler(ConCtrlHandler, TRUE);
  /* Our rc and theirs are inverted */
  if (rc) {
    return 0;
  }
  return 1;
}

#else
/*
 * Signal handler
 * No printf, no malloc / strdup, no longjump
 */
static void
sigHandler(int sig)
{
  RequestStop (sig) ;
}


static int
sigHandlerSet(int sig, handler_t handler)
{
  struct sigaction new_action;
  struct sigaction old_action;

  memset(&new_action, 0, sizeof(struct sigaction));
  new_action.sa_handler = handler;
  /* Temporarily block all other signals during handler execution */
  sigfillset (&new_action.sa_mask);
  /* Auto restart syscalls after handler return */
#if !defined(SB_P_OS_LYNX)
  new_action.sa_flags = SA_RESTART;
#endif /* Not on Lynx */

  if (sigaction(sig, NULL, &old_action) != 0) {
    return -1;
  }
  /* Honor inherited SIG_IGN that's set by some shell's */
  if (old_action.sa_handler != SIG_IGN) {
    if (sigaction(sig, &new_action, NULL) != 0) {
      return -1;
    }
  }
  
  return 0;
}

int HandleSignals(void)
{
  if (sigHandlerSet(SIGINT, sigHandler) != 0) {
    perror("Unable to set signal handler for SIGINT");
    return 1;
  }
  if (sigHandlerSet(SIGTERM, sigHandler) != 0) {
    perror("Unable to set signal handler for SIGTERM");
    return 1;
  }
  if (sigHandlerSet(SIGHUP, sigHandler) != 0) {
    perror("Unable to set signal handler for SIGHUP");
    return 1;
  }
  if (sigHandlerSet(SIGPIPE, sigHandler) != 0) {
    perror("Unable to set signal handler for SIGPIPE");
    return 1;
  }
  return 0;
}
#endif /* Handling ctrl-c on Windows and Unix */


/*---------------------------------------------------------------------*/
void
processArgs(int argc, char *argv[])
/*---------------------------------------------------------------------*/
{
  int ac;
  char errbuf[256];

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
    exit(-1);
  } else if (dsn[0]) {
    /* Got a DSN, turn it into a connection string. */
    if (strlen(dsn)+5 >= sizeof(connstr)) {
      fprintf(stderr, "%s: DSN '%s' too long.\n", cmdname, dsn);
      exit(-1);
    }
    sprintf(connstr, "DSN=%s", dsn);
  } else if (connstr[0]) {
    /* Got a connection string. */
  } else {
    sprintf (connstr, "DSN=%s;%s", DSNNAME, XLAUID);
  }
}


/*-----------------------------------------------------------------------------*/
void handleError(SQLRETURN rc, SQLHENV henv, SQLHDBC hdbc, SQLHSTMT hstmt,
                 SQLCHAR * err_msg, SQLINTEGER * native_error)
/*-----------------------------------------------------------------------------*/
{
  SQLCHAR     err_msg_tmp[1024];
  SQLCHAR     sql_state[128];
  SQLSMALLINT pcb_err_msg;
  SQLRETURN   ret = SQL_SUCCESS;
  SQLINTEGER  nat_err;
  int firstError=0;

  char * err_msg_ptr;
  err_msg_ptr = (char*) &err_msg[0] ;

  if (rc == SQL_SUCCESS || rc == SQL_NO_DATA_FOUND)
    return;  /* not an error, return immediately    */

  while (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
  {
    ret = SQLError(henv, hdbc, hstmt, sql_state,
                   &nat_err, err_msg_tmp, sizeof(err_msg_tmp), &pcb_err_msg);
    switch (ret)
    {
    case SQL_SUCCESS:
      sprintf(err_msg_ptr, "*** %s\nODBC Error/Warning: %s, "
              "TimesTen Error/Warning: %d\n",
              err_msg_tmp, sql_state, nat_err);
      break;

    case SQL_SUCCESS_WITH_INFO:
      sprintf(err_msg_ptr, "SQLError() failed with return code of SQL_SUCCESS_WITH_INFO.\n"
              "Need to increase size of message buffer.\n");
      break;

    case SQL_INVALID_HANDLE:
      sprintf(err_msg_ptr, "SQLError() failed with return code of SQL_INVALID_HANDLE.\n");
      break;

    case SQL_ERROR:
      sprintf(err_msg_ptr, "SQLError() failed with return code of SQL_ERROR.\n");
      break;

    case SQL_NO_DATA_FOUND:
      /* SQLError() has returned all the errors.  No more calls to SQLError are necessary. */
      break;

    default:
      sprintf(err_msg_ptr, "SQLError() failed with return code of %d.\n", ret);
      break;
    }

    /* set the (non-zero) native_error value, if it's not already set */
    if (!firstError && nat_err!=0) {
      firstError=1;
      *native_error = nat_err;
    }

    /* append any other error messages */
    err_msg_ptr = (char*) &err_msg[0] + strlen((char*)err_msg);
  }
}


/*---------------------------------------------------------------------*/
void handleXLAerror(SQLRETURN rc, ttXlaHandle_h xlaHandle, 
                    SQLCHAR * err_msg, SQLINTEGER * native_error)
/*---------------------------------------------------------------------*/
{
  SQLINTEGER retLen;
  SQLINTEGER code;

  char * err_msg_ptr;

  /* initialize return codes */
  rc = SQL_ERROR;
  *native_error = -1;
  err_msg[0] = '\0';
  
  err_msg_ptr = (char *)err_msg;
  
  while (1)
  {
    int rc = ttXlaError(xlaHandle, &code, err_msg_ptr,
                        ERR_BUF_LEN - (err_msg_ptr - (char *)err_msg), &retLen);
    if (rc == SQL_NO_DATA_FOUND)
    {
      break;
    }
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      sprintf(err_msg_ptr,
              "*** Error fetching error message via ttXlaError(); rc=<%d>.",rc) ; 
      break;
    }
    rc = SQL_ERROR;
    *native_error = code ; 
    /* append any other error messages */
    err_msg_ptr += retLen;
  }
}


/*---------------------------------------------------------------------*/
void TerminateGracefully(int status)
/*---------------------------------------------------------------------*/
{
  /*******************************************************
   *  Proper program exit: disconnect and free handles.  *
   *******************************************************/

  SQLRETURN     rc;
  SQLINTEGER    native_error ; 
  SQLINTEGER    oldstatus;
  SQLINTEGER    newstatus = 0;
    
  /* If the table has been subscribed to via XLA, unsubscribe it. */

  if (SYSTEM_TABLE_ID != 0) {
    rc = ttXlaTableStatus(xla_handle, SYSTEM_TABLE_ID, 0,
                          &oldstatus, &newstatus);
    if (rc != SQL_SUCCESS) {
      handleXLAerror (rc, xla_handle, err_buf, &native_error);
      fprintf(stderr, "Error when unsubscribing from "TABLE_OWNER"."TABLE_NAME
              " table <%d>: %s", rc, err_buf);
    }
    SYSTEM_TABLE_ID = 0;
  }


  /* Close the XLA connection. */

  if (xla_handle != NULL) {
    rc = ttXlaClose(xla_handle);
    if (rc != SQL_SUCCESS) {
      fprintf(stderr, "Error when disconnecting from XLA:<%d>", rc);
    }
    xla_handle = NULL;
  }

  if (hstmt != SQL_NULL_HSTMT) {
    rc = SQLFreeStmt(hstmt, SQL_DROP);
    if (rc != SQL_SUCCESS) {
      handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
      fprintf(stderr, "Error when freeing statement handle:\n%s\n", err_buf);
    }
    hstmt = SQL_NULL_HSTMT;
  }

  /* Disconnect from TimesTen entirely. */
    
  if (hdbc != SQL_NULL_HDBC) {
    rc = SQLTransact(henv, hdbc, SQL_ROLLBACK);
    if (rc != SQL_SUCCESS) {
      handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
      fprintf(stderr, "Error when rolling back transaction:\n%s\n", err_buf);
    }

    rc = SQLDisconnect(hdbc);
    if (rc != SQL_SUCCESS) {
      handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
      fprintf(stderr, "Error when disconnecting from TimesTen:\n%s\n", err_buf);
    }

    rc = SQLFreeConnect(hdbc);
    if (rc != SQL_SUCCESS) {
      handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
      fprintf(stderr, "Error when freeing connection handle:\n%s\n", err_buf);
    }
    hdbc = SQL_NULL_HDBC;
  }

  if (henv != SQL_NULL_HENV) {
    rc = SQLFreeEnv(henv);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
      fprintf(stderr, "Error when freeing environment handle:\n%s\n", err_buf);
    }
    henv = SQL_NULL_HENV;
  }

  exit(status);
}


/*---------------------------------------------------------------------*/
void ConnectAndCheckTable ()
/*---------------------------------------------------------------------*/
{
  SQLRETURN rc = SQL_SUCCESS;
  SQLINTEGER native_error;

  /* First, allocate an environment handle */

  rc = SQLAllocEnv(&henv);
  if (rc != SQL_SUCCESS) {
    fprintf(stderr, "Unable to allocate an environment handle.\n");
    TerminateGracefully(1);
  }

  /* Next, allocate a connection handle */

  rc = SQLAllocConnect(henv, &hdbc);
  if (rc != SQL_SUCCESS) {
    handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
    fprintf(stderr, "Unable to allocate a connection handle:\n%s\n", err_buf);
    TerminateGracefully(1);
  }

  printf("Connecting to the database as %s ...\n\n", connstr);

  /* Use DriverConnect because it allows more options then Connect */
  rc = SQLDriverConnect(hdbc, NULL,
                        (SQLCHAR*)connstr, SQL_NTS,
                        NULL, 0,
                        NULL, SQL_DRIVER_COMPLETE);
  if (rc != SQL_SUCCESS) {
    handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
    if (rc == SQL_SUCCESS_WITH_INFO) {
      fprintf(stderr, "*** WARNING: While connecting to data source: %s\n%s\n", 
              connstr, err_buf);        
    }   
    else {
      fprintf(stderr, "*** ERROR: Unable to connect to data source: %s\n%s\n", 
              connstr, err_buf);        
      TerminateGracefully(1);
    }
  }
  
  printf("Connected as %s\n\n", connstr);

  /* Turn auto-commit off.
   * This MUST be done, as AutoCommit is on by default in ODBC.
   */

  rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  if (rc != SQL_SUCCESS) {
    handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
    fprintf(stderr, "Unable to set AutoCommit off:\n%s\n", err_buf);
    TerminateGracefully(1);
  }

  /* Allocate a statement handle */

  rc = SQLAllocStmt(hdbc, &hstmt);
  if (rc != SQL_SUCCESS) {
    handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
    fprintf(stderr, "Unable to allocate statement handle:\n%s\n", err_buf);
    TerminateGracefully(1);
  }

  rc = SQLExecDirect(hstmt, 
                     (SQLCHAR*) "select count(*) from "TABLE_OWNER"."TABLE_NAME" ",
                     SQL_NTS);

  if (rc != SQL_SUCCESS) {

    handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
    if (native_error == tt_ErrTableDoesNotExist) {
      fprintf(stderr, "xlauser is trying to respond to events from table "TABLE_OWNER"."TABLE_NAME" which does not exist, please create it.\n\n");
    }
    else {
      fprintf(stderr, "Native error: <%d>\n", native_error);
      fprintf(stderr, "Unable to access table "TABLE_OWNER"."TABLE_NAME":\n%s\n", err_buf);
    }   

    /* This is fatal so die */
    TerminateGracefully(1);
  }     


  rc = SQLTransact(henv, hdbc, SQL_COMMIT);
  if (rc != SQL_SUCCESS) {
    handleError(rc, henv, hdbc, hstmt, err_buf, &native_error);
    fprintf(stderr, "Unable to create table "TABLE_OWNER"."TABLE_NAME":\n%s\n", err_buf);
    TerminateGracefully(1);
  }

  /* 
   * Now we're done with this connection handle.  The next time we'll 
   * use it will be to connect to XLA, in the InitHandler() function. 
   */
}

/*---------------------------------------------------------------------*/
void InitHandler (unsigned char * bookmarkName)
/*---------------------------------------------------------------------*/
{
  SQLRETURN rc ;
  SQLUBIGINT userID; /* not actually used for anything */
 /* SQLINTEGER ncols;  Made global */
  SQLINTEGER newstatus = 1;
  SQLINTEGER oldstatus; 
  SQLINTEGER native_error;

  /* 
   * We've already connected to TimesTen.  Connect to TimesTen
   * via XLA.
   *
   * Then, collect information about the subscribed table's columns.
   * (store in NAME_COLUMN and VALUE_COLUMN global variables).
   */

  rc = ttXlaPersistOpen(hdbc, bookmarkName, XLACREAT, &xla_handle);
  if (rc != SQL_SUCCESS) {
    handleXLAerror (rc, xla_handle, err_buf, &native_error);

    /* If the connection failed but we have a valid handle we need to 
       close the handle to free an resources already allocated to it
    */
    if (xla_handle != (ttXlaHandle_h)SQL_INVALID_HANDLE)
        ttXlaClose(xla_handle);
    /* 
     * If the error was simply that the bookmark already exists, then
     * try to connect via XLA again, but with the XLAREUSE flag instead of
     * the XLACREAT flag 
     */
    if ( native_error == 907 ) { /* tt_ErrKeyExists */
      rc = ttXlaPersistOpen(hdbc, bookmarkName, XLAREUSE, &xla_handle);
      if (rc != SQL_SUCCESS) {
        handleXLAerror (rc, xla_handle, err_buf, &native_error);
        fprintf(stderr, "Unable to connect via XLA:\n%s\n", err_buf);

        /* If the connection failed but we have a valid handle we need to 
           close the handle to free an resources already allocated to it
        */
        if (xla_handle != (ttXlaHandle_h)SQL_INVALID_HANDLE)
            ttXlaClose(xla_handle);

        TerminateGracefully(1);
      }
    } 
    else {
      fprintf(stderr, "Unable to connect via XLA:\n%s\n", err_buf);
      TerminateGracefully(1);
    }
  }
  
  /* We've successfully connected via XLA. */

  /* We now want to discover the ordinal positions of the two table columns. 
   * First we get information about the table. */

  rc = ttXlaTableByName(xla_handle, TABLE_OWNER, TABLE_NAME, &SYSTEM_TABLE_ID, &userID);
  if (rc != SQL_SUCCESS) {
    handleXLAerror (rc, xla_handle, err_buf, &native_error);
    fprintf(stderr, "ttXlaTableByName() returns an error <%d>: %s", rc, err_buf);
    TerminateGracefully(1);
  }


  /* "Subscribe" to changes to this table via XLA. */

  rc = ttXlaTableStatus(xla_handle, SYSTEM_TABLE_ID, 0,
                        &oldstatus, &newstatus);
  if (rc != SQL_SUCCESS) {
    handleXLAerror (rc, xla_handle, err_buf, &native_error);
    fprintf(stderr, "ttXlaTableStatus() returns an error <%d>: %s", rc, err_buf);
    TerminateGracefully(1);
  }


  /* TESTING 'OLDSTATUS' RETURN VALUE */

  rc = ttXlaTableStatus(xla_handle, SYSTEM_TABLE_ID, 0,
                        &oldstatus, NULL);
  if (rc != SQL_SUCCESS) {
    handleXLAerror (rc, xla_handle, err_buf, &native_error);
    fprintf(stderr, "ttXlaTableStatus() returns an error <%d>: %s", rc, err_buf);
    TerminateGracefully(1);
  }
  if (oldstatus != 0) 
    printf("XLA is currently tracking changes to table %s.%s\n", TABLE_OWNER, TABLE_NAME);
  else 
    printf("XLA is not tracking changes to table %s.%s\n", TABLE_OWNER, TABLE_NAME);


  /* Get information about the columns in the table. */

  rc = ttXlaGetColumnInfo(xla_handle, SYSTEM_TABLE_ID, userID,
                          xla_column_defs, MAX_XLA_COLUMNS, &ncols);
  if (rc != SQL_SUCCESS) {
    handleXLAerror (rc, xla_handle, err_buf, &native_error);
    fprintf(stderr, "ttXlaGetColumnInfo() returns an error <%d>: %s", rc, err_buf);
    TerminateGracefully(1);
  }

}


/*----------------------------------------------------------------------
 *  PrintColValues contains all
 * of the logic to be executed whenever the XLA-subscribed table is
 * changed.
 * *---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
void PrintColValues(void * tup)
/*---------------------------------------------------------------------*/
{    

  SQLRETURN rc ; /* make these global?? */
  SQLINTEGER native_error;

  void * pColVal;
  char buffer[50+1]; /* No strings over 50 bytes */
  int i;


  for (i = 0; i < ncols; i++)
  {

    if (xla_column_defs[i].nullOffset != 0) {  /* Check to see if column is NULL */
      /* this means col could be NULL */
      if (*((unsigned char*) tup + xla_column_defs[i].nullOffset) == 1) {
        /* this means that value is SQL NULL */
        printf("  %s: NULL\n", 
               ((unsigned char*) xla_column_defs[i].colName));
        continue; /* Skip rest and re-loop */
      }
    }

    /* Fixed-length data types: */

    /* For INTEGER, recast as int */

    if (xla_column_defs[i].dataType == TTXLA_INTEGER) {

      printf("  %s: %d\n",
             ((unsigned char*) xla_column_defs[i].colName),
             *((int*) ((unsigned char*) tup + xla_column_defs[i].offset)));                     
    }

        
    /* For CHAR, just get value and null-terminate string */

    else if (   xla_column_defs[i].dataType == TTXLA_CHAR_TT
             || xla_column_defs[i].dataType == TTXLA_CHAR) {

      pColVal = (void*) ((unsigned char*) tup + xla_column_defs[i].offset);

      memcpy(buffer, pColVal, xla_column_defs[i].size);
      buffer[xla_column_defs[i].size] = '\0';

      printf("  %s: %s\n", 
             ((unsigned char*) xla_column_defs[i].colName),
             buffer);
    }
                

    /* For NCHAR, recast as SQLWCHAR.
       NCHAR strings must be parsed one character at a time */

    else if (   xla_column_defs[i].dataType == TTXLA_NCHAR_TT  
             || xla_column_defs[i].dataType == TTXLA_NCHAR ) {
      SQLUINTEGER j;
      SQLWCHAR * CharBuf;                               

      CharBuf = (SQLWCHAR*) ((unsigned char*) tup + xla_column_defs[i].offset);
                        
      printf("  %s: ", ((unsigned char*) xla_column_defs[i].colName));                            
      for (j = 0; j < xla_column_defs[i].size / 2; j++)
      { 
        printf("%c", CharBuf[j]);
      }
      printf("\n");      
    }

    /* Variable-length data types:
                
       For VARCHAR, locate value at its variable-length offset and null-terminate.

       VARBINARY types are handled in a similar manner.

       For NVARCHARs, initialize 'var_data' as a SQLWCHAR, get the value as shown 
       below, then iterate through 'var_len' as shown for NCHAR above */

    else if (   xla_column_defs[i].dataType == TTXLA_VARCHAR
             || xla_column_defs[i].dataType == TTXLA_VARCHAR_TT) {

      SQLULEN *  var_len;
      char * var_data;
      pColVal = (void*) ((unsigned char*) tup + xla_column_defs[i].offset);

      /*
       * If column is out-of-line, pColVal points to an offset
       * else column is inline so pColVal points directly to the string length.
       */
      if (xla_column_defs[i].flags & TT_COLOUTOFLINE)
        var_len = (SQLULEN*)((char*)pColVal + *((unsigned int*)pColVal));
      else
        var_len = (SQLULEN*)pColVal;

      var_data = (char*)(var_len+1);

      memcpy(buffer,var_data,*var_len);
      buffer[*var_len] = '\0';

      printf("  %s: %s\n", 
             ((unsigned char*) xla_column_defs[i].colName),
             buffer);      
    }


    /* Complex data types require conversion by the XLA conversion methods

       Read and convert a TimesTen TIMESTAMP value.
       DATE and TIME types are handled in a similar manner  */
                
    else if (   xla_column_defs[i].dataType == TTXLA_TIMESTAMP
             || xla_column_defs[i].dataType == TTXLA_TIMESTAMP_TT) {
                                                                                
      TIMESTAMP_STRUCT timestamp;
      char *convFunc;
                                                                                
      pColVal = (void*) ((unsigned char*) tup + xla_column_defs[i].offset);                                                                                 
      if (xla_column_defs[i].dataType == TTXLA_TIMESTAMP_TT) {
        rc = ttXlaTimeStampToODBCCType(pColVal, &timestamp);
        convFunc="ttXlaTimeStampToODBCCType";
      }
      else {
        rc = ttXlaOraTimeStampToODBCTimeStamp(pColVal, &timestamp);
        convFunc="ttXlaOraTimeStampToODBCTimeStamp";
      }

      if (rc != SQL_SUCCESS) {
        handleXLAerror (rc, xla_handle, err_buf, &native_error);
        fprintf(stderr, "%s() returns an error <%d>: %s", 
                convFunc, rc, err_buf);
        TerminateGracefully(1);
      }
      printf("  %s: %04d-%02d-%02d %02d:%02d:%02d.%06d\n",
             ((unsigned char*) xla_column_defs[i].colName),
             timestamp.year,timestamp.month, timestamp.day,
             timestamp.hour,timestamp.minute,timestamp.second,
             timestamp.fraction);
    }

    /* Read and convert a TimesTen DECIMAL value to a string. */

    else if (xla_column_defs[i].dataType == TTXLA_DECIMAL_TT) {

      char decimalData[50]; 
      short precision, scale;
      pColVal = (float*) ((unsigned char*) tup + xla_column_defs[i].offset);
      precision = (short) (xla_column_defs[i].precision);
      scale = (short) (xla_column_defs[i].scale);

      rc = ttXlaDecimalToCString(pColVal, (char*)&decimalData, precision, scale);
      if (rc != SQL_SUCCESS) {
        handleXLAerror (rc, xla_handle, err_buf, &native_error);
        fprintf(stderr, "ttXlaDecimalToCString() returns an error <%d>: %s", rc, err_buf);
        TerminateGracefully(1);
      }

      printf("  %s: %s\n",
             ((unsigned char*) xla_column_defs[i].colName),
             decimalData);                      
    }
    else if (xla_column_defs[i].dataType == TTXLA_NUMBER) {
      char numbuf[32];
      pColVal = (void*) ((unsigned char*) tup + xla_column_defs[i].offset);
 
      rc=ttXlaNumberToCString(xla_handle,
                              pColVal,
                              numbuf,
                              sizeof(numbuf));
      if (rc != SQL_SUCCESS) {
        handleXLAerror (rc, xla_handle, err_buf, &native_error);
        fprintf(stderr, "ttXlaNumberToDouble() returns an error <%d>: %s", rc, err_buf);
        TerminateGracefully(1);
      }
      printf("  %s: %s\n",
             ((unsigned char*) xla_column_defs[i].colName),
             numbuf);                   
    }

  } /* End FOR loop */
}

/*---------------------------------------------------------------------*/
void HandleChange(ttXlaUpdateDesc_t* xlaP)
/*---------------------------------------------------------------------*/
{
  void * tup1;
  void * tup2;


  /* First make change that the XLA update is for the table we care about. */
  if (xlaP->sysTableID != SYSTEM_TABLE_ID)
    return ;

  /* OK, it's for the table we're monitoring. */

  /* The last record in the ttXlaUpdateDesc_t record is the "tuple2"
   * field.  Immediately following this field is the first XLA record
   * "row".
   */


  tup1 = (void*) ((char*) xlaP + sizeof(ttXlaUpdateDesc_t));

  switch(xlaP->type) {

  case INSERTTUP:
    printf("Inserted new row:\n");
    PrintColValues(tup1);
    break;

  case UPDATETUP:

    /* If this is an update ttXlaUpdateDesc_t, then following that is
     * the second XLA record "row".  
     */

    tup2 = (void*) ((char*) tup1 + xlaP->tuple1);
    printf("Updated row:\n");
    PrintColValues(tup1);
    printf("To:\n");
    PrintColValues(tup2);
    break;

  case DELETETUP:
    printf("Deleted row:\n");
    PrintColValues(tup1);
    break;

  default:
    /* Ignore any XLA records that aren't for inserts/update/delete SQL ops. */
    break;

  } /* switch (xlaP->type) */
}




/*---------------------------------------------------------------------*/

const char * howto_string = 
"This application monitors the table \n"
"    '"TABLE_OWNER"."TABLE_NAME"' \n"
"for changes and reports such changes as they occur.  To run it, \n"
"in one window run this application.  In another window run \n"
"\n"
"      ttIsql -connstr \"%s\" \n"
"\n"
"to make changes to this table.  The changes will be visible as \n"
"output from this application.\n" 
"\n";

/*---------------------------------------------------------------------*/

int 
main(int argc, char* argv[])
{
  char howto[2048];
  char temp[2048];
  unsigned char bookmarkName [32] ; 
  int deleteBookmark = 0;
  int len, couldNotSetupSignalHandling;

  SQLRETURN rc;
  SQLINTEGER records;
  SQLINTEGER native_error;
  ttXlaUpdateDesc_t ** arry;
  int j;
  /*---------------------------------------------------------------
   * Parse command-line arguments
   *--------------------------------------------------------------*/
  
  processArgs (argc, argv);
  /*--------------------------------------------------------------*/

  /*--------------------------------------------------------------
   * Set up signal handling
   * This code simply registers the signal-handling.
   *
   * Subsequent calls to StopRequested() and TerminateIfRequested()
   * stop the execution loops and rolls back/disconnects/exits gracefully.
   *--------------------------------------------------------------*/
  couldNotSetupSignalHandling = HandleSignals();
  if (couldNotSetupSignalHandling) {
    fprintf(stderr, "Could not set up signal handling.  Aborting.\n");
    exit(-1);
  }
  /*--------------------------------------------------------------*/

  len = strlen(argv[0]);
  strcpy((char *)bookmarkName, "xlaSimple");

  /* Tell the user what's going on. */

  sprintf(temp, "DSN=%s;uid=%s", DEMODSN, UIDNAME);
  sprintf(howto, howto_string, temp);
  printf(howto);

  /*---------------------------------------------------------------  
   * First, connect to the database and check that the table that will
   * be monitored exists.
   *--------------------------------------------------------------*/
  
  ConnectAndCheckTable();

  /*--------------------------------------------------------------- 
   * Now connect via XLA.  This may require two connection attempts, since
   * we want this program to work correctly both when (1) the bookmark
   * does not exist; and (2) the bookmark *does* exist.
   *--------------------------------------------------------------*/

  InitHandler(bookmarkName);

  /*--------------------------------------------------------------
   * Get updates.  Dispatch them via HandleChange().
   *
   * This loop will handle updates to the XLA-subscribed table.
   *
   * NOTE: if multiple tables were registered, this loop is exactly the
   * same, except that the HandleChange() function would need
   * to check which table had been updated, and then call that
   * particular table's Handle{Insert/Update/Delete} function.
   *-------------------------------------------------------------*/

  while (!StopRequested()) {
    
    rc = ttXlaNextUpdateWait(xla_handle, &arry, 100, &records, FETCH_WAIT_SECS);
    if (rc != SQL_SUCCESS) {  
      handleXLAerror (rc, xla_handle, err_buf, &native_error);
      fprintf(stderr, "Error when fetching records via XLA <%d>: %s", rc, err_buf);
      TerminateGracefully(1);
    }
    
    /* Interpret the updates */
    for(j=0;j < records;j++){
      ttXlaUpdateDesc_t *p;
      p = arry[j];
      HandleChange(p);
    } /* end for each record fetched */
    
    if (records) {
      printf("Processed %d records.\n", records);
    }
    
    /* 
     * Need to "acknowledge updates" in order to move the XLA bookmark
     * forward.
     */

    rc = ttXlaAcknowledge(xla_handle);
    if (rc != SQL_SUCCESS) {  
      handleXLAerror (rc, xla_handle, err_buf, &native_error);
      fprintf(stderr, "Error when acknowledging updates <%d>: %s", rc, err_buf);
      TerminateGracefully(1);
    }
    
  } /* end while !StopRequested() */
  
  /*
   * When we reach this point, we've received a signal, indicating the user
   * wants the program to terminate.
   *
   * The following code does all of the required cleanup to handle a signal
   * (in particular, gracefully disconnect from TimesTen before exiting).
   */
  
  /*
   * Depending on the application semantics, it might or might not make
   * sense to delete the XLA bookmark when the program terminates.  
   *
   * The advantage of *not* deleting the bookmark is that when the XLA
   * program connects again, any updates that have occurred since the
   * program terminated will be interpreted by the XLA program; i.e., the
   * updates are persistent in the TimesTen transaction log.
   * 
   * The advantage of deleting the bookmark is to simply clean it up, so
   * that subsequent checkpoint operations will purge the transaction log 
   * files.  If the XLA bookmark is not deleted when this process terminates, 
   * then these transaction files can build up and consume a lot of disk space.
   *
   * This program by default deletes the XLA bookmark upon exit.
   */
  
  if (deleteBookmark) {
    rc = ttXlaDeleteBookmark(xla_handle);
    if (rc != SQL_SUCCESS) {  
      handleXLAerror (rc, xla_handle, err_buf, &native_error);
      fprintf(stderr, "Error when deleting XLA bookmark <%d>: %s", rc, err_buf);
      TerminateGracefully(1);
    }
    xla_handle = NULL; /* Deleting the bookmark has the effect of disconnecting from XLA. */
  }

  TerminateGracefully(0);
  
  return 0;
}




