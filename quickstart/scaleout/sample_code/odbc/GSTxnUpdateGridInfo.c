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
 *  GSTxnUpdateGridInfo.c - Transaction to update grid info for a connection.
 *
 * This transaction checks if the connected database is a Scaleout
 * database or a Classic database and then sets the relevant flag in
 * the database connection structure and the associated context. If the
 * database is a Scaleout database then the database element to which the
 * application is connected is determined and the current/previous element
 * information in the context is updated.
 *
 * This transaction is executed once on initial connection and on every retry
 * attempt after a connection failover event.
 *
 * Typically applications do not need to be aware of whether they are working
 * against a Scaleout database but sometimes they may wish to know in order
 * to enable locality based optimizations, conditionalize error handling and
 * so on.
 *
 * Rarely do applications need to concern themselves with the details of which
 * database element they are connected to; this application does so as it
 * explicitly reports connection failover events including the element that it
 * is connected to.
 *
 ****************************************************************************/

#include "GridSample.h"

#define   TTGRIDENABLE_TRUE    "1"

static char const * const 
gridQuery = "call ttConfiguration('TTGridEnable')";

static char const * const 
elementIdQuery = "SELECT elementid# FROM DUAL";

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

/*
 * Allocate a txnUpdateGridInfo_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated and structure, or NULL if error.
 */
