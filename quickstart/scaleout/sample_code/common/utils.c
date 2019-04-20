/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

#include "utils.h"
#include <timesten.h>
#ifndef WIN32
#include <sys/types.h>
#endif /* Not Win32 */


#ifndef max
#define max(a,b) (a>b?a:b)
#endif

#ifndef min
#define min(x,y) (x>y?y:x)
#endif

/*********************************************************************
 *  The following function is included for completeness, but is not
 *  relevant for understanding the function of ODBC.
 *********************************************************************/

#define MAX_NUM_PRECISION 15

/*********************************************************************
 *  Define max length of char string representation of number as:
 *    =  sign + max(prec) + decimal pt + E + exp sign + max exp leng
 *    =  1    + (above)   + 1          + 1 + 1        + 3
 *    =  MAX_NUM_PRECISION + 7
 *********************************************************************/

#define MAX_NUM_STRING_SIZE (MAX_NUM_PRECISION + 7)

extern SQLHENV  henv;
int DBMSType = DBMS_TIMESTEN;

static int sigReceived;

/*
 * Signal handling
 */
void
StopRequestClear(void)
{
  sigReceived = 0;
}

int
StopRequested(void)
{
  return (sigReceived != 0);
}

int
SigReceived(void)
{
  return (sigReceived);
}

#ifdef WIN32
/*
 * WIN32's signal is not the BSD signal(), it's the old SYSV signal().
 *
 * Since SYSV signal() is not really robust, and since signal() is not
 * really a native WIN32 API, we use SetConsoleCtrlHandler instead to do
 * things, the Windows way.
 */
