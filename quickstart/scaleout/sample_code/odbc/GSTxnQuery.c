/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/****************************************************************************
 *
 * TimesTen Scaleout sample programs - GridSample
 *
 *  GSTxnQuery.c - Transaction to query a customer.
 *
 ****************************************************************************/

#include "GridSample.h"

static char const * const 
customerQuery = 
    "SELECT c.cust_id, c.last_name, c.member_since, a.account_id, a.phone, s.status, a.current_balance FROM customers c, accounts a, account_status s WHERE c.cust_id = :cst_id AND c.cust_id = a.cust_id AND a.status = s.status";


/****************************************************************************
 *
 * Internal functions
 *
 ****************************************************************************/

/*
 * Clear down the result set variables.
 *
 * INPUTS:
 *     qtxn - pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
static void
clearRowData(
             txnQuery_t * qtxn
            )
{
    debugMessage( qtxn->txn->ctxt, "DEBUG: clearRowData" );
    qtxn->rdvalid = false;
    qtxn->cust_id = 0;
    qtxn->last_name = NULL;
    qtxn->member_since = NULL;
    qtxn->account_id = 0;
    qtxn->phone = NULL;
    qtxn->status = 0;
    qtxn->current_balance = 0.0;
} // clearRowData

/*
 * Set the result set variables.
 *
 * INPUTS:
 *     qtxn - pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
static int
setRowData(
           txnQuery_t * qtxn
          )
{
    int ret;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ENTER: setRowData" );
    debugMessage( qtxn->txn->ctxt,
                  "DEBUG: ODBCGetCol_BIGINT(cust_id)" );
    ret = ODBCGetCol_BIGINT( qtxn->cursor, 1, &(qtxn->cust_id) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff, 
             "DEBUG: cust_id = %ld", qtxn->cust_id );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif

    debugMessage( qtxn->txn->ctxt,
                  "DEBUG: ODBCGetCol_CHAR(last_name)" );
    ret = ODBCGetCol_CHAR( qtxn->cursor, 2, &(qtxn->last_name) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff, 
             "DEBUG: last_name >%s<", qtxn->last_name );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif

    debugMessage( qtxn->txn->ctxt,
                  "DEBUG: ODBCGetCol_CHAR(member_since)" );
    ret = ODBCGetCol_CHAR( qtxn->cursor, 3, &(qtxn->member_since) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff, 
             "DEBUG: member_since >%s<", qtxn->member_since );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif

    debugMessage( qtxn->txn->ctxt,
                  "DEBUG: ODBCGetCol_BIGINT(account_id)" );
    ret = ODBCGetCol_BIGINT( qtxn->cursor, 4, &(qtxn->account_id) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff, 
             "DEBUG: account_id = %ld", qtxn->account_id );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif

    debugMessage( qtxn->txn->ctxt,
                  "DEBUG: ODBCGetCol_CHAR(phone)" );
    ret = ODBCGetCol_CHAR( qtxn->cursor, 5, &(qtxn->phone) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff, 
             "DEBUG: phone >%s<", qtxn->phone );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif

    debugMessage( qtxn->txn->ctxt,
                  "DEBUG: ODBCGetCol_SMALLINT(status)" );
    ret = ODBCGetCol_SMALLINT( qtxn->cursor, 6, &(qtxn->status) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff, 
             "DEBUG: status = %d", (int)qtxn->status );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif

    debugMessage( qtxn->txn->ctxt,
                  "DEBUG: ODBCGetCol_DOUBLE(current_balance)" );
    ret = ODBCGetCol_DOUBLE( qtxn->cursor, 7, &(qtxn->current_balance) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff, 
             "DEBUG: current_balance = %.2fd", qtxn->current_balance );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif

    qtxn->rdvalid = true;

    debugMessage( qtxn->txn->ctxt, "DEBUG: EXITOK: setRowData" );
    return SUCCESS;

error:
    clearRowData( qtxn );

#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff, "DEBUG: EXITERR: setRowData: %d", ret );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif

    return ret;
} // setRowData

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

/*
 * Allocate a txnQuery_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnQuery_t *
createTxnQuery(
               context_t * ctxt
              )
{
    txnQuery_t * qtxn = NULL;

    if (  ctxt == NULL  )
        return NULL;

    debugMessage( ctxt, "DEBUG: ENTER: createTxnQuery" );
    qtxn = (txnQuery_t *)calloc( 1, sizeof( txnQuery_t ) );
    if (  qtxn == NULL  )
        goto error;
    qtxn->txn = (apptxn_t *)calloc( 1, sizeof( apptxn_t ) );
    if (  qtxn->txn == NULL  )
        goto error;

    qtxn->txn->parent = (void *)qtxn;
    qtxn->txn->init = (FPinit_t)initTxnQuery;
    qtxn->txn->execute = (FPexecute_t)executeTxnQuery;
    qtxn->txn->fetch = (FPfetch_t)fetchTxnQuery;
    qtxn->txn->close = (FPclose_t)closeTxnQuery;
    qtxn->txn->cleanup = (FPcleanup_t)cleanupTxnQuery;
    qtxn->txn->ctxt = ctxt;
    qtxn->txn->txntype = TXN_QUERY;
    LIST_LINK( ctxt->apptxns, qtxn->txn );
    ctxt->qtxn = qtxn;

    debugMessage( ctxt, "DEBUG: EXITOK: createTxnQuery" );
    return qtxn;

error:
    if (  qtxn != NULL  )
        free( (void *)qtxn );
    ctxt->errNumber = ERR_NOMEM;
    displayError( ctxt, errorMessage( ERR_NOMEM, ctxt ), NULL );

    debugMessage( ctxt, "DEBUG: EXITERR: createTxnQuery" );
    return NULL;
} // createTxnQuery

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnQuery(
             txnQuery_t * qtxn
            )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (qtxn == NULL) || (qtxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    if (  qtxn->queryStmt != NULL  )
        ODBCFreeStmt( &(qtxn->queryStmt) );

    debugMessage( qtxn->txn->ctxt, "DEBUG: ENTER: initTxnQuery" );
    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCAllocStmt(queryStmt)" );
    errStack = qtxn->txn->ctxt->dbconn->errors;
    ret = ODBCAllocStmt( qtxn->txn->ctxt->dbconn, &(qtxn->queryStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCPrepare(queryStmt)" );
    errStack = qtxn->queryStmt->errors;
    ret = ODBCPrepare( qtxn->queryStmt, customerQuery  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCBindParameter(queryStmt,1)" );
    ret = ODBCBindParameter( qtxn->queryStmt, 1, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 10, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCBindColumn(queryStmt,1)" );
    ret = ODBCBindColumn( qtxn->queryStmt, 1, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCBindColumn(queryStmt,2)" );
    ret = ODBCBindColumn( qtxn->queryStmt, 2, SQL_C_CHAR, 30 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCBindColumn(queryStmt,3)" );
    ret = ODBCBindColumn( qtxn->queryStmt, 3, SQL_C_CHAR, 19 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCBindColumn(queryStmt,4)" );
    ret = ODBCBindColumn( qtxn->queryStmt, 4, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCBindColumn(queryStmt,5)" );
    ret = ODBCBindColumn( qtxn->queryStmt, 5, SQL_C_CHAR, 16 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCBindColumn(queryStmt,6)" );
    ret = ODBCBindColumn( qtxn->queryStmt, 6, SQL_C_SSHORT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCBindColumn(queryStmt,7)" );
    ret = ODBCBindColumn( qtxn->queryStmt, 7, SQL_C_DOUBLE, 0 );
    if (  ret != SUCCESS )
        goto error;

    LIST_LINK( qtxn->txn->stmts, qtxn->queryStmt );

    debugMessage( qtxn->txn->ctxt, "DEBUG: EXITOK: initTxnQuery" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( qtxn->txn->ctxt, 
                      errorMessage( ret, qtxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( qtxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( qtxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( qtxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

    if (  qtxn->queryStmt != NULL  )
        ODBCFreeStmt( &(qtxn->queryStmt) );

#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: initTxnQuery: %d", ret );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif
    return ret;
} // initTxnQuery

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnQuery(
                txnQuery_t * qtxn
               )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (qtxn == NULL) || (qtxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ENTER: executeTxnQuery" );
    if (  (qtxn->queryStmt == NULL) || (qtxn->cursor != NULL)  )
        return ERR_STATE_INTERNAL;

    clearRowData( qtxn );

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCSetParam_BIGINT(quertStmt,1)" );
    errStack = NULL;
    ret = ODBCSetParam_BIGINT( qtxn->queryStmt, 1, qtxn->custID );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCExecute(quertStmt)" );
    errStack = qtxn->queryStmt->errors;
    ret = ODBCExecute( qtxn->queryStmt );
    if (  ret != SUCCESS )
        goto error;
    qtxn->cursor = qtxn->queryStmt;

    debugMessage( qtxn->txn->ctxt, "DEBUG: EXITOK: executeTxnQuery" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( qtxn->txn->ctxt, 
                      errorMessage( ret, qtxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    clearRowData( qtxn );
    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( qtxn->txn->ctxt->dbconn );
    qtxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( qtxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( qtxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: executeTxnQuery: %d", ret );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif
     return ret;
} // executeTxnQuery

/*
 * Fetch from the cursor.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
fetchTxnQuery(
              txnQuery_t * qtxn
             )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (qtxn == NULL) || (qtxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ENTER: fetchTxnQuery" );
    if (  (qtxn->queryStmt == NULL) || (qtxn->cursor == NULL)  )
        return ERR_STATE_INTERNAL;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCFetch(cursor)" );
    errStack = qtxn->cursor->errors;
    ret = ODBCFetch( qtxn->cursor );
    if (  ret == ERR_ODBC_NO_DATA  )
    {
        clearRowData( qtxn );
        return ret;
    }

    if (  ret != SUCCESS )
        goto error;

    setRowData( qtxn );

    debugMessage( qtxn->txn->ctxt, "DEBUG: EXITOK: fetchTxnQuery" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( qtxn->txn->ctxt,
                      errorMessage( ret, qtxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    clearRowData( qtxn );
    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( qtxn->txn->ctxt->dbconn );
    qtxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( qtxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( qtxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: fetchTxnQuery: %d", ret );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif
    return ret;
} // fetchTxnQuery

/*
 * Close any open cursor.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
closeTxnQuery(
              txnQuery_t * qtxn
             )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (qtxn == NULL) || (qtxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( qtxn->txn->ctxt, "DEBUG: ENTER: closeTxnQuery" );
    if (  (qtxn->queryStmt == NULL) || (qtxn->cursor == NULL)  )
        return SUCCESS;

    clearRowData( qtxn );
    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCCloseCursor(cursor)" );
    errStack = qtxn->cursor->errors;
    ret = ODBCCloseCursor( qtxn->cursor );
    if (  ret != SUCCESS )
        goto error;
    qtxn->cursor = NULL;

    if (  qtxn->txn->ctxt->commitReadTxn  )
    {
        debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCCommit" );
        errStack = qtxn->txn->ctxt->dbconn->errors;
        ret = ODBCCommit( qtxn->txn->ctxt->dbconn );
        if (  ret != SUCCESS )
            goto error;
    }

    debugMessage( qtxn->txn->ctxt, "DEBUG: EXITOK: closeTxnQuery" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( qtxn->txn->ctxt,
                      errorMessage( ret, qtxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    clearRowData( qtxn );
    debugMessage( qtxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( qtxn->txn->ctxt->dbconn );
    qtxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( qtxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( qtxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( qtxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: closeTxnQuery: %d", ret );
    debugMessage( qtxn->txn->ctxt, NULL );
#endif
    return ret;
} // closeTxnQuery

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnQuery(
                txnQuery_t * qtxn
               )
{
    if (  qtxn != NULL  )
    {
        debugMessage( qtxn->txn->ctxt, "DEBUG: ENTER: cleanupTxnQuery" );
        if (  qtxn->txn->stmts != NULL  )
        {
            if ( qtxn->queryStmt != NULL  )
                LIST_UNLINK( qtxn->txn->stmts, qtxn->queryStmt );
        }
        ODBCFreeStmt( &(qtxn->queryStmt) );
        debugMessage( qtxn->txn->ctxt, "DEBUG: EXITOK: cleanupTxnQuery" );
        free( (void *)qtxn );
    }
} // cleanupTxnQuery