txnUpdateGridInfo_t *
createTxnUpdateGridInfo(
                        context_t * ctxt
                       )
{
    txnUpdateGridInfo_t * ugitxn = NULL;

    if (  ctxt == NULL  )
        return NULL;

    debugMessage( ctxt, "DEBUG: ENTER: createTxnUpdateGridInfo" );
    ugitxn = (txnUpdateGridInfo_t *)calloc( 1, sizeof( txnUpdateGridInfo_t ) );
    if (  ugitxn == NULL  )
        goto error;
    ugitxn->txn = (apptxn_t *)calloc( 1, sizeof( apptxn_t ) );
    if (  ugitxn->txn == NULL  )
        goto error;

    ugitxn->txn->parent = (void *)ugitxn;
    ugitxn->txn->init = (FPinit_t)initTxnUpdateGridInfo;
    ugitxn->txn->execute = (FPexecute_t)executeTxnUpdateGridInfo;
    ugitxn->txn->cleanup = (FPcleanup_t)cleanupTxnUpdateGridInfo;
    ugitxn->txn->ctxt = ctxt;
    ugitxn->txn->txntype = TXN_UPDGRIDINFO;
    LIST_LINK( ctxt->apptxns, ugitxn->txn );
    ctxt->ugitxn = ugitxn;
    
    debugMessage( ctxt, "DEBUG: EXITOK: createTxnUpdateGridInfo" );
    return ugitxn;

error:
    if (  ugitxn != NULL  )
        free( (void *)ugitxn );
    ctxt->errNumber = ERR_NOMEM;
    displayError( ctxt, errorMessage( ERR_NOMEM, ctxt ), NULL );

    debugMessage( ctxt, "DEBUG: EXITERR: createTxnUpdateGridInfo" );
    return NULL;
} // createTxnUpdateGridInfo

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     ugitxn - a pointer to a valid txnUpdateGridInfo_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnUpdateGridInfo(
                      txnUpdateGridInfo_t * ugitxn
                     )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (ugitxn == NULL) || (ugitxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ENTER: initTxnUpdateGridInfo" );
    if (  ugitxn->isGridStmt != NULL  )
        ODBCFreeStmt( &(ugitxn->isGridStmt) );
    if (  ugitxn->localElementStmt != NULL  )
        ODBCFreeStmt( &(ugitxn->localElementStmt) );

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCAllocStmt(isGridStmt)" );
    errStack = ugitxn->txn->ctxt->dbconn->errors;
    ret = ODBCAllocStmt( ugitxn->txn->ctxt->dbconn, &(ugitxn->isGridStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCAllocStmt(localElementStmt)" );
    ret = ODBCAllocStmt( ugitxn->txn->ctxt->dbconn, 
                         &(ugitxn->localElementStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCPrepare(gridQuery)" );
    errStack = ugitxn->isGridStmt->errors;
    ret = ODBCPrepare( ugitxn->isGridStmt, gridQuery );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCBindColumn(isGridStmt,1)" );
    ret = ODBCBindColumn( ugitxn->isGridStmt, 1, SQL_C_CHAR, 30 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCBindColumn(isGridStmt,2)" );
    ret = ODBCBindColumn( ugitxn->isGridStmt, 2, SQL_C_CHAR, 1024 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCPrepare(elementIdQuery)" );
    errStack = ugitxn->localElementStmt->errors;
    ret = ODBCPrepare( ugitxn->localElementStmt, elementIdQuery );
    if (  ret != SUCCESS )
    {  // could be transient error, failover or a real error
        if (  isRetryable( ret, errStack, &rtdelay ) ||
              isFailover( ret, errStack, &rtdelay )  )
            goto error; // handle it
        // real error - may not support elementid#
        if (  ugitxn->localElementStmt != NULL  )
            ODBCFreeStmt( &(ugitxn->localElementStmt) );
    }
    else
    {
        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: ODBCBindColumn(localElementStmt,1)" );
        ret = ODBCBindColumn( ugitxn->localElementStmt, 1, SQL_C_SLONG, 0 );
        if (  ret != SUCCESS )
            goto error;
        LIST_LINK( ugitxn->txn->stmts, ugitxn->localElementStmt );
    }
    LIST_LINK( ugitxn->txn->stmts, ugitxn->isGridStmt );

    debugMessage( ugitxn->txn->ctxt, "DEBUG: EXITOK: initTxnUpdateGridInfo" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( ugitxn->txn->ctxt,
                      errorMessage( ret, ugitxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( ugitxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( ugitxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( ugitxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

    if (  ugitxn->isGridStmt != NULL  )
        ODBCFreeStmt( &(ugitxn->isGridStmt) );
    if (  ugitxn->localElementStmt != NULL  )
        ODBCFreeStmt( &(ugitxn->localElementStmt) );

#if defined(DEBUG)
    sprintf( ugitxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: initTxnUpdateGridInfo: %d", ret );
    debugMessage( ugitxn->txn->ctxt, NULL );
#endif

    return ret;
} // initTxnUpdateGridInfo

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     ugitxn - a pointer to a valid txnUpdateGridInfo_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnUpdateGridInfo(
                         txnUpdateGridInfo_t * ugitxn
                        )
{
    int ret;
    int rtdelay = 0;
    char * val = NULL;
    odbcerrlist_t * errStack = NULL;

    if (  (ugitxn == NULL) || (ugitxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ENTER: executeTxnUpdateGridInfo" );
    if (  (ugitxn->isGridStmt == NULL) || 
          (ugitxn->cursor != NULL)  )
        return ERR_STATE_INTERNAL;

    ugitxn->localelement = 0;
    ugitxn->griddb = true;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCExecute(isGridStmt)" );
    errStack = ugitxn->isGridStmt->errors;
    ret = ODBCExecute( ugitxn->isGridStmt );
    if (  ret != SUCCESS )
        goto error;
    ugitxn->cursor = ugitxn->isGridStmt;

    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCFetch(isGridStmt)" );
    ret = ODBCFetch( ugitxn->isGridStmt );
    if (  ret == ERR_ODBC_NO_DATA  )
    {
        ugitxn->griddb = false;
        ugitxn->txn->ctxt->isGrid = ugitxn->griddb;
    }
    else
    if (  ret != SUCCESS )
        goto error;


    if (  ugitxn->griddb  )
    {
        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: ODBCGetCol_CHAR(isGridStmt,2)" );
        errStack = NULL;
        ret = ODBCGetCol_CHAR( ugitxn->isGridStmt, 2, &val );
        if (  ret != SUCCESS  )
            goto error;
#if defined(DEBUG)
        sprintf( ugitxn->txn->ctxt->msgBuff, "DEBUG: TTGridEnable >%s<", val );
        debugMessage( ugitxn->txn->ctxt, NULL );
#endif
        if (  strcmp( val, TTGRIDENABLE_TRUE) == 0  )
            ugitxn->griddb = true;
        else
            ugitxn->griddb = false;

        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: ODBCCloseCursor(isGridStmt)" );
        errStack = ugitxn->isGridStmt->errors;
        ret = ODBCCloseCursor( ugitxn->isGridStmt );
        if (  ret != SUCCESS )
            goto error;
        ugitxn->cursor = NULL;
    }

    ugitxn->txn->ctxt->isGrid = ugitxn->griddb;
    if (  ! ugitxn->griddb  )
        return SUCCESS;

    if (  ugitxn->localElementStmt == NULL  )
        ugitxn->griddb = false;
    else
    {
        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: ODBCExecute(localElementStmt)" );
        errStack = ugitxn->localElementStmt->errors;
        ret = ODBCExecute( ugitxn->localElementStmt );
        if (  ret != SUCCESS )
            goto error;
        ugitxn->cursor = ugitxn->localElementStmt;

        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: ODBCFetch(localElementStmt)" );
        ret = ODBCFetch( ugitxn->localElementStmt );
        if (  ret == ERR_ODBC_NO_DATA  )
        {
            errStack = NULL;
            goto error;
        }
        else
        if (  ret != SUCCESS )
            goto error;

        errStack = NULL;
        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: ODBCGetCol_INTEGER(localElementStmt,1)" );
        ret = ODBCGetCol_INTEGER( ugitxn->localElementStmt, 1, 
                                  &(ugitxn->localelement) );
        if (  ret != SUCCESS  )
            goto error;
#if defined(DEBUG)
        sprintf( ugitxn->txn->ctxt->msgBuff, 
                 "DEBUG: localelement = %d", ugitxn->localelement );
        debugMessage( ugitxn->txn->ctxt, NULL );
#endif
    
        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: ODBCCloseCursor(localElementStmt)" );
        errStack = ugitxn->localElementStmt->errors;
        ret = ODBCCloseCursor( ugitxn->localElementStmt );
        if (  ret != SUCCESS )
            goto error;
        ugitxn->cursor = NULL;

        ugitxn->txn->ctxt->prevElementID = ugitxn->txn->ctxt->currElementID;
        ugitxn->txn->ctxt->currElementID = ugitxn->localelement;
    }

    if (  ugitxn->txn->ctxt->commitReadTxn  )
    {
        debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCCommit" );
        errStack = ugitxn->txn->ctxt->dbconn->errors;
        ret = ODBCCommit( ugitxn->txn->ctxt->dbconn );
        if (  ret != SUCCESS )
            goto error;
    }

    debugMessage( ugitxn->txn->ctxt, 
                  "DEBUG: EXITOK: executeTxnUpdateGridInfo" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( ugitxn->txn->ctxt,
                      errorMessage( ret, ugitxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( ugitxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( ugitxn->txn->ctxt->dbconn );
    ugitxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( ugitxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( ugitxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( ugitxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: executeTxnUpdateGridInfo: %d", ret );
    debugMessage( ugitxn->txn->ctxt, NULL );
#endif

    return ret;
} // executeTxnUpdateGridInfo

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     ugitxn - a pointer to a valid txnUpdateGridInfo_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnUpdateGridInfo(
                         txnUpdateGridInfo_t * ugitxn
                        )
{
    if (  ugitxn != NULL  )
    {
        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: ENTER: cleanupTxnUpdateGridInfo" );
        ugitxn->cursor = NULL;
        if (  ugitxn->txn->stmts != NULL  )
        {
            if ( ugitxn->isGridStmt != NULL  )
                LIST_UNLINK( ugitxn->txn->stmts, ugitxn->isGridStmt );
            if ( ugitxn->localElementStmt != NULL  )
                LIST_UNLINK( ugitxn->txn->stmts, ugitxn->localElementStmt );
        }
        ODBCFreeStmt( &(ugitxn->localElementStmt) );
        ODBCFreeStmt( &(ugitxn->isGridStmt) );
        debugMessage( ugitxn->txn->ctxt, 
                      "DEBUG: EXITOK: cleanupTxnUpdateGridInfo" );
        free( (void *)ugitxn );
    }
} // cleanupTxnUpdateGridInfo