static BOOL WINAPI
ConCtrlHandler(DWORD ctrlType)
{
  switch(ctrlType) {
  case CTRL_C_EVENT :
    sigReceived = SIGINT;
    /* Fall Through */
  case CTRL_BREAK_EVENT :
    sigReceived = SIGBREAK;
    return TRUE; /* We've handled these */

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
  /* Our rc and their's is inverted */
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
  sigReceived = sig;
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


/*********************************************************************
 *
 *  FUNCTION:       QuitTest
 *
 *  DESCRIPTION:    This function disconnects from the data source,
 *                  exiting the application with the specified
 *                  return code.
 *
 *  PARAMETERS:     SQLHENV henv    SQL Environment handle
 *                  SQLHDBC hdbc    SQL Connection handle
 *                  SQLRETURN inRC  Program exit code
 *
 *  RETURNS:        void
 *
 *  NOTES:          Please note that handle_errors might call QuitTest.
 *                  Therefore, this routine should ALWAYS call handle_errors
 *                  routine with the ERROR_EXIT parameter. The ERROR_EXIT
 *                  parameter allows SQL error processing and once it is
 *                  done the program is exit without call QuitTest AVOIDING
 *                  the possible infinite loop of calls.
 *
 *********************************************************************/

void
QuitTest (SQLHENV henv, SQLHDBC hdbc, int exitStatus, int abortXA)
{
  SQLRETURN rc;

  out_msg0("Disconnecting from the database...\n\n");

  if (abortXA) {
    rc = SQLTransact(henv,hdbc,SQL_ROLLBACK);
    // handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
    //               "rolling back transaction",
    //               __FILE__, __LINE__);
  }

  rc = SQLDisconnect(hdbc);
  // handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
  //               "disconnecting",
  //               __FILE__, __LINE__);

  rc = SQLFreeConnect(hdbc);
  // handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
  //               "freeing connection handle",
  //               __FILE__, __LINE__);

  rc = SQLFreeEnv(henv);
  // handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
  //               "freeing environment handle",
  //               __FILE__, __LINE__);
  exit(exitStatus);
}

/*********************************************************************
 *
 *  FUNCTION:       handle_errors
 *
 *  DESCRIPTION:    This function handles the output of any messages.
 *                  These messages are a composite of a user-
 *                  supplied message information (text, file, line#)
 *                  and information from the database.
 *
 *  PARAMETERS:     SQLHDBC hdbc    SQL Connection handle
 *                  SQLHSTMT hstmt  SQL Statement handle
 *                  SQLRETURN rc    SQL return code
 *                  int abort       Flag to signal a program abort
 *                  char* msg       Application message
 *                  char* filename  Source file where error signaled
 *                  int lineno      Source file line# for error
 *
 *  RETURNS:        int (returns native error)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int
handle_errors(SQLHDBC hdbc, SQLHSTMT hstmt, SQLRETURN rc, int abort,
              char* msg, char *filename, int lineno)
{
  SQLCHAR       szSqlState[MSG_LNG];    /* SQL state string  */
  SQLINTEGER    pfNativeError;          /* Native error code */
  SQLCHAR       szErrorMsg[MSG_LNG];    /* Error msg text buffer pointer */
  SQLSMALLINT   pcbErrorMsg;            /* Error msg text Available bytes */
  SQLRETURN     ret = SQL_SUCCESS;
  int nativeErr = 0;
  int firstError = 0;
  int corruptError = 0;
  int isError = 0;

  /* Check for signals */
  if (StopRequested()) {
    err_msg1("Signal %d caught, Aborting\n", SigReceived());
    StopRequestClear();
    if (abort == ABORT_DISCONNECT_EXIT) {
      QuitTest(henv, hdbc, 2, XA_ABORT);
    } else {
      QuitTest(henv, hdbc, 3, NO_XA_ABORT);
    }
  }

  /*
   * Decide what to do based on the value of rc.
   */

  if ( rc == SQL_SUCCESS || rc == SQL_NO_DATA_FOUND )
    return 0;
  else if ( rc == SQL_INVALID_HANDLE ) {
    err_msg3("ERROR in %s, line %d: %s: invalid handle\n",
             filename, lineno, msg);
    isError = 1;
  }
  else if ( rc == SQL_SUCCESS_WITH_INFO ) {
    if (  msg != NULL  )
        err_msg3("WARNING in %s, line %d: %s\n",
             filename, lineno, msg);
    isError = 0;
  }
  else if ( rc == SQL_ERROR ) {
    if (  msg != NULL  )
        err_msg3("ERROR in %s, line %d: %s\n",
             filename, lineno, msg);
    isError = 1;
  }

  /*
   * If we got an error (as opposed to a warning) and we are supposed to
   * exit without calling SQLError, do so now.
   */

  if ( isError ) {
    if ( abort == JUST_EXIT ) {
      exit(1);
    }
    else
    if ( abort == JUST_DISCONNECT_EXIT ) {
      QuitTest(henv, hdbc, 1, NO_XA_ABORT);
    }
  }

  /*
   * Print the error message(s) from SQLError (this only makes sense when
   * rc is SQL_SUCCESS_WITH_INFO or SQL_ERROR).
   */

  if ( rc == SQL_SUCCESS_WITH_INFO || rc == SQL_ERROR ) {

    /* flush stdout, so stderr messages appear in order */
    fflush(stdout);

    /* get all of the error info from SQLError */
    while ( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO ) {
      ret = SQLError(henv, hdbc, hstmt, szSqlState, &pfNativeError, szErrorMsg,
                     MSG_LNG, &pcbErrorMsg);


      switch (ret) {

      case SQL_SUCCESS:

        /* Bug 8328146 - Prettier error msgs for PermSize and TempSize out of space */
        if (tt_ErrDbAllocFailed == pfNativeError) {

          /* Was the problem with the Perm or Temp memory */
          ret = SQLError(henv, hdbc, hstmt, szSqlState, &pfNativeError, szErrorMsg,
                         MSG_LNG, &pcbErrorMsg);
 
          if (tt_ErrPermSpaceExhausted == pfNativeError) {
            err_msg0b("\n\nThe PermSize was not large enough for the SQL operation:\n");
#ifdef WIN32
            err_msg0b("1. Use the MS ODBC Data Source Administrator to increase the 'PermSize':\n");
            err_msg0b("2. Edit the User or System DSN for this database'.\n");
            err_msg0b("3. Use the 'First Connection' tab\n");
            err_msg0b("4. Increase the 'Permanent Data Size'.\n");
            err_msg0b("5. Click 'OK'.\n");
            err_msg0b("6. Use ttIsql as the instance administrator\n");
            err_msg0b("   to connect to the DB and alter the PermSize.\n");
            err_msg0b("7. eg ttisql sampledb\n");
            err_msg0b("8. Run your program again\n\n");
#else
            err_msg0b("1. Edit the sys.odbc.ini or odbc.ini file for this DB to increase the 'PermSize':\n");
            err_msg0b("2. Increase the 'PermSize' value for the DSN of this database.\n");
            err_msg0b("3. Save the change to your sys.odbc.ini or odbc.ini file'.\n");
            err_msg0b("4. Use ttIsql as the instance administrator\n");
            err_msg0b("   to connect to the DB and alter the PermSize.\n");
            err_msg0b("5. eg ttisql sampledb\n");
            err_msg0b("6. Run your program again\n\n");

#endif
          } else if (tt_ErrTempSpaceExhausted == pfNativeError) {
            err_msg0b("\n\nThe TempSize parameter was not large enough for the SQL operation:\n");

#ifdef WIN32
            err_msg0b("1. Use the MS ODBC Data Source Administrator to increase the 'TempSize':\n");
            err_msg0b("2. Edit the User or System DSN for this database'.\n");
            err_msg0b("3. Use the 'First Connection' tab\n");
            err_msg0b("4. Increase the 'Temporary Data Size'.\n");
            err_msg0b("5. Click 'OK'.\n");
            err_msg0b("6. Use ttIsql as the instance administrator\n");
            err_msg0b("   to connect to the DB and alter the TempSize.\n");
            err_msg0b("7. eg ttisql sampledb\n");
            err_msg0b("8. Run your program again\n\n");
#else
            err_msg0b("1. Edit the sys.odbc.ini or odbc.ini file for this DB to increase the 'TempSize':\n");
            err_msg0b("2. Increase the 'TempSize' value for the DSN of this database.\n");
            err_msg0b("3. Save the change to your sys.odbc.ini or odbc.ini file'.\n");
            err_msg0b("4. Use ttIsql as the instance administrator\n");
            err_msg0b("   to connect to the DB and alter the TempSize.\n");
            err_msg0b("5. eg ttisql sampledb\n");
            err_msg0b("6. Run your program again\n\n");

#endif
          } else {
            err_msg0b("\n\nThe DB size was not large enough for the SQL operation.\n\n");
          }

          ret     = SQL_ERROR;
          abort   = ERROR_EXIT;
          isError = 1;
          break;
        }

      case SQL_SUCCESS_WITH_INFO:

        /* print the error message */
        err_msg3("%s\n*** ODBC Error/Warning = %s, "
                 "Additional Error/Warning = %d\n",
                 szErrorMsg, szSqlState, pfNativeError);

        /* if the error message was truncated, say so */
        if(ret == SQL_SUCCESS_WITH_INFO)
          err_msg0("(Note: error message was truncated.\n");

        /* keep track of (a) the first error we recieved and (b) any error
         * indicating a corrupt data store */
        if (!firstError){
          nativeErr = pfNativeError;
          firstError=1;
        }
        if (pfNativeError == BAD_CONNECT || pfNativeError == DIRTY_BYTE)
          corruptError = pfNativeError;
        break;

      case SQL_INVALID_HANDLE:
        err_msg0("Call to SQLError failed with return code of "
                 "SQL_INVALID_HANDLE.\n");
        break;
      case SQL_ERROR:
        err_msg0("Call to SQLError failed with return code of SQL_ERROR.\n");
        break;

      case SQL_NO_DATA_FOUND:
        break;

      default:
        err_msg1("Call to SQLError failed with return code of %d.\n", ret);
      }
    }
  }


  /*
   * If we are supposed to exit after receiving an error, do so now.
   */

  if ( isError ) {
    if ( abort == ERROR_EXIT ) {
      exit(1);
    }
    else if ( abort == DISCONNECT_EXIT ) {
      QuitTest(henv, hdbc, 1, NO_XA_ABORT);
    }
    else if ( abort == ABORT_DISCONNECT_EXIT ) {
      QuitTest(henv, hdbc, 1, XA_ABORT);
    }
  }


  /*
   * Otherwise, return the native error code.  If we got an error
   * indicating a corrupt data store or an invalid connection, return that.
   * Otherwise, return the first native error code returned from SQLError.
   */
  if ( corruptError )
    return corruptError;
  else
    return nativeErr;
}


/*********************************************************************
 *
 *  FUNCTION:       display_size
 *
 *  DESCRIPTION:    This function estimates the maximum size of the
 *                  supplied column used in fetching the results of
 *                  a SELECT.
 *
 *  PARAMETERS:     SQLSMALLINT coltype       Column type
 *                  SQLUINTEGER collen        Column length
 *                  SQLCHAR* colname          Column name
 *
 *  RETURNS:        SQLUINTEGER               SQL unsigned integer
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

SQLUINTEGER
display_size (SQLSMALLINT coltype, SQLUINTEGER collen, SQLCHAR* colname)
{
  switch (coltype) {
  case SQL_CHAR:
  case SQL_VARCHAR:
    return( max( max((int) collen,(int) strlen((char *)colname)), 16 ) );
  case SQL_WCHAR:
  case SQL_WVARCHAR:
    /*
    ** Not display size, but size (in bytes) of SQL_C_WCHAR buffer required
    ** since we don't support binding SQL_C_CHAR to NCHAR type columns.
    */
    return( max( max((int) (collen * sizeof(SQLWCHAR)),
        (int) strlen((char *)colname)), 16 ) );
  case SQL_DATE:
  case SQL_TIMESTAMP:
  case SQL_TIME:
  case SQL_BIT:
    return(max((int) collen, (int) strlen((char *) colname)));
    case SQL_BIGINT:
  case SQL_SMALLINT:
  case SQL_INTEGER:
  case SQL_TINYINT:
    return(max((int) collen+1, (int) strlen((char *) colname)));
  case SQL_DECIMAL:
  case SQL_NUMERIC:
    return(max((int) collen+2, (int) strlen((char *) colname)));
  case SQL_REAL:
  case SQL_FLOAT:
  case SQL_DOUBLE:
    return(max((int) MAX_NUM_STRING_SIZE, (int)strlen((char *) colname)));
  case SQL_BINARY:
  case SQL_VARBINARY:
    return(max((int) 2*collen, (int) strlen((char *) colname)));
  case SQL_LONGVARBINARY:
  case SQL_LONGVARCHAR:
    /*
     * treat longvarchar and longvarbinary as character data for
     * now - note this is *not* the correct way to do this.
     * binary has no terminating character and so will need alternative
     * handling.
     */
    return( max( max((int) collen,(int) strlen((char *)colname)), 16 ) );
  default:
    err_msg1("Unknown datatype, %d\n", coltype);
    return (0);
  }                            /* end switch (coltype)            */
}


