/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */


#include <stdio.h>
#include <stdlib.h>

#if defined(WIN32)
#include <windows.h>
#else
#include "sqlunix.h"
#endif

#include <sql.h>
#include <sqlext.h>
#include "utils.h"
#include "timesten.h"

extern int DBMSType;

#define SQL_OP_FAILED(a)        ((BOOL)( ((a) != SQL_SUCCESS) && \
                                         ((a) != SQL_SUCCESS_WITH_INFO)))

#define SQL_OP_ENDOFSET(a)      ((BOOL)( ((a) == SQL_NO_DATA_FOUND) ))

/*********************************************************************
 *
 *  FUNCTION:       wiscUpdateStats
 *
 *  DESCRIPTION:    This function updates data store statistics on
 *                  named table 'szTableName' using statement
 *                  handle 'hstmt'.
 *
 *  PARAMETERS:     SQLHDBC hdbc            SQL Connection handle
 *                  SQLHSTMT hstmt          SQL Statement handle
 *                  char* szTableName       Table name
 *
 *  RETURNS:        void (failures cause application exit)
 *
 *
 *********************************************************************/

void
wiscUpdateStats(SQLHDBC hdbc, SQLHSTMT hstmt, char * szTableName )
{
  SQLRETURN rc = SQL_SUCCESS;
  char    buf[100];

  if (DBMSType == DBMS_TIMESTEN) {
    sprintf(buf, "{ CALL TTOptUpdateStats('%s', 1) }", szTableName);
    rc = SQLExecDirect(hstmt, (SQLCHAR *) buf, SQL_NTS);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Error in TTOptUpdateStats",
                  __FILE__, __LINE__);

  }

  rc = SQLStatistics( hstmt,(SQLCHAR *)NULL,0,
                      (SQLCHAR *)NULL,0,
                      (SQLCHAR *)szTableName,
                      SQL_NTS,
                      SQL_INDEX_ALL,
                      SQL_ENSURE);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "Error in updating stats",
                __FILE__, __LINE__);

  /*
   * Having forced the statistics update we now close the query
   * without fetching any results as we don't really care to
   * fetch the data
   */

  rc = SQLFreeStmt(hstmt, SQL_CLOSE);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "Error in SQL_CLOSE for the statement handle",
                __FILE__, __LINE__);
}


/*********************************************************************
 *
 *  FUNCTION:       showPlan
 *
 *  DESCRIPTION:    This function shows the contents of the PLAN table.
 *
 *  PARAMETERS:     SQLHDBC hDbc        SQL Connection handle
 *
 *  RETURNS:        SQLRETURN           (return code from SQL statements)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

SQLRETURN
showPlan(SQLHDBC hDbc )
{
  SQLRETURN             rc = SQL_SUCCESS;
  BOOL                  fEndOfSet = 0;
  int                   i = 0;
  SQLHSTMT              hStmt = SQL_NULL_HSTMT;

  SQLINTEGER  dwStep = 0;
  SQLLEN      cbStep = sizeof(int);
  SQLINTEGER  dwLevel = 0;
  SQLLEN      cbLevel = sizeof(int);
  SQLCHAR     szOperation[40];
  SQLLEN      cbOperation;
  SQLCHAR     szTblName[40];
  SQLLEN      cbTblName;
  SQLCHAR     szIxName[40];
  SQLLEN      cbIxName;
  SQLCHAR     szPred[1024];
  SQLLEN      cbPred;
  SQLCHAR     szOtherPred[1024];
  SQLLEN      cbOtherPred;


  if ( hDbc == SQL_NULL_HDBC ) {
    err_msg0("Invalid Connection Handle\n");
    return SQL_ERROR;
  }

  rc = SQLAllocStmt(hDbc,&hStmt);
  handle_errors(hDbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                "Error in allocating statement",
                __FILE__, __LINE__);

  rc = SQLBindCol(hStmt, 1, SQL_C_SLONG, &dwStep,
                  sizeof(int), &cbStep);
  handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindCol(hStmt, 2, SQL_C_SLONG, &dwLevel,
                  sizeof(int), &cbLevel);
  handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindCol(hStmt, 3, SQL_C_CHAR, szOperation, 40, &cbOperation);
  handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindCol(hStmt, 4, SQL_C_CHAR, szTblName, 40, &cbTblName);
  handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindCol(hStmt, 5, SQL_C_CHAR, szIxName, 40, &cbIxName);
  handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindCol(hStmt, 6, SQL_C_CHAR, szPred, 1024, &cbPred);
  handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindCol(hStmt, 7, SQL_C_CHAR, szOtherPred, 1024, &cbOtherPred);
  handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLExecDirect(hStmt,(SQLCHAR *) "SELECT * FROM SYS.PLAN",SQL_NTS);
  handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                "Error in executing statement",
                __FILE__, __LINE__);

  while ( !fEndOfSet ) {

    rc = SQLFetch(hStmt);
    handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                  "Error in fetching row",
                  __FILE__, __LINE__);

    if ( SQL_OP_ENDOFSET(rc) ) {
      fEndOfSet = 1;
      break;
    }
    else {
      if ( SQL_OP_FAILED(rc) ) {
        handle_errors(hDbc, hStmt, rc, ABORT_DISCONNECT_EXIT,
                      "Error in fetching row",
                      __FILE__, __LINE__);
        break;
      }
      else {
        i++;

        if ( cbOperation == SQL_NULL_DATA )
          szOperation[0] = '\0';
        else szOperation[cbOperation] = '\0';

        if ( cbTblName == SQL_NULL_DATA )
          szTblName[0] = '\0';
        else szTblName[cbTblName] = '\0';

        if ( cbIxName == SQL_NULL_DATA )
          szIxName[0] = '\0';
        else szIxName[cbIxName] = '\0';

        if ( cbPred == SQL_NULL_DATA )
          szPred[0] = '\0';
        else szPred[cbPred] = '\0';

        if ( cbOtherPred == SQL_NULL_DATA )
          szOtherPred[0] = '\0';
        else szOtherPred[cbOtherPred] = '\0';

        err_msg7("<%d,%d,%s,%s,%s,%s,%s>\n", dwStep, dwLevel,
                 szOperation,szTblName,szIxName,szPred,szOtherPred);
      }
    }

  } /* end while */

  rc = SQLFreeStmt(hStmt,SQL_DROP);
  handle_errors(hDbc, hStmt, rc, 1, "freeing statement handle",
                __FILE__, __LINE__);

  return rc;
}

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