/*********************************************************************
 *
 *  FUNCTION:       tt_MemErase
 *
 *  DESCRIPTION:    Securely erase memory, for example to clear any trace
 *                  of a plaintext password.  Do it in a way that shouldn't
 *                  be optimized away by the compiler.
 *
 *  PARAMETERS:     void * buf       The plaintext password to erase
 *                  size_t len       The length of the plaintext password
 *
 *  RETURNS:        Nothing
 *
 *  NOTES:          memset of the buffer is not enough as optimizing compilers
 *                  a may think they know better and not erase the buffer
 *
 *********************************************************************/

void
tt_MemErase(volatile void *buf, size_t len)
{
   if (buf != NULL) {
     /* ?? use SecureZeroMemory on Windows */
     volatile char* p = buf;
     size_t i;
     for (p = buf, i = 0; i < len; ++p, ++i) {
       *p = 0;
     }
   }
} /* tt_MemErase */

int IsGridEnable(SQLHDBC hdbc)
{
  SQLRETURN rc;
  SQLHSTMT  hstmt;
  char      szParamName[1024];
  SQLLEN    szParamNameInd;
  char      szParamVal[1024];
  SQLLEN    szParamValInd;
  int       gridEnable = 0;

  rc = SQLAllocStmt (hdbc, &hstmt);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                 "allocating statement handle", __FILE__, __LINE__);

  rc = SQLExecDirect(hstmt, "call ttconfiguration ('TTGridEnable')", SQL_NTS);
  if (rc != SQL_SUCCESS) 
    printf("Error call ttconfiguration");

  rc = SQLBindCol(hstmt, 1, SQL_C_CHAR,
    szParamName, sizeof(szParamName),  &szParamNameInd);
  rc = SQLBindCol(hstmt, 2, SQL_C_CHAR,
    szParamVal, sizeof(szParamVal),  &szParamValInd);

  while ( ( rc = SQLFetch(hstmt)) != SQL_NO_DATA_FOUND) {
    if (rc != SQL_SUCCESS) {
    printf("Error SQLFetch ttconfiguration");
      break;
    }
    if (szParamNameInd != SQL_NULL_DATA && strcmp(szParamName, "TTGridEnable") == 0) {
      if (szParamValInd != SQL_NULL_DATA) {
        gridEnable = atoi(szParamVal);
        break;
      }
    }
  }
  if (rc != SQL_SUCCESS) {
    printf("Error executing allocstmt");
  }

  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction", __FILE__, __LINE__);

  SQLFreeStmt(hstmt, SQL_DROP);

  return gridEnable;
}


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
