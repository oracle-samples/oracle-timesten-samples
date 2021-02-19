/*
 ******************************************************************
 *
 * Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Project:    TimesTen Driver Manager
 * Version:    18.1.1
 * Date:       19th February 2021
 * Author:     chris.jenkins@oracle.com
 *
 ******************************************************************
 *
 *   This is a lightweight ODBC driver manager. It is specifically
 *   designed for use with the Oracle TimesTen IMDB to allow a
 *   single application to utilise both direct mode and client/server
 *   connections concurrently. Unlike many other driver managers,
 *   the performance penalty from using this DM is quite small.
 *
 *   To use this DM, link with this (as an object or a shared library)
 *   instead of one of the TimesTen libraries (client or direct mode).
 *   As long as your LD_LIBRARY_PATH is set correctly, the TimesTen
 *   libraries will be loaded dynamically at run-time.
 *
 *   Since it is intended for use with TimesTen, this DM only provides
 *   the ODBC functions that are supported and exposed by the TimesTen
 *   driver libraries. In addition, for direct mode connections 
 *   this DM supports use of the XLA API and the TimesTen utility API.
 *   The DM also supports the TimesTen Scaleout Routing API.
 *
 *   The DM does NOT support use of the TimesTen XA API.
 *
 *   The choice between a direct mode or client/server connection is
 *   made dynamically at connection time (when the application calls
 *   SQLConnect() or SQLDriverConnect()) based on whether the DSN 
 *   being connected to is a direct mode DSN or a client DSN.
 *
 *   For TimesTen applications, use of the DM should be transparent.
 *   An application coded to work with one of the TimesTen drivers
 *   (direct or client) should work with the DM without any code 
 *   changes.
 *
 ******************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#include <ttdrvmgr.h>

#define    TTDMVERS  "18.1.1"
#define    TTDMVERN  180101

/*
 * Special define to enable code to workaround a bug in the SQLEndTran and
 * SQLTransact functions in the TT drivers when called against an environment 
 * handle. Under some circumstances the functions should return SQL_SUCCESS 
 * but in fact they erroneously return SQL_ERROR. A subsequent call to
 * SQLError() returns SQL_NO_DATA however.
 */

// #define   ENABLE_SQLENDTRAN_HENV_WORKAROUND   1

/*
 ========================================================================
                 Build control defines
 ========================================================================
 */

#include <ttdmbldopt.h>

#if ! defined(ENABLE_THREAD_SAFETY)
#define ENABLE_THREAD_SAFETY
#endif
#if ! defined(SHLIB_SUFFIX)
#define SHLIB_SUFFIX   ".so"
#endif

/*
 ========================================================================
                 Other defines
 ========================================================================
 */

#if defined(ENABLE_XLA_DIRECT)
#include  <tt_xla.h>
#endif /* ENABLE_XLA_DIRECT */
#if defined(ENABLE_UTIL_LIB)
#include <ttutillib.h>
#endif /* ENABLE_UTIL_LIB */

#define    FMT_HVAL                "%16.16x"

#if defined(FUNC_TRACING)

#define  TRACE_FENTER(x)       fprintf(stderr,"TRACE: FENTER: %s\n",(x))
#define  TRACE_FLEAVE(x)       fprintf(stderr,"TRACE: FLEAVE: %s\n",(x))
#define  TRACE_FLEAVE_RC(x,rc) fprintf(stderr,"TRACE: FLEAVE: %s, rc = %d\n", \
                                       (x),(rc))

#else /* !FUNC_TRACING */

#define  TRACE_FENTER(x)
#define  TRACE_FLEAVE(x)
#define  TRACE_FLEAVE_RC(x,rc)

#endif /* !FUNC_TRACING */

#if defined(FUNC_TRACING) && defined(HVAL_TRACING)

#define  TRACE_HVAL(nm,val) fprintf(stderr,"TRACE: HVAL: %s = " FMT_HVAL "\n",\
                                    (nm),(val));
#define  TRACE_IVAL(nm,val) fprintf(stderr,"TRACE: IVAL: %s = %d\n",\
                                    (nm),(val));
#define  TRACE_DEBUG(stmt)  stmt

#else /* !HVAL_TRACING */

#define  TRACE_HVAL(nm,val) 
#define  TRACE_IVAL(nm,val) 
#define  TRACE_DEBUG(stmt)

#endif /* !HVAL_TRACING */

#if defined(SQL_API)
#undef     SQL_API
#endif /* SQL_API */
#define    SQL_API
#define    XLA_API
#define    UTIL_API

#if defined(ENABLE_THREAD_SAFETY)

#include  <pthread.h>

#define    MUTEX_INITIALISER       NULL
#define    MUTEX_LOCK(m)           ttMutexLock(m)
#define    MUTEX_UNLOCK(m)         ttMutexUnlock(m)
#define    MUTEX_CREATE(m)         m = ttMutexCreate()
#define    MUTEX_INIT(m)           ttMutexInit(m)
#define    MUTEX_DESTROY(m)        ttMutexDestroy(m)
#define    CLEAR_ERROR_STACK_ALL_LOCK(obj) clearErrorStackAll((obj),1);
#define    CLEAR_ERROR_STACK_LOCK(obj)     clearErrorStackPrivate((obj),1);
#define    CLEAR_ERROR_STACK_ALL(obj)      clearErrorStackAll((obj),0);
#define    CLEAR_ERROR_STACK(obj)          clearErrorStackPrivate((obj),0);

#else /* !ENABLE_THREAD_SAFETY */

#define    MUTEX_INITIALISER       NULL
#define    MUTEX_LOCK(m)
#define    MUTEX_UNLOCK(m)
#define    MUTEX_CREATE(m)
#define    MUTEX_INIT(m)
#define    MUTEX_DESTROY(m)
#define    CLEAR_ERROR_STACK_ALL_LOCK(obj) clearErrorStackAll((obj));
#define    CLEAR_ERROR_STACK_LOCK(obj)     clearErrorStackPrivate((obj));
#define    CLEAR_ERROR_STACK_ALL(obj)      clearErrorStackAll((obj));
#define    CLEAR_ERROR_STACK(obj)          clearErrorStackPrivate((obj));
#endif /* !ENABLE_THREAD_SAFETY */

#define    TTDM_CONNSTR_MAX        512

#define    TTDM_STRUCT_TAG         0x5474446d    /* TtDm */
#define    TTDM_HID_ENV            0x456e5600    /* EnV */
#define    TTDM_HID_DBC            0x44624300    /* DbC */
#define    TTDM_HID_STMT           0x53744d74    /* StMt */
#define    TTDM_HID_DESC           0x44655363    /* DeSc */
#define    TTDM_HID_XLA            0x586c4100    /* XlA */
#define    TTDM_HID_UTIL           0x5574496c    /* UtIl */

#define    SQLHDESC                SQLHANDLE
#define    TT_MAX_CONN_NAME_LEN    1024

/*
 * Character encoding related defines for error stack.
 */
#define    ENCODING_ANSI           8
#define    ENCODING_UTF16          16

/*
 * SQLSTATE related values
 */
#define    TT_SQLSTATE_LEN          5
#define    TT_DM_SQLSTATE_NOTSUPP   "IM001"
#define    TT_DM_SQLSTATE_CONNERR25 "S1000"
#define    TT_DM_SQLSTATE_CONNERR35 "HY000"
#define    TT_DM_SQLSTATE_NOMEM     "S1001"
#define    TT_DM_SQLSTATE_ARGERR25  "S1009"
#define    TT_DM_SQLSTATE_ARGERR35  "HY009"
#define    TT_DM_SQLSTATE_FUNCSEQ   "S1010"
#define    TT_DM_SQLSTATE_BADLEN25  "S1090"
#define    TT_DM_SQLSTATE_BADLEN35  "HY090"
#define    TT_DM_SQLSTATE_NODSN     "IM002"
#define    TT_DM_SQLSTATE_LIBLOAD   "IM003"
#define    TT_DM_SQLSTATE_DATATRUNC "01004"
#define    TT_DM_SQLSTATE_CONNUSED  "08002"
#define    TT_DM_SQLSTATE_NOTCONN   "08003"
#define    TT_DM_SQLSTATE_INVATTR25 "S1C00"
#define    TT_DM_SQLSTATE_INVATTR35 "HY024"

/*
 * Library related values
 */

#define _ARG2STRING(n)        #n
#define ARG2STRING(n)         _ARG2STRING(n)

#ifdef TTDEBUG
#define DBG_LIB_EXTENSION "D"
#else
#define DBG_LIB_EXTENSION 
#endif

#define    TT_LIB_NO_TYPE     0
#define    TT_LIB_DM_TYPE     1
#define    TT_LIB_CS_TYPE     2
#define    TT_LIB_DM_NAME     "libtten" DBG_LIB_EXTENSION SHLIB_SUFFIX
#define    TT_LIB_CS_NAME     "libttclient" SHLIB_SUFFIX

#define    TTDMERRPFX         "[TimesTen][TimesTen Driver Manager " TTDMVERS "]"

#if defined(ENABLE_UTIL_LIB)
#define    TT_LIB_UT_TYPE     3
#define    TT_LIB_UT_NAME     "libttutil" DBG_LIB_EXTENSION SHLIB_SUFFIX

#define    TT_DM_UTIL_LIBERR  TTDMERRPFX "ttUtilAllocEnv: " \
                              "Utility library not loaded"
#define    TT_DM_UTIL_ENV1ERR TTDMERRPFX "ttUtilAllocEnv: " \
                              "Unable to allocate an ODBC environment"
#define    TT_DM_UTIL_MEMERR  TTDMERRPFX " ttUtilAllocEnv: " \
                              "Unable to allocate memory"
#define    TT_DM_UTIL_ENV2ERR TTDMERRPFX "ttUtilFreeEnv: " \
                              "Unable to free the ODBC environment"
#endif /* ENABLE_UTIL_LIB */

/*
 * Values for ODBC function tables. These functions exist in both
 * the direct and client libraries.
 */
#define    NUM_ODBC_FUNCTIONS                 115
#define    MAX_ODBC_FUNCNAME_LEN              33
 
#define    F_POS_ODBC_SQLAllocEnv             0
#define    F_NM_ODBC_SQLAllocEnv              "SQLAllocEnv"
#define    F_POS_ODBC_SQLAllocConnect         1
#define    F_NM_ODBC_SQLAllocConnect          "SQLAllocConnect"
#define    F_POS_ODBC_SQLDriverConnect        2
#define    F_NM_ODBC_SQLDriverConnect         "SQLDriverConnect"
#define    F_POS_ODBC_SQLDisconnect           3
#define    F_NM_ODBC_SQLDisconnect            "SQLDisconnect"
#define    F_POS_ODBC_SQLFreeConnect          4
#define    F_NM_ODBC_SQLFreeConnect           "SQLFreeConnect"
#define    F_POS_ODBC_SQLFreeEnv              5
#define    F_NM_ODBC_SQLFreeEnv               "SQLFreeEnv"
#define    F_POS_ODBC_SQLAllocStmt            6
#define    F_NM_ODBC_SQLAllocStmt             "SQLAllocStmt"
#define    F_POS_ODBC_SQLError                7
#define    F_NM_ODBC_SQLError                 "SQLError"
#define    F_POS_ODBC_SQLBindCol              8
#define    F_NM_ODBC_SQLBindCol               "SQLBindCol"
#define    F_POS_ODBC_SQLCancel               9
#define    F_NM_ODBC_SQLCancel                "SQLCancel"
#define    F_POS_ODBC_SQLColAttributes        10
#define    F_NM_ODBC_SQLColAttributes         "SQLColAttributes"
#define    F_POS_ODBC_SQLConnect              11
#define    F_NM_ODBC_SQLConnect               "SQLConnect"
#define    F_POS_ODBC_SQLDescribeCol          12
#define    F_NM_ODBC_SQLDescribeCol           "SQLDescribeCol"
#define    F_POS_ODBC_SQLExecDirect           13
#define    F_NM_ODBC_SQLExecDirect            "SQLExecDirect"
#define    F_POS_ODBC_SQLExecute              14
#define    F_NM_ODBC_SQLExecute               "SQLExecute"
#define    F_POS_ODBC_SQLFetch                15
#define    F_NM_ODBC_SQLFetch                 "SQLFetch"
#define    F_POS_ODBC_SQLFreeStmt             16
#define    F_NM_ODBC_SQLFreeStmt              "SQLFreeStmt"
#define    F_POS_ODBC_SQLGetCursorName        17
#define    F_NM_ODBC_SQLGetCursorName         "SQLGetCursorName"
#define    F_POS_ODBC_SQLNumResultCols        18
#define    F_NM_ODBC_SQLNumResultCols         "SQLNumResultCols"
#define    F_POS_ODBC_SQLPrepare              19
#define    F_NM_ODBC_SQLPrepare               "SQLPrepare"
#define    F_POS_ODBC_SQLRowCount             20
#define    F_NM_ODBC_SQLRowCount              "SQLRowCount"
#define    F_POS_ODBC_SQLSetCursorName        21
#define    F_NM_ODBC_SQLSetCursorName         "SQLSetCursorName"
#define    F_POS_ODBC_SQLTransact             22
#define    F_NM_ODBC_SQLTransact              "SQLTransact"
#define    F_POS_ODBC_SQLColumns              23
#define    F_NM_ODBC_SQLColumns               "SQLColumns"
#define    F_POS_ODBC_SQLGetConnectOption     24
#define    F_NM_ODBC_SQLGetConnectOption      "SQLGetConnectOption"
#define    F_POS_ODBC_SQLGetData              25
#define    F_NM_ODBC_SQLGetData               "SQLGetData"
#define    F_POS_ODBC_SQLGetFunctions         26
#define    F_NM_ODBC_SQLGetFunctions          "SQLGetFunctions"
#define    F_POS_ODBC_SQLGetInfo              27
#define    F_NM_ODBC_SQLGetInfo               "SQLGetInfo"
#define    F_POS_ODBC_SQLBindParameter        28
#define    F_NM_ODBC_SQLBindParameter         "SQLBindParameter"
#define    F_POS_ODBC_SQLGetStmtOption        29
#define    F_NM_ODBC_SQLGetStmtOption        "SQLGetStmtOption"
#define    F_POS_ODBC_SQLGetTypeInfo          30
#define    F_NM_ODBC_SQLGetTypeInfo           "SQLGetTypeInfo"
#define    F_POS_ODBC_SQLParamData            31
#define    F_NM_ODBC_SQLParamData             "SQLParamData"
#define    F_POS_ODBC_SQLPutData              32
#define    F_NM_ODBC_SQLPutData               "SQLPutData"
#define    F_POS_ODBC_SQLSetConnectOption     33
#define    F_NM_ODBC_SQLSetConnectOption      "SQLSetConnectOption"
#define    F_POS_ODBC_SQLSetStmtOption        34
#define    F_NM_ODBC_SQLSetStmtOption         "SQLSetStmtOption"
#define    F_POS_ODBC_SQLSpecialColumns       35
#define    F_NM_ODBC_SQLSpecialColumns        "SQLSpecialColumns"
#define    F_POS_ODBC_SQLStatistics           36
#define    F_NM_ODBC_SQLStatistics            "SQLStatistics"
#define    F_POS_ODBC_SQLTables               37
#define    F_NM_ODBC_SQLTables                "SQLTables"
#define    F_POS_ODBC_SQLColumnPrivileges     38
#define    F_NM_ODBC_SQLColumnPrivileges      "SQLColumnPrivileges"
#define    F_POS_ODBC_SQLDescribeParam        39
#define    F_NM_ODBC_SQLDescribeParam         "SQLDescribeParam"
#define    F_POS_ODBC_SQLExtendedFetch        40
#define    F_NM_ODBC_SQLExtendedFetch         "SQLExtendedFetch"
#define    F_POS_ODBC_SQLForeignKeys          41
#define    F_NM_ODBC_SQLForeignKeys           "SQLForeignKeys"
#define    F_POS_ODBC_SQLMoreResults          42
#define    F_NM_ODBC_SQLMoreResults           "SQLMoreResults"
#define    F_POS_ODBC_SQLNativeSql            43
#define    F_NM_ODBC_SQLNativeSql             "SQLNativeSql"
#define    F_POS_ODBC_SQLNumParams            44
#define    F_NM_ODBC_SQLNumParams             "SQLNumParams"
#define    F_POS_ODBC_SQLParamOptions         45
#define    F_NM_ODBC_SQLParamOptions          "SQLParamOptions"
#define    F_POS_ODBC_SQLPrimaryKeys          46
#define    F_NM_ODBC_SQLPrimaryKeys           "SQLPrimaryKeys"
#define    F_POS_ODBC_SQLProcedureColumns     47
#define    F_NM_ODBC_SQLProcedureColumns      "SQLProcedureColumns"
#define    F_POS_ODBC_SQLProcedures           48
#define    F_NM_ODBC_SQLProcedures            "SQLProcedures"
#define    F_POS_ODBC_SQLSetPos               49
#define    F_NM_ODBC_SQLSetPos                "SQLSetPos"
#define    F_POS_ODBC_SQLTablePrivileges      50
#define    F_NM_ODBC_SQLTablePrivileges       "SQLTablePrivileges"
#define   F_POS_ODBC_SQLColAttribute          51
#define   F_NM_ODBC_SQLColAttribute           "SQLColAttribute"
#define   F_POS_ODBC_SQLColAttributesW        52
#define   F_NM_ODBC_SQLColAttributesW         "SQLColAttributesW"
#define   F_POS_ODBC_SQLColAttributeW         53
#define   F_NM_ODBC_SQLColAttributeW          "SQLColAttributeW"
#define   F_POS_ODBC_SQLColumnPrivilegesW     54
#define   F_NM_ODBC_SQLColumnPrivilegesW      "SQLColumnPrivilegesW"
#define   F_POS_ODBC_SQLColumnsW              55
#define   F_NM_ODBC_SQLColumnsW               "SQLColumnsW"
#define   F_POS_ODBC_SQLDescribeColW          56
#define   F_NM_ODBC_SQLDescribeColW           "SQLDescribeColW"
#define   F_POS_ODBC_SQLDriverConnectW        57
#define   F_NM_ODBC_SQLDriverConnectW         "SQLDriverConnectW"
#define   F_POS_ODBC_SQLEndTran               58
#define   F_NM_ODBC_SQLEndTran                "SQLEndTran"
#define   F_POS_ODBC_SQLErrorW                59
#define   F_NM_ODBC_SQLErrorW                 "SQLErrorW"
#define   F_POS_ODBC_SQLExecDirectW           60
#define   F_NM_ODBC_SQLExecDirectW            "SQLExecDirectW"
#define   F_POS_ODBC_SQLFetchScroll           61
#define   F_NM_ODBC_SQLFetchScroll            "SQLFetchScroll"
#define   F_POS_ODBC_SQLForeignKeysW          62
#define   F_NM_ODBC_SQLForeignKeysW           "SQLForeignKeysW"
#define   F_POS_ODBC_SQLGetConnectAttr        63
#define   F_NM_ODBC_SQLGetConnectAttr         "SQLGetConnectAttr"
#define   F_POS_ODBC_SQLGetConnectAttrW       64
#define   F_NM_ODBC_SQLGetConnectAttrW        "SQLGetConnectAttrW"
#define   F_POS_ODBC_SQLGetConnectOptionW     65
#define   F_NM_ODBC_SQLGetConnectOptionW      "SQLGetConnectOptionW"
#define   F_POS_ODBC_SQLGetCursorNameW        66
#define   F_NM_ODBC_SQLGetCursorNameW         "SQLGetCursorNameW"
#define   F_POS_ODBC_SQLGetDescField          67
#define   F_NM_ODBC_SQLGetDescField           "SQLGetDescField"
#define   F_POS_ODBC_SQLGetDescFieldW         68
#define   F_NM_ODBC_SQLGetDescFieldW          "SQLGetDescFieldW"
#define   F_POS_ODBC_SQLGetDescRec            69
#define   F_NM_ODBC_SQLGetDescRec             "SQLGetDescRec"
#define   F_POS_ODBC_SQLGetDescRecW           70
#define   F_NM_ODBC_SQLGetDescRecW            "SQLGetDescRecW"
#define   F_POS_ODBC_SQLGetDiagField          71
#define   F_NM_ODBC_SQLGetDiagField           "SQLGetDiagField"
#define   F_POS_ODBC_SQLGetDiagFieldW         72
#define   F_NM_ODBC_SQLGetDiagFieldW          "SQLGetDiagFieldW"
#define   F_POS_ODBC_SQLGetDiagRec            73
#define   F_NM_ODBC_SQLGetDiagRec             "SQLGetDiagRec"
#define   F_POS_ODBC_SQLGetDiagRecW           74
#define   F_NM_ODBC_SQLGetDiagRecW            "SQLGetDiagRecW"
#define   F_POS_ODBC_SQLGetEnvAttr            75
#define   F_NM_ODBC_SQLGetEnvAttr             "SQLGetEnvAttr"
#define   F_POS_ODBC_SQLGetInfoW              76
#define   F_NM_ODBC_SQLGetInfoW               "SQLGetInfoW"
#define   F_POS_ODBC_SQLGetStmtAttr           77
#define   F_NM_ODBC_SQLGetStmtAttr            "SQLGetStmtAttr"
#define   F_POS_ODBC_SQLGetStmtAttrW          78
#define   F_NM_ODBC_SQLGetStmtAttrW           "SQLGetStmtAttrW"
#define   F_POS_ODBC_SQLGetTypeInfoW          79
#define   F_NM_ODBC_SQLGetTypeInfoW           "SQLGetTypeInfoW"
#define   F_POS_ODBC_SQLNativeSqlW            80
#define   F_NM_ODBC_SQLNativeSqlW             "SQLNativeSqlW"
#define   F_POS_ODBC_SQLPrepareW              81
#define   F_NM_ODBC_SQLPrepareW               "SQLPrepareW"
#define   F_POS_ODBC_SQLPrimaryKeysW          82
#define   F_NM_ODBC_SQLPrimaryKeysW           "SQLPrimaryKeysW"
#define   F_POS_ODBC_SQLProcedureColumnsW     83
#define   F_NM_ODBC_SQLProcedureColumnsW      "SQLProcedureColumnsW"
#define   F_POS_ODBC_SQLProceduresW           84
#define   F_NM_ODBC_SQLProceduresW            "SQLProceduresW"
#define   F_POS_ODBC_SQLSetConnectAttr        85
#define   F_NM_ODBC_SQLSetConnectAttr         "SQLSetConnectAttr"
#define   F_POS_ODBC_SQLSetConnectAttrW       86
#define   F_NM_ODBC_SQLSetConnectAttrW        "SQLSetConnectAttrW"
#define   F_POS_ODBC_SQLSetConnectOptionW     87
#define   F_NM_ODBC_SQLSetConnectOptionW      "SQLSetConnectOptionW"
#define   F_POS_ODBC_SQLSetCursorNameW        88
#define   F_NM_ODBC_SQLSetCursorNameW         "SQLSetCursorNameW"
#define   F_POS_ODBC_SQLSetDescField          89
#define   F_NM_ODBC_SQLSetDescField           "SQLSetDescField"
#define   F_POS_ODBC_SQLSetDescFieldW         90
#define   F_NM_ODBC_SQLSetDescFieldW          "SQLSetDescFieldW"
#define   F_POS_ODBC_SQLSetDescRec            91
#define   F_NM_ODBC_SQLSetDescRec             "SQLSetDescRec"
#define   F_POS_ODBC_SQLSetEnvAttr            92
#define   F_NM_ODBC_SQLSetEnvAttr             "SQLSetEnvAttr"
#define   F_POS_ODBC_SQLSetStmtAttr           93
#define   F_NM_ODBC_SQLSetStmtAttr            "SQLSetStmtAttr"
#define   F_POS_ODBC_SQLSetStmtAttrW          94
#define   F_NM_ODBC_SQLSetStmtAttrW           "SQLSetStmtAttrW"
#define   F_POS_ODBC_SQLSpecialColumnsW       95
#define   F_NM_ODBC_SQLSpecialColumnsW        "SQLSpecialColumnsW"
#define   F_POS_ODBC_SQLStatisticsW           96
#define   F_NM_ODBC_SQLStatisticsW            "SQLStatisticsW"
#define   F_POS_ODBC_SQLTablePrivilegesW      97
#define   F_NM_ODBC_SQLTablePrivilegesW       "SQLTablePrivilegesW"
#define   F_POS_ODBC_SQLTablesW               98
#define   F_NM_ODBC_SQLTablesW                "SQLTablesW"
#define   F_POS_ODBC_SQLAllocHandle           99
#define   F_NM_ODBC_SQLAllocHandle            "SQLAllocHandle"
#define   F_POS_ODBC_ttSQLConnectW            100
#define   F_NM_ODBC_ttSQLConnectW             "ttSQLConnectW"
#define   F_POS_ODBC_SQLFreeHandle            101
#define   F_NM_ODBC_SQLFreeHandle             "SQLFreeHandle"
#define   F_POS_ODBC_SQLBulkOperations        102
#define   F_NM_ODBC_SQLBulkOperations         "SQLBulkOperations"
#define   F_POS_ODBC_SQLCloseCursor           103
#define   F_NM_ODBC_SQLCloseCursor            "SQLCloseCursor"
#define   F_POS_ODBC_SQLCopyDesc              104
#define   F_NM_ODBC_SQLCopyDesc               "SQLCopyDesc"
#define   F_POS_ODBC_SQLSetParam              105
#define   F_NM_ODBC_SQLSetParam               "SQLSetParam"
#define   F_POS_ODBC_SQLBindParameterExtended 106
#define   F_NM_ODBC_SQLBindParameterExtended  "SQLBindParameterExtended"
// Scaleout routing API functions
#define   F_POS_ODBC_ttGridDistClear          107
#define   F_NM_ODBC_ttGridDistClear           "ttGridDistClear"
#define   F_POS_ODBC_ttGridDistCreate         108
#define   F_NM_ODBC_ttGridDistCreate          "ttGridDistCreate"
#define   F_POS_ODBC_ttGridDistElementGet     109
#define   F_NM_ODBC_ttGridDistElementGet      "ttGridDistElementGet"
#define   F_POS_ODBC_ttGridDistReplicaGet     110
#define   F_NM_ODBC_ttGridDistReplicaGet      "ttGridDistReplicaGet"
#define   F_POS_ODBC_ttGridDistFree           111
#define   F_NM_ODBC_ttGridDistFree            "ttGridDistFree"
#define   F_POS_ODBC_ttGridDistValueSet       112
#define   F_NM_ODBC_ttGridDistValueSet        "ttGridDistValueSet"
#define   F_POS_ODBC_ttGridMapCreate          113
#define   F_NM_ODBC_ttGridMapCreate           "ttGridMapCreate"
#define   F_POS_ODBC_ttGridMapFree            114
#define   F_NM_ODBC_ttGridMapFree             "ttGridMapFree"

#if defined(ENABLE_XLA_DIRECT)
/*
 * Values for XLA function tables. Direct mode only.
 */
#define   NUM_XLA_FUNCTIONS                    46
#define   MAX_XLA_FUNCNAME_LEN                 35

#define   F_POS_XLA_ttXlaAcknowledge            0
#define   F_NM_XLA_ttXlaAcknowledge             "ttXlaAcknowledge"
#define   F_POS_XLA_ttXlaApply                  1
#define   F_NM_XLA_ttXlaApply                   "ttXlaApply"
#define   F_POS_XLA_ttXlaClose                  2
#define   F_NM_XLA_ttXlaClose                   "ttXlaClose"
#define   F_POS_XLA_ttXlaCommit                 3
#define   F_NM_XLA_ttXlaCommit                  "ttXlaCommit"
#define   F_POS_XLA_ttXlaConvertCharType        4
#define   F_NM_XLA_ttXlaConvertCharType         "ttXlaConvertCharType"
#define   F_POS_XLA_ttXlaDateToODBCCType        5
#define   F_NM_XLA_ttXlaDateToODBCCType         "ttXlaDateToODBCCType"
#define   F_POS_XLA_ttXlaDecimalToCString       6
#define   F_NM_XLA_ttXlaDecimalToCString        "ttXlaDecimalToCString"
#define   F_POS_XLA_ttXlaDeleteBookmark         7
#define   F_NM_XLA_ttXlaDeleteBookmark          "ttXlaDeleteBookmark"
#define   F_POS_XLA_ttXlaError                  8
#define   F_NM_XLA_ttXlaError                   "ttXlaError"
#define   F_POS_XLA_ttXlaErrorRestart           9
#define   F_NM_XLA_ttXlaErrorRestart            "ttXlaErrorRestart"
#define   F_POS_XLA_ttXlaGenerateSQL           10
#define   F_NM_XLA_ttXlaGenerateSQL             "ttXlaGenerateSQL"
#define   F_POS_XLA_ttXlaGetColumnInfo         11
#define   F_NM_XLA_ttXlaGetColumnInfo           "ttXlaGetColumnInfo"
#define   F_POS_XLA_ttXlaGetLSN                12
#define   F_NM_XLA_ttXlaGetLSN                  "ttXlaGetLSN"
#define   F_POS_XLA_ttXlaGetTableInfo          13
#define   F_NM_XLA_ttXlaGetTableInfo            "ttXlaGetTableInfo"
#define   F_POS_XLA_ttXlaGetVersion            14
#define   F_NM_XLA_ttXlaGetVersion              "ttXlaGetVersion"
#define   F_POS_XLA_ttXlaLookup                15
#define   F_NM_XLA_ttXlaLookup                  "ttXlaLookup"
#define   F_POS_XLA_ttXlaNextUpdate            16
#define   F_NM_XLA_ttXlaNextUpdate              "ttXlaNextUpdate"
#define   F_POS_XLA_ttXlaNextUpdateWait        17
#define   F_NM_XLA_ttXlaNextUpdateWait          "ttXlaNextUpdateWait"
#define   F_POS_XLA_ttXlaNumberToBigInt        18
#define   F_NM_XLA_ttXlaNumberToBigInt          "ttXlaNumberToBigInt"
#define   F_POS_XLA_ttXlaNumberToCString       19
#define   F_NM_XLA_ttXlaNumberToCString         "ttXlaNumberToCString"
#define   F_POS_XLA_ttXlaNumberToDouble        20
#define   F_NM_XLA_ttXlaNumberToDouble          "ttXlaNumberToDouble"
#define   F_POS_XLA_ttXlaNumberToInt           21
#define   F_NM_XLA_ttXlaNumberToInt             "ttXlaNumberToInt"
#define   F_POS_XLA_ttXlaNumberToSmallInt      22
#define   F_NM_XLA_ttXlaNumberToSmallInt        "ttXlaNumberToSmallInt"
#define   F_POS_XLA_ttXlaNumberToTinyInt       23
#define   F_NM_XLA_ttXlaNumberToTinyInt         "ttXlaNumberToTinyInt"
#define   F_POS_XLA_ttXlaNumberToUInt          24
#define   F_NM_XLA_ttXlaNumberToUInt            "ttXlaNumberToUInt"
#define   F_POS_XLA_ttXlaOraDateToODBCTimeStamp 25
#define   F_NM_XLA_ttXlaOraDateToODBCTimeStamp  "ttXlaOraDateToODBCTimeStamp"
#define   F_POS_XLA_ttXlaOraTimeStampToODBCTimeStamp 26
#define   F_NM_XLA_ttXlaOraTimeStampToODBCTimeStamp  "ttXlaOraTimeStampToODBCTimeStamp"
#define   F_POS_XLA_ttXlaPersistOpen           27
#define   F_NM_XLA_ttXlaPersistOpen             "ttXlaPersistOpen"
#define   F_POS_XLA_ttXlaRollback              28
#define   F_NM_XLA_ttXlaRollback                "ttXlaRollback"
#define   F_POS_XLA_ttXlaSetLSN                29
#define   F_NM_XLA_ttXlaSetLSN                  "ttXlaSetLSN"
#define   F_POS_XLA_ttXlaSetVersion            30
#define   F_NM_XLA_ttXlaSetVersion              "ttXlaSetVersion"
#define   F_POS_XLA_ttXlaTableByName           31
#define   F_NM_XLA_ttXlaTableByName             "ttXlaTableByName"
#define   F_POS_XLA_ttXlaTableCheck            32
#define   F_NM_XLA_ttXlaTableCheck              "ttXlaTableCheck"
#define   F_POS_XLA_ttXlaTableStatus           33
#define   F_NM_XLA_ttXlaTableStatus             "ttXlaTableStatus"
#define   F_POS_XLA_ttXlaTableVersionVerify    34
#define   F_NM_XLA_ttXlaTableVersionVerify      "ttXlaTableVersionVerify"
#define   F_POS_XLA_ttXlaTimeStampToODBCCType  35
#define   F_NM_XLA_ttXlaTimeStampToODBCCType    "ttXlaTimeStampToODBCCType"
#define   F_POS_XLA_ttXlaTimeToODBCCType       36
#define   F_NM_XLA_ttXlaTimeToODBCCType         "ttXlaTimeToODBCCType"
#define   F_POS_XLA_ttXlaVersionColumnInfo     37
#define   F_NM_XLA_ttXlaVersionColumnInfo       "ttXlaVersionColumnInfo"
#define   F_POS_XLA_ttXlaVersionCompare        38
#define   F_NM_XLA_ttXlaVersionCompare          "ttXlaVersionCompare"
#define   F_POS_XLA_ttXlaVersionTableInfo      39
#define   F_NM_XLA_ttXlaVersionTableInfo        "ttXlaVersionTableInfo"
/*
 * Undocumented public functions
 */
#define   F_POS_XLA_ttXlaEpilog2               40
#define   F_NM_XLA_ttXlaEpilog2                 "ttXlaEpilog2"
#define   F_POS_XLA_ttXlaInvalidateTbl2        41
#define   F_NM_XLA_ttXlaInvalidateTbl2          "ttXlaInvalidateTbl2"
#define   F_POS_XLA_ttXlaGenerateSQL2          42
#define   F_NM_XLA_ttXlaGenerateSQL2            "ttXlaGenerateSQL2"
#define   F_POS_XLA_ttXlaTranslate             43
#define   F_NM_XLA_ttXlaTranslate               "ttXlaTranslate"
#define   F_POS_XLA_ttXlaSqlOption             44
#define   F_NM_XLA_ttXlaSqlOption               "ttXlaSqlOption"
#define   F_POS_XLA_ttXlaRowidToCString        45
#define   F_NM_XLA_ttXlaRowidToCString         "ttXlaRowidToCString"

#endif /* ENABLE_XLA_DIRECT */

#if defined(ENABLE_UTIL_LIB)
/*
 * Values for Utility function tables. Direct mode only.
 * Depends on presence of the utility library.
 */
#define   NUM_UTIL_FUNCTIONS                   14
#define   MAX_UTIL_FUNCNAME_LEN                32

#define   F_POS_UTIL_ttUtilAllocEnv            0
#define   F_NM_UTIL_ttUtilAllocEnv             "ttUtilAllocEnv"
#define   F_POS_UTIL_ttUtilFreeEnv             1
#define   F_NM_UTIL_ttUtilFreeEnv              "ttUtilFreeEnv"
#define   F_POS_UTIL_ttUtilGetErrorCount       2
#define   F_NM_UTIL_ttUtilGetErrorCount        "ttUtilGetErrorCount"
#define   F_POS_UTIL_ttUtilGetError            3
#define   F_NM_UTIL_ttUtilGetError             "ttUtilGetError"
#define   F_POS_UTIL_ttBackup                  4
#define   F_NM_UTIL_ttBackup                   "ttBackup"
#define   F_POS_UTIL_ttDestroyDataStore        5
#define   F_NM_UTIL_ttDestroyDataStore         "ttDestroyDataStore"
#define   F_POS_UTIL_ttDestroyDataStoreForce   6
#define   F_NM_UTIL_ttDestroyDataStoreForce    "ttDestroyDataStoreForce"
#define   F_POS_UTIL_ttRamGrace                7
#define   F_NM_UTIL_ttRamGrace                 "ttRamGrace"
#define   F_POS_UTIL_ttRamLoad                 8
#define   F_NM_UTIL_ttRamLoad                  "ttRamLoad"
#define   F_POS_UTIL_ttRamPolicy               9
#define   F_NM_UTIL_ttRamPolicy                "ttRamPolicy"
#define   F_POS_UTIL_ttRamUnload               10
#define   F_NM_UTIL_ttRamUnload                "ttRamUnload"
#define   F_POS_UTIL_ttRepDuplicateEx          11
#define   F_NM_UTIL_ttRepDuplicateEx           "ttRepDuplicateEx"
#define   F_POS_UTIL_ttRestore                 12
#define   F_NM_UTIL_ttRestore                  "ttRestore"
#define   F_POS_UTIL_ttXactIdRollback          13
#define   F_NM_UTIL_ttXactIdRollback           "ttXactIdRollback"
// Beyond here are not really public/supported
#if 0
#define   F_POS_UTIL_ttXactIdRollbackExecuting 14
#define   F_NM_UTIL_ttXactIdRollbackExecuting  "ttXactIdRollbackExecuting"
#define   F_POS_UTIL_ttXactIdCommit            15
#define   F_NM_UTIL_ttXactIdCommit             "ttXactIdCommit"
#define   F_POS_UTIL_ttDisconnect              16
#define   F_NM_UTIL_ttDisconnect               "ttDisconnect"
#define   F_POS_UTIL_ttRamNoPersist            17
#define   F_NM_UTIL_ttRamNoPersist             "ttRamNoPersist"
#define   F_POS_UTIL_ttRamPersist              18
#define   F_NM_UTIL_ttRamPersist               "ttRamPersist"
#define   F_POS_UTIL_ttReadEpochs              19
#define   F_NM_UTIL_ttReadEpochs               "ttReadEpochs"
#define   F_POS_UTIL_ttRepDuplicateCheck       20
#define   F_NM_UTIL_ttRepDuplicateCheck        "ttRepDuplicateCheck"
#define   F_POS_UTIL_ttRepGetCTNCksum          21
#define   F_NM_UTIL_ttRepGetCTNCksum           "ttRepGetCTNCksum"
#define   F_POS_UTIL_ttRepGetStateRole         22
#define   F_NM_UTIL_ttRepGetStateRole          "ttRepGetStateRole"
#define   F_POS_UTIL_ttRepGetSubState          23
#define   F_NM_UTIL_ttRepGetSubState           "ttRepGetSubState"
#define   F_POS_UTIL_ttRepRoleCheck            24
#define   F_NM_UTIL_ttRepRoleCheck             "ttRepRoleCheck"
#define   F_POS_UTIL_ttUtDbClose               25
#define   F_NM_UTIL_ttUtDbClose                "ttUtDbClose"
#define   F_POS_UTIL_ttUtDbOpen                26
#define   F_NM_UTIL_ttUtDbOpen                 "ttUtDbOpen"
#define   F_POS_UTIL_ttUtilGetDaemonStatus     27
#define   F_NM_UTIL_ttUtilGetDaemonStatus      "ttUtilGetDaemonStatus"
#endif /* unsupported functions */

#endif /* ENABLE_UTIL_LIB */

/*
 ========================================================================
                 Type definitions
 ========================================================================
 */

/*
 * Type definitions. Defined here so can be used below.
 */

typedef void *                  tt_libptr_t;
typedef void *                  tt_funcptr_t;
typedef pthread_mutex_t       * tt_mutex_t;
typedef struct s_funcmapper     funcmapper_t;
typedef struct s_ODBC_ERR       ODBC_ERR_t;
typedef struct s_ODBC_ERR_STACK ODBC_ERR_STACK_t;
typedef void *                  TTHANDLE;
typedef struct s_TT_ENV         TT_ENV_t;
typedef TT_ENV_t              * TTSQLHENV;
typedef struct s_TT_DBC         TT_DBC_t;
typedef TT_DBC_t              * TTSQLHDBC;
typedef struct s_TT_STMT        TT_STMT_t;
typedef TT_STMT_t             * TTSQLHSTMT;
typedef struct s_TT_DESC        TT_DESC_t;
typedef TT_DESC_t             * TTSQLHDESC;
#if defined(ENABLE_XLA_DIRECT)
typedef struct s_ttXlaHandle    TTXlaHandle_t;
#endif
#if defined(ENABLE_UTIL_LIB)
typedef struct s_TTUtilH        TTUtilHandle_t;
typedef TTUtilHandle_t        * TTUtilHandle;
#endif
typedef struct s_DMGlobalData   DMGlobalData_t;

/*
 * Structure used to map functions to library specific entry points
 */

struct s_funcmapper
{
    int          libType; /* TT_LIB_DM_TYPE, TT_LIB_CS_TYPE or TT_LIB_UT_TYPE */
    tt_libptr_t  libHandle;
    void     * * odbcFuncPtr;
#if defined(ENABLE_XLA_DIRECT)
    void     * * xlaFuncPtr;
#endif /* ENABLE_XLA_DIRECT */
#if defined(ENABLE_UTIL_LIB)
    void     * * utilFuncPtr;
#endif /* ENABLE_UTIL_LIB */
};

/*
 * ODBC Error/Warning Information.
 *
 * TTDM maintains its own error stacks. Each entry on the stack
 * (an ODBC_ERR_t) may contain text in either UTF-8/ANSI or UTF-16
 * encoding (depending on the source). Generally conversion to
 * the final target encoding is deferred until needed, since the
 * error may never actually be used by the application.
 */

struct s_ODBC_ERR
{
    SQLRETURN           rc;
    int                 encoding; /* 8 = ANSI/UTF-8, 16 = UTF-16 */
    SQLCHAR             sqlState[2*(TT_SQLSTATE_LEN+1)];
    SQLINTEGER          nativeError;
    SQLCHAR             errorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];
    SQLCHAR           * diagClassOrigin;
    SQLCHAR           * diagSubclassOrigin;
    SQLCHAR           * diagConnectionName;
    SQLCHAR           * diagServerName;
    ODBC_ERR_t        * next;
};

struct s_ODBC_ERR_STACK
{
    int                 msgcnt;
    ODBC_ERR_t        * stackH;
    ODBC_ERR_t        * stackT;
};

/*
 * Our version of an Environment. There is a dependency between
 * an environment and the connections allocated from it so the
 * environment maintains a list of all its dependent connections.
 */

struct s_TT_ENV
{
    unsigned int       tag;        /* Structure validation tag */
    unsigned int       hid;        /* Handle type id */
    int                v3;         /* is ODBC V3 */
    SQLHENV            directEnv;  /* The direct ODBC HENV for this TT_ENV_t */
    SQLHENV            clientEnv;  /* The client ODBC HENV for this TT_ENV_t */
    funcmapper_t     * pDirectMap; /* pointer to Direct function map */
    funcmapper_t     * pClientMap; /* pointer to Client function map */
    ODBC_ERR_STACK_t   errorStack;
    TT_ENV_t         * prev;       /* List of all TT_ENV_t */
    TT_ENV_t         * next;    
    TT_DBC_t         * head;       /* List of all associated TT_DBC_t */
    TT_DBC_t         * tail;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t         mutex;
#endif /* ENABLE_THREAD_SAFETY */
};

/*
 * Our version of a Connection. There is a dependency between
 * a connection and the statements/descriptors allocated from
 * it so we maintain lists of those.
 */

struct s_TT_DBC
{
    unsigned int       tag;        /* Structure validation tag */
    unsigned int       hid;        /* Handle type id */
    int                v3;         /* is ODBC V3 */
    TTSQLHENV          TThEnv;     /* The TT_ENV_t that owns this connection */
    SQLHENV            directEnv;  /* The direct ODBC HENV for this TT_ENV_t */
    SQLHENV            clientEnv;  /* The client ODBC HENV for this TT_ENV_t */
    SQLHENV            hEnv;       /* The actual HENV that owns this connection */
    SQLHDBC            directDbc;  /* The direct ODBC HDBC for this connection */
    SQLHDBC            clientDbc;  /* The client ODBC HDBC for this connection */
    SQLHDBC            hDbc;       /* The actual HDBC for this connection */
    SQLCHAR          * connName;
    SQLWCHAR         * connNameW;
    SQLCHAR          * serverName;
    SQLWCHAR         * serverNameW;
    funcmapper_t     * pDirectMap; /* pointer to Client function map */
    funcmapper_t     * pClientMap; /* pointer to Direct function map */
    funcmapper_t     * pFuncMap;   /* pointer to in use function map */
    ODBC_ERR_STACK_t   errorStack;
#if defined(ENABLE_XLA_DIRECT)
    TTXlaHandle_t    * TThXla;
#endif /* ENABLE_XLA_DIRECT */
    TT_DBC_t         * prev;       /* List of all TT_DBC_t bellonging to TThEnv */
    TT_DBC_t         * next;
    TT_STMT_t        * head;       /* List of all associated TT_STMT_t */
    TT_STMT_t        * tail;
    TT_DESC_t        * dhead;      /* List of all associated TT_DESC_t */
    TT_DESC_t        * dtail;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t         mutex;
#endif /* ENABLE_THREAD_SAFETY */
};

/*
 * Our version of a Statement
 */

struct s_TT_STMT
{
    unsigned int       tag;        /* Structure validation tag */
    unsigned int       hid;        /* Handle type id */
    int                v3;         /* is ODBC V3 */
    TTSQLHENV          TThEnv;     /* The TT_ENV_t that owns this statement */
    TTSQLHDBC          TThDbc;     /* The TT_DBC_t that owns this statement */
    SQLHENV            hEnv;       /* The real ODBC HENV that owns this statement */
    SQLHDBC            hDbc;       /* The real ODBC HDBC that owns this statement */
    SQLHSTMT           hStmt;      /* The real ODBC HSTMT for this statement */
    SQLHDESC           IRD;        /* The implicit row descriptor */
    SQLHDESC           IPD;        /* The implicit parameter descriptor */
    funcmapper_t     * pFuncMap;   /* pointer to function map */
    ODBC_ERR_STACK_t   errorStack;
    TT_STMT_t        * prev;       /* List of all TT_STMT_t bellonging to TThDbc */
    TT_STMT_t        * next;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t         mutex;
#endif /* ENABLE_THREAD_SAFETY */
};

/*
 * Our version of a Descriptor
 */

struct s_TT_DESC
{
    unsigned int       tag;        /* Structure validation tag */
    unsigned int       hid;        /* Handle type id */
    int                implicit;   /* non-zero = imlicit descriptor */
    TTSQLHENV          TThEnv;     /* The TT_ENV_t that owns this descriptor */
    TTSQLHDBC          TThDbc;     /* The TT_DBC_t that owns this descriptor */
    SQLHENV            hEnv;       /* The real ODBC HENV that owns this statement */
    SQLHDBC            hDbc;       /* The real ODBC HDBC that owns this statement */
    SQLHDESC           hDesc;      /* The real ODBC handle for the descriptor */
    funcmapper_t     * pFuncMap;   /* pointer to function map */
    ODBC_ERR_STACK_t   errorStack;
    TT_DESC_t        * prev;       /* List of all TT_DESC_t bellonging to TThDbc */
    TT_DESC_t        * next;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t         mutex;
#endif /* ENABLE_THREAD_SAFETY */
};

#if defined(ENABLE_XLA_DIRECT)
/*
 * Our version of a ttXlaHandle
 */

struct s_ttXlaHandle
{
    unsigned int       tag;        /* Structure validation tag */
    unsigned int       hid;        /* Handle type id */
    TTSQLHENV          TThEnv;
    TTSQLHDBC          TThDbc;
    ttXlaHandle_h      hXla;
    funcmapper_t     * pFuncMap;   /* pointer to function map */
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t         pMutex;     /* pointer to mutex */
#endif /* ENABLE_THREAD_SAFETY */
};
#endif /* ENABLE_XLA_DIRECT */

#if defined(ENABLE_UTIL_LIB)
/*
 * Our version of a ttUtilHandle
 */

struct s_TTUtilH
{
    unsigned int       tag;
    unsigned int       hid;        /* Handle type id */
    ttUtilHandle       hUtil;      /* actual utility library handle */
    TTUtilHandle_t   * prev;
    TTUtilHandle_t   * next;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t         mutex;
#endif /* ENABLE_THREAD_SAFETY */
};
#endif /* ENABLE_UTIL_LIB */

/*
 * Global data for an instance of the driver manager (i.e. a process).
 *
 * This is all localised into a single structure to facilitate making the
 * DM thread safe.
 */

struct s_DMGlobalData
{
    /* Master list of all environments */
    TT_ENV_t       * ELhead;
    TT_ENV_t       * ELtail;
    funcmapper_t   * directFuncs;
    funcmapper_t   * clientFuncs;
#if defined(ENABLE_UTIL_LIB)
    TTUtilHandle_t * UThead;
    TTUtilHandle_t * UTtail;
    funcmapper_t   * utilFuncs;
    TTSQLHENV        utilEnv;
#endif /* ENABLE_UTIL_LIB */
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t       mutex;
#if defined(ENABLE_UTIL_LIB)
    tt_mutex_t       utilMutex;
#endif /* ENABLE_UTIL_LIB */
#endif /* ENABLE_THREAD_SAFETY */
};

/*
 ========================================================================
                 Private Data
 ========================================================================
 */

#define    TTDM_DIAG_CLASS_ORIGIN     "ODBC 3.0"
#define    TTDM_DIAG_SUBCLASS_ORIGIN  "ODBC 3.0"
SQLWCHAR   TTDM_DIAG_CLASS_ORIGIN_W[] = { 79, 68, 66, 67, 32, 51, 46, 48, 0 };
SQLWCHAR   TTDM_DIAG_SUBCLASS_ORIGIN_W[] = { 79, 68, 66, 67, 32, 51, 46, 48, 0 };

#if defined(ENABLE_THREAD_SAFETY)
/*
 * Mutexes used to avoid race conditions durong initialisation.
 */
static pthread_mutex_t  globalMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  globalUtilMutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* ENABLE_THREAD_SAFETY */

/*
 * Array of ODBC Function Names.
 *
 * This is never modified during execution.
 */
static const char
ODBCFuncName[NUM_ODBC_FUNCTIONS][MAX_ODBC_FUNCNAME_LEN+1] =
{
    F_NM_ODBC_SQLAllocEnv,      
    F_NM_ODBC_SQLAllocConnect,  
    F_NM_ODBC_SQLDriverConnect, 
    F_NM_ODBC_SQLDisconnect,   
    F_NM_ODBC_SQLFreeConnect, 
    F_NM_ODBC_SQLFreeEnv,         
    F_NM_ODBC_SQLAllocStmt,      
    F_NM_ODBC_SQLError,         
    F_NM_ODBC_SQLBindCol,      
    F_NM_ODBC_SQLCancel,      
    F_NM_ODBC_SQLColAttributes,    
    F_NM_ODBC_SQLConnect,         
    F_NM_ODBC_SQLDescribeCol,    
    F_NM_ODBC_SQLExecDirect,    
    F_NM_ODBC_SQLExecute,      
    F_NM_ODBC_SQLFetch,       
    F_NM_ODBC_SQLFreeStmt,            
    F_NM_ODBC_SQLGetCursorName,      
    F_NM_ODBC_SQLNumResultCols,     
    F_NM_ODBC_SQLPrepare,          
    F_NM_ODBC_SQLRowCount,        
    F_NM_ODBC_SQLSetCursorName,  
    F_NM_ODBC_SQLTransact,      
    F_NM_ODBC_SQLColumns,      
    F_NM_ODBC_SQLGetConnectOption,     
    F_NM_ODBC_SQLGetData,             
    F_NM_ODBC_SQLGetFunctions,       
    F_NM_ODBC_SQLGetInfo,          
    F_NM_ODBC_SQLBindParameter,   
    F_NM_ODBC_SQLGetStmtOption,  
    F_NM_ODBC_SQLGetTypeInfo,   
    F_NM_ODBC_SQLParamData,    
    F_NM_ODBC_SQLPutData,     
    F_NM_ODBC_SQLSetConnectOption,    
    F_NM_ODBC_SQLSetStmtOption,      
    F_NM_ODBC_SQLSpecialColumns,    
    F_NM_ODBC_SQLStatistics,       
    F_NM_ODBC_SQLTables,          
    F_NM_ODBC_SQLColumnPrivileges,
    F_NM_ODBC_SQLDescribeParam,
    F_NM_ODBC_SQLExtendedFetch,
    F_NM_ODBC_SQLForeignKeys,
    F_NM_ODBC_SQLMoreResults,
    F_NM_ODBC_SQLNativeSql,
    F_NM_ODBC_SQLNumParams,
    F_NM_ODBC_SQLParamOptions,
    F_NM_ODBC_SQLPrimaryKeys,
    F_NM_ODBC_SQLProcedureColumns,
    F_NM_ODBC_SQLProcedures,
    F_NM_ODBC_SQLSetPos,
    F_NM_ODBC_SQLTablePrivileges,
    F_NM_ODBC_SQLColAttribute,
    F_NM_ODBC_SQLColAttributesW,
    F_NM_ODBC_SQLColAttributeW,
    F_NM_ODBC_SQLColumnPrivilegesW,
    F_NM_ODBC_SQLColumnsW,
    F_NM_ODBC_SQLDescribeColW,
    F_NM_ODBC_SQLDriverConnectW,
    F_NM_ODBC_SQLEndTran,
    F_NM_ODBC_SQLErrorW,
    F_NM_ODBC_SQLExecDirectW,
    F_NM_ODBC_SQLFetchScroll,
    F_NM_ODBC_SQLForeignKeysW,
    F_NM_ODBC_SQLGetConnectAttr,
    F_NM_ODBC_SQLGetConnectAttrW,
    F_NM_ODBC_SQLGetConnectOptionW,
    F_NM_ODBC_SQLGetCursorNameW,
    F_NM_ODBC_SQLGetDescField,
    F_NM_ODBC_SQLGetDescFieldW,
    F_NM_ODBC_SQLGetDescRec,
    F_NM_ODBC_SQLGetDescRecW,
    F_NM_ODBC_SQLGetDiagField,
    F_NM_ODBC_SQLGetDiagFieldW,
    F_NM_ODBC_SQLGetDiagRec,
    F_NM_ODBC_SQLGetDiagRecW,
    F_NM_ODBC_SQLGetEnvAttr,
    F_NM_ODBC_SQLGetInfoW,
    F_NM_ODBC_SQLGetStmtAttr,
    F_NM_ODBC_SQLGetStmtAttrW,
    F_NM_ODBC_SQLGetTypeInfoW,
    F_NM_ODBC_SQLNativeSqlW,
    F_NM_ODBC_SQLPrepareW,
    F_NM_ODBC_SQLPrimaryKeysW,
    F_NM_ODBC_SQLProcedureColumnsW,
    F_NM_ODBC_SQLProceduresW,
    F_NM_ODBC_SQLSetConnectAttr,
    F_NM_ODBC_SQLSetConnectAttrW,
    F_NM_ODBC_SQLSetConnectOptionW,
    F_NM_ODBC_SQLSetCursorNameW,
    F_NM_ODBC_SQLSetDescField,
    F_NM_ODBC_SQLSetDescFieldW,
    F_NM_ODBC_SQLSetDescRec,
    F_NM_ODBC_SQLSetEnvAttr,
    F_NM_ODBC_SQLSetStmtAttr,
    F_NM_ODBC_SQLSetStmtAttrW,
    F_NM_ODBC_SQLSpecialColumnsW,
    F_NM_ODBC_SQLStatisticsW,
    F_NM_ODBC_SQLTablePrivilegesW,
    F_NM_ODBC_SQLTablesW,
    F_NM_ODBC_SQLAllocHandle,
    F_NM_ODBC_ttSQLConnectW,
    F_NM_ODBC_SQLFreeHandle,
    F_NM_ODBC_SQLBulkOperations,
    F_NM_ODBC_SQLCloseCursor,
    F_NM_ODBC_SQLCopyDesc,
    F_NM_ODBC_SQLSetParam,
    F_NM_ODBC_SQLBindParameterExtended,
    F_NM_ODBC_ttGridDistClear,
    F_NM_ODBC_ttGridDistCreate,
    F_NM_ODBC_ttGridDistElementGet,
    F_NM_ODBC_ttGridDistReplicaGet,
    F_NM_ODBC_ttGridDistFree,
    F_NM_ODBC_ttGridDistValueSet,
    F_NM_ODBC_ttGridMapCreate,
    F_NM_ODBC_ttGridMapFree
};

#if defined(ENABLE_XLA_DIRECT)
/*
 * Array of ODBC Function Names.
 *
 * This is never modified during execution.
 */
static const char
XLAFuncName[NUM_XLA_FUNCTIONS][MAX_XLA_FUNCNAME_LEN+1] =
{
    F_NM_XLA_ttXlaAcknowledge,
    F_NM_XLA_ttXlaApply,
    F_NM_XLA_ttXlaClose,
    F_NM_XLA_ttXlaCommit,
    F_NM_XLA_ttXlaConvertCharType,
    F_NM_XLA_ttXlaDateToODBCCType,
    F_NM_XLA_ttXlaDecimalToCString,
    F_NM_XLA_ttXlaDeleteBookmark,
    F_NM_XLA_ttXlaError,
    F_NM_XLA_ttXlaErrorRestart,
    F_NM_XLA_ttXlaGenerateSQL,
    F_NM_XLA_ttXlaGetColumnInfo,
    F_NM_XLA_ttXlaGetLSN,
    F_NM_XLA_ttXlaGetTableInfo,
    F_NM_XLA_ttXlaGetVersion,
    F_NM_XLA_ttXlaLookup,
    F_NM_XLA_ttXlaNextUpdate,
    F_NM_XLA_ttXlaNextUpdateWait,
    F_NM_XLA_ttXlaNumberToBigInt,
    F_NM_XLA_ttXlaNumberToCString,
    F_NM_XLA_ttXlaNumberToDouble,
    F_NM_XLA_ttXlaNumberToInt,
    F_NM_XLA_ttXlaNumberToSmallInt,
    F_NM_XLA_ttXlaNumberToTinyInt,
    F_NM_XLA_ttXlaNumberToUInt,
    F_NM_XLA_ttXlaOraDateToODBCTimeStamp,
    F_NM_XLA_ttXlaOraTimeStampToODBCTimeStamp,
    F_NM_XLA_ttXlaPersistOpen,
    F_NM_XLA_ttXlaRollback,
    F_NM_XLA_ttXlaSetLSN,
    F_NM_XLA_ttXlaSetVersion,
    F_NM_XLA_ttXlaTableByName,
    F_NM_XLA_ttXlaTableCheck,
    F_NM_XLA_ttXlaTableStatus,
    F_NM_XLA_ttXlaTableVersionVerify,
    F_NM_XLA_ttXlaTimeStampToODBCCType,
    F_NM_XLA_ttXlaTimeToODBCCType,
    F_NM_XLA_ttXlaVersionColumnInfo,
    F_NM_XLA_ttXlaVersionCompare,
    F_NM_XLA_ttXlaVersionTableInfo,
    F_NM_XLA_ttXlaEpilog2,
    F_NM_XLA_ttXlaInvalidateTbl2,
    F_NM_XLA_ttXlaGenerateSQL2,
    F_NM_XLA_ttXlaTranslate,
    F_NM_XLA_ttXlaSqlOption,
    F_NM_XLA_ttXlaRowidToCString
};
#endif /* ENABLE_XLA_DIRECT */

#if defined(ENABLE_UTIL_LIB)
/*
 * Array of Utility Function Names.
 *
 * This is never modified during execution.
 */
static const char
UTILFuncName[NUM_UTIL_FUNCTIONS][MAX_UTIL_FUNCNAME_LEN+1] =
{
    F_NM_UTIL_ttUtilAllocEnv,
    F_NM_UTIL_ttUtilFreeEnv,
    F_NM_UTIL_ttUtilGetErrorCount,
    F_NM_UTIL_ttUtilGetError,
    F_NM_UTIL_ttBackup,
    F_NM_UTIL_ttDestroyDataStore,
    F_NM_UTIL_ttDestroyDataStoreForce,
    F_NM_UTIL_ttRamGrace,
    F_NM_UTIL_ttRamLoad,
    F_NM_UTIL_ttRamPolicy,
    F_NM_UTIL_ttRamUnload,
    F_NM_UTIL_ttRepDuplicateEx,
    F_NM_UTIL_ttRestore,
    F_NM_UTIL_ttXactIdRollback
#if 0
   ,F_NM_UTIL_ttXactIdRollbackExecuting,
    F_NM_UTIL_ttXactIdCommit,
    F_NM_UTIL_ttDisconnect,
    F_NM_UTIL_ttRamNoPersist,
    F_NM_UTIL_ttRamPersist,
    F_NM_UTIL_ttReadEpochs,
    F_NM_UTIL_ttRepDuplicateCheck,
    F_NM_UTIL_ttRepGetCTNCksum,
    F_NM_UTIL_ttRepGetStateRole,
    F_NM_UTIL_ttRepGetSubState,
    F_NM_UTIL_ttRepRoleCheck,
    F_NM_UTIL_ttUtDbClose,
    F_NM_UTIL_ttUtDbOpen,
    F_NM_UTIL_ttUtilGetDaemonStatus
#endif /* unsupported functions */
};
#endif /* ENABLE_UTIL_LIB */

/*
 * Global Data for this DM instance.
 *
 * This *is* modified during execution.
 */
static DMGlobalData_t 
DMGlobalData = 
{ 
    NULL, 
    NULL,
    NULL,
    NULL
#if defined(ENABLE_UTIL_LIB)
    , NULL
    , NULL
    , NULL
    , NULL
#endif /* ENABLE_UTIL_LIB */
#if defined(ENABLE_THREAD_SAFETY)
    , &globalMutex
#if defined(ENABLE_UTIL_LIB)
    , &globalUtilMutex
#endif /* ENABLE_UTIL_LIB */
#endif /* ENABLE_THREAD_SAFETY */
};

/*
 ========================================================================
                 Special DEBUG functions
                 Intended to aid debugging the Driver Manager
 ========================================================================
 */

#if defined(ENABLE_DEBUG_FUNCTIONS)

static void
debugPrintODBCErrorStack(
                    char       * msg,
                    void       * pSQLGetDiagRec,
                    SQLHENV      henv,
                    SQLHDBC      hdbc,
                    SQLHDBC      hstmt
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    SQLCHAR     SQLSTATE[TT_SQLSTATE_LEN+1];
    SQLINTEGER  NATIVEERROR;
    SQLCHAR     ERRMSG[SQL_MAX_MESSAGE_LENGTH+1];
    SQLSMALLINT hType;
    SQLHANDLE   handle;
    SQLSMALLINT recNo = 1;

    TRACE_FENTER("debugPrintODBCErrorStack");
    TRACE_HVAL("henv",henv);
    TRACE_HVAL("hdbc",hdbc);
    TRACE_HVAL("hstmt",hstmt);

    if (  hstmt != SQL_NULL_HSTMT  )
    {
        hType = SQL_HANDLE_STMT;
        handle = (SQLHANDLE)hstmt;
    }
    else
    if (  hdbc != SQL_NULL_HDBC  )
    {
        hType = SQL_HANDLE_DBC;
        handle = (SQLHANDLE)hdbc;
    }
    else
    {
        hType = SQL_HANDLE_ENV;
        handle = (SQLHANDLE)henv;
    }

    if (  msg != NULL  )
        fprintf(stderr,"TTDM DEBUG: %s\n", msg);

    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT,
                           SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                           SQLSMALLINT, SQLSMALLINT *))
                          (pSQLGetDiagRec)))
                           (hType, handle, recNo++,
                            SQLSTATE, &NATIVEERROR, ERRMSG,
                            SQL_MAX_MESSAGE_LENGTH,NULL);
    while (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
    {
        fprintf(stderr,"TTDM DEBUG: SQLSTATE = %s, NativeError = %d, Message = %s\n",
                SQLSTATE, NATIVEERROR, ERRMSG);
    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT,
                           SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                           SQLSMALLINT, SQLSMALLINT *))
                          (pSQLGetDiagRec)))
                           (hType, handle, recNo++,
                            SQLSTATE, &NATIVEERROR, ERRMSG,
                            SQL_MAX_MESSAGE_LENGTH,NULL);
    }
    fprintf(stderr,"TTDM DEBUG: SQLGetDiagRec returned %d\n", rc);

    TRACE_FLEAVE("debugPrintODBCErrorStack");
} // debugPrintODBCErrorStack

static void
debugPrintErrorStack(
                    char             * msg,
                    ODBC_ERR_STACK_t * stkptr
)
{
    ODBC_ERR_t * err;
    int          msgno = 1;

    TRACE_FENTER("debugPrintErrorStack");

    if (  msg != NULL  )
        fprintf(stderr,"TTDM DEBUG: %s\n", msg);
    if ( stkptr == NULL  )
        fprintf(stderr,"TTDM DEBUG: stkptr is NULL\n");
    else
    {
        fprintf(stderr,"TTDM DEBUG: stkptr %16.16x, count = %d\n",
                       stkptr, stkptr->msgcnt);
        err = stkptr->stackH;
        while (  err != NULL  )
        {
            fprintf(stderr,"TTDM DEBUG: #%d: rc=%d, sqlState=%s, nativeError=%d, errorMsg='%s'\n",
                    msgno++, err->rc, err->sqlState, err->nativeError, err->errorMsg);
            err = err->next;
        }
    }

    TRACE_FLEAVE("debugPrintErrorStack");
} // debugPrintErrorStack

#else

#define debugPrintODBCErrorStack( msg, pSQLError, henv, hdbc, hstmt )

#define debugPrintErrorStack( msg, stkptr )

#endif /* ENABLE_DEBUG_FUNCTIONS */

/*
 ========================================================================
                 Private Functions
 ========================================================================
 */

/*
 * Some 'wide character' routines. These are necessary because ODBC
 * 'W' routines work in UTF-16, which is not well supported on *nix
 * OSes.
 */

static int
utf16len(SQLWCHAR * str)
{
    register int l = 0;
    register SQLWCHAR * s = str;

    while (  *s++  )
	l++;

    return l;
} // utf16len

static SQLWCHAR *
utf16cpy(SQLWCHAR * dst, SQLWCHAR * src)
{
    int l;
    register SQLWCHAR * s = src;
    register SQLWCHAR * d = dst;

    l = utf16len(src) + 1;
    memcpy((void *)dst, (void *)src, l*sizeof(SQLWCHAR));

    return dst;
} // utf16cpy

static SQLWCHAR *
utf16ncpy(SQLWCHAR * dst, SQLWCHAR * src, int n)
{
    register int lsrc;
    register int lcopy;

    lsrc = utf16len(src);
    if (  lsrc <= n  )
	lcopy = lsrc;
    else
	lcopy = n;
    memcpy((void *)dst, (void *)src, lcopy*sizeof(SQLWCHAR));

    if (  n > lsrc  )
	*(dst+lcopy) = 0;

    return dst;
} // utf16ncpy

static SQLWCHAR *
utf16cat(SQLWCHAR * dst, SQLWCHAR * src)
{
    SQLWCHAR * dend = dst;

    while (  *dend  )
        /* find terminating 0 marker */ 
	dend++;
    utf16cpy(dend, src); /* append src */

    return dst;
} // utf16cat

/*
 * Unicode <-> ANSI translation routines.
 *
 * At the moment these make the simplistic (and likely incorrect) assumption
 * that we are always dealing with plain ASCII or at worst ISO-8859-1 
 * (Linux/Unix) character sets. As these routines are only used for error 
 * and warning message translation this may not be too big a deal. Nonetheless
 * it is highly desirable that these routines be replaced at some point
 * with ones that perform a better transation based on the actual encoding
 * being used by the system.
 */

static char *
translateUTF16toANSI(char * sANSI, SQLWCHAR * sUTF16)
{
    register char * sA = sANSI;
    register SQLWCHAR * sU = sUTF16;

    while (  *sU  )
	*sA++ = (char)(*sU++ & 0xff);
    *sA = '\0';

    return sANSI;
} // translateUTF16toANSI

static SQLWCHAR *
translateANSItoUTF16(SQLWCHAR * sUTF16, char * sANSI)
{
    register char * sA = sANSI;
    register SQLWCHAR * sU = sUTF16;

    while (  *sA  )
	*sU++ = (SQLWCHAR)*sA++;
    *sU = (SQLWCHAR)0;

    return sUTF16;
} // translateANSItoUTF16

static SQLWCHAR *
translateANSItoUTF16N(SQLWCHAR * sUTF16, char * sANSI, int n)
{
    register char * sA = sANSI;
    register SQLWCHAR * sU = sUTF16;

    if ( n > 0 )
    {
        while (  *sA && n--  )
            *sU++ = (SQLWCHAR)*sA++;
    }

    return sUTF16;
} // translateANSItoUTF16

/*
 * Function to classify a specific Diagnostic Field code (as passed to
 * SQLGetDiagField(W)) to determine (a) if it is a header field and (b)
 * if it needs special processing by TTDM.
 */
static void
classifyDiagField(SQLSMALLINT diagFieldCode, 
		 int * isHeader, 
		 int * specialHandling,
		 int * stmtOnly,
		 int * isString)
{
    switch (  diagFieldCode  )
    {
	case SQL_DIAG_NUMBER:
	case SQL_DIAG_RETURNCODE:
	    *isHeader = 1;
	    *specialHandling = 1;
	    *stmtOnly = 0;
	    *isString = 0;
	    break;
	case SQL_DIAG_CURSOR_ROW_COUNT:
	case SQL_DIAG_DYNAMIC_FUNCTION:
	case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
	case SQL_DIAG_ROW_COUNT:
	    *isHeader = 1;
	    *specialHandling = 0;
	    *stmtOnly = 1;
	    *isString = 0;
	    break;
	case SQL_DIAG_CLASS_ORIGIN:
	case SQL_DIAG_SUBCLASS_ORIGIN:
	case SQL_DIAG_CONNECTION_NAME:
	case SQL_DIAG_SERVER_NAME:
	case SQL_DIAG_SQLSTATE:
	case SQL_DIAG_MESSAGE_TEXT:
	    *isHeader = 0;
	    *specialHandling = 0;
	    *stmtOnly = 0;
	    *isString = 1;
	    break;
	case SQL_DIAG_COLUMN_NUMBER:
	case SQL_DIAG_ROW_NUMBER:
	    *isHeader = 0;
	    *specialHandling = 0;
	    *stmtOnly = 1;
	    *isString = 0;
	    break;
	default: /* SQL_DIAG_NATIVE */
            *isHeader = 0;
	    *specialHandling = 0;
	    *stmtOnly = 0;
	    *isString = 0;
	    break;
    }
} // classifyDiagField

/*
 * Copy a character buffer that is either ANSI or UTF-16 encoded
 * to another buffer. The output string is always ANSI encoded.
 */
static SQLRETURN
diagCopy(int encoding,
	 SQLCHAR * srcBuffer, 
	 SQLCHAR * dstBuffer, 
	 SQLSMALLINT byteBufferLen,
	 SQLSMALLINT * stringLen
	)
{
    SQLRETURN rc = SQL_SUCCESS;
    int txtLen;
    int bufferAllocated = 0;
    SQLCHAR *  tmpBuf = NULL;

    if (  (srcBuffer != NULL) && (dstBuffer != NULL)  )
    {
        if (  encoding != ENCODING_ANSI  )
        {   /* need to convert to ANSI first */
	    tmpBuf = (SQLCHAR *)calloc(1,utf16len((SQLWCHAR *)srcBuffer)+1);
	    if (  tmpBuf != NULL  )
	    {
		bufferAllocated = 1;
                translateUTF16toANSI((char *)tmpBuf, (SQLWCHAR *)srcBuffer);
	    }
        }
	else
	    tmpBuf = srcBuffer;
        if (  tmpBuf == NULL  )
	    rc = SQL_ERROR;
        else
        {
            txtLen = strlen((const char *)tmpBuf);
	    if (  stringLen != NULL  )
                *stringLen = (SQLSMALLINT)txtLen;
	    if (  dstBuffer != NULL  )
	    {
                if (  txtLen < (int)byteBufferLen  )
                    strcpy((char *)dstBuffer, (const char *)tmpBuf);
                else
                {
                    rc = SQL_SUCCESS_WITH_INFO;
                    if (  byteBufferLen > 0  )
                    {
                        strncpy((char *)dstBuffer, (const char *)tmpBuf, (byteBufferLen-1));
                        dstBuffer[byteBufferLen-1] = '\0';
                    }
                    else
                        dstBuffer[0] = '\0';
                }
	    }
        }
	if (  bufferAllocated && (tmpBuf != NULL)  )
	    free((void *)tmpBuf);
    }

    return rc;
} // diagCopy

/*
 * Copy a wide character buffer that is either ANSI or UTF-16
 * encoded to another wide character buffer. The output string
 * is always UTF-16 encoded.
 */
static SQLRETURN
diagCopyW(int encoding,
	 SQLWCHAR * srcBuffer, 
	 SQLWCHAR * dstBuffer, 
	 SQLSMALLINT byteBufferLen,
	 SQLSMALLINT * stringLen
	)
{
    SQLRETURN rc = SQL_SUCCESS;
    int txtLen;
    int bufferAllocated = 0;
    SQLWCHAR *  tmpBuf = NULL;

    if (  (srcBuffer != NULL) && (dstBuffer != NULL)  )
    {
        if (  encoding != ENCODING_UTF16  )
        {   /* need to convert to UTF16 first */
	    tmpBuf = (SQLWCHAR *)calloc(1,2*(strlen((const char *)srcBuffer)+1));
	    if (  tmpBuf != NULL  )
	    {
		bufferAllocated = 1;
                translateANSItoUTF16(tmpBuf, (char *)srcBuffer);
	    }
        }
	else
	    tmpBuf = (SQLWCHAR *)srcBuffer;
        if (  tmpBuf == NULL  )
	    rc = SQL_ERROR;
        else
        {
            txtLen = utf16len(tmpBuf);
	    if (  stringLen != NULL  )
                *stringLen = (SQLSMALLINT)(txtLen*2);
	    if (  dstBuffer != NULL  )
	    {
                if (  txtLen < (int)(byteBufferLen/2)  )
                    utf16cpy(dstBuffer, tmpBuf);
                else
                {
                    rc = SQL_SUCCESS_WITH_INFO;
                    if (  byteBufferLen > 0  )
                    {
			byteBufferLen = (byteBufferLen / 2);
                        utf16ncpy(dstBuffer, tmpBuf, byteBufferLen-1);
                        dstBuffer[byteBufferLen-1] = 0;
                    }
                    else
                        dstBuffer[0] = 0;
                }
	    }
        }
	if (  bufferAllocated && (tmpBuf != NULL)  )
	    free((void *)tmpBuf);
    }

    return rc;
} // diagCopyW

#if defined(ENABLE_THREAD_SAFETY)

/*
 * Initialises an existing mutex
 */

static void 
ttMutexInit(tt_mutex_t mutex)
{
    pthread_mutex_init(mutex,NULL);
} // ttMutexInit

/*
 * Creates an initialised mutex and returns it
 */

static tt_mutex_t 
ttMutexCreate(void)
{
    tt_mutex_t  mutex;

    mutex = (tt_mutex_t)calloc(1, sizeof(pthread_mutex_t));
    if (  mutex != NULL  )
        ttMutexInit(mutex);
    return mutex;
} // ttMutexCreate

/*
 * Destroys a mutex
 */

static int 
ttMutexDestroy(tt_mutex_t mutex)
{
    int retval = 0;

    retval = pthread_mutex_destroy(mutex);
    if (  retval == 0  )
	free(mutex);
    return retval;
} // ttMutexDestroy

/*
 * Locks a mutex
 */
static int 
ttMutexLock(tt_mutex_t mutex)
{
    int retval = 0;

    retval = pthread_mutex_lock(mutex);
    return retval;
} // ttMutexLock

/*
 * Unlocks a mutex
 */
static int 
ttMutexUnlock(tt_mutex_t mutex)
{
    int retval = 0;

    retval = pthread_mutex_unlock(mutex);
    return retval;
} // ttMutexUnlock

#endif /* ENABLE_THREAD_SAFETY */

#if defined(ENABLE_UTIL_LIB)

/*
 * Post ttUtilAllocEnv and ttUtilFreeEnv errors
 */

static void
postUtilEnvError(
                       char*          errMsg,
                       char*          errBuff,
                       unsigned int   buffLen,
                       unsigned int*  errLen
)
{
    int msgLen;

    TRACE_FENTER("postUtilEnvError");

    msgLen = strlen(errMsg);
    if (  errLen != NULL  )
        *errLen = msgLen;
    if (  (buffLen > 0) && (errBuff != NULL)  )
    {
        if (  msgLen < buffLen  )
            strcpy(errBuff,errMsg);
        else
        {
            strncpy(errBuff,errMsg,(buffLen-1));
            errBuff[buffLen-1] = '\0';
        }
    }

    TRACE_FLEAVE("postUtilEnvError");
} // postUtilEnvError

#endif /* ENABLE_UTIL_LIB */

/*
 * maxODBCError
 *
 * Takes two ODBC return codes and returns the 'most severe' according
 * to this order of prcedence (from least to most severe):
 *     SQL_SUCCESS
 *     SQL_NO_DATA_FOUND
 *     SQL_SUCCESS_WITH_INFO
 *     SQL_ERROR
 *     SQL_INVALID_HANDLE
 */
static SQLRETURN
maxODBCError( 
              SQLRETURN rc1,
              SQLRETURN rc2
)
{
    SQLRETURN   finalrc = SQL_SUCCESS;

    TRACE_FENTER("maxODBCError");
    TRACE_IVAL("rc1", rc1);
    TRACE_IVAL("rc2", rc2);

    if (  (rc1 == SQL_INVALID_HANDLE) ||
          (rc2 == SQL_INVALID_HANDLE)  )
        finalrc = SQL_INVALID_HANDLE;
    else
    if (  (rc1 == SQL_ERROR) ||
          (rc2 == SQL_ERROR)  )
        finalrc = SQL_ERROR;
    else
    if (  (rc1 == SQL_SUCCESS_WITH_INFO) ||
          (rc2 == SQL_SUCCESS_WITH_INFO)  )
        finalrc = SQL_SUCCESS_WITH_INFO;
    else
    if (  (rc1 == SQL_NO_DATA_FOUND) ||
          (rc2 == SQL_NO_DATA_FOUND)  )
        finalrc = SQL_NO_DATA_FOUND;

    TRACE_FLEAVE_RC("maxODBCError",finalrc);

    return finalrc;
} // maxODBCError

/*
 * Convenience function for retrieving the next error from
 * an ODBC connection's error stack.
 */
static SQLRETURN
getODBCConnectError(
                    void       * pSQLGetDiagRec,
                    SQLHDBC      hdbc,
                    SQLSMALLINT  RECNO,
                    SQLCHAR    * SQLSTATE,    /* might be SQLWCHAR * */
                    SQLINTEGER * NATIVEERROR,
                    SQLCHAR    * ERRMSG       /* might be SQLWCHAR * */
)
{
    SQLRETURN  rc = SQL_SUCCESS;

    TRACE_FENTER("getODBCConnectError");
    TRACE_HVAL("hdbc",hdbc);

    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                           SQLSMALLINT, SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                           SQLSMALLINT, SQLSMALLINT *))
                          (pSQLGetDiagRec)))
                           (SQL_HANDLE_DBC, hdbc, RECNO,
                            SQLSTATE, NATIVEERROR, ERRMSG,
                            SQL_MAX_MESSAGE_LENGTH,NULL);

    TRACE_FLEAVE_RC("getODBCConnectError",rc);

    return rc;
} // getODBCConnectError

/*
 * Clears the ODBC error stack (by retrieving all
 * the errors). Only supported for Environment
 * and connection handles.
 */
static SQLRETURN
clearODBCErrorStack(
                    void   * pSQLError,
                    SQLHENV  henv,
                    SQLHDBC  hdbc
)
{
    SQLRETURN   rc;
    SQLSMALLINT recNo;
    SQLCHAR     sqlState[2*(TT_SQLSTATE_LEN+1)];
    SQLINTEGER  nativeError= 0;
    SQLCHAR     errorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];

    TRACE_FENTER("clearODBCErrorStack");
    TRACE_HVAL("henv",henv);
    TRACE_HVAL("hdbc",hdbc);
    {
        rc = SQL_SUCCESS; recNo = 1;
        while (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                           SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                           SQLSMALLINT, SQLSMALLINT *))
                          (pSQLError)))
                           (SQL_NULL_HENV,hdbc,SQL_NULL_HSTMT,
                            sqlState, &nativeError, errorMsg,
                            SQL_MAX_MESSAGE_LENGTH,NULL);
        }
    }

    if (  henv != SQL_NULL_HENV  )
    {
        rc = SQL_SUCCESS; recNo = 1;
        while (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                           SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                           SQLSMALLINT, SQLSMALLINT *))
                          (pSQLError)))
                           (henv,SQL_NULL_HDBC,SQL_NULL_HSTMT,
                            sqlState, &nativeError, errorMsg,
                            SQL_MAX_MESSAGE_LENGTH,NULL);
        }
    }

    rc = SQL_SUCCESS;

    TRACE_FLEAVE_RC("clearODBCErrorStack",rc);

    return rc;
} // clearODBCErrorStack

/*
 * Clears an error stack.
 */
static void
clearErrorStackInternal(
                ODBC_ERR_STACK_t * stkptr
)
{
    ODBC_ERR_t  * p;
    ODBC_ERR_t  * np;

    TRACE_FENTER("clearErrorStackInternal");
    TRACE_HVAL("stkptr",stkptr);

    if (  stkptr != NULL  )
    {
        if (  (*stkptr).stackH != NULL  )
        {
            p = (*stkptr).stackH;
            while (  p != NULL  )
            {
                np = p;
                p = p->next;
                free( (void *)np );
            }
            (*stkptr).stackH = NULL;
            (*stkptr).msgcnt = 0;
        }
    }

    TRACE_FLEAVE("clearErrorStackInternal");
} // clearErrorStackInternal

/*
 * Clears an object's private error stack.
 */
static SQLRETURN
clearErrorStackPrivate(
                TTHANDLE handle
#if defined(ENABLE_THREAD_SAFETY)
              , int lock
#endif /* ENABLE_THREAD_SAFETY */
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHENV   TThEnv = NULL;
    TTSQLHDBC   TThDbc = NULL;
    TTSQLHSTMT  TThStmt = NULL;
    TTSQLHDESC  TThDesc = NULL;
    ODBC_ERR_t  * p;
    ODBC_ERR_t  * np;

    TRACE_FENTER("clearErrorStackPrivate");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
    {
        rc = SQL_INVALID_HANDLE;
        TRACE_FLEAVE_RC("clearErrorStackPrivate",rc);
        return rc;
    }

    TThEnv = (TTSQLHENV)handle;
    if (  TThEnv->tag != TTDM_STRUCT_TAG  )
    {
        rc = SQL_INVALID_HANDLE;
        TRACE_FLEAVE_RC("clearErrorStackPrivate",rc);
        return rc;
    }

    switch (  TThEnv->hid  )
    {
        case TTDM_HID_ENV:
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_LOCK(TThEnv->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            clearErrorStackInternal( &(TThEnv->errorStack) );
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_UNLOCK(TThEnv->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            break;

        case TTDM_HID_DBC:
            TThEnv = NULL;
            TThDbc = (TTSQLHDBC)handle;
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_LOCK(TThDbc->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            clearErrorStackInternal( &(TThDbc->errorStack) );
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_UNLOCK(TThDbc->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            break;

        case TTDM_HID_STMT:
            TThEnv = NULL;
            TThStmt = (TTSQLHSTMT)handle;
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_LOCK(TThStmt->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            clearErrorStackInternal( &(TThStmt->errorStack) );
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_UNLOCK(TThStmt->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            break;

        case TTDM_HID_DESC:
            TThEnv = NULL;
            TThDesc = (TTSQLHDESC)handle;
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_LOCK(TThDesc->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            clearErrorStackInternal( &(TThDesc->errorStack) );
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_UNLOCK(TThDesc->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            break;

        default:
            rc = SQL_INVALID_HANDLE;
            break;
    }

    TRACE_FLEAVE_RC("clearErrorStackPrivate", rc);

    return rc;
} // clearErrorStackPrivate

/*
 * Clears all error stacks for an object (internal and ODBC).
 * Only supported for environment and connection handles.
 */
static SQLRETURN
clearErrorStackAll(
                TTHANDLE handle
#if defined(ENABLE_THREAD_SAFETY)
              , int lock
#endif /* ENABLE_THREAD_SAFETY */
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHENV   TThEnv = NULL;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("clearErrorStackAll");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
    {
        rc = SQL_INVALID_HANDLE;
        TRACE_FLEAVE_RC("clearErrorStack",rc);
        return rc;
    }

    TThEnv = (TTSQLHENV)handle;
    if (  TThEnv->tag != TTDM_STRUCT_TAG  )
    {
        rc = SQL_INVALID_HANDLE;
        TRACE_FLEAVE_RC("clearErrorStack",rc);
        return rc;
    }

    switch (  TThEnv->hid  )
    {
        case TTDM_HID_ENV:
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_LOCK(TThEnv->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            clearErrorStackInternal( &(TThEnv->errorStack) );
            if (  (TThEnv->pDirectMap != NULL) && (TThEnv->directEnv != SQL_NULL_HENV)  )
                clearODBCErrorStack(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLError],
                                    TThEnv->directEnv,SQL_NULL_HDBC);
            if (  (TThEnv->pClientMap != NULL) && (TThEnv->clientEnv != SQL_NULL_HENV)  )
                clearODBCErrorStack(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLError],
                                    TThEnv->clientEnv,SQL_NULL_HDBC);
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_UNLOCK(TThEnv->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            break;

        case TTDM_HID_DBC:
            TThEnv = NULL;
            TThDbc = (TTSQLHDBC)handle;
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_LOCK(TThDbc->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            clearErrorStackInternal( &(TThDbc->errorStack) );
            if (  (TThDbc->pDirectMap != NULL) && (TThDbc->directDbc != SQL_NULL_HDBC)  )
                clearODBCErrorStack(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLError],
                                    SQL_NULL_HENV,TThDbc->directDbc);
            if (  (TThDbc->pClientMap != NULL) && (TThDbc->clientDbc != SQL_NULL_HDBC)  )
                clearODBCErrorStack(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLError],
                                    SQL_NULL_HENV,TThDbc->clientDbc);
#if defined(ENABLE_THREAD_SAFETY)
            if (  lock  )
                MUTEX_UNLOCK(TThDbc->mutex);
#endif /* ENABLE_THREAD_SAFETY */
            break;

        default:
            rc = SQL_INVALID_HANDLE;
            break;
    }

    TRACE_FLEAVE_RC("clearErrorStackAll",rc);

    return rc;
} // clearErrorStackAll

/*
 * Push an ODBC error onto an error stack. The text components of
 * the error may be UTF-8 or UTF-16 encoded (indicated by the
 * 'encoding' field).
 */
static SQLRETURN
pushError(
          ODBC_ERR_STACK_t * stkptr,
          SQLRETURN      RC,
	  int            encoding,
          SQLCHAR      * SQLSTATE,   /* may actually be SQLWCHAR * */
          SQLINTEGER     NATIVEERROR,
          SQLCHAR      * ERRMSG,     /* may actually be SQLWCHAR * */
	  SQLCHAR      * DiagClassOrigin,    /* may be SQLWCHAR * */
	  SQLCHAR      * DiagSubclassOrigin, /* may be SQLWCHAR * */
	  SQLCHAR      * ConnectionName,     /* may be SQLWCHAR * */
	  SQLCHAR      * ServerName          /* may be SQLWCHAR * */
)
{
    SQLRETURN    rc = SQL_SUCCESS;
    ODBC_ERR_t * err = NULL;

    TRACE_FENTER("pushError");
    TRACE_HVAL("stkptr",stkptr);

    if (  stkptr == NULL  )
        rc = SQL_ERROR;
    else
    {
        err = (ODBC_ERR_t *)calloc(1,sizeof(ODBC_ERR_t));
        if (  err == NULL  )
            rc = SQL_ERROR;
        else
        {
            err->rc = RC;
            err->nativeError = NATIVEERROR;
            err->encoding = encoding;
	    if (  encoding == ENCODING_ANSI  )
	    {
                strcpy((char *)err->sqlState,(char *)SQLSTATE);
                strcpy((char *)err->errorMsg,(char *)ERRMSG);
	    }
	    else
	    {
                utf16cpy((SQLWCHAR *)err->sqlState,(SQLWCHAR *)SQLSTATE);
                utf16cpy((SQLWCHAR *)err->errorMsg,(SQLWCHAR *)ERRMSG);
	    }
            err->diagClassOrigin = DiagClassOrigin;
            err->diagSubclassOrigin = DiagSubclassOrigin;
            err->diagConnectionName = ConnectionName;
            err->diagServerName = ServerName;
            err->next = (*stkptr).stackH;
            if (  (*stkptr).stackH == NULL  )
                (*stkptr).stackT = err;
            (*stkptr).stackH = err;
            (*stkptr).msgcnt += 1;
        }
    }

    TRACE_FLEAVE_RC("pushError",rc);

    return rc;
} // pushError

/*
 * Append an ODBC error to an error stack. The text components of
 * the error may be UTF-8 or UTF-16 encoded (indicated by the
 * 'encoding' field).
 *
 * Used to ensure correct ordering of errors on the stack
 * in some circumstances.
 */
static SQLRETURN
appendError(
          ODBC_ERR_STACK_t * stkptr,
          SQLRETURN      RC,
          int            encoding,
          SQLCHAR      * SQLSTATE,   /* may actually be SQLWCHAR * */
          SQLINTEGER     NATIVEERROR,
          SQLCHAR      * ERRMSG,     /* may actually be SQLWCHAR * */
          SQLCHAR      * DiagClassOrigin,    /* may be SQLWCHAR * */
          SQLCHAR      * DiagSubclassOrigin, /* may be SQLWCHAR * */
          SQLCHAR      * ConnectionName,     /* may be SQLWCHAR * */
          SQLCHAR      * ServerName          /* may be SQLWCHAR * */
)
{
    SQLRETURN    rc = SQL_SUCCESS;
    ODBC_ERR_t * err = NULL;

    TRACE_FENTER("appendError");
    TRACE_HVAL("stkptr",stkptr);

    if (  stkptr == NULL  )
        rc = SQL_ERROR;
    else
    {
        err = (ODBC_ERR_t *)calloc(1,sizeof(ODBC_ERR_t));
        if (  err == NULL  )
            rc = SQL_ERROR;
        else
        {
            err->rc = RC;
            err->nativeError = NATIVEERROR;
            err->encoding = encoding;
            if (  encoding == ENCODING_ANSI  )
            {
                strcpy((char *)err->sqlState,(char *)SQLSTATE);
                strcpy((char *)err->errorMsg,(char *)ERRMSG);
            }
            else
            {
                utf16cpy((SQLWCHAR *)err->sqlState,(SQLWCHAR *)SQLSTATE);
                utf16cpy((SQLWCHAR *)err->errorMsg,(SQLWCHAR *)ERRMSG);
            }
            err->diagClassOrigin = DiagClassOrigin;
            err->diagSubclassOrigin = DiagSubclassOrigin;
            err->diagConnectionName = ConnectionName;
            err->diagServerName = ServerName;

            err->next = NULL;
            if (  (*stkptr).stackT == NULL  )
            {
                (*stkptr).stackH = err;
                (*stkptr).stackT = err;
            }
            else
            {
                (*stkptr).stackT->next = err;
            }
            (*stkptr).msgcnt += 1;
        }
    }

    TRACE_FLEAVE_RC("appendError",rc);

    return rc;
} // appendError

/*
 * Pops an ODBC error from an error stack. The text components of
 * the error may be UTF-8 or UTF-16 encoded (indicated by the
 * 'encoding' field).
 */
static SQLRETURN
popError(
          ODBC_ERR_STACK_t * stkptr,
          SQLRETURN    * RC,
	  int          * encoding,
          SQLCHAR      * SQLSTATE,      /* may actually be SQLWCHAR * */
          SQLINTEGER   * NATIVEERROR,
          SQLCHAR      * ERRMSG,        /* may actually be SQLWCHAR * */
	  SQLCHAR    * * DiagClassOrigin,    /* may be SQLWCHAR * */
	  SQLCHAR    * * DiagSubclassOrigin, /* may be SQLWCHAR * */
	  SQLCHAR    * * ConnectionName,     /* may be SQLWCHAR * */
	  SQLCHAR    * * ServerName          /* may be SQLWCHAR * */
)
{
    SQLRETURN    rc = SQL_SUCCESS;
    ODBC_ERR_t * err = NULL;

    TRACE_FENTER("popError");
    TRACE_HVAL("stkptr",stkptr);

    if (  (stkptr == NULL) || ((*stkptr).stackH == NULL)  )
        rc = SQL_NO_DATA_FOUND;
    else
    {
        err = (*stkptr).stackH;
        (*stkptr).stackH = err->next;
        if (  (*stkptr).stackH == NULL  )
            (*stkptr).stackT = NULL;
        (*stkptr).msgcnt -= 1;
        *RC = err->rc;
        *NATIVEERROR = err->nativeError;
	*encoding = err->encoding;
	if (  err->encoding == ENCODING_ANSI  )
	{
            strcpy((char *)SQLSTATE,(char *)err->sqlState);
            strcpy((char *)ERRMSG,(char *)err->errorMsg);
	}
	else
	{
            utf16cpy((SQLWCHAR *)SQLSTATE,(SQLWCHAR *)err->sqlState);
            utf16cpy((SQLWCHAR *)ERRMSG,(SQLWCHAR *)err->errorMsg);
	}
	if (  DiagClassOrigin != NULL  )
            *DiagClassOrigin = err->diagClassOrigin;
	if (  DiagSubclassOrigin != NULL  )
            *DiagSubclassOrigin = err->diagSubclassOrigin;
	if (  ConnectionName != NULL  )
            *ConnectionName = err->diagConnectionName;
	if (  ServerName != NULL  )
            *ServerName = err->diagServerName;
        free( (void *)err );
    }

    TRACE_FLEAVE_RC("popError",rc);

    return rc;
} // popError

/*
 * Copies one error stack to another.
 */
static SQLRETURN
copyErrorStack(
               ODBC_ERR_STACK_t * instkptr,
               ODBC_ERR_STACK_t * otstkptr,
               int                clear
)
{
    SQLRETURN rc = SQL_SUCCESS;
    ODBC_ERR_t * err = NULL;

    TRACE_FENTER("copyErrorStack");
    TRACE_HVAL("instkptr",instkptr);
    TRACE_HVAL("otstkptr",otstkptr);

    if (  clear  )
        clearErrorStackInternal( otstkptr );
    if (  (instkptr != NULL) && (otstkptr != NULL)  )
    {
        err = (*instkptr).stackH;
        while (  (rc == SQL_SUCCESS) && (err != NULL)  )
        {
            rc = appendError( otstkptr,
                              err->rc, 
                              err->encoding,
                              err->sqlState, 
                              err->nativeError,
                              err->errorMsg,
                              err->diagClassOrigin,
                              err->diagSubclassOrigin,
                              err->diagConnectionName,
                              err->diagServerName);
            err = err->next;
        }
    }

    TRACE_FLEAVE_RC("copyErrorStack",rc);

    return rc;
} // copyErrorStack

/*
 * Retrieves a specific record number (starts from 1) off an error stack.
 * As usual, text components may be UTF-8 or UTF-16 encoded.
 */
static SQLRETURN
getError(
          ODBC_ERR_STACK_t * stkptr,
	  int            msgno,
          SQLRETURN    * RC,
	  int          * encoding,
          SQLCHAR      * SQLSTATE,      /* may actually be SQLWCHAR * */
          SQLINTEGER   * NATIVEERROR,
          SQLCHAR      * ERRMSG,        /* may actually be SQLWCHAR * */
	  SQLCHAR    * * DiagClassOrigin,    /* may be SQLWCHAR * */
	  SQLCHAR    * * DiagSubclassOrigin, /* may be SQLWCHAR * */
	  SQLCHAR    * * ConnectionName,     /* may be SQLWCHAR * */
	  SQLCHAR    * * ServerName          /* may be SQLWCHAR * */
)
{
    SQLRETURN    rc = SQL_SUCCESS;
    ODBC_ERR_t * err = NULL;
    int          mno = 1;

    TRACE_FENTER("getError");
    TRACE_HVAL("stkptr",stkptr);

    if (  msgno <= 0  )
	rc = SQL_ERROR;
    else
    if (  stkptr == NULL  )
        rc = SQL_NO_DATA;
    else
    if (  ((*stkptr).stackH == NULL) || ((*stkptr).msgcnt < msgno)  )
        rc = SQL_NO_DATA;
    else
    {
        // position to the correct error
        err = (*stkptr).stackH;
        while (  mno++ < msgno  )
            err = err->next;

        // err = (*stkptr).stackH + (msgno-1);

        *RC = err->rc;
	*encoding = err->encoding;
        *NATIVEERROR = err->nativeError;
	if (  err->encoding == ENCODING_ANSI  )
	{
            strcpy((char *)SQLSTATE,(char *)err->sqlState);
            strcpy((char *)ERRMSG,(char *)err->errorMsg);
	}
	else
	{
            utf16cpy((SQLWCHAR *)SQLSTATE,(SQLWCHAR *)err->sqlState);
            utf16cpy((SQLWCHAR *)ERRMSG,(SQLWCHAR *)err->errorMsg);
	}
	if (  DiagClassOrigin != NULL  )
            *DiagClassOrigin = err->diagClassOrigin;
	if (  DiagSubclassOrigin != NULL  )
            *DiagSubclassOrigin = err->diagSubclassOrigin;
	if (  ConnectionName != NULL  )
            *ConnectionName = err->diagConnectionName;
	if (  ServerName != NULL  )
            *ServerName = err->diagServerName;
    }

    TRACE_FLEAVE_RC("getError",rc);

    return rc;
} // getError

/*
 * Copies errors from the underlying direct or client
 * connection error stack to the TTDM connection error stack.
 */
static void
populateConnErrorStack(TTSQLHDBC TThDbc, int isclient, int encoding)
{
    SQLRETURN   rc = SQL_SUCCESS;
    SQLSMALLINT recNo = 1;
    void      * fPtr = NULL;
    SQLHDBC     hDbc = SQL_NULL_HDBC;
    SQLINTEGER  nativeError= 0;
    SQLCHAR     sqlState[2*(TT_SQLSTATE_LEN+1)];
    SQLCHAR     errorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];

    TRACE_FENTER("populateConnErrorStack");
    TRACE_HVAL("TThDbc",TThDbc);

    if (  isclient  )
    {
	fPtr = TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
	hDbc = TThDbc->clientDbc;
    }
    else
    {
	fPtr = TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
	hDbc = TThDbc->directDbc;
    }

    rc = getODBCConnectError( fPtr, hDbc, recNo++, sqlState, &nativeError, errorMsg );
    while (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
    {
        appendError(&(TThDbc->errorStack), rc,
	            encoding,
	            sqlState, nativeError, errorMsg,
	            (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
	            (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
	            TThDbc->connName,
	            TThDbc->serverName
                );
        rc = getODBCConnectError( fPtr, hDbc, recNo++, sqlState, &nativeError, errorMsg );
    }
    TRACE_FLEAVE_RC("populateConnErrorStack",rc);
} // populateConnErrorStack

#if defined(ENABLE_SQLENDTRAN_HENV_WORKAROUND)
static int
isBogusError(
                    SQLRETURN    originalRC,
                    void       * pSQLError,
                    SQLHENV      henv
)
{
    int        retval = 0;
    TTSQLHENV  TThEnv = NULL;
    SQLRETURN  rc = SQL_SUCCESS;
    SQLCHAR    SQLSTATE[TT_SQLSTATE_LEN+1];
    SQLINTEGER NATIVEERROR;
    SQLCHAR    ERRMSG[SQL_MAX_MESSAGE_LENGTH+1];

    TRACE_FENTER("isBogusError");

    rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                           SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                           SQLSMALLINT, SQLSMALLINT *))
                          (pSQLError)))
                           (henv,SQL_NULL_HDBC, SQL_NULL_HSTMT,
                            SQLSTATE, &NATIVEERROR, ERRMSG,
                            SQL_MAX_MESSAGE_LENGTH,NULL);

    if (  rc == SQL_NO_DATA_FOUND  )
        retval = 1;
    else
    {
        TThEnv = (TTSQLHENV)henv;
        pushError(&(TThEnv->errorStack),
                          originalRC,
			  ENCODING_ANSI,
                          SQLSTATE,
                          NATIVEERROR,
                          ERRMSG,
			  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			  NULL, NULL
                          );
    }

    TRACE_FLEAVE("isBogusError");

    return retval;
} // isBogusError
#endif /* ENABLE_SQLENDTRAN_HENV_WORKAROUND */

static int 
loadODBCfunctions(
                  tt_libptr_t    lh, 
                  funcmapper_t * libFuncs
)
{
    int i;
    int ret = 0;
    tt_funcptr_t fptr;

    TRACE_FENTER("loadODBCfunctions");
    TRACE_HVAL("lh",lh);
    TRACE_HVAL("libFuncs",libFuncs);

    for (i = 0; (ret==0) && (i < NUM_ODBC_FUNCTIONS); i++)
    {
        fptr = dlsym(lh, ODBCFuncName[i]);
        if (  fptr == NULL  )
	{
            ret = 1;
        }
        else
	{
            libFuncs->odbcFuncPtr[i] = fptr;
        }
    }
    libFuncs->odbcFuncPtr[i] = NULL;

    TRACE_FLEAVE_RC("loadODBCfunctions", ret);

    return ret;
}

#if defined(ENABLE_XLA_DIRECT)
static int 
loadXLAfunctions(
                 tt_libptr_t    lh, 
                 funcmapper_t * libFuncs
)
{
    int i;
    int ret = 0;
    tt_funcptr_t fptr;

    TRACE_FENTER("loadXLAfunctions");
    TRACE_HVAL("lh", lh);
    TRACE_HVAL("libFuncs",libFuncs);

    for (i = 0; (ret==0) && (i < NUM_XLA_FUNCTIONS); i++)
    {
        fptr = dlsym(lh, XLAFuncName[i]);
        if (  fptr == NULL  )
            ret = 1;
        else
            libFuncs->xlaFuncPtr[i] = fptr;
    }
    libFuncs->xlaFuncPtr[i] = NULL;

    TRACE_FLEAVE_RC("loadXLAfunctions", ret);

    return ret;
} // loadXLAfunctions
#endif /* ENABLE_XLA_DIRECT */

#if defined(ENABLE_UTIL_LIB)
static int 
loadUTILfunctions(
                 tt_libptr_t    lh, 
                 funcmapper_t * libFuncs
)
{
    int i;
    int ret = 0;
    tt_funcptr_t fptr;

    TRACE_FENTER("loadUTILfunctions");
    TRACE_HVAL("lh", lh);
    TRACE_HVAL("libFuncs",libFuncs);

    for (i = 0; (ret==0) && (i < NUM_UTIL_FUNCTIONS); i++)
    {
        fptr = dlsym(lh, UTILFuncName[i]);
        if (  fptr == NULL  )
            ret = 1;
        else
            libFuncs->utilFuncPtr[i] = fptr;
    }
    libFuncs->utilFuncPtr[i] = NULL;

    TRACE_FLEAVE_RC("loadUTILfunctions", ret);

    return ret;
} // loadUTILfunctions
#endif /* ENABLE_UTIL_LIB */

static int
loadLibrary(
            char         * libName, 
            int            libType, 
            funcmapper_t * libFuncs
)
{
    tt_libptr_t lh;
    int     ret = 0;

    TRACE_FENTER("loadLibrary");

    if (  libName == NULL || libFuncs == NULL  )
        return -1;
     libFuncs->libHandle = NULL;
     libFuncs->odbcFuncPtr = NULL;
#if defined(ENABLE_XLA_DIRECT)
     libFuncs->xlaFuncPtr = NULL;
#endif /* ENABLE_XLA_DIRECT */
#if defined(ENABLE_UTIL_LIB)
     libFuncs->utilFuncPtr = NULL;
#endif /* ENABLE_UTIL_LIB */
    if (  !*libName  )
        ret = -1;
    else
    {
        lh = dlopen(libName, RTLD_NOW);
        if (  lh == NULL  )
            ret = -1;
        else
        {
            libFuncs->libType = libType;
            libFuncs->libHandle = lh;
            switch (  libType  )
            {
                case TT_LIB_DM_TYPE:
                    libFuncs->odbcFuncPtr = 
                       (void * *)calloc(1,sizeof(void *) * (NUM_ODBC_FUNCTIONS+1));
                    if (  libFuncs->odbcFuncPtr == NULL  )
                        ret = -1;
#if defined(ENABLE_XLA_DIRECT)
                    else
                    {
                        libFuncs->xlaFuncPtr = 
                        (void * *)calloc(1,sizeof(void *) * (NUM_XLA_FUNCTIONS+1));
                        if (  libFuncs->xlaFuncPtr == NULL  )
                        {
                            free( (void *)libFuncs->odbcFuncPtr);
                            ret = -1;
                        }
                    }
#endif /* ENABLE_XLA_DIRECT */
                    if (  ret == 0  )
                    {
                        ret = loadODBCfunctions(lh, libFuncs);
#if defined(ENABLE_XLA_DIRECT)
                        if (  ret == 0  )
                        {
                            ret = loadXLAfunctions(lh, libFuncs);
                        }
#endif /* ENABLE_XLA_DIRECT */
                    }
                    break;

                case TT_LIB_CS_TYPE:
                    libFuncs->odbcFuncPtr = 
                       (void * *)calloc(1,sizeof(void *) * (NUM_ODBC_FUNCTIONS+1));
                    if (  libFuncs->odbcFuncPtr == NULL  )
                        ret = -1;
                    else
		    {
                        ret = loadODBCfunctions(lh, libFuncs);
		    }
                    break;

#if defined(ENABLE_UTIL_LIB)
                case TT_LIB_UT_TYPE:
                    libFuncs->utilFuncPtr = 
                       (void * *)calloc(1,sizeof(void *) * (NUM_UTIL_FUNCTIONS+1));
                    if (  libFuncs->utilFuncPtr == NULL  )
                        ret = -1;
                    else
		    {
                        ret = loadUTILfunctions(lh, libFuncs);
		    }
                    break;
#endif /* ENABLE_UTIL_LIB */

                default:
                    ret = -1;
                    break;
            }
        }

        if (  ret != 0  )
        {
            libFuncs->libType = TT_LIB_NO_TYPE;
            if (  libFuncs->libHandle != NULL  )
            {
                dlclose(libFuncs->libHandle);
                libFuncs->libHandle = NULL;
            }
            if (  libFuncs->odbcFuncPtr != NULL  )
            {
                free( (void *)libFuncs->odbcFuncPtr);
                libFuncs->odbcFuncPtr = NULL;
            }
#if defined(ENABLE_XLA_DIRECT)
            if (  libFuncs->xlaFuncPtr != NULL  )
            {
                free( (void *)libFuncs->xlaFuncPtr);
                libFuncs->xlaFuncPtr = NULL;
            }
#endif /* ENABLE_XLA_DIRECT */
#if defined(ENABLE_UTIL_LIB)
            if (  libFuncs->utilFuncPtr != NULL  )
            {
                free( (void *)libFuncs->utilFuncPtr);
                libFuncs->utilFuncPtr = NULL;
            }
#endif /* ENABLE_UTIL_LIB */
        }
    }

    TRACE_FLEAVE_RC("loadLibrary",ret);

    return ret;
} // loadLibrary

static int
unloadLibrary(
              funcmapper_t * libFuncs
)
{
    int    ret = 0;

    TRACE_FENTER("unloadLibrary");

    if (  libFuncs != NULL  )
    {
        if (  libFuncs->libHandle != NULL  )
            dlclose(libFuncs->libHandle);
        if (  libFuncs->odbcFuncPtr != NULL  )
            free( (void *)libFuncs->odbcFuncPtr);
#if defined(ENABLE_XLA_DIRECT)
        if (  libFuncs->xlaFuncPtr != NULL  )
            free( (void *)libFuncs->xlaFuncPtr);
#endif /* ENABLE_XLA_DIRECT */
#if defined(ENABLE_UTIL_LIB)
        if (  libFuncs->utilFuncPtr != NULL  )
            free( (void *)libFuncs->utilFuncPtr);
#endif /* ENABLE_UTIL_LIB */
    }

    TRACE_FLEAVE_RC("unloadLibrary",ret);

    return ret;
} // unloadLibrary

static void
freeTThStmt(
            TTSQLHSTMT hstmt
)
{
    TRACE_FENTER("freeTThStmt");
    TRACE_HVAL("hstmt",hstmt);

    CLEAR_ERROR_STACK(hstmt);
    hstmt->tag = hstmt->hid = 0;
    hstmt->TThEnv = NULL;
    hstmt->TThDbc = NULL;
    hstmt->hEnv = SQL_NULL_HENV;
    hstmt->hDbc = SQL_NULL_HDBC;
    hstmt->hStmt = SQL_NULL_HSTMT;
    hstmt->pFuncMap = NULL;
    hstmt->prev = NULL;
    hstmt->next = NULL;
    free( (void *)hstmt);

    TRACE_FLEAVE("freeTThStmt");
} // freeTThStmt

static void
freeTThDbc(
           TTSQLHDBC hdbc
)
{
    TRACE_FENTER("freeTThDbc");
    TRACE_HVAL("hdbc",hdbc);

    CLEAR_ERROR_STACK(hdbc);
    hdbc->tag = hdbc->hid = 0;
    hdbc->TThEnv = NULL;
    hdbc->clientEnv = SQL_NULL_HENV;
    hdbc->directEnv = SQL_NULL_HENV;
    hdbc->hEnv = SQL_NULL_HENV;
    hdbc->clientDbc = SQL_NULL_HDBC;
    hdbc->directDbc = SQL_NULL_HDBC;
    hdbc->hDbc = SQL_NULL_HDBC;
    hdbc->pClientMap = NULL;
    hdbc->pDirectMap = NULL;
    hdbc->pFuncMap = NULL;
#if defined(ENABLE_XLA_DIRECT)
    hdbc->TThXla = NULL;
#endif /* ENABLE_XLA_DIRECT */
    hdbc->prev = NULL;
    hdbc->next = NULL;
    hdbc->head = NULL;
    hdbc->tail = NULL;
    hdbc->dhead = NULL;
    hdbc->dtail = NULL;
    if (  hdbc->connName != NULL  )
    {
	free((void *)hdbc->connName);
	hdbc->connName = NULL;
    }
    if (  hdbc->connNameW != NULL  )
    {
	free((void *)hdbc->connNameW);
	hdbc->connNameW = NULL;
    }
    if (  hdbc->serverName != NULL  )
    {
	free((void *)hdbc->serverName);
	hdbc->serverName = NULL;
    }
    if (  hdbc->serverNameW != NULL  )
    {
	free((void *)hdbc->serverNameW);
	hdbc->serverNameW = NULL;
    }
    free( (void *)hdbc);

    TRACE_FLEAVE("freeTThDbc");
} // freeTThDbc

static void
freeTThEnv(
           TTSQLHENV henv
)
{
    TRACE_FENTER("freeTThEnv");
    TRACE_HVAL("henv",henv);

    CLEAR_ERROR_STACK(henv);
    henv->tag = henv->hid = 0;
    henv->directEnv = SQL_NULL_HENV;
    henv->clientEnv = SQL_NULL_HENV;
    henv->pDirectMap = NULL;
    henv->pClientMap = NULL;
    henv->prev = NULL;
    henv->next = NULL;
    henv->head = NULL;
    henv->tail = NULL;
    free( (void *)henv);

    TRACE_FLEAVE("freeTThEnv");
} // freeTThEnv

static void
freeTThDesc(
            TTSQLHDESC hdesc
)
{
    TRACE_FENTER("freeTThDesc");
    TRACE_HVAL("hdesc",hdesc);

    hdesc->tag = hdesc->hid = 0;
    hdesc->TThEnv = NULL;
    hdesc->TThDbc = NULL;
    hdesc->hEnv = SQL_NULL_HENV;
    hdesc->hDbc = SQL_NULL_HDBC;
    hdesc->hDesc = SQL_NULL_HDESC;
    free( (void *)hdesc);

    TRACE_FLEAVE("freeTThDesc");
} // freeTThDesc

static SQLRETURN
sqlAllocDesc(
    SQLHDBC            hdbc,
    int                implicit,
    SQLHDESC          *phdesc
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;
    TTSQLHDESC  TThDesc = NULL;

    TRACE_FENTER("sqlAllocDesc");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  phdesc == NULL  )
        {
            MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
                      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_ARGERR35,
                      0,
                      (SQLCHAR *)TTDMERRPFX "sqlAllocDesc: "
                                 "NULL passed for descriptor handle pointer",
                      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                      TThDbc->connName,
                      TThDbc->serverName
                  );
            MUTEX_UNLOCK(TThDbc->mutex);
        }
        else
        {
            *phdesc = SQL_NULL_HDESC;
            if (  TThDbc->hDbc == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
            else
            {
                MUTEX_LOCK(TThDbc->mutex);
                TThDesc = (TTSQLHDESC)calloc(1,sizeof(TT_DESC_t));
                if (  TThDesc == NULL  )
                    pushError(&(TThDbc->errorStack),
                              rc = SQL_ERROR,
			      ENCODING_ANSI,
                              (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                              tt_ErrDMNoMemory,
                              (SQLCHAR *)TTDMERRPFX "sqlAllocDesc: "
                                         "Unable to allocate memory for Descriptor",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      TThDbc->connName,
			      TThDbc->serverName
                              );
                else
                {
#if defined(ENABLE_THREAD_SAFETY)
                    MUTEX_CREATE(TThDesc->mutex);
		    if (  TThDesc->mutex == NULL  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
			          ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                                  tt_ErrDMNoMemory,
                                  (SQLCHAR *)TTDMERRPFX "sqlAllocDesc: "
                                             "Unable to allocate memory for mutex",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          NULL,
			          NULL
                                 );
		    else
		    {
#endif /* ENABLE_THREAD_SAFETY */
                        TThDesc->tag = TTDM_STRUCT_TAG;
                        TThDesc->hid = TTDM_HID_DESC;
                        TThDesc->implicit = implicit;
                        TThDesc->TThEnv = TThDbc->TThEnv;
                        TThDesc->TThDbc = TThDbc;
                        TThDesc->hEnv = TThDbc->hEnv;
                        TThDesc->hDbc = TThDbc->hDbc;
                        TThDesc->hDesc = SQL_NULL_HDESC;
                        TThDesc->pFuncMap = TThDbc->pFuncMap;
                        if (  ! implicit  )                 
                            rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLHDESC * ))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLAllocHandle])))(SQL_HANDLE_DESC, TThDesc->hDbc,&(TThDesc->hDesc));
    #if defined(ENABLE_THREAD_SAFETY)
		    }
#endif /* ENABLE_THREAD_SAFETY */

                    if (  rc != SQL_SUCCESS  )
		    {
#if defined(ENABLE_THREAD_SAFETY)
		        if (  TThDesc->mutex != NULL  )
			    MUTEX_DESTROY(TThDesc->mutex);
#endif /* ENABLE_THREAD_SAFETY */
                        free( (void *)TThDesc );
		    }
		    else
                    {
                        /* Link it into list */
                        if (  TThDbc->dhead == NULL  )
                        {
                            TThDbc->dhead = TThDbc->dtail = TThDesc;
                            TThDesc->prev = TThDesc->next = NULL;
                        }
                        else
                        {
                            TThDbc->dtail->next = (void *)TThDesc;
                            TThDesc->prev = TThDbc->dtail;
                            TThDesc->next = NULL;
                            TThDbc->dtail = TThDesc;
                        }
                        *phdesc = (SQLHDESC)TThDesc;
                    }
                }
                MUTEX_UNLOCK(TThDbc->mutex);
            }
        }
    }

    if (  (rc == SQL_SUCCESS) && (phdesc != NULL)  )
        TRACE_HVAL("*phdesc",*phdesc);

    TRACE_FLEAVE_RC("sqlAllocDesc",rc);

    return rc;
} // sqlAllocDesc

static SQLRETURN
sqlFreeDesc(
    SQLHDESC            hdesc
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("sqlFreeDesc");
    TRACE_HVAL("hdesc",hdesc);

    rc = CLEAR_ERROR_STACK_LOCK(hdesc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDesc = (TTSQLHDESC)hdesc;
        if (  TThDesc->hDesc == SQL_NULL_HDESC  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            TThDbc = TThDesc->TThDbc;
            MUTEX_LOCK(TThDesc->mutex);
            rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_DESC, TThDesc->hDesc);
	    if (  rc == SQL_SUCCESS  )
	    {
                MUTEX_LOCK(TThDbc->mutex);

                /* unlink from list in TThDbc and free */
                if  (  TThDbc->dhead == TThDbc->dtail  )
                {
                    /* Only entry in list */
                    TThDbc->dhead = TThDbc->dtail = NULL;
                }
                else
                if (  TThDesc == TThDbc->dhead  )
                {
                    /* First entry in list */
                    TThDbc->dhead = TThDbc->dhead->next;
                    TThDbc->dhead->prev = NULL;
                }
                else
                if (  TThDesc == TThDbc->dtail  )
                {
                    /* Last entry in list */
                    TThDbc->dtail = TThDbc->dtail->prev;
                    TThDbc->dtail->next = NULL;
                }
                else
                {
                    /* somewhere else in list */
                    TThDesc->prev->next = TThDesc->next;
                    TThDesc->next->prev = TThDesc->prev;
                }
                MUTEX_UNLOCK(TThDbc->mutex);
                MUTEX_UNLOCK(TThDesc->mutex);
                MUTEX_DESTROY(TThDesc->mutex);
                freeTThDesc(TThDesc);
	    }
	    else
		MUTEX_UNLOCK(TThDesc->mutex);
        }
    }

    TRACE_FLEAVE_RC("sqlFreeDesc",rc);

    return rc;
} // sqlFreeDesc

static SQLRETURN
sqlAllocStmt(
    SQLHDBC            hdbc,
    SQLHSTMT          *phstmt,
    int                v3
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;
    TTSQLHSTMT  TThStmt = NULL;

    TRACE_FENTER("sqlAllocStmt");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  phstmt == NULL  )
        {
            MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      v3?(SQLCHAR *)TT_DM_SQLSTATE_ARGERR35:(SQLCHAR *)TT_DM_SQLSTATE_ARGERR25,
                      0,
                      (SQLCHAR *)TTDMERRPFX "sqlAllocStmt: "
                                 "NULL passed for statement handle pointer",
		      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		      TThDbc->connName,
		      TThDbc->serverName
                  );
            MUTEX_UNLOCK(TThDbc->mutex);
        }
        else
        {
            *phstmt = SQL_NULL_HSTMT;
	    if (  TThDbc->hDbc == SQL_NULL_HDBC  )
	    {
                MUTEX_LOCK(TThDbc->mutex);
                pushError(&(TThDbc->errorStack),
                          rc = SQL_ERROR,
		          ENCODING_ANSI,
                          (SQLCHAR *)TT_DM_SQLSTATE_NOTCONN,
                          0,
                          (SQLCHAR *)TTDMERRPFX "sqlAllocStmt: Not connected",
		          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		          TThDbc->connName,
		          TThDbc->serverName
                     );
                MUTEX_UNLOCK(TThDbc->mutex);
	    }
            else
            {
                MUTEX_LOCK(TThDbc->mutex);
                TThStmt = (TTSQLHSTMT)calloc(1,sizeof(TT_STMT_t));
                if (  TThStmt == NULL  )
                    pushError(&(TThDbc->errorStack),
                              rc = SQL_ERROR,
			      ENCODING_ANSI,
                              (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                              tt_ErrDMNoMemory,
                              (SQLCHAR *)TTDMERRPFX "sqlAllocStmt: "
                                         "Unable to allocate memory for Statement",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      TThDbc->connName,
			      TThDbc->serverName
                              );
                else
                {
                    TThStmt->tag = TTDM_STRUCT_TAG;
                    TThStmt->hid = TTDM_HID_STMT;
                    TThStmt->TThEnv = TThDbc->TThEnv;
                    TThStmt->v3 = TThDbc->v3;
                    TThStmt->hEnv = TThDbc->hEnv;
                    TThStmt->TThDbc = TThDbc;
                    TThStmt->hDbc = TThDbc->hDbc;
                    TThStmt->hStmt = SQL_NULL_HSTMT;
                    TThStmt->pFuncMap = TThDbc->pFuncMap;
                    TThStmt->errorStack.stackH = NULL;
                    TThStmt->errorStack.stackT = NULL;
                    TThStmt->errorStack.msgcnt = 0;
#if defined(ENABLE_THREAD_SAFETY)
                    MUTEX_CREATE(TThStmt->mutex);
		    if (  TThStmt->mutex == NULL  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
			          ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                                  tt_ErrDMNoMemory,
                                  (SQLCHAR *)TTDMERRPFX "sqlAllocStmt: "
                                             "Unable to allocate memory for mutex",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          TThDbc->connName,
			          TThDbc->serverName
                                 );
		    else
		    {
#endif /* ENABLE_THREAD_SAFETY */
                        if (  v3  )
                            rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLHANDLE * ))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLAllocHandle])))(SQL_HANDLE_STMT,TThStmt->hDbc,&(TThStmt->hStmt));
                        else
                            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHSTMT * ))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLAllocStmt])))(TThStmt->hDbc,&(TThStmt->hStmt));
                        if (  rc == SQL_SUCCESS  )
                        { // retrieve the implicit descriptors
                            (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetStmtAttr])))(TThStmt->hStmt, SQL_ATTR_IMP_ROW_DESC, (SQLPOINTER)&(TThStmt->IRD), 0, NULL);
                            (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetStmtAttr])))(TThStmt->hStmt, SQL_ATTR_IMP_PARAM_DESC, (SQLPOINTER)&(TThStmt->IPD), 0, NULL);
                        }
#if defined(ENABLE_THREAD_SAFETY)
		    }
#endif /* ENABLE_THREAD_SAFETY */
                    if (  rc != SQL_SUCCESS  )
		    {
#if defined(ENABLE_THREAD_SAFETY)
		        if (  TThStmt->mutex != NULL  )
			    MUTEX_DESTROY(TThStmt->mutex);
#endif /* ENABLE_THREAD_SAFETY */
                        free( (void *)TThStmt );
		    }
                    else
                    {  /* Link it into list */
                        if (  TThDbc->head == NULL  )
                        {
                            TThDbc->head = TThDbc->tail = TThStmt;
                            TThStmt->prev = TThStmt->next = NULL;
                        }
                        else
                        {
                            TThDbc->tail->next = (void *)TThStmt;
                            TThStmt->prev = TThDbc->tail;
                            TThStmt->next = NULL;
                            TThDbc->tail = TThStmt;
                        }
                        *phstmt = (SQLHSTMT)TThStmt;
                    }
                }
                MUTEX_UNLOCK(TThDbc->mutex);
            }
        }
    }

    if (  (rc == SQL_SUCCESS) && (phstmt != NULL)  )
        TRACE_HVAL("*phstmt",*phstmt);

    TRACE_FLEAVE_RC("sqlAllocStmt",rc);

    return rc;
} // sqlAllocStmt

static SQLRETURN
sqlAllocConnect(
    SQLHENV             henv,
    SQLHDBC            *phdbc,
    int                 v3
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    SQLRETURN   rc1 = SQL_SUCCESS;
    SQLRETURN   rc2 = SQL_SUCCESS;
    TTSQLHENV   TThEnv = NULL;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("sqlAllocConnect");
    TRACE_HVAL("henv",henv);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(henv);
    if (  rc == SQL_SUCCESS  )
    {
        TThEnv = (TTSQLHENV)henv;
        if (  phdbc == NULL  )
        {
            MUTEX_LOCK(TThEnv->mutex);
            pushError(&(TThEnv->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      v3?(SQLCHAR *)TT_DM_SQLSTATE_ARGERR35:(SQLCHAR *)TT_DM_SQLSTATE_ARGERR25,
                      0,
                      (SQLCHAR *)TTDMERRPFX "sqlAllocConnect: "
                                 "NULL passed for connection handle pointer",
		      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		      NULL,
		      NULL
	        );
            MUTEX_UNLOCK(TThEnv->mutex);
        }
        else
        {
            *phdbc = SQL_NULL_HDBC;
            if (  ((TThEnv->directEnv == SQL_NULL_HENV) &&
                   (TThEnv->clientEnv == SQL_NULL_HENV))  )
                rc = SQL_INVALID_HANDLE;
            else
            {
                MUTEX_LOCK(TThEnv->mutex);
                TThDbc = (TTSQLHDBC)calloc(1,sizeof(TT_DBC_t));
                if (  TThDbc == NULL  )
                    pushError(&(TThEnv->errorStack),
                              rc = SQL_ERROR,
			      ENCODING_ANSI,
                              (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                              tt_ErrDMNoMemory,
                              (SQLCHAR *)TTDMERRPFX "sqlAllocConnect: "
                                         "Unable to allocate memory for Connection",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      NULL,
			      NULL
                              );
                else
                {
                    TThDbc->tag = TTDM_STRUCT_TAG;
                    TThDbc->hid = TTDM_HID_DBC;
                    TThDbc->TThEnv = TThEnv;
                    TThDbc->v3 = TThEnv->v3;
                    TThDbc->directEnv = TThEnv->directEnv;
                    TThDbc->clientEnv = TThEnv->clientEnv;
                    TThDbc->hEnv = SQL_NULL_HENV;
                    TThDbc->clientDbc = SQL_NULL_HDBC;
                    TThDbc->directDbc = SQL_NULL_HDBC;
                    TThDbc->hDbc = SQL_NULL_HDBC;
                    TThDbc->pDirectMap = TThEnv->pDirectMap;
                    TThDbc->pClientMap = TThEnv->pClientMap;
                    TThDbc->pFuncMap = NULL;
                    TThDbc->errorStack.stackH = NULL;
                    TThDbc->errorStack.stackT = NULL;
                    TThDbc->errorStack.msgcnt = 0;
#if defined(ENABLE_XLA_DIRECT)
                    TThDbc->TThXla = NULL;
#endif /* ENABLE_XLA_DIRECT */
                    TThDbc->head = NULL;
                    TThDbc->tail = NULL;
		    TThDbc->connName = NULL;
		    TThDbc->connNameW = NULL;
		    TThDbc->serverName = NULL;
		    TThDbc->serverNameW = NULL;
                    TThDbc->dhead = NULL;
                    TThDbc->dtail = NULL;
#if defined(ENABLE_THREAD_SAFETY)
                    MUTEX_CREATE(TThDbc->mutex);
		    if (  TThDbc->mutex == NULL  )
                        pushError(&(TThEnv->errorStack),
                                  rc = SQL_ERROR,
			          ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                                  tt_ErrDMNoMemory,
                                  (SQLCHAR *)TTDMERRPFX "sqlAllocConnect: "
                                             "Unable to allocate memory for mutex",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          NULL,
			          NULL
                                 );
		    else
		    {
#endif /* ENABLE_THREAD_SAFETY */
                        if (  TThDbc->pDirectMap != NULL  )
                        {
                            if (  v3  )
                                rc1 = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLHANDLE * ))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLAllocHandle])))(SQL_HANDLE_DBC,TThDbc->directEnv,&(TThDbc->directDbc));
                            else
                                rc1 = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC * ))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLAllocConnect])))(TThDbc->directEnv,&(TThDbc->directDbc));
                        }
                        if (  TThDbc->pClientMap != NULL  )
                        {
                            if (  v3  )
                                rc2 = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLHANDLE * ))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLAllocHandle])))(SQL_HANDLE_DBC,TThDbc->clientEnv,&(TThDbc->clientDbc));
                            else
                                rc2 = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC * ))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLAllocConnect])))(TThDbc->clientEnv,&(TThDbc->clientDbc));
                        }
                        rc = maxODBCError(rc1, rc2);
#if defined(ENABLE_THREAD_SAFETY)
		    }
#endif /* ENABLE_THREAD_SAFETY */

                    if (  rc != SQL_SUCCESS  )
                    {
                        if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                        {
                            if (  v3  )
                                (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_DBC,TThDbc->directDbc);
                            else
                                (*((SQLRETURN (SQL_API *)(SQLHDBC))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeConnect])))(TThDbc->directDbc);
                        }
                        if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                        {
                            if (  v3  )
                                (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_DBC,TThDbc->clientDbc);
                            else
                            (*((SQLRETURN (SQL_API *)(SQLHDBC))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLFreeConnect])))(TThDbc->clientDbc);
                        }
#if defined(ENABLE_THREAD_SAFETY)
		        if (  TThDbc->mutex != NULL  )
			    MUTEX_DESTROY(TThDbc->mutex);
#endif /* ENABLE_THREAD_SAFETY */
		        free( (void *)TThDbc);
                    }
                    else
                    {  
                        /* Link it into list */
                        if (  TThEnv->head == NULL  )
                        {
                            TThEnv->head = TThEnv->tail = TThDbc;
                            TThDbc->prev = TThDbc->next = NULL;
                        }
                        else
                        {
                            TThEnv->tail->next = TThDbc;
                            TThDbc->prev = TThEnv->tail;
                            TThDbc->next = NULL;
                            TThEnv->tail = TThDbc;
                        }
                        *phdbc = (SQLHDBC)TThDbc;
                    }
                }
                MUTEX_UNLOCK(TThEnv->mutex);
            }
        }
    }

    if (  (rc == SQL_SUCCESS) && (phdbc != NULL)  )
        TRACE_HVAL("*phdbc",*phdbc);

    TRACE_FLEAVE_RC("sqlAllocConnect",rc);

    return rc;
} // sqlAllocConnect

static SQLRETURN
sqlAllocEnv(
    SQLHENV            *phenv,
    int                 v3
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  rcDirect = SQL_SUCCESS;
    SQLRETURN  rcClient = SQL_SUCCESS;
#if defined(ENABLE_UTIL_LIB)
    SQLRETURN  rcUtil = SQL_SUCCESS;
#endif /* ENABLE_UTIL_LIB */
    TTSQLHENV  TThEnv = NULL;

    TRACE_FENTER("sqlAllocEnv");
    TRACE_HVAL("phenv",phenv);

    if (  phenv == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        *phenv = SQL_NULL_HENV;
        TThEnv = (TTSQLHENV)calloc(1,sizeof(TT_ENV_t));
        if (  TThEnv == NULL  )
            rc = SQL_INVALID_HANDLE;
	else
	{
#if defined(ENABLE_THREAD_SAFETY)
            MUTEX_CREATE(TThEnv->mutex);
	    if (  TThEnv->mutex == NULL  )
		rc = SQL_INVALID_HANDLE;
	    else
            {
#endif /* ENABLE_THREAD_SAFETY */
            MUTEX_LOCK(DMGlobalData.mutex);
            TThEnv->tag = TTDM_STRUCT_TAG;
            TThEnv->hid = TTDM_HID_ENV;
            TThEnv->directEnv = SQL_NULL_HENV;
            TThEnv->clientEnv = SQL_NULL_HENV;
            TThEnv->errorStack.msgcnt = 0;
            TThEnv->errorStack.stackH = NULL;
            TThEnv->errorStack.stackT = NULL;
            TThEnv->head = NULL;
            TThEnv->tail = NULL;

            /* First check if we are initialised and if not then do it */
            /* Try to load direct mode library first */
            if (  DMGlobalData.directFuncs == NULL  ) 
            {
                DMGlobalData.directFuncs = (funcmapper_t *)calloc(1,sizeof(funcmapper_t));
                if (  DMGlobalData.directFuncs == NULL  )
                    pushError(&(TThEnv->errorStack),
                              /* rc = SQL_ERROR, */
                              rcDirect = SQL_ERROR,
			      ENCODING_ANSI,
                              (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                              tt_ErrDMNoMemory,
                              (SQLCHAR *)TTDMERRPFX "sqlAllocEnv: "
                                         "Unable to allocate memory "
                                         "to load direct mode driver",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      NULL,
			      NULL
			     );
                else
                {
                    if (  loadLibrary(TT_LIB_DM_NAME, TT_LIB_DM_TYPE,
                                      DMGlobalData.directFuncs)  )
                    {
                        free( (void *)DMGlobalData.directFuncs );
                        DMGlobalData.directFuncs = NULL;
                        pushError(&(TThEnv->errorStack),
                                  rcDirect = SQL_ERROR,
			          ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_LIBLOAD,
                                  tt_ErrDMDriverLoad,
                                  (SQLCHAR *)TTDMERRPFX "sqlAllocEnv: "
                                             "Unable to load direct mode driver",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          NULL,
			          NULL
			          );
                    }
                }
            }
            TThEnv->pDirectMap = DMGlobalData.directFuncs;

            /* Now try loading client library */
            if (  DMGlobalData.clientFuncs == NULL  ) 
            {
                DMGlobalData.clientFuncs = (funcmapper_t *)calloc(1,sizeof(funcmapper_t));
                if (  DMGlobalData.clientFuncs == NULL  )
                    pushError(&(TThEnv->errorStack),
                              rcClient = SQL_ERROR,
			      ENCODING_ANSI,
                              (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                              tt_ErrDMNoMemory,
                              (SQLCHAR *)TTDMERRPFX "sqlAllocEnv: "
                                         "Unable to allocate memory "
                                         "to load client/server driver",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      NULL,
			      NULL
			      );
                else
                {
                    if (  loadLibrary(TT_LIB_CS_NAME, TT_LIB_CS_TYPE,
                                      DMGlobalData.clientFuncs)  )
                    {
                        free( (void *)DMGlobalData.clientFuncs );
                        DMGlobalData.clientFuncs = NULL;
                        pushError(&(TThEnv->errorStack),
                                  rcClient = SQL_ERROR,
			          ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_LIBLOAD,
                                  tt_ErrDMDriverLoad,
                                  (SQLCHAR *)TTDMERRPFX "sqlAllocEnv: "
                                             "Unable to load client/server driver",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          NULL,
			          NULL
			          );
                    }
                }
            }
            TThEnv->pClientMap = DMGlobalData.clientFuncs;

#if defined(ENABLE_UTIL_LIB)
            /* Now try loading util library - depends on the direct library */
            if (  (DMGlobalData.utilFuncs == NULL) && 
                  (DMGlobalData.directFuncs != NULL)  ) 
            {
                DMGlobalData.utilFuncs = (funcmapper_t *)calloc(1,sizeof(funcmapper_t));
                if (  DMGlobalData.utilFuncs == NULL  )
                    pushError(&(TThEnv->errorStack),
                              rcUtil = SQL_ERROR,
			      ENCODING_ANSI,
                              (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                              tt_ErrDMNoMemory,
                              (SQLCHAR *)TTDMERRPFX "sqlAllocEnv: "
                                         "Unable to allocate memory "
                                         "to load utility library",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      NULL,
			      NULL
			      );
                else
                {
                    if (  loadLibrary(TT_LIB_UT_NAME, TT_LIB_UT_TYPE,
                                      DMGlobalData.utilFuncs)  )
                    {
                        free( (void *)DMGlobalData.utilFuncs );
                        DMGlobalData.utilFuncs = NULL;
                        pushError(&(TThEnv->errorStack),
                                  rcUtil = SQL_ERROR,
			          ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_LIBLOAD,
                                  tt_ErrDMDriverLoad,
                                  (SQLCHAR *)TTDMERRPFX "sqlAllocEnv: "
                                             "Unable to load utility library",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          NULL,
			          NULL
			          );
                    }
                }
            }
#endif /* ENABLE_UTIL_LIB */

            if (  (TThEnv->pDirectMap == NULL) &&
                  (TThEnv->pClientMap == NULL)  )
		/* couldn't load either driver */
                rc = SQL_ERROR;
            else
            {
		rcDirect = rcClient = SQL_SUCCESS;
                if (  TThEnv->pDirectMap != NULL  )
                {
                    if (  v3  )
                        rcDirect = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLHANDLE *))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLAllocHandle])))(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&(TThEnv->directEnv));
                    else
                        rcDirect = (*((SQLRETURN (SQL_API *)(SQLHENV *))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLAllocEnv])))(&(TThEnv->directEnv));
                }
                if (  TThEnv->pClientMap != NULL  )
                {
                    if (  v3  )
                        rcClient = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLHANDLE *))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLAllocHandle])))(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&(TThEnv->clientEnv));
                    else
                        rcClient = (*((SQLRETURN (SQL_API *)(SQLHENV *))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLAllocEnv])))(&(TThEnv->clientEnv));
                }
                rc = maxODBCError(rcDirect, rcClient);

                if (  rc != SQL_SUCCESS  )
                {
                    if (  TThEnv->clientEnv != SQL_NULL_HENV  )
                    {
                        if (  v3  )
                            (*((SQLRETURN (SQL_API *)(SQLSMALLINT,SQLHANDLE))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_ENV,TThEnv->clientEnv);
                        else
                            (*((SQLRETURN (SQL_API *)(SQLHENV))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLFreeEnv])))(TThEnv->clientEnv);
                    }
                    if (  TThEnv->directEnv != SQL_NULL_HENV  )
                    {
                        if (  v3  )
                            (*((SQLRETURN (SQL_API *)(SQLSMALLINT,SQLHANDLE))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_ENV,TThEnv->directEnv);
                        else
                            (*((SQLRETURN (SQL_API *)(SQLHENV))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeEnv])))(TThEnv->directEnv);
                    }
                }
                else
                {  /* Link it into list */
                    if (  DMGlobalData.ELhead == NULL  )
                    {
                        DMGlobalData.ELhead = DMGlobalData.ELtail = TThEnv;
                        TThEnv->prev = TThEnv->next = NULL;
                    }
                    else
                    {
                        DMGlobalData.ELtail->next = (void *)TThEnv;
                        TThEnv->prev = DMGlobalData.ELtail;
                        TThEnv->next = NULL;
                        DMGlobalData.ELtail = TThEnv;
                    }
                    *phenv = (SQLHENV)TThEnv;
                }
            }
            MUTEX_UNLOCK(DMGlobalData.mutex);
#if defined(ENABLE_THREAD_SAFETY)
	    }
#endif /* ENABLE_THREAD_SAFETY */
	    if (  rc != SQL_SUCCESS  )
	    {
#if defined(ENABLE_THREAD_SAFETY)
		if (  TThEnv->mutex != NULL  )
		    MUTEX_DESTROY(TThEnv->mutex);
#endif /* ENABLE_THREAD_SAFETY */
                free( (void *)TThEnv );
	    }
        }
    }

    if (  (rc == SQL_SUCCESS) && (phenv != NULL)  )
        TRACE_HVAL("*phenv",*phenv);

    TRACE_FLEAVE_RC("sqlAllocEnv",rc);

    return rc;
} // sqlAllocEnv

static SQLRETURN
sqlFreeEnv(
    SQLHENV             henv,
    int                 v3
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  rc1 = SQL_SUCCESS;
    SQLRETURN  rc2 = SQL_SUCCESS;
    TTSQLHENV  TThEnv = NULL;

    TRACE_FENTER("sqlFreeEnv");
    TRACE_HVAL("henv",henv);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(henv);
    if (  rc == SQL_SUCCESS  )
    {
        TThEnv = (TTSQLHENV)henv;
        if (  ((TThEnv->directEnv == SQL_NULL_HENV) &&
               (TThEnv->clientEnv == SQL_NULL_HENV))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(TThEnv->mutex);
            if (  TThEnv->directEnv != NULL  )
            {
                if (  v3  )
                    rc1 = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_ENV,TThEnv->directEnv);
                else
                    rc1 = (*((SQLRETURN (SQL_API *)(SQLHENV))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeEnv])))(TThEnv->directEnv);
                if (  rc1 == SQL_SUCCESS  )
                    TThEnv->directEnv = SQL_NULL_HENV;
            }
            if (  TThEnv->clientEnv != NULL  )
            {
                if (  v3  )
                    rc2 = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_ENV,TThEnv->clientEnv);
                else
                    rc2 = (*((SQLRETURN (SQL_API *)(SQLHENV))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLFreeEnv])))(TThEnv->clientEnv);
                if (  rc2 == SQL_SUCCESS  )
                    TThEnv->clientEnv = SQL_NULL_HENV;
            }
            rc = maxODBCError(rc1, rc2);

            if (  rc == SQL_SUCCESS  )
            {
                /* mark invalid */
                TThEnv->tag = 0;
                TThEnv->hid = 0;

                MUTEX_LOCK(DMGlobalData.mutex);

                /* unlink from master list and free */
                if  (  DMGlobalData.ELhead == DMGlobalData.ELtail  )
                {
                    /* Only entry in list */
                    DMGlobalData.ELhead = DMGlobalData.ELtail = NULL;
                }
                else
                if (  TThEnv == DMGlobalData.ELhead  )
                {
                    /* First entry in list */
                    DMGlobalData.ELhead = DMGlobalData.ELhead->next;
                    DMGlobalData.ELhead->prev = NULL;
                }
                else
                if (  TThEnv == DMGlobalData.ELtail  )
                {
                    /* Last entry in list */
                    DMGlobalData.ELtail = DMGlobalData.ELtail->prev;
                    DMGlobalData.ELtail->next = NULL;
                }
                else
                {
                    /* somewhere else in list */
                    TThEnv->prev->next = TThEnv->next;
                    TThEnv->next->prev = TThEnv->prev;
                }
                MUTEX_UNLOCK(TThEnv->mutex);
                MUTEX_DESTROY(TThEnv->mutex);
                freeTThEnv(TThEnv);

                /* if no more Environments, unload libraries */
                if (  DMGlobalData.ELhead == NULL  )
                {
                    if (  DMGlobalData.directFuncs != NULL  )
                    { 
                        unloadLibrary(DMGlobalData.directFuncs);
                        free( (void *)DMGlobalData.directFuncs );
                        DMGlobalData.directFuncs = NULL;
                    }
                    if (  DMGlobalData.clientFuncs != NULL  )
                    {
                        unloadLibrary(DMGlobalData.clientFuncs);
                        free( (void *)DMGlobalData.clientFuncs );
                        DMGlobalData.clientFuncs = NULL;
                    }
#if defined(ENABLE_UTIL_LIB)
                    if (  DMGlobalData.utilFuncs != NULL  )
                    {
                        unloadLibrary(DMGlobalData.utilFuncs);
                        free( (void *)DMGlobalData.utilFuncs );
                        DMGlobalData.utilFuncs = NULL;
                    }
#endif /* ENABLE_UTIL_LIB */
                }
                MUTEX_UNLOCK(DMGlobalData.mutex);
            }
	    else
		MUTEX_UNLOCK(TThEnv->mutex);
        }
    }

    TRACE_FLEAVE_RC("sqlFreeEnv",rc);

    return rc;
} // sqlFreeEnv

static SQLRETURN
sqlFreeConnect(
    SQLHDBC             hdbc,
    int                 v3
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  rc1 = SQL_SUCCESS;
    SQLRETURN  rc2 = SQL_SUCCESS;
    TTSQLHENV  TThEnv = NULL;
    TTSQLHDBC  TThDbc = NULL;

    TRACE_FENTER("sqlFreeConnect");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        if (  TThDbc->hDbc != NULL  ) /* still connected */
        {
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
	              ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_FUNCSEQ,
                      tt_ErrDMNotDisconnected,
                      (SQLCHAR *)TTDMERRPFX "sqlFreeConnect: "
                                 "Still connected",
	              (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
	              (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
	              TThDbc->connName,
	              TThDbc->serverName
                      );
        }
        else
        {
            TThEnv = TThDbc->TThEnv;
            MUTEX_LOCK(TThDbc->mutex);
            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
            {
                if (  v3  )
                    rc1 = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_DBC,TThDbc->directDbc);
                else
                    rc1 = (*((SQLRETURN (SQL_API *)(SQLHDBC))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLFreeConnect])))(TThDbc->directDbc);
                if (  rc1 == SQL_SUCCESS  )
                    TThDbc->directDbc = SQL_NULL_HDBC;
            }
            if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
            {
                if (  v3  )
                    rc2 = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_DBC,TThDbc->clientDbc);
                else
                    rc2 = (*((SQLRETURN (SQL_API *)(SQLHDBC))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLFreeConnect])))(TThDbc->clientDbc);
                if (  rc2 == SQL_SUCCESS  )
                    TThDbc->clientDbc = SQL_NULL_HDBC;
            }
            rc = maxODBCError(rc1, rc2);

            if (  rc == SQL_SUCCESS  )
            {
                MUTEX_LOCK(TThEnv->mutex);
                /* unlink from list in TThEnv and free */
                if  (  TThEnv->head == TThEnv->tail  )
                {
                    /* Only entry in list */
                    TThEnv->head = TThEnv->tail = NULL;
                }
                else
                if (  TThDbc == TThEnv->head  )
                {
                    /* First entry in list */
                    TThEnv->head = TThEnv->head->next;
                    TThEnv->head->prev = NULL;
                }
                else
                if (  TThDbc == TThEnv->tail  )
                {
                    /* Last entry in list */
                    TThEnv->tail = TThEnv->tail->prev;
                    TThEnv->tail->next = NULL;
                }
                else
                {
                    /* somewhere else in list */
                    TThDbc->prev->next = TThDbc->next;
                    TThDbc->next->prev = TThDbc->prev;
                }
                MUTEX_UNLOCK(TThDbc->mutex);
                MUTEX_DESTROY(TThDbc->mutex);
                freeTThDbc(TThDbc);

                MUTEX_UNLOCK(TThEnv->mutex);
            }
            else
                MUTEX_UNLOCK(TThDbc->mutex);
        }
    }

    TRACE_FLEAVE_RC("sqlFreeConnect",rc);

    return rc;
} // sqlFreeConnect


static SQLRETURN
sqlFreeStmt(
    SQLHSTMT            hstmt,
    SQLUSMALLINT        fOption,
    int                 v3
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDBC  TThDbc = NULL;

    TRACE_FENTER("sqlFreeStmt");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            TThDbc = TThStmt->TThDbc;
            MUTEX_LOCK(TThStmt->mutex);
            if (  v3  )
                rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLFreeHandle])))(SQL_HANDLE_STMT,TThStmt->hStmt);
            else
                rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLFreeStmt])))(TThStmt->hStmt, fOption);
	    if (  rc != SQL_SUCCESS )
                MUTEX_UNLOCK(TThStmt->mutex);
	    else
            if (  fOption == SQL_DROP  )
            {
                MUTEX_LOCK(TThDbc->mutex);

                /* unlink from list in TThDbc and free */
                if  (  TThDbc->head == TThDbc->tail  )
                {
                    /* Only entry in list */
                    TThDbc->head = TThDbc->tail = NULL;
                }
                else
                if (  TThStmt == TThDbc->head  )
                {
                    /* First entry in list */
                    TThDbc->head = TThDbc->head->next;
                    TThDbc->head->prev = NULL;
                }
                else
                if (  TThStmt == TThDbc->tail  )
                {
                    /* Last entry in list */
                    TThDbc->tail = TThDbc->tail->prev;
                    TThDbc->tail->next = NULL;
                }
                else
                {
                    /* somewhere else in list */
                    TThStmt->prev->next = TThStmt->next;
                    TThStmt->next->prev = TThStmt->prev;
                }
                MUTEX_UNLOCK(TThDbc->mutex);
                MUTEX_UNLOCK(TThStmt->mutex);

                MUTEX_DESTROY(TThStmt->mutex);
                freeTThStmt(TThStmt);
            }
	    else
                MUTEX_UNLOCK(TThStmt->mutex);
        }
    }

    TRACE_FLEAVE_RC("sqlFreeStmt",rc);

    return rc;
} // sqlFreeStmt

/* 
 * Populates connection structure with ANSI and Unicode versions of
 * connection and server names.
 *
 */
static void
populateConnectionData(
		TTSQLHDBC TThDbc
)
{
    SQLRETURN       rc = SQL_SUCCESS;
    SQLHSTMT        hstmt = SQL_NULL_HSTMT;
    SQLSMALLINT     InfoType = SQL_DATA_SOURCE_NAME;
    SQLPOINTER      InfoPtr = NULL;
    SQLSMALLINT     BufferLength = 0;
    SQLSMALLINT     StringLength = 0;
    SQLLEN          conNameLen = 0;
    SQLCHAR       * srvName = NULL;
    SQLWCHAR      * srvNameW = NULL;
    SQLCHAR       * conName = NULL;
    SQLWCHAR      * conNameW = NULL;
    ODBC_ERR_STACK_t tmpStack;

   /*
    * Save current error stack from TThDbc and then 'clear' it.
    */
   tmpStack.msgcnt = TThDbc->errorStack.msgcnt;
   tmpStack.stackH = TThDbc->errorStack.stackH;
   tmpStack.stackT = TThDbc->errorStack.stackT;
   TThDbc->errorStack.msgcnt = 0;
   TThDbc->errorStack.stackH = NULL;
   TThDbc->errorStack.stackT = NULL;

    /*
     * Get ANSI server name.
     */
    rc = SQLGetInfo((SQLHDBC)TThDbc, InfoType, InfoPtr, 
		     BufferLength, &StringLength);
    if (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
    {
	BufferLength = StringLength + 1;
        srvName = (SQLCHAR *)calloc(1,BufferLength);
	if (  srvName != NULL  )
	{
	    InfoPtr = (SQLPOINTER)srvName;
            rc = SQLGetInfo((SQLHDBC)TThDbc, InfoType, InfoPtr, 
		             BufferLength, &StringLength);
	    if (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
		TThDbc->serverName = srvName;
	    else
		free((void *)srvName);
	}
    }

    /*
     * Get UTF16 server name.
     */
    StringLength = BufferLength = 0;
    InfoPtr = NULL;
    rc = SQLGetInfoW((SQLHDBC)TThDbc, InfoType, InfoPtr, 
		     BufferLength, &StringLength);
    if (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
    {
	BufferLength = StringLength + 2;
        srvNameW = (SQLWCHAR *)calloc(1,BufferLength);
	if (  srvNameW != NULL  )
	{
	    InfoPtr = (SQLPOINTER)srvNameW;
            rc = SQLGetInfoW((SQLHDBC)TThDbc, InfoType, InfoPtr, 
		             BufferLength, &StringLength);
	    if (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
		TThDbc->serverNameW = srvNameW;
	    else
		free((void *)srvNameW);
	}
    }

    /*
     * Get ANSI connection name.
     */
    rc = SQLAllocStmt((SQLHDBC)TThDbc, &hstmt);
    if (  rc == SQL_SUCCESS  )
    {
	rc = SQLExecDirect(hstmt, 
			   (SQLCHAR *)"call ttConfiguration('ConnectionName')", SQL_NTS);
        if (  rc == SQL_SUCCESS  )
	    rc = SQLFetch(hstmt);
        if (  rc == SQL_SUCCESS  )
        {
	    conName = (SQLCHAR *)calloc(1,TT_MAX_CONN_NAME_LEN+1);
	    if (  conName != NULL  )
	    {
	        rc = SQLGetData(hstmt, 2, SQL_C_CHAR, conName, 
			        TT_MAX_CONN_NAME_LEN+1, &conNameLen);
	        if (  (rc == SQL_SUCCESS) && (conNameLen > 0)  )
		{
		    SQLCHAR * tmp;

		    tmp = (SQLCHAR *)strdup((char *)conName);
		    if (  tmp != NULL  )
		    {
		        TThDbc->connName = tmp;
			free((void *)conName);
		    }
		    else
			TThDbc->connName = conName;
		}
	        else
		    free((void *)conName);
	    }
        }
	SQLFreeStmt(hstmt,SQL_DROP);
    }

    /*
     * Create UTF16 connection name.
     */
    if (  TThDbc->connName != NULL  )
    {
        conNameW = (SQLWCHAR *)calloc(1,2*(strlen((const char *)TThDbc->connName)+1));
        if (  (conNameW != NULL) && (TThDbc->connName != NULL)  )
        {
            translateANSItoUTF16(conNameW, (char *)TThDbc->connName);
	    TThDbc->connNameW = conNameW;
	}
    }

    /* clear actual ODBC error stack, if any */
    clearODBCErrorStack(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLError],
                        SQL_NULL_HENV,TThDbc->hDbc);

   /*
    * Restore ttHdbc error stack
    */
   TThDbc->errorStack.msgcnt = tmpStack.msgcnt;
   TThDbc->errorStack.stackH = tmpStack.stackH;
   TThDbc->errorStack.stackT = tmpStack.stackT;

} // populateConnectionData

/*
 ========================================================================
                 Public functions - ODBC
 ========================================================================
 */

/*
 * Most of these functions simply do the bare minimum of work and simply
 * pass the call straight through to the relevant direct or client 
 * library function. In a few cases such as connecting, error handling etc.
 * (significant) additional work is needed. See comments for individual
 * functions
 */

// TimesTen Proprietary function!!!
SQLRETURN SQL_API 
SQLBindParameterExtended(
  SQLHSTMT      hstmt,        /* INP: statement handle */
  SQLUSMALLINT  ipar,         /* INP: parameter number */
  SQLSMALLINT   fParamType,   /* INP: parameter type */
  SQLSMALLINT   fCType,       /* INP: C data type of parameter */
  SQLSMALLINT   fSqlType,     /* INP: SQL data type of parameter */
  SQLULEN       cbColDef,     /* INP: param precision */
  SQLSMALLINT   ibScale,      /* INP: parameter scale */
  SQLPOINTER    rgbValue,     /* I/O: ptr to parameter buffer */
  SQLLEN        cbValueMax,   /* INP: max len of rgbValue */
  SQLLEN        stride,       /* INP: distance to next value */
  SQLLEN        maxArrLen,    /* INP: max number of elts in array */
  SQLLEN*       actualArrLen, /* I/O: ptr to the actual num. elts */
  SQLLEN*       pcbValue
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLBindParameterExtended");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT, SQLSMALLINT, SQLSMALLINT, SQLULEN, SQLSMALLINT, SQLPOINTER, SQLLEN, SQLLEN, SQLLEN, SQLLEN *, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLBindParameterExtended])))(TThStmt->hStmt, ipar, fParamType, fCType, fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, stride, maxArrLen, actualArrLen, pcbValue);
        }
    }

    TRACE_FLEAVE_RC("SQLBindParameterExtended",rc);

    return rc;
} // SQLBindParameterExtended

// Not supported by TimesTen so just return suitable error
SQLRETURN SQL_API 
SQLBrowseConnect(
    SQLHDBC               hdbc,
    SQLCHAR             * szConnStrIn,
    SQLSMALLINT           cbConnStrIn,
    SQLCHAR             * szConnStrOut,
    SQLSMALLINT           cbConnStrOutMax,
    SQLSMALLINT         * pcbConnStrOut
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;

    TRACE_FENTER("SQLBrowseConnect");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
                      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NOTSUPP,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLBrowseConnect: "
                                 "Function not supported",
                      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                      NULL, NULL
                      );
            MUTEX_UNLOCK(TThDbc->mutex);
        }
    }

    TRACE_FLEAVE_RC("SQLBrowseConnect",rc);

    return rc;
} // SQLBrowseConnect

SQLRETURN SQL_API 
SQLBulkOperations(
    SQLHSTMT              hstmt,
    SQLSMALLINT           operation
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLBulkOperations");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLBulkOperations])))(TThStmt->hStmt,operation);
        }
    }

    TRACE_FLEAVE_RC("SQLBulkOperations",rc);

    return rc;
} // SQLBulkOperations

SQLRETURN SQL_API
SQLCloseCursor(
    SQLHSTMT              hstmt
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLCloseCursor");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLCloseCursor])))(TThStmt->hStmt);
        }
    }

    TRACE_FLEAVE_RC("SQLCloseCursor",rc);

    return rc;
} // SQLCloseCursor

SQLRETURN SQL_API 
SQLCopyDesc(
    SQLHDESC              hsource,
    SQLHDESC              htarget
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDESC TThDescS = NULL;
    TTSQLHDESC TThDescT = NULL;

    TRACE_FENTER("SQLCopyDesc");
    TRACE_HVAL("hsource",hsource);
    TRACE_HVAL("htarget",htarget);

    rc = CLEAR_ERROR_STACK_LOCK(hsource);
    if (  rc == SQL_SUCCESS  )
        rc = CLEAR_ERROR_STACK_LOCK(htarget);
    if (  rc == SQL_SUCCESS  )
    {
        TThDescS = (TTSQLHDESC)hsource;
        TThDescT = (TTSQLHDESC)htarget;
        if (  (TThDescS->hDesc == SQL_NULL_HDESC) ||
              (TThDescT->hDesc == SQL_NULL_HDESC)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDESC, SQLHDESC))(TThDescS->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLCopyDesc])))(TThDescS->hDesc, TThDescT->hDesc);
        }
    }

    TRACE_FLEAVE_RC("SQLCopyDesc",rc);

    return rc;
} // SQLCopyDesc

SQLRETURN SQL_API
SQLSetParam(
  SQLHSTMT     hstmt,
  SQLUSMALLINT ipar,
  SQLSMALLINT  fValueType,
  SQLSMALLINT  fParamType,
  SQLULEN      cbColDef,
  SQLSMALLINT  ibScale,
  SQLPOINTER   rgbValue,
  SQLLEN     * pcbValue
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSetParam");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT, SQLSMALLINT, SQLULEN, SQLSMALLINT, SQLPOINTER, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetParam])))(TThStmt->hStmt, ipar, fValueType, fParamType, cbColDef, ibScale, rgbValue, pcbValue);
        }
    }

    TRACE_FLEAVE_RC("SQLSetParam",rc);

    return rc;
} // SQLSetParam

SQLRETURN SQL_API
SQLAllocEnv(
    SQLHENV            *phenv
)
{
    return sqlAllocEnv( phenv, 0 );
} // SQLAllocEnv

SQLRETURN SQL_API
SQLAllocConnect(
    SQLHENV             henv,
    SQLHDBC            *phdbc
)
{
    return sqlAllocConnect( henv, phdbc, 0 );
} // SQLAllocConnect

SQLRETURN SQL_API
SQLAllocStmt(
    SQLHDBC            hdbc,
    SQLHSTMT          *phstmt
)
{
    return sqlAllocStmt( hdbc, phstmt, 0 );
} // SQLAllocStmt

/*
 * SQLError - ANSI
 *
 * This needs lots of extra work due to the multiple error
 * stacks and encodings that need to be handled. Luckily
 * this function generally should not be in any performance
 * critial path.
 */
SQLRETURN  SQL_API
SQLError(
  SQLHENV        henv,
  SQLHDBC        hdbc,
  SQLHSTMT       hstmt,
  SQLCHAR      * szSqlState,
  SQLINTEGER   * pfNativeError,
  SQLCHAR      * szErrorMsg,
  SQLSMALLINT    cbErrorMsgMax,
  SQLSMALLINT  * pcbErrorMsg
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  errRC = SQL_SUCCESS;
    int        encoding = 0;
    SQLCHAR    sqlState[2*(TT_SQLSTATE_LEN+1)];
    SQLINTEGER nativeError= 0;
    SQLCHAR    errorMsg[SQL_MAX_MESSAGE_LENGTH+1];
    SQLCHAR    tErrorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];
    TTSQLHENV  TThEnv = NULL;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT TThStmt = NULL;
    SQLHENV    hEnv1 = SQL_NULL_HENV;
    SQLHENV    hEnv2 = SQL_NULL_HENV;
    SQLHDBC    hDbc1 = SQL_NULL_HDBC;
    SQLHDBC    hDbc2 = SQL_NULL_HDBC;
    SQLHSTMT   hStmt = SQL_NULL_HSTMT;
    void     * odbcFuncPtr1 = NULL;
    void     * odbcFuncPtr2 = NULL;
    int        errMsgLen = 0;
    ODBC_ERR_STACK_t * errorStack = NULL;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t mutex = MUTEX_INITIALISER;
#endif /* ENABLE_THREAD_SAFETY */
    
    TRACE_FENTER("SQLError");
    TRACE_HVAL("henv",henv);
    TRACE_HVAL("hdbc",hdbc);
    TRACE_HVAL("hstm",hstmt);

    TThEnv = (TTSQLHENV)henv;
    TThDbc = (TTSQLHDBC)hdbc;
    TThStmt = (TTSQLHSTMT)hstmt;
    if (  hstmt != SQL_NULL_HSTMT  ) /* want errors from hstmt */
    {
        if (  (TThStmt->tag != TTDM_STRUCT_TAG) ||
	      (TThStmt->hid != TTDM_HID_STMT)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(mutex = TThStmt->mutex);
            errorStack = &(TThStmt->errorStack);
            hStmt = TThStmt->hStmt;
            odbcFuncPtr1 = TThStmt->pFuncMap->
                           odbcFuncPtr[F_POS_ODBC_SQLError];
        }
    }
    else
    if (  hdbc != SQL_NULL_HDBC  ) /* want errors from hdbc */
    {
        if (  (TThDbc->tag != TTDM_STRUCT_TAG) ||
	      (TThDbc->hid != TTDM_HID_DBC)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(mutex = TThDbc->mutex);
            errorStack = &(TThDbc->errorStack);
            if (  TThDbc->hDbc != SQL_NULL_HDBC  ) /* connected */
            {
                hDbc1 = TThDbc->hDbc;
                odbcFuncPtr1 = TThDbc->pFuncMap->
                               odbcFuncPtr[F_POS_ODBC_SQLError];
            }
            else /* not connected */
            {
                if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                {
                    hDbc1 = TThDbc->clientDbc;
                    odbcFuncPtr1 = TThDbc->pClientMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLError];
                    if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                    {
                        hDbc2 = TThDbc->directDbc;
                        odbcFuncPtr2 = TThDbc->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLError];
                    }
                }
                else
                {
                    hDbc1 = TThDbc->directDbc;
                    odbcFuncPtr1 = TThDbc->pDirectMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLError];
                }
            }
        }
    }
    else
    if (  henv != SQL_NULL_HENV  ) /* want errors from henv */
    {
        if (  (TThEnv->tag != TTDM_STRUCT_TAG) ||
	      (TThEnv->hid != TTDM_HID_ENV)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(mutex = TThEnv->mutex);
            errorStack = &(TThEnv->errorStack);
            if (  TThEnv->clientEnv != SQL_NULL_HENV  )
            {
                hEnv1 = TThEnv->clientEnv;
                odbcFuncPtr1 = TThEnv->pClientMap->
                               odbcFuncPtr[F_POS_ODBC_SQLError];
                if (  TThEnv->directEnv != SQL_NULL_HENV  )
                {
                    hEnv2 = TThEnv->directEnv;
                    odbcFuncPtr2 = TThEnv->pDirectMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLError];
                }
            }
            else
            {
                hEnv1 = TThEnv->directEnv;
                odbcFuncPtr1 = TThEnv->pDirectMap->
                               odbcFuncPtr[F_POS_ODBC_SQLError];
            }
        }
    }
    else
    {
        errorStack = NULL;
        rc = SQL_INVALID_HANDLE;    
    }

    if (  rc == SQL_SUCCESS  )
    {   // Consume errors from the appropriate TTDM error stack first
        rc = popError(errorStack, &errRC, &encoding, 
		      sqlState, &nativeError, tErrorMsg
		      , NULL, NULL, NULL, NULL );
        if (  rc == SQL_SUCCESS  )
        {
	    if (  pfNativeError != NULL  )
                *pfNativeError = nativeError;
	    if (  encoding == ENCODING_ANSI  )
	    { /* can copy directly */
		if (  szSqlState != NULL  )
                    strcpy((char *)szSqlState,(const char *)sqlState);
		strcpy((char *)errorMsg,(const char *)tErrorMsg);
	    }
	    else
	    { /* need to convert UTF-16 to ANSI */
		if (  szSqlState != NULL  )
		    translateUTF16toANSI((char *)szSqlState,(SQLWCHAR *)sqlState);
		translateUTF16toANSI((char *)errorMsg,(SQLWCHAR *)tErrorMsg);
            }
	    /* now copy error message to return buffer */
            errMsgLen = strlen((const char *)errorMsg);
	    if (  pcbErrorMsg != NULL  )
                *pcbErrorMsg = (SQLSMALLINT)errMsgLen;
	    if (  szErrorMsg != NULL  )
	    {
                if (  errMsgLen < (int)cbErrorMsgMax  )
                    strcpy((char *)szErrorMsg, (const char *)errorMsg);
                else
                {
                    rc = SQL_SUCCESS_WITH_INFO;
                    if (  cbErrorMsgMax > 0  )
                    {
                        strncpy((char *)szErrorMsg, (const char *)errorMsg, (cbErrorMsgMax-1));
                        szErrorMsg[cbErrorMsgMax-1] = 0;
                    }
                    else
                        szErrorMsg[0] = 0;
		}
            }
        }
        else
        {   // Consume actual ODBC error stack
            if (  hstmt != SQL_NULL_HSTMT  )
                rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr1)))
                                   (SQL_NULL_HENV,SQL_NULL_HDBC,hStmt,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
            else
            if (  hdbc != SQL_NULL_HDBC )
            {
                rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr1)))
                                   (SQL_NULL_HENV,hDbc1,SQL_NULL_HSTMT,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
                if (  (rc == SQL_NO_DATA_FOUND) &&
                      (odbcFuncPtr2 != NULL)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr2)))
                                   (SQL_NULL_HENV,hDbc2,SQL_NULL_HSTMT,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
            }
            else
            {
                rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr1)))
                                   (hEnv1,SQL_NULL_HDBC,SQL_NULL_HSTMT,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
                if (  (rc == SQL_NO_DATA_FOUND) &&
                      (odbcFuncPtr2 != NULL)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr2)))
                                   (hEnv2,SQL_NULL_HDBC,SQL_NULL_HSTMT,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
            }
        }
        MUTEX_UNLOCK(mutex);
    }

    TRACE_FLEAVE_RC("SQLError",rc);

    return rc;
}

/*
 * Depending on which libraries are available (direct, client or
 * both) this function must figure out which one will actually work
 * for the provide connection string, make the connection and then set
 * everything up for the future use of the connection.
 */
SQLRETURN SQL_API
SQLDriverConnect(
    SQLHDBC             hdbc, 
    SQLHWND             hwnd,
    SQLCHAR           * szConnStrIn,
    SQLSMALLINT         cbConnStrIn,
    SQLCHAR           * szConnStrOut,
    SQLSMALLINT         cbConnStrOutMax,
    SQLSMALLINT       * pcbConnStrOut,
    SQLUSMALLINT        fDriverCompletion
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;
    SQLCHAR             sqlState[TT_SQLSTATE_LEN+1];
    SQLINTEGER          nativeError = 0;
    SQLCHAR             errorMsg[SQL_MAX_MESSAGE_LENGTH+1];

    TRACE_FENTER("SQLDriverConnect");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
	    MUTEX_LOCK(TThDbc->mutex);
            if (  (TThDbc->directDbc != SQL_NULL_HDBC) &&
                  (TThDbc->clientDbc != SQL_NULL_HDBC)  )
            {
		/* Try client driver first ... */
                rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHWND, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLUSMALLINT))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLDriverConnect])))(TThDbc->clientDbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
                if (  (rc == SQL_SUCCESS) ||
                      (rc == SQL_SUCCESS_WITH_INFO)  )
                {
                    TThDbc->hEnv = TThDbc->clientEnv;
                    TThDbc->hDbc = TThDbc->clientDbc;
                    TThDbc->pFuncMap = TThDbc->pClientMap;
		    /* ensure any warnings are pushed onto the error stack */
		    if (  rc != SQL_SUCCESS  )
			populateConnErrorStack(TThDbc, 1, ENCODING_ANSI);
                }
                else
                if (  rc == SQL_ERROR  )
                {
                    sqlState[0] = '\0';
                    getODBCConnectError(
		       TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec],
		       TThDbc->clientDbc,
                       1,
                       sqlState, 
		       &nativeError, 
		       errorMsg);
                    if (  (nativeError != 0) ||
                          ( ( strcmp((char *)sqlState,TT_DM_SQLSTATE_CONNERR25) &&
                              strcmp((char *)sqlState,TT_DM_SQLSTATE_CONNERR35) )  )  )
		    {
			/* a 'real' error, push it onto our stack */
                        pushError(&(TThDbc->errorStack), rc,
			          ENCODING_ANSI, 
				  sqlState, nativeError,
                                  errorMsg,
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          TThDbc->connName,
			          TThDbc->serverName
                                 );
		        /* push any additional errors/warnings onto our stack */
			populateConnErrorStack(TThDbc, 1, ENCODING_ANSI);
		    }
                    else
                    {
			/* tried to use client driver to connect to a non-client DSN */
                        /* discard any extra errors/warnings on client handle */
                        clearODBCErrorStack(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLError],
                                            SQL_NULL_HENV,TThDbc->clientDbc);
                        /* ... then try direct driver */
                        rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHWND, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLUSMALLINT))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLDriverConnect])))(TThDbc->directDbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
                        if (  (rc == SQL_SUCCESS) ||
                              (rc == SQL_SUCCESS_WITH_INFO)  )
                        {
                            TThDbc->hEnv = TThDbc->directEnv;
                            TThDbc->hDbc = TThDbc->directDbc;
                            TThDbc->pFuncMap = TThDbc->pDirectMap;
                        }
			/* ensure any errors or warnings are stacked */
			if (  rc != SQL_SUCCESS  )
			    populateConnErrorStack(TThDbc, 0, ENCODING_ANSI);
                    }
                }
            }
            else
            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
            {
                /* Just try direct */
                rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHWND, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLUSMALLINT))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLDriverConnect])))(TThDbc->directDbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
                if (  (rc == SQL_SUCCESS) ||
                      (rc == SQL_SUCCESS_WITH_INFO)  )
                {
                    TThDbc->hEnv = TThDbc->directEnv;
                    TThDbc->hDbc = TThDbc->directDbc;
                    TThDbc->pFuncMap = TThDbc->pDirectMap;
                }
		/* ensure any errors or warnings are stacked */
		if (  rc != SQL_SUCCESS  )
		    populateConnErrorStack(TThDbc, 0, ENCODING_ANSI);
            }
            else
            {
                /* Just try client */
                rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHWND, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLUSMALLINT))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLDriverConnect])))(TThDbc->clientDbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
                if (  (rc == SQL_SUCCESS) ||
                      (rc == SQL_SUCCESS_WITH_INFO)  )
                {
                    TThDbc->hEnv = TThDbc->clientEnv;
                    TThDbc->hDbc = TThDbc->clientDbc;
                    TThDbc->pFuncMap = TThDbc->pClientMap;
                }
		/* ensure any errors or warnings are stacked */
		if (  rc != SQL_SUCCESS  )
		    populateConnErrorStack(TThDbc, 1, ENCODING_ANSI);
            }

	    MUTEX_UNLOCK(TThDbc->mutex);
        }
    }

    if (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
	populateConnectionData(TThDbc);

    TRACE_FLEAVE_RC("SQLDriverConnect",rc);

    return rc;
} // SQLDriverConnect

/*
 * This function needs to do a similar job to SQLDriverConnect.
 * It validates the input parameters to make sure they are
 * sane then builds a connection string and passes it to
 * SQLDriverConnect to do the heavy lifting.
 */
SQLRETURN SQL_API
SQLConnect(
    SQLHDBC             hdbc, 
    SQLCHAR           * szDSN,
    SQLSMALLINT         cbDSN,
    SQLCHAR           * szUID,
    SQLSMALLINT         cbUID,
    SQLCHAR           * szAuthStr,
    SQLSMALLINT         cbAuthStr
)
{
/* Some ANSI constants */
static SQLCHAR ansiDSN[] = "DSN=";
static SQLCHAR ansiUID[] = ";UID=";
static SQLCHAR ansiPWD[] = ";PWD=";
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;
    SQLCHAR  * connStr = NULL;
    char     * cspos = NULL;

    TRACE_FENTER("SQLConnect");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
	else
	if (  TThDbc->hDbc != SQL_NULL_HDBC  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_CONNUSED,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLConnect: Already connected",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      TThDbc->connName,
			      TThDbc->serverName
                              );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
	if (  (szDSN == NULL) || 
              ((cbDSN <= 0) && (cbDSN != SQL_NTS)) || 
	      ((szDSN != NULL) && (*szDSN == 0))  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NODSN,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLConnect: No DSN specified",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      TThDbc->connName,
			      TThDbc->serverName
                              );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
	else
        {
            if (  cbDSN == SQL_NTS  )
                cbDSN = (SQLSMALLINT)strlen((char *)szDSN);
	    if (  szUID == NULL  )
		cbUID = 0;
	    else
            if (  cbUID  < 0  )
                cbUID = (SQLSMALLINT)strlen((char *)szUID);
	    if (  szAuthStr == NULL  )
		cbAuthStr = 0;
	    else
            if (  cbAuthStr  < 0  )
                cbAuthStr = (SQLSMALLINT)strlen((char *)szAuthStr);
	    connStr = (SQLCHAR *)calloc(1,20+cbDSN+cbUID+cbAuthStr);
	    if (  connStr == NULL  )
	    {
	        MUTEX_LOCK(TThDbc->mutex);
                pushError(&(TThDbc->errorStack),
                          rc = SQL_ERROR,
		          ENCODING_ANSI,
                          (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                          0,
                          (SQLCHAR *)TTDMERRPFX "SQLConnect: Memory allocation failed",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          TThDbc->connName,
			          TThDbc->serverName
                                  );
	        MUTEX_UNLOCK(TThDbc->mutex);
	    }
	    else
	    {
	        /* Build the connection string */
	        cspos = (char *)connStr;
    
	        strcpy(cspos, (const char *)ansiDSN);
	        cspos += strlen((const char *)ansiDSN);
	        strncpy(cspos, (const char *)szDSN, cbDSN);
	        cspos += cbDSN;

		if (  cbUID > 0  )
		{
	            strcpy(cspos, (const char *)ansiUID);
	            cspos += strlen((const char *)ansiUID);
	            strncpy(cspos, (const char *)szUID, cbUID);
	            cspos += cbUID;
	        }
    
		if (  cbAuthStr > 0  )
		{
	            strcpy(cspos, (const char *)ansiPWD);
	            cspos += strlen((const char *)ansiPWD);
	            strncpy(cspos, (const char *)szAuthStr, cbAuthStr);
	            cspos += cbAuthStr;
		}
                *cspos = 0;

                rc = SQLDriverConnect(hdbc, NULL, connStr, (SQLSMALLINT)SQL_NTS,
                                      (SQLCHAR *)NULL, (SQLSMALLINT)0, NULL,
                                       SQL_DRIVER_NOPROMPT);

		free( (void *)connStr);
	    }
        }
    }

    TRACE_FLEAVE_RC("SQLConnect",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLCancel(
  SQLHSTMT       hstmt
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLCancel");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLCancel])))(TThStmt->hStmt);
        }
    }

    TRACE_FLEAVE_RC("SQLCancel",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLDescribeCol(
      SQLHSTMT       hstmt,
      SQLUSMALLINT   icol,
      SQLCHAR*       szColName,
      SQLSMALLINT    cbColNameMax,
      SQLSMALLINT*   pcbColName,
      SQLSMALLINT*   pfSqlType,
      SQLULEN*       pcbColDef,
      SQLSMALLINT*   pibScale,
      SQLSMALLINT*   pfNullable
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLDescribeCol");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLSMALLINT *, SQLULEN *, SQLSMALLINT *, SQLSMALLINT *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLDescribeCol])))(TThStmt->hStmt, icol, szColName, cbColNameMax, pcbColName, pfSqlType, pcbColDef, pibScale, pfNullable);
        }
    }

    TRACE_FLEAVE_RC("SQLDescribeCol",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLExecDirect(
    SQLHSTMT       hstmt,
    SQLCHAR*       szSqlStr,
    SQLINTEGER     cbSqlStr
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLExecDirect");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLINTEGER))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLExecDirect])))(TThStmt->hStmt,szSqlStr,cbSqlStr);
        }
    }

    TRACE_FLEAVE_RC("SQLExecDirect",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLGetCursorName(
    SQLHSTMT       hstmt,
    SQLCHAR*       szCursor,
    SQLSMALLINT    cbCursorMax,
    SQLSMALLINT*   pcbCursor
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLGetCursorName");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetCursorName])))(TThStmt->hStmt,szCursor,cbCursorMax,pcbCursor);
        }
    }

    TRACE_FLEAVE_RC("SQLGetCursorName",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLNumResultCols(
    SQLHSTMT       hstmt,
    SQLSMALLINT*   pccol
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLNumResultCols");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLSMALLINT *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLNumResultCols])))(TThStmt->hStmt,pccol);
        }
    }

    TRACE_FLEAVE_RC("SQLNumResultCols",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLPrepare(
    SQLHSTMT       hstmt,
    SQLCHAR      * szSqlStr,
    SQLINTEGER     cbSqlStr
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLPrepare");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLINTEGER))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLPrepare])))(TThStmt->hStmt,szSqlStr,cbSqlStr);
        }
    }

    TRACE_FLEAVE_RC("SQLPrepare",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLRowCount(
    SQLHSTMT       hstmt,
    SQLLEN*        pcrow
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLRowCount");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLRowCount])))(TThStmt->hStmt,pcrow);
        }
    }

    TRACE_FLEAVE_RC("SQLRowcount",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLSetCursorName(
    SQLHSTMT       hstmt,
    SQLCHAR*       szCursor,
    SQLSMALLINT    cbCursor
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSetCursorName");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetCursorName])))(TThStmt->hStmt,szCursor,cbCursor);
        }
    }

    TRACE_FLEAVE_RC("SQLCursorName",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLGetStmtOption(
  SQLHSTMT       hstmt,
  SQLUSMALLINT   fOption,
  SQLPOINTER     pvParam
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLGetStmtOption");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLPOINTER))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetStmtOption])))(TThStmt->hStmt, fOption, pvParam);
        }
    }

    TRACE_FLEAVE_RC("SQLGetStmtOption",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLSetStmtOption(
  SQLHSTMT       hstmt,
  SQLUSMALLINT   fOption,
  SQLROWCOUNT    vParam
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSetStmtOption");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLROWCOUNT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetStmtOption])))(TThStmt->hStmt, fOption, vParam);
        }
    }

    TRACE_FLEAVE_RC("SQLSetStmtOption",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLParamData(
  SQLHSTMT       hstmt,
  SQLPOINTER   * prgbValue
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLParamData");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLPOINTER *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLParamData])))(TThStmt->hStmt, prgbValue);
        }
    }

    TRACE_FLEAVE_RC("SQLParamData",rc);

    return rc;
}

SQLRETURN SQL_API
SQLPutData(
  SQLHSTMT       hstmt,
  SQLPOINTER     rgbValue,
  SQLLEN         cbValue
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLPutData");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLPOINTER, SQLLEN))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLPutData])))(TThStmt->hStmt, rgbValue, cbValue);
        }
    }

    TRACE_FLEAVE_RC("SQLPutData",rc);

    return rc;
}

SQLRETURN SQL_API
SQLSpecialColumns(
  SQLHSTMT       hstmt,
  SQLUSMALLINT   fColType,
  SQLCHAR      * szCatalogName,
  SQLSMALLINT    cbCatalogName,
  SQLCHAR      * szSchemaName,
  SQLSMALLINT    cbSchemaName,
  SQLCHAR      * szTableName,
  SQLSMALLINT    cbTableName,
  SQLUSMALLINT   fScope,
  SQLUSMALLINT   fNullable
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSpecialColumns");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLUSMALLINT, SQLUSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSpecialColumns])))(TThStmt->hStmt, fColType, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, fScope, fNullable);
        }
    }

    TRACE_FLEAVE_RC("SQLSpecialColumns",rc);

    return rc;
}

SQLRETURN SQL_API
SQLStatistics(
  SQLHSTMT       hstmt,
  SQLCHAR      * szCatalogName,
  SQLSMALLINT    cbCatalogName,
  SQLCHAR      * szSchemaName,
  SQLSMALLINT    cbSchemaName,
  SQLCHAR      * szTableName,
  SQLSMALLINT    cbTableName,
  SQLUSMALLINT   fUnique,
  SQLUSMALLINT   fAccuracy
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLStatistics");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLUSMALLINT, SQLUSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLStatistics])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, fUnique, fAccuracy);
        }
    }

    TRACE_FLEAVE_RC("SQLStatistics",rc);

    return rc;
}

SQLRETURN SQL_API
SQLTables(
  SQLHSTMT       hstmt,
  SQLCHAR      * szCatalogName,
  SQLSMALLINT    cbCatalogName,
  SQLCHAR      * szSchemaName,
  SQLSMALLINT    cbSchemaName,
  SQLCHAR      * szTableName,
  SQLSMALLINT    cbTableName,
  SQLCHAR      * szTableType,
  SQLSMALLINT    cbTableType
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLTables");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLTables])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, szTableType, cbTableType);
        }
    }

    TRACE_FLEAVE_RC("SQLTables",rc);

    return rc;
}

SQLRETURN SQL_API
SQLGetTypeInfo(
  SQLHSTMT       hstmt,
  SQLSMALLINT    fSqlType
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLGetTypeInfo");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetTypeInfo])))(TThStmt->hStmt, fSqlType);
        }
    }

    TRACE_FLEAVE_RC("SQLGetTypeInfo",rc);

    return rc;
}

SQLRETURN SQL_API
SQLExecute(
    SQLHSTMT       hstmt
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLExecute");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLExecute])))(TThStmt->hStmt);
        }
    }

    TRACE_FLEAVE_RC("SQLExecute",rc);

    return rc;
}

SQLRETURN SQL_API
SQLFetch(
    SQLHSTMT       hstmt
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLFetch");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLFetch])))(TThStmt->hStmt);
        }
    }

    TRACE_FLEAVE_RC("SQLFetch",rc);

    return rc;
}

/*
 * Because SQLTransact can be applied at either the connection
 * or environment level (ugh!!!) this needs special handling.
 */
SQLRETURN SQL_API
SQLTransact(
  SQLHENV        henv,
  SQLHDBC        hdbc,
  SQLUSMALLINT   fType
)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLRETURN rc1 = SQL_SUCCESS;
    SQLRETURN rc2 = SQL_SUCCESS;
    TTSQLHENV TThEnv = NULL;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLTransact");
    TRACE_HVAL("henv",henv);
    TRACE_HVAL("hdbc",hdbc);

    if (  hdbc != NULL  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  (TThDbc->tag != TTDM_STRUCT_TAG) ||
	      (TThDbc->hid != TTDM_HID_DBC)  )
            rc = SQL_INVALID_HANDLE;
	else
	if (  TThDbc->hDbc == SQL_NULL_HDBC  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            CLEAR_ERROR_STACK(TThDbc);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NOTCONN,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLTransact: Not connected",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      TThDbc->connName,
			      TThDbc->serverName
                              );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
        {
            CLEAR_ERROR_STACK_LOCK(TThDbc);
            rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLUSMALLINT))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLTransact])))(SQL_NULL_HENV, TThDbc->hDbc, fType);
        }
    }
    else
    if (  henv != NULL  )
    {
        TThEnv = (TTSQLHENV)henv;
        if (  (TThEnv->tag != TTDM_STRUCT_TAG) ||
	      (TThEnv->hid != TTDM_HID_ENV) ||
              ((TThEnv->directEnv == SQL_NULL_HENV) &&
               (TThEnv->clientEnv == SQL_NULL_HENV))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            CLEAR_ERROR_STACK_ALL_LOCK(TThEnv);
            if (  TThEnv->directEnv != NULL  )
            {
                rc1 = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLUSMALLINT))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLTransact])))(TThEnv->directEnv, SQL_NULL_HDBC, fType);
#if defined(ENABLE_SQLENDTRAN_HENV_WORKAROUND)
                if (  (rc1 == SQL_ERROR) &&
                     isBogusError(rc1,TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLError],TThEnv->directEnv)  )
                    rc1 = SQL_SUCCESS;
#endif /* ENABLE_SQLENDTRAN_HENV_WORKAROUND */
            }
            if (  TThEnv->clientEnv != NULL  )
            {
                rc2 = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLUSMALLINT))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLTransact])))(TThEnv->clientEnv, SQL_NULL_HDBC, fType);
#if defined(ENABLE_SQLENDTRAN_HENV_WORKAROUND)
                if (  (rc2 == SQL_ERROR) &&
                     isBogusError(rc2,TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLError],TThEnv->clientEnv)  )
                    rc2 = SQL_SUCCESS;
#endif /* ENABLE_SQLENDTRAN_HENV_WORKAROUND */
            }
            rc = maxODBCError(rc1, rc2);
        }
    }
    else
        rc = SQL_INVALID_HANDLE;

    TRACE_FLEAVE_RC("SQLTransact",rc);

    return rc;
}

SQLRETURN SQL_API
SQLColAttributes(
    SQLHSTMT       hstmt,
    SQLUSMALLINT   icol,
    SQLUSMALLINT   fDescType,
    SQLPOINTER     rgbDesc,
    SQLSMALLINT    cbDescMax,
    SQLSMALLINT  * pcbDesc,
    SQLLEN       * pfDesc
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLColAttributes");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLColAttributes])))(TThStmt->hStmt, icol, fDescType, rgbDesc, cbDescMax, pcbDesc, pfDesc);
        }
    }

    TRACE_FLEAVE_RC("SQLColAttributes",rc);

    return rc;
}

SQLRETURN SQL_API
SQLColumns(
  SQLHSTMT       hstmt,
  SQLCHAR*       szCatalogName,
  SQLSMALLINT    cbCatalogName,
  SQLCHAR*       szSchemaName,
  SQLSMALLINT    cbSchemaName,
  SQLCHAR*       szTableName,
  SQLSMALLINT    cbTableName,
  SQLCHAR*       szColumnName,
  SQLSMALLINT    cbColumnName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLColumns");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLColumns])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, szColumnName, cbColumnName);
        }
    }

    TRACE_FLEAVE_RC("SQLColumns",rc);

    return rc;
}

/*
 * If the requested option is one of the TTDM specific
 * options then we need to handle it here rather than
 * pass it to the driver. Also, if the HDBC is not in
 * a connected state then we need to handle that 
 * properly too.
 */
SQLRETURN SQL_API
SQLGetConnectOption(
  SQLHDBC        hdbc,
  SQLUSMALLINT   fOption,
  SQLPOINTER     pvParam
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLGetConnectOption");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            if (  fOption == TTDM_CONNECTION_TYPE  )
            {  /* we must handle this one! */
                if (  TThDbc->hDbc == NULL  )
                    *(SQLINTEGER *)pvParam = TTDM_CONN_NONE;
                else
                if (  TThDbc->hDbc == TThDbc->directDbc  )
                    *(SQLINTEGER *)pvParam = TTDM_CONN_DIRECT;
                else
                    *(SQLINTEGER *)pvParam = TTDM_CONN_CLIENT;
            }
            else
            if (  TThDbc->hDbc != SQL_NULL_HDBC  ) /* connected */
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLPOINTER))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectOption])))(TThDbc->hDbc, fOption, pvParam);
            else
            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLPOINTER))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectOption])))(TThDbc->directDbc, fOption, pvParam);
            else
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLPOINTER))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectOption])))(TThDbc->clientDbc, fOption, pvParam);
        }
    }

    TRACE_FLEAVE_RC("SQLGetConnectOption",rc);

    return rc;
}

/*
 * We need to handle this here in the case that the
 * HDBC is not connected (we just return an error).
 */
SQLRETURN SQL_API
SQLGetFunctions(
  SQLHDBC        hdbc,
  SQLUSMALLINT   fFunction,
  SQLUSMALLINT * pfExists
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLGetFunctions");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
	if (  TThDbc->hDbc == SQL_NULL_HDBC ) /* not connected */
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_FUNCSEQ,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLGetFunctions: Not connected",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      TThDbc->connName,
			      TThDbc->serverName
                              );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLUSMALLINT *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetFunctions])))(TThDbc->hDbc, fFunction, pfExists);
        }
    }

    TRACE_FLEAVE_RC("SQLGetFunctions",rc);

    return rc;
}

/*
 * This is potentially complex as some options need additional
 * or special handling while others do not. See inline comments
 * for more details.
 */
SQLRETURN SQL_API
SQLGetInfo(
  SQLHDBC        hdbc,
  SQLUSMALLINT   fInfoType,
  SQLPOINTER     rgbInfoValue,
  SQLSMALLINT    cbInfoValueMax,
  SQLSMALLINT  * pcbInfoValue
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLGetInfo");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;

#if 0
	if (  cbInfoValueMax < 0  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_BADLEN25,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLGetInfo: Negative buffer size",
		      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		      TThDbc->connName,
		      TThDbc->serverName
                 );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
#endif
        switch (fInfoType)
        {
            case SQL_DRIVER_HENV: 
            case SQL_DRIVER_HDBC: 
            case SQL_DRIVER_HSTMT: 
            case SQL_DRIVER_HDESC: 
            case SQL_DRIVER_HLIB:
	        if (  rgbInfoValue == NULL  )
	        {
	            MUTEX_LOCK(TThDbc->mutex);
                    pushError(&(TThDbc->errorStack),
                              rc = SQL_ERROR,
		              ENCODING_ANSI,
                              TThDbc->v3?(SQLCHAR *)TT_DM_SQLSTATE_INVATTR35:(SQLCHAR *)TT_DM_SQLSTATE_INVATTR25,
                              0,
                              (SQLCHAR *)TTDMERRPFX "SQLGetInfo: NULL pointer passed",
		              (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		              (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		              TThDbc->connName,
		              TThDbc->serverName
                         );
	            MUTEX_UNLOCK(TThDbc->mutex);
	        }
                break;
            default:
                break;
        }
        if (  rc == SQL_SUCCESS  )
        {
	    if (  TThDbc->hDbc == SQL_NULL_HDBC  )
	    {
	        MUTEX_LOCK(TThDbc->mutex);
                pushError(&(TThDbc->errorStack),
                          rc = SQL_ERROR,
		          ENCODING_ANSI,
                          (SQLCHAR *)TT_DM_SQLSTATE_NOTCONN,
                          0,
                          (SQLCHAR *)TTDMERRPFX 
                                     "SQLGetInfo: Not connected",
		          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		          TThDbc->connName,
		          TThDbc->serverName
                     );
	        MUTEX_UNLOCK(TThDbc->mutex);
	    }
            else
            {
                /*
                ** If SQLGetInfo() is called specifying one of
                **   SQL_DRIVER_HENV
                **   SQL_DRIVER_HDBC
                ** then this needs special handling. In this case
                ** we simply return the real driver handle value
                ** without calling the 'real' SQLGetInfo().
                **
                ** In the case of a request for
                **   SQL_DRIVER_HLIB
                ** we need to return the proper handle from dlopen.
                **
                ** In the case of a request for on of
                **   SQL_DRIVER_HSTMT
                **   SQL_DRIVER_HDESC
                ** we still don't need to call the real SQLGetInfo but 
                ** some additional work and validation is needed to get 
                ** the real driver value.
                **
                ** In the case of a request for
                **   SQL_DM_VER
                ** we need to return our version info.
                **
                */
                switch (fInfoType)
                {
                case SQL_DM_VER:
                    if (  ! TThDbc->v3  )
                    {
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
                                  ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_INVATTR25,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "Invalid attribute requested",
                                  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                                  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                                  TThDbc->connName,
                                  TThDbc->serverName
                                  );
                    }
                    else
                    {
                        int vlen = strlen(TTDMVERS);

                        if (  pcbInfoValue != NULL  )
                            *pcbInfoValue = (SQLSMALLINT)vlen;
                        if (  (rgbInfoValue != NULL) && (cbInfoValueMax > 0)  )
                        {
                            vlen += 1;
                            if (  cbInfoValueMax < vlen  )
                                vlen = cbInfoValueMax - 1;
                            strncpy( (char *)rgbInfoValue, TTDMVERS, vlen );
                            ((char *)rgbInfoValue)[vlen] = '\0';
                        }
                    }
                    break;

                case SQL_DRIVER_HENV: 
                    if (TThDbc->hEnv != SQL_NULL_HENV)
                        *(SQLHENV *)rgbInfoValue = TThDbc->hEnv;
                    else 
                        rc = SQL_INVALID_HANDLE;
                    break;
    
                case SQL_DRIVER_HDBC:
                    *(SQLHDBC *)rgbInfoValue = TThDbc->hDbc;
                    break;
    
                case SQL_DRIVER_HLIB:
                    *(tt_libptr_t *)rgbInfoValue = TThDbc->pFuncMap->libHandle;
                    break;
    
                case SQL_DRIVER_HSTMT:
                    TThStmt = *(TTSQLHSTMT*)(rgbInfoValue);
                    if (  (TThStmt == NULL) ||
                          (TThStmt->tag != TTDM_STRUCT_TAG) ||
	                  (TThStmt->hid != TTDM_HID_STMT) ||
                          (TThStmt->hStmt == SQL_NULL_HSTMT)  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
			          ENCODING_ANSI,
                                  TThDbc->v3?(SQLCHAR *)TT_DM_SQLSTATE_INVATTR35:(SQLCHAR *)TT_DM_SQLSTATE_INVATTR25,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "Invalid value passed for SQLHSTMT value",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          TThDbc->connName,
			          TThDbc->serverName
                                  );
                    else
                    if (  TThStmt->hDbc != TThDbc->hDbc  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
			          ENCODING_ANSI,
                                  TThDbc->v3?(SQLCHAR *)TT_DM_SQLSTATE_INVATTR35:(SQLCHAR *)TT_DM_SQLSTATE_INVATTR25,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "SQLHSTMT does not belong to the passed SQLHDBC",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          TThDbc->connName,
			          TThDbc->serverName
                                  );
                    else
                    *(SQLHSTMT *)rgbInfoValue = TThStmt->hStmt;
                    break;
    
                case SQL_DRIVER_HDESC:
                    TThDesc = *(TTSQLHDESC*)(rgbInfoValue);
                    if (  (TThDesc == NULL) ||
                          (TThDesc->tag != TTDM_STRUCT_TAG) ||
                          (TThDesc->hid != TTDM_HID_DESC) ||
                          (TThDesc->hDesc == SQL_NULL_HDESC)  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
                                  ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_INVATTR35,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "Invalid value passed for SQLHDESC value",
                                  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                                  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                                  TThDbc->connName,
                                  TThDbc->serverName
                                  );
                    else
                    if (  TThDesc->hDbc != TThDbc->hDbc  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
                                  ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_INVATTR35,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "SQLHDESC does not belong to the passed SQLHDBC",
                                  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                                  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                                  TThDbc->connName,
                                  TThDbc->serverName
                                  );
                    else
                    *(SQLHDESC *)rgbInfoValue = TThDesc->hDesc;
                    break;

                default:
                   rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetInfo])))(TThDbc->hDbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue);
                    break;
                }
            }
        }
    }

    TRACE_FLEAVE_RC("SQLGetInfo",rc);

    return rc;
}

/*
 * Need to handle the case where the HDBC is not yet connected.
 */
SQLRETURN SQL_API
SQLSetConnectOption(
  SQLHDBC        hdbc,
  SQLUSMALLINT   fOption,
  SQLULEN        pvParam
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  rc1 = SQL_SUCCESS;
    SQLRETURN  rc2 = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLSetConnectOption");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            if (  TThDbc->hDbc != SQL_NULL_HDBC  ) /* connected */
                rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLULEN))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectOption])))(TThDbc->hDbc, fOption, pvParam);
            else
            {
                if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                    rc1 = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLULEN))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectOption])))(TThDbc->directDbc, fOption, pvParam);
                if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                    rc2 = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLULEN))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectOption])))(TThDbc->clientDbc, fOption, pvParam);
                rc = maxODBCError(rc1, rc2);
            }
        }
    }

    TRACE_FLEAVE_RC("SQLSetConnectOption",rc);

    return rc;
}

SQLRETURN SQL_API
SQLBindCol(
  SQLHSTMT       hstmt,
  SQLUSMALLINT   icol,
  SQLSMALLINT    fCType,
  SQLPOINTER     rgbValue,
  SQLLEN         cbValueMax,
  SQLLEN       * pcbValue
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLBindCol");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT, SQLPOINTER, SQLLEN, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLBindCol])))(TThStmt->hStmt, icol, fCType, rgbValue, cbValueMax, pcbValue);
        }
    }

    TRACE_FLEAVE_RC("SQLBindCol",rc);

    return rc;
}

SQLRETURN SQL_API
SQLGetData(
  SQLHSTMT       hstmt,
  SQLUSMALLINT   icol,
  SQLSMALLINT    fCType,
  SQLPOINTER     rgbValue,
  SQLLEN         cbValueMax,
  SQLLEN       * pcbValue
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLGetData");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT, SQLPOINTER, SQLLEN, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetData])))(TThStmt->hStmt, icol, fCType, rgbValue, cbValueMax, pcbValue);
        }
    }

    TRACE_FLEAVE_RC("SQLGetData",rc);

    return rc;
}

SQLRETURN SQL_API
SQLBindParameter(
  SQLHSTMT     hstmt,
  SQLUSMALLINT ipar,
  SQLSMALLINT  fParamType,
  SQLSMALLINT  fCType,
  SQLSMALLINT  fSqlType,
  SQLULEN      cbColDef,
  SQLSMALLINT  ibScale,
  SQLPOINTER   rgbValue,
  SQLLEN       cbValueMax,
  SQLLEN     * pcbValue
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLBindParameter");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT, SQLSMALLINT, SQLSMALLINT, SQLULEN, SQLSMALLINT, SQLPOINTER, SQLLEN, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLBindParameter])))(TThStmt->hStmt, ipar, fParamType, fCType, fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, pcbValue);
        }
    }

    TRACE_FLEAVE_RC("SQLBindParameter",rc);

    return rc;
}

SQLRETURN SQL_API
SQLFreeStmt(
    SQLHSTMT            hstmt,
    SQLUSMALLINT        fOption
)
{
    return sqlFreeStmt( hstmt, fOption, 0 );
}

/*
 * Needs some additional work to clean up the function maps
 * and other resources for this connection object.
 */
SQLRETURN SQL_API
SQLDisconnect(
    SQLHDBC             hdbc
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHENV  TThEnv = NULL;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT currTThStmt = NULL;
    TTSQLHSTMT nextTThStmt = NULL;
    TTSQLHDESC currTThDesc = NULL;
    TTSQLHDESC nextTThDesc = NULL;

    TRACE_FENTER("SQLDisconnect");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
	if (  TThDbc->hDbc == SQL_NULL_HDBC  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NOTCONN,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLDisconnect: Not connected",
		      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		      TThDbc->connName,
		      TThDbc->serverName
                 );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLDisconnect])))(TThDbc->hDbc);
            if (  rc == SQL_SUCCESS  )
            {
		MUTEX_LOCK(TThDbc->mutex);

                /* reset direct/client pointers */
                TThDbc->hEnv = SQL_NULL_HENV;
                TThDbc->hDbc = SQL_NULL_HDBC;
                TThDbc->pFuncMap = NULL;

                /* free all dependant TThStmt */
                currTThStmt = TThDbc->head;
                while (  currTThStmt != NULL  )
                {
                    nextTThStmt = currTThStmt->next;
                    freeTThStmt(currTThStmt);
                    currTThStmt = nextTThStmt;
                }
                TThDbc->head = NULL;
                TThDbc->tail = NULL;

                /* free all dependant TThDesc */
                currTThDesc = TThDbc->dhead;
                while (  currTThDesc != NULL  )
                {
                    nextTThDesc = currTThDesc->next;
                    freeTThDesc(currTThDesc);
                    currTThDesc = nextTThDesc;
                }
                TThDbc->dhead = NULL;
                TThDbc->dtail = NULL;

		MUTEX_UNLOCK(TThDbc->mutex);
            }
        }
    }

    TRACE_FLEAVE_RC("SQLDisconnect",rc);

    return rc;
}

SQLRETURN SQL_API
SQLFreeConnect(
    SQLHDBC             hdbc
)
{
    return sqlFreeConnect( hdbc, 0 );
}

SQLRETURN SQL_API
SQLFreeEnv(
    SQLHENV             henv
)
{
    return sqlFreeEnv( henv, 0 );
}

SQLRETURN  SQL_API
SQLColumnPrivileges(
  SQLHSTMT        hstmt,
  SQLCHAR       * szCatalogName,
  SQLSMALLINT     cbCatalogName,
  SQLCHAR       * szSchemaName,
  SQLSMALLINT     cbSchemaName,
  SQLCHAR       * szTableName,
  SQLSMALLINT     cbTableName,
  SQLCHAR       * szColumnName,
  SQLSMALLINT     cbColumnName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLColumnPrivileges");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLColumnPrivileges])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, szColumnName, cbColumnName);
        }
    }

    TRACE_FLEAVE_RC("SQLColumnPrivileges",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLDescribeParam(
  SQLHSTMT        hstmt,
  SQLUSMALLINT    ipar,
  SQLSMALLINT*    pfSqlType,
  SQLULEN*        pcbColDef,
  SQLSMALLINT*    pibScale,
  SQLSMALLINT*    pfNullable
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLDescribeParam");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLSMALLINT *, SQLULEN *, SQLSMALLINT *, SQLSMALLINT *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLDescribeParam])))(TThStmt->hStmt, ipar, pfSqlType, pcbColDef, pibScale, pfNullable);
        }
    }

    TRACE_FLEAVE_RC("SQLDescribeParam",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLExtendedFetch(
  SQLHSTMT        hstmt,
  SQLUSMALLINT    fFetchType,
  SQLLEN          irow,
  SQLULEN       * pcrow,
  SQLUSMALLINT  * rgfRowStatus
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLExtendedFetch");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLLEN, SQLULEN *, SQLUSMALLINT *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLExtendedFetch])))(TThStmt->hStmt, fFetchType, irow, pcrow, rgfRowStatus);
        }
    }

    TRACE_FLEAVE_RC("SQLExtendedFetch",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLForeignKeys(
  SQLHSTMT        hstmt,
  SQLCHAR       * szPkCatalogName,
  SQLSMALLINT     cbPkCatalogName,
  SQLCHAR       * szPkSchemaName,
  SQLSMALLINT     cbPkSchemaName,
  SQLCHAR       * szPkTableName,
  SQLSMALLINT     cbPkTableName,
  SQLCHAR       * szFkCatalogName,
  SQLSMALLINT     cbFkCatalogName,
  SQLCHAR       * szFkSchemaName,
  SQLSMALLINT     cbFkSchemaName,
  SQLCHAR       * szFkTableName,
  SQLSMALLINT     cbFkTableName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLForeignKeys");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLForeignKeys])))(TThStmt->hStmt, szPkCatalogName, cbPkCatalogName, szPkSchemaName, cbPkSchemaName, szPkTableName, cbPkTableName, szFkCatalogName, cbFkCatalogName, szFkSchemaName, cbFkSchemaName, szFkTableName, cbFkTableName);
        }
    }

    TRACE_FLEAVE_RC("SQLForeignKeys",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLMoreResults(
  SQLHSTMT        hstmt
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLMoreResults");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLMoreResults])))(TThStmt->hStmt);
        }
    }

    TRACE_FLEAVE_RC("SQLMoreResults",rc);

    return rc;
}

/*
 * Need to handle the 'not connected' state.
 */
SQLRETURN  SQL_API
SQLNativeSql(
  SQLHDBC         hdbc,
  SQLCHAR       * szSqlStrIn,
  SQLINTEGER      cbSqlStrIn,
  SQLCHAR       * szSqlStr,
  SQLINTEGER      cbSqlStrMax,
  SQLINTEGER    * pcbSqlStr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLNativeSql");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NOTCONN,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLNativeSql: Not connected",
		      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		      TThDbc->connName,
		      TThDbc->serverName
                 );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLCHAR *, SQLINTEGER, SQLCHAR *, SQLINTEGER, SQLINTEGER *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLNativeSql])))(TThDbc->hDbc, szSqlStrIn, cbSqlStrIn, szSqlStr, cbSqlStrMax, pcbSqlStr);
        }
    }

    TRACE_FLEAVE_RC("SQLNativeSql",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLNumParams(
  SQLHSTMT        hstmt,
  SQLSMALLINT   * pcpar
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLNumParams");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLSMALLINT *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLNumParams])))(TThStmt->hStmt, pcpar);
        }
    }

    TRACE_FLEAVE_RC("SQLNumParams",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLParamOptions(
  SQLHSTMT        hstmt,
  SQLROWSETSIZE   crow,
  SQLROWSETSIZE * pirow
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLParamOptions");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLROWSETSIZE, SQLROWSETSIZE *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLParamOptions])))(TThStmt->hStmt, crow, pirow);
        }
    }

    TRACE_FLEAVE_RC("SQLParamOptions",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLPrimaryKeys(
  SQLHSTMT        hstmt,
  SQLCHAR       * szCatalogName,
  SQLSMALLINT     cbCatalogName,
  SQLCHAR       * szSchemaName,
  SQLSMALLINT     cbSchemaName,
  SQLCHAR       * szTableName,
  SQLSMALLINT     cbTableName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLPrimaryKeys");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLPrimaryKeys])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName);
        }
    }

    TRACE_FLEAVE_RC("SQLPrimaryKeys",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLProcedureColumns(
  SQLHSTMT        hstmt,
  SQLCHAR       * szCatalogName,
  SQLSMALLINT     cbCatalogName,
  SQLCHAR       * szSchemaName,
  SQLSMALLINT     cbSchemaName,
  SQLCHAR       * szProcName,
  SQLSMALLINT     cbProcName,
  SQLCHAR       * szColumnName,
  SQLSMALLINT     cbColumnName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLProcedureColumns");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLProcedureColumns])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szProcName, cbProcName, szColumnName, cbColumnName);
        }
    }

    TRACE_FLEAVE_RC("SQLProcedureColumns",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLProcedures(
  SQLHSTMT        hstmt,
  SQLCHAR       * szCatalogName,
  SQLSMALLINT     cbCatalogName,
  SQLCHAR       * szSchemaName,
  SQLSMALLINT     cbSchemaName,
  SQLCHAR       * szProcName,
  SQLSMALLINT     cbProcName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLProcedures");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLProcedures])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szProcName, cbProcName);
        }
    }

    TRACE_FLEAVE_RC("SQLProcedures",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLSetPos(
  SQLHSTMT       hstmt,
  SQLSETPOSIROW  irow,
  SQLUSMALLINT   fOption,
  SQLUSMALLINT   fLock
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSetPos");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLSETPOSIROW, SQLUSMALLINT, SQLUSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetPos])))(TThStmt->hStmt, irow, fOption, fLock);
        }
    }

    TRACE_FLEAVE_RC("SQLSetPos",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLTablePrivileges(
  SQLHSTMT        hstmt,
  SQLCHAR       * szTableQualifier,
  SQLSMALLINT     cbTableQualifier,
  SQLCHAR       * szTableOwner,
  SQLSMALLINT     cbTableOwner,
  SQLCHAR       * szTableName,
  SQLSMALLINT     cbTableName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLTablePrivileges");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLTablePrivileges])))(TThStmt->hStmt, szTableQualifier, cbTableQualifier, szTableOwner, cbTableOwner, szTableName, cbTableName);
        }
    }

    TRACE_FLEAVE_RC("SQLTablePrivileges",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLColAttribute(
  SQLHSTMT        hstmt,
  SQLUSMALLINT    ColumnNumber,
  SQLUSMALLINT    FieldIdentifier,
  SQLPOINTER      CharacterAttributePtr,
  SQLSMALLINT     BufferLength,
  SQLSMALLINT *   StringLengthPtr,
  SQLLEN *        NumericAttributePtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLColAttribute");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLColAttribute])))(TThStmt->hStmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr);
        }
    }

    TRACE_FLEAVE_RC("SQLColAttribute",rc);

    return rc;
}

// Not supported by TimesTen so just return suitable error
SQLRETURN SQL_API 
SQLBrowseConnectW(
    SQLHDBC               hdbc,
    SQLWCHAR            * szConnStrIn,
    SQLSMALLINT           cbConnStrIn,
    SQLWCHAR            * szConnStrOut,
    SQLSMALLINT           cbConnStrOutMax,
    SQLSMALLINT         * pcbConnStrOut
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;

    TRACE_FENTER("SQLBrowseConnectW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
                      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NOTSUPP,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLBrowseConnectW: "
                                 "Function not supported",
                      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                      NULL, NULL
                      );
            MUTEX_UNLOCK(TThDbc->mutex);
        }
    }

    TRACE_FLEAVE_RC("SQLBrowseConnectW",rc);

    return rc;
} // SQLBrowseConnectW

SQLRETURN  SQL_API
SQLColAttributeW(
  SQLHSTMT        hstmt,
  SQLUSMALLINT    ColumnNumber,
  SQLUSMALLINT    FieldIdentifier,
  SQLPOINTER      CharacterAttributePtr,
  SQLSMALLINT     BufferLength,
  SQLSMALLINT *   StringLengthPtr,
  SQLLEN *        NumericAttributePtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLColAttributeW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLColAttributeW])))(TThStmt->hStmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr);
        }
    }

    TRACE_FLEAVE_RC("SQLColAttributeW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLColAttributesW(
    SQLHSTMT       hstmt,
    SQLUSMALLINT   icol,
    SQLUSMALLINT   fDescType,
    SQLPOINTER     rgbDesc,
    SQLSMALLINT    cbDescMax,
    SQLSMALLINT  * pcbDesc,
    SQLLEN       * pfDesc
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLColAttributesW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *, SQLLEN *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLColAttributesW])))(TThStmt->hStmt, icol, fDescType, rgbDesc, cbDescMax, pcbDesc, pfDesc);
        }
    }

    TRACE_FLEAVE_RC("SQLColAttributesW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLColumnPrivilegesW(
  SQLHSTMT        hstmt,
  SQLWCHAR      * szCatalogName,
  SQLSMALLINT     cbCatalogName,
  SQLWCHAR      * szSchemaName,
  SQLSMALLINT     cbSchemaName,
  SQLWCHAR      * szTableName,
  SQLSMALLINT     cbTableName,
  SQLWCHAR      * szColumnName,
  SQLSMALLINT     cbColumnName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLColumnPrivilegesW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLColumnPrivilegesW])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, szColumnName, cbColumnName);
        }
    }

    TRACE_FLEAVE_RC("SQLColumnPrivilegesW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLColumnsW(
  SQLHSTMT       hstmt,
  SQLWCHAR     * szCatalogName,
  SQLSMALLINT    cbCatalogName,
  SQLWCHAR     * szSchemaName,
  SQLSMALLINT    cbSchemaName,
  SQLWCHAR     * szTableName,
  SQLSMALLINT    cbTableName,
  SQLWCHAR     * szColumnName,
  SQLSMALLINT    cbColumnName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLColumnsW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLColumnsW])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, szColumnName, cbColumnName);
        }
    }

    TRACE_FLEAVE_RC("SQLColumnsW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLDescribeColW(
      SQLHSTMT       hstmt,
      SQLUSMALLINT   icol,
      SQLWCHAR     * szColName,
      SQLSMALLINT    cbColNameMax,
      SQLSMALLINT*   pcbColName,
      SQLSMALLINT*   pfSqlType,
      SQLULEN*       pcbColDef,
      SQLSMALLINT*   pibScale,
      SQLSMALLINT*   pfNullable
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLDescribeColW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLSMALLINT *, SQLULEN *, SQLSMALLINT *, SQLSMALLINT *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLDescribeColW])))(TThStmt->hStmt, icol, szColName, cbColNameMax, pcbColName, pfSqlType, pcbColDef, pibScale, pfNullable);
        }
    }

    TRACE_FLEAVE_RC("SQLDescribeColW",rc);

    return rc;
}

/*
 * Depending on which libraries are available (direct, client or
 * both) this function must figure out which one will actually work
 * for the provide connection string, make the connection and then set
 * everything up for the future use of the connection.
 */
SQLRETURN SQL_API
SQLDriverConnectW(
    SQLHDBC             hdbc, 
    SQLHWND             hwnd,
    SQLWCHAR          * szConnStrIn,
    SQLSMALLINT         cbConnStrIn,
    SQLWCHAR          * szConnStrOut,
    SQLSMALLINT         cbConnStrOutMax,
    SQLSMALLINT       * pcbConnStrOut,
    SQLUSMALLINT        fDriverCompletion
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;
    SQLWCHAR            sqlStateW[(TT_SQLSTATE_LEN+1)*2];
    SQLCHAR             sqlState[TT_SQLSTATE_LEN+1];
    SQLINTEGER          nativeError = 0;
    SQLWCHAR            errorMsg[(SQL_MAX_MESSAGE_LENGTH+1)*2];

    TRACE_FENTER("SQLDriverConnectW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
	    MUTEX_LOCK(TThDbc->mutex);
            if (  (TThDbc->directDbc != SQL_NULL_HDBC) &&
                  (TThDbc->clientDbc != SQL_NULL_HDBC)  )
            {
		/* Try client driver first ... */
                rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHWND, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLUSMALLINT))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLDriverConnectW])))(TThDbc->clientDbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
                if (  (rc == SQL_SUCCESS) ||
                      (rc == SQL_SUCCESS_WITH_INFO)  )
                {
                    TThDbc->hEnv = TThDbc->clientEnv;
                    TThDbc->hDbc = TThDbc->clientDbc;
                    TThDbc->pFuncMap = TThDbc->pClientMap;
		    /* ensure any warnings are pushed onto the error stack */
		    if (  rc != SQL_SUCCESS  )
			populateConnErrorStack(TThDbc, 1, ENCODING_UTF16);
                }
                else
                if (  rc == SQL_ERROR  )
                {
                    sqlStateW[0] = 0;
                    getODBCConnectError(
		        TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW],
			TThDbc->clientDbc,
                        1,
                        (SQLCHAR *)sqlStateW, 
			&nativeError, 
			(SQLCHAR *)errorMsg);
		    translateUTF16toANSI((char *)sqlState,sqlStateW);
                    if (  (nativeError != 0) ||
                          ( ( strcmp((char *)sqlState,TT_DM_SQLSTATE_CONNERR25) &&
                              strcmp((char *)sqlState,TT_DM_SQLSTATE_CONNERR35) )  )  )
		    {
			/* a real error, push it onto our stack */
                        pushError(&(TThDbc->errorStack), rc,
				  ENCODING_UTF16,
                                  (SQLCHAR *)sqlStateW, nativeError,
                                  (SQLCHAR *)errorMsg,
	                          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN_W,
	                          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN_W,
	                          (SQLCHAR *)TThDbc->connNameW,
	                          (SQLCHAR *)TThDbc->serverNameW
                                  );
		        /* push any additional errors/warnings onto our stack */
			populateConnErrorStack(TThDbc, 1, ENCODING_UTF16);
                    }
                    else
                    {
			/* tried to use client driver to connect to a non-client DSN */
                        /* discard any extra errors/warnings on client handle */
                        clearODBCErrorStack(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLError],
                                            SQL_NULL_HENV,TThDbc->clientDbc);
                        /* ... then try direct driver */
                        rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHWND, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLUSMALLINT))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLDriverConnectW])))(TThDbc->directDbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
                        if (  (rc == SQL_SUCCESS) ||
                              (rc == SQL_SUCCESS_WITH_INFO)  )
                        {
                            TThDbc->hEnv = TThDbc->directEnv;
                            TThDbc->hDbc = TThDbc->directDbc;
                            TThDbc->pFuncMap = TThDbc->pDirectMap;
                        }
			/* ensure any errors or warnings are stacked */
			if (  rc != SQL_SUCCESS  )
			    populateConnErrorStack(TThDbc, 0, ENCODING_UTF16);
                    }
                }
            }
            else
            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
            {
                /* Just try direct */
                rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHWND, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLUSMALLINT))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLDriverConnectW])))(TThDbc->directDbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
                if (  (rc == SQL_SUCCESS) ||
                      (rc == SQL_SUCCESS_WITH_INFO)  )
                {
                    TThDbc->hEnv = TThDbc->directEnv;
                    TThDbc->hDbc = TThDbc->directDbc;
                    TThDbc->pFuncMap = TThDbc->pDirectMap;
                }
		/* ensure any errors or warnings are stacked */
		if (  rc != SQL_SUCCESS  )
		    populateConnErrorStack(TThDbc, 0, ENCODING_UTF16);
            }
            else
            {
                /* Just try client */
                rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLHWND, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLUSMALLINT))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLDriverConnectW])))(TThDbc->clientDbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
                if (  (rc == SQL_SUCCESS) ||
                      (rc == SQL_SUCCESS_WITH_INFO)  )
                {
                    TThDbc->hEnv = TThDbc->clientEnv;
                    TThDbc->hDbc = TThDbc->clientDbc;
                    TThDbc->pFuncMap = TThDbc->pClientMap;
                }
		/* ensure any errors or warnings are stacked */
		if (  rc != SQL_SUCCESS  )
		    populateConnErrorStack(TThDbc, 1, ENCODING_UTF16);
            }

	    MUTEX_UNLOCK(TThDbc->mutex);
        }
    }

    if (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
	populateConnectionData(TThDbc);

    TRACE_FLEAVE_RC("SQLDriverConnectW",rc);

    return rc;
}

/*
 * Because SQLEndTran can be applied at either the connection
 * or environment level (ugh!!!) this needs special handling.
 */
SQLRETURN SQL_API
SQLEndTran(
  SQLSMALLINT    HandleType,
  SQLHANDLE      Handle,
  SQLSMALLINT    CompletionType
)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLRETURN rc1 = SQL_SUCCESS;
    SQLRETURN rc2 = SQL_SUCCESS;
    TTSQLHENV TThEnv = NULL;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLEndTran");
    TRACE_HVAL("Handle",Handle);

    if (  (Handle == NULL) || 
          ((HandleType != SQL_HANDLE_ENV) && (HandleType != SQL_HANDLE_DBC)) )
	rc = SQL_INVALID_HANDLE;
    else
    if (  HandleType == SQL_HANDLE_DBC  )
    {
        TThDbc = (TTSQLHDBC)Handle;
        if (  (TThDbc->tag != TTDM_STRUCT_TAG) ||
	      (TThDbc->hid != TTDM_HID_DBC)  )
            rc = SQL_INVALID_HANDLE;
        else
	if (  TThDbc->hDbc == SQL_NULL_HDBC  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            CLEAR_ERROR_STACK(TThDbc);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NOTCONN,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLEndTran: Not connected",
		      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		      TThDbc->connName,
		      TThDbc->serverName
                 );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
    }
    else
    if (  HandleType == SQL_HANDLE_ENV  )
    {
        TThEnv = (TTSQLHENV)Handle;
        if (  (TThEnv->tag != TTDM_STRUCT_TAG) ||
	      (TThEnv->hid != TTDM_HID_ENV) ||
              ((TThEnv->directEnv == SQL_NULL_HENV) &&
               (TThEnv->clientEnv == SQL_NULL_HENV))  )
            rc = SQL_INVALID_HANDLE;
    }


    if ( rc == SQL_SUCCESS  )
    {
	if (  HandleType == SQL_HANDLE_DBC  )
        {
            CLEAR_ERROR_STACK_LOCK(TThDbc);
            rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLEndTran])))(HandleType, TThDbc->hDbc, CompletionType);
        }
        else
        {
            CLEAR_ERROR_STACK_LOCK(TThEnv);
            if (  TThEnv->directEnv != NULL  )
            {
                rc1 = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLEndTran])))(HandleType, TThEnv->directEnv, CompletionType);
#if defined(ENABLE_SQLENDTRAN_HENV_WORKAROUND)
                if (  (rc1 == SQL_ERROR) &&
                     isBogusError(rc1,TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLError],TThEnv->directEnv)  )
                    rc1 = SQL_SUCCESS;
#endif /* ENABLE_SQLENDTRAN_HENV_WORKAROUND */
            }
            if (  TThEnv->clientEnv != NULL  )
            {
                rc2 = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLEndTran])))(HandleType, TThEnv->clientEnv, CompletionType);
#if defined(ENABLE_SQLENDTRAN_HENV_WORKAROUND)
                if (  (rc2 == SQL_ERROR) &&
                     isBogusError(rc2,TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLError],TThEnv->clientEnv)  )
                    rc2 = SQL_SUCCESS;
#endif /* ENABLE_SQLENDTRAN_HENV_WORKAROUND */
            }

            rc = maxODBCError(rc1, rc2);
        }
    }
    
    TRACE_FLEAVE_RC("SQLEndTran",rc);

    return rc;
}

/*
 * SQLError - UTF16
 *
 * This needs lots of extra work due to the multiple error
 * stacks and encodings that need to be handled. Luckily
 * this function generally should not be in any performance
 * critial path.
 */
SQLRETURN  SQL_API
SQLErrorW(
  SQLHENV        henv,
  SQLHDBC        hdbc,
  SQLHSTMT       hstmt,
  SQLWCHAR     * szSqlState,
  SQLINTEGER   * pfNativeError,
  SQLWCHAR     * szErrorMsg,
  SQLSMALLINT    cbErrorMsgMax,
  SQLSMALLINT  * pcbErrorMsg
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  errRC = SQL_SUCCESS;
    SQLWCHAR   sqlState[TT_SQLSTATE_LEN+1];
    SQLINTEGER nativeError= 0;
    SQLCHAR    tErrorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];
    SQLWCHAR   errorMsg[SQL_MAX_MESSAGE_LENGTH+1];
    TTSQLHENV  TThEnv = NULL;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT TThStmt = NULL;
    SQLHENV    hEnv1 = SQL_NULL_HENV;
    SQLHENV    hEnv2 = SQL_NULL_HENV;
    SQLHDBC    hDbc1 = SQL_NULL_HDBC;
    SQLHDBC    hDbc2 = SQL_NULL_HDBC;
    SQLHSTMT   hStmt = SQL_NULL_HSTMT;
    void     * odbcFuncPtr1 = NULL;
    void     * odbcFuncPtr2 = NULL;
    int        errMsgLen = 0;
    int        encoding = 0;
    ODBC_ERR_STACK_t * errorStack = NULL;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t mutex = MUTEX_INITIALISER;
#endif /* ENABLE_THREAD_SAFETY */
    
    TRACE_FENTER("SQLErrorW");
    TRACE_HVAL("henv",henv);
    TRACE_HVAL("hdbc",hdbc);
    TRACE_HVAL("hstm",hstmt);

    TThEnv = (TTSQLHENV)henv;
    TThDbc = (TTSQLHDBC)hdbc;
    TThStmt = (TTSQLHSTMT)hstmt;
    if (  hstmt != SQL_NULL_HSTMT  ) /* want errors from hstmt */
    {
        if (  (TThStmt->tag != TTDM_STRUCT_TAG) ||
	      (TThStmt->hid != TTDM_HID_STMT)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(mutex = TThStmt->mutex);
            errorStack = &(TThStmt->errorStack);
            hStmt = TThStmt->hStmt;
            odbcFuncPtr1 = TThStmt->pFuncMap->
                           odbcFuncPtr[F_POS_ODBC_SQLErrorW];
        }
    }
    else
    if (  hdbc != SQL_NULL_HDBC  ) /* want errors from hdbc */
    {
        if (  (TThDbc->tag != TTDM_STRUCT_TAG) ||
	      (TThDbc->hid != TTDM_HID_DBC)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(mutex = TThDbc->mutex);
            errorStack = &(TThDbc->errorStack);
            if (  TThDbc->hDbc != SQL_NULL_HDBC  ) /* connected */
            {
                hDbc1 = TThDbc->hDbc;
                odbcFuncPtr1 = TThDbc->pFuncMap->
                               odbcFuncPtr[F_POS_ODBC_SQLErrorW];
            }
            else /* not connected */
            {
                if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                {
                    hDbc1 = TThDbc->clientDbc;
                    odbcFuncPtr1 = TThDbc->pClientMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLErrorW];
                    if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                    {
                        hDbc2 = TThDbc->directDbc;
                        odbcFuncPtr2 = TThDbc->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLErrorW];
                    }
                }
                else
                {
                    hDbc1 = TThDbc->directDbc;
                    odbcFuncPtr1 = TThDbc->pDirectMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLErrorW];
                }
            }
        }
    }
    else
    if (  henv != SQL_NULL_HENV  ) /* want errors from henv */
    {
        if (  (TThEnv->tag != TTDM_STRUCT_TAG) ||
	      (TThEnv->hid != TTDM_HID_ENV)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(mutex = TThEnv->mutex);
            errorStack = &(TThEnv->errorStack);
            if (  TThEnv->clientEnv != SQL_NULL_HENV  )
            {
                hEnv1 = TThEnv->clientEnv;
                odbcFuncPtr1 = TThEnv->pClientMap->
                               odbcFuncPtr[F_POS_ODBC_SQLErrorW];
                if (  TThEnv->directEnv != SQL_NULL_HENV  )
                {
                    hEnv2 = TThEnv->directEnv;
                    odbcFuncPtr2 = TThEnv->pDirectMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLErrorW];
                }
            }
            else
            {
                hEnv1 = TThEnv->directEnv;
                odbcFuncPtr1 = TThEnv->pDirectMap->
                               odbcFuncPtr[F_POS_ODBC_SQLErrorW];
            }
        }
    }
    else
    {
        errorStack = NULL;
        rc = SQL_INVALID_HANDLE;    
    }

    if (  rc == SQL_SUCCESS  )
    {
        rc = popError(errorStack, &errRC, &encoding, 
		      (SQLCHAR *)sqlState, &nativeError, 
		      tErrorMsg , NULL, NULL, NULL, NULL);
        if (  rc == SQL_SUCCESS  )
        {
	    if (  pfNativeError != NULL  )
                *pfNativeError = nativeError;
	    if (  encoding == ENCODING_UTF16  )
	    { /* can copy directly */
		if (  szSqlState != NULL  )
                    utf16cpy(szSqlState,sqlState);
		utf16cpy(errorMsg,(SQLWCHAR *)tErrorMsg);
	    }
	    else
	    { /* need to convert ANSI to UTF-16 */
		if (  szSqlState != NULL  )
		    translateANSItoUTF16(szSqlState,(char *)sqlState);
		translateANSItoUTF16(errorMsg,(char *)tErrorMsg);
            }
	    /* now copy error message to return buffer */
            errMsgLen = utf16len(errorMsg);
	    if (  pcbErrorMsg != NULL  )
                *pcbErrorMsg = (SQLSMALLINT)errMsgLen;
	    if (  szErrorMsg != NULL  )
	    {
                if (  errMsgLen < (int)cbErrorMsgMax  )
                    utf16cpy(szErrorMsg, errorMsg);
                else
                {
                    rc = SQL_SUCCESS_WITH_INFO;
                    if (  cbErrorMsgMax > 0  )
                    {
                        utf16ncpy(szErrorMsg, errorMsg, (cbErrorMsgMax-1));
                        szErrorMsg[cbErrorMsgMax-1] = 0;
                    }
                    else
                        szErrorMsg[0] = 0;
		}
            }
        }
        else
        {
            if (  hstmt != SQL_NULL_HSTMT  )
                rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLWCHAR *, SQLINTEGER *, SQLWCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr1)))
                                   (SQL_NULL_HENV,SQL_NULL_HDBC,hStmt,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
            else
            if (  hdbc != SQL_NULL_HDBC )
            {
                rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLWCHAR *, SQLINTEGER *, SQLWCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr1)))
                                   (SQL_NULL_HENV,hDbc1,SQL_NULL_HSTMT,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
                if (  (rc == SQL_NO_DATA_FOUND) &&
                      (odbcFuncPtr2 != NULL)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLWCHAR *, SQLINTEGER *, SQLWCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr2)))
                                   (SQL_NULL_HENV,hDbc2,SQL_NULL_HSTMT,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
            }
            else
            {
                rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLWCHAR *, SQLINTEGER *, SQLWCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr1)))
                                   (hEnv1,SQL_NULL_HDBC,SQL_NULL_HSTMT,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
                if (  (rc == SQL_NO_DATA_FOUND) &&
                      (odbcFuncPtr2 != NULL)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLHDBC, SQLHSTMT,
                                   SQLWCHAR *, SQLINTEGER *, SQLWCHAR *,
                                   SQLSMALLINT, SQLSMALLINT *))
                                   (odbcFuncPtr2)))
                                   (hEnv2,SQL_NULL_HDBC,SQL_NULL_HSTMT,
                                   szSqlState, pfNativeError, szErrorMsg,
                                   cbErrorMsgMax,pcbErrorMsg);
            }
        }
        MUTEX_UNLOCK(mutex);
    }

    TRACE_FLEAVE_RC("SQLErrorW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLExecDirectW(
    SQLHSTMT       hstmt,
    SQLWCHAR     * szSqlStr,
    SQLINTEGER     cbSqlStr
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLExecDirectW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLINTEGER))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLExecDirectW])))(TThStmt->hStmt,szSqlStr,cbSqlStr);
        }
    }

    TRACE_FLEAVE_RC("SQLExecDirectW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLForeignKeysW(
  SQLHSTMT        hstmt,
  SQLWCHAR      * szPkCatalogName,
  SQLSMALLINT     cbPkCatalogName,
  SQLWCHAR      * szPkSchemaName,
  SQLSMALLINT     cbPkSchemaName,
  SQLWCHAR      * szPkTableName,
  SQLSMALLINT     cbPkTableName,
  SQLWCHAR      * szFkCatalogName,
  SQLSMALLINT     cbFkCatalogName,
  SQLWCHAR      * szFkSchemaName,
  SQLSMALLINT     cbFkSchemaName,
  SQLWCHAR      * szFkTableName,
  SQLSMALLINT     cbFkTableName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLForeignKeysW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLForeignKeysW])))(TThStmt->hStmt, szPkCatalogName, cbPkCatalogName, szPkSchemaName, cbPkSchemaName, szPkTableName, cbPkTableName, szFkCatalogName, cbFkCatalogName, szFkSchemaName, cbFkSchemaName, szFkTableName, cbFkTableName);
        }
    }

    TRACE_FLEAVE_RC("SQLForeignKeysW",rc);

    return rc;
}

/*
 * If the requested option is one of the TTDM specific
 * options then we need to handle it here rather than
 * pass it to the driver. Also, if the HDBC is not in
 * a connected state then we need to handle that
 * properly too.
 */
SQLRETURN SQL_API
SQLGetConnectOptionW(
  SQLHDBC        hdbc,
  SQLUSMALLINT   fOption,
  SQLPOINTER     pvParam
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLGetConnectOptionW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            if (  fOption == TTDM_CONNECTION_TYPE  )
            {  /* we must handle this one! */
                if (  TThDbc->hDbc == NULL  )
                    *(SQLINTEGER *)pvParam = TTDM_CONN_NONE;
                else
                if (  TThDbc->hDbc == TThDbc->directDbc  )
                    *(SQLINTEGER *)pvParam = TTDM_CONN_DIRECT;
                else
                    *(SQLINTEGER *)pvParam = TTDM_CONN_CLIENT;
            }
            else
            if (  TThDbc->hDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLPOINTER))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectOptionW])))(TThDbc->hDbc, fOption, pvParam);
            else
            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLPOINTER))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectOptionW])))(TThDbc->directDbc, fOption, pvParam);
            else
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLPOINTER))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectOptionW])))(TThDbc->clientDbc, fOption, pvParam);
        }
    }

    TRACE_FLEAVE_RC("SQLGetConnectOptionW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLGetCursorNameW(
    SQLHSTMT       hstmt,
    SQLWCHAR     * szCursor,
    SQLSMALLINT    cbCursorMax,
    SQLSMALLINT*   pcbCursor
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLGetCursorNameW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetCursorNameW])))(TThStmt->hStmt,szCursor,cbCursorMax,pcbCursor);
        }
    }

    TRACE_FLEAVE_RC("SQLGetCursorNameW",rc);

    return rc;
}

/*
 * This is potentially complex as some options need additional
 * or special handling while others do not. See inline comments
 * for more details.
 */
SQLRETURN SQL_API
SQLGetInfoW(
  SQLHDBC        hdbc,
  SQLUSMALLINT   fInfoType,
  SQLPOINTER     rgbInfoValue,
  SQLSMALLINT    cbInfoValueMax,
  SQLSMALLINT  * pcbInfoValue
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLGetInfoW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
#if 0
	if (  cbInfoValueMax < 0  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_BADLEN35,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLGetInfoW: Buffer size negative",
		      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		      TThDbc->connName,
		      TThDbc->serverName
                 );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
#endif
        switch (fInfoType)
        {
            case SQL_DRIVER_HENV: 
            case SQL_DRIVER_HDBC: 
            case SQL_DRIVER_HSTMT: 
            case SQL_DRIVER_HLIB:
	        if (  rgbInfoValue == NULL  )
	        {
	            MUTEX_LOCK(TThDbc->mutex);
                    pushError(&(TThDbc->errorStack),
                              rc = SQL_ERROR,
		              ENCODING_ANSI,
                              (SQLCHAR *)TT_DM_SQLSTATE_INVATTR35,
                              0,
                              (SQLCHAR *)TTDMERRPFX "SQLGetInfoW: NULL pointer passed",
		              (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		              (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		              TThDbc->connName,
		              TThDbc->serverName
                         );
	            MUTEX_UNLOCK(TThDbc->mutex);
	        }
                break;
            default:
                break;
        }
        if (  rc == SQL_SUCCESS  )
        {
	    if (  TThDbc->hDbc == SQL_NULL_HDBC  )
	    {
	        MUTEX_LOCK(TThDbc->mutex);
                pushError(&(TThDbc->errorStack),
                          rc = SQL_ERROR,
		          ENCODING_ANSI,
                          (SQLCHAR *)TT_DM_SQLSTATE_NOTCONN,
                          0,
                          (SQLCHAR *)TTDMERRPFX "SQLGetInfoW: Not connected",
		          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
		          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
		          TThDbc->connName,
		          TThDbc->serverName
                     );
	        MUTEX_UNLOCK(TThDbc->mutex);
	    }
            else
            {
                /*
                ** If SQLGetInfo() is called specifying one of
                **   SQL_DRIVER_HENV
                **   SQL_DRIVER_HDBC
                ** then this needs special handling. In this case
                ** we simply return the real driver handle value
                ** without calling the 'real' SQLGetInfo().
                **
                ** In the case of a request for
                **   SQL_DRIVER_HLIB
                ** we need to return the proper handle from dlopen.
                **
                ** In the case of a request for one of
                **   SQL_DRIVER_HSTMT
                **   SQL_DRIVER_HDESC
                ** we still don't need to call the real SQLGetInfo but 
                ** some additional work and validation is needed to get 
                ** the real driver value.
                **
                ** In the case of a request for
                **   SQL_DM_VER
                ** we need to return our version info.
                */
                switch (fInfoType)
                {
                case SQL_DM_VER:
                    if (  ! TThDbc->v3  )
                    {
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
                                  ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_INVATTR25,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "Invalid attribute requested",
                                  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                                  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                                  TThDbc->connName,
                                  TThDbc->serverName
                                  );
                    }
                    else
                    if (  cbInfoValueMax % 2  )
                    {
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
                                  ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_BADLEN35,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "Invalid string or buffer length",
                                  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                                  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                                  TThDbc->connName,
                                  TThDbc->serverName
                                  );
                    }
                    else
                    {
                        int vlen = strlen(TTDMVERS);

                        if (  pcbInfoValue != NULL  )
                            *pcbInfoValue = (SQLSMALLINT)vlen;
                        if (  (rgbInfoValue != NULL) && (cbInfoValueMax > 0)  )
                        {
                            vlen += 1;
                            if (  cbInfoValueMax < vlen  )
                                vlen = (cbInfoValueMax - 2) / 2;
                            translateANSItoUTF16N( (SQLWCHAR *)rgbInfoValue, TTDMVERS, vlen );
                            ((SQLWCHAR *)rgbInfoValue)[vlen] = 0;
                        }
                    }
                    break;

                case SQL_DRIVER_HENV: 
                    if (TThDbc->hEnv != SQL_NULL_HENV)
                        *(SQLHENV *)rgbInfoValue = TThDbc->hEnv;
                    else 
                        rc = SQL_INVALID_HANDLE;
                    break;
    
                case SQL_DRIVER_HDBC:
                    *(SQLHDBC *)rgbInfoValue = TThDbc->hDbc;
                    break;
    
                case SQL_DRIVER_HLIB:
                    *(tt_libptr_t *)rgbInfoValue = TThDbc->pFuncMap->libHandle;
                    break;
    
                case SQL_DRIVER_HSTMT:
                    TThStmt = *(TTSQLHSTMT*)(rgbInfoValue);
                    if (  (TThStmt == NULL) ||
                          (TThStmt->tag != TTDM_STRUCT_TAG) ||
	                  (TThStmt->hid != TTDM_HID_STMT) ||
                          (TThStmt->hStmt == SQL_NULL_HSTMT)  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
			          ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_INVATTR35,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfoW: "
                                             "Invalid value passed for SQLHSTMT value",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          TThDbc->connName,
			          TThDbc->serverName
                                  );
                    else
                    if (  TThStmt->hDbc != TThDbc->hDbc  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
			          ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_INVATTR35,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfoW: "
                                             "SQLHSTMT does not belong to the passed SQLHDBC",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          TThDbc->connName,
			          TThDbc->serverName
                                  );
                    else
                    *(SQLHSTMT *)rgbInfoValue = TThStmt->hStmt;
                    break;
    
                case SQL_DRIVER_HDESC:
                    TThDesc = *(TTSQLHDESC*)(rgbInfoValue);
                    if (  (TThDesc == NULL) ||
                          (TThDesc->tag != TTDM_STRUCT_TAG) ||
                          (TThDesc->hid != TTDM_HID_DESC) ||
                          (TThDesc->hDesc == SQL_NULL_HDESC)  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
                                  ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_INVATTR35,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "Invalid value passed for SQLHDESC value",
                                  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                                  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                                  TThDbc->connName,
                                  TThDbc->serverName
                                  );
                    else
                    if (  TThDesc->hDbc != TThDbc->hDbc  )
                        pushError(&(TThDbc->errorStack),
                                  rc = SQL_ERROR,
                                  ENCODING_ANSI,
                                  (SQLCHAR *)TT_DM_SQLSTATE_INVATTR35,
                                  tt_ErrDMInvalidArg,
                                  (SQLCHAR *)TTDMERRPFX "SQLGetInfo: "
                                             "SQLHDESC does not belong to the passed SQLHDBC",
                                  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                                  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                                  TThDbc->connName,
                                  TThDbc->serverName
                                  );
                    else
                    *(SQLHDESC *)rgbInfoValue = TThDesc->hDesc;
                    break;

                default:
                   rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetInfoW])))(TThDbc->hDbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue);
                    break;
                }
            }
        }
    }

    TRACE_FLEAVE_RC("SQLGetInfoW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLGetTypeInfoW(
  SQLHSTMT       hstmt,
  SQLSMALLINT    fSqlType
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLGetTypeInfoW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetTypeInfoW])))(TThStmt->hStmt, fSqlType);
        }
    }

    TRACE_FLEAVE_RC("SQLGetTypeInfoW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLNativeSqlW(
  SQLHDBC         hdbc,
  SQLWCHAR      * szSqlStrIn,
  SQLINTEGER      cbSqlStrIn,
  SQLWCHAR      * szSqlStr,
  SQLINTEGER      cbSqlStrMax,
  SQLINTEGER    * pcbSqlStr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLNativeSqlW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
        {
            MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
                      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NOTCONN,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLNativeSql: Not connected",
                      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
                      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
                      TThDbc->connName,
                      TThDbc->serverName
                 );
            MUTEX_UNLOCK(TThDbc->mutex);
        }
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLWCHAR *, SQLINTEGER, SQLWCHAR *, SQLINTEGER, SQLINTEGER *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLNativeSqlW])))(TThDbc->hDbc, szSqlStrIn, cbSqlStrIn, szSqlStr, cbSqlStrMax, pcbSqlStr);
        }
    }

    TRACE_FLEAVE_RC("SQLNativeSqlW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLPrepareW(
    SQLHSTMT       hstmt,
    SQLWCHAR     * szSqlStr,
    SQLINTEGER     cbSqlStr
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLPrepareW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLINTEGER))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLPrepareW])))(TThStmt->hStmt,szSqlStr,cbSqlStr);
        }
    }

    TRACE_FLEAVE_RC("SQLPrepareW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLPrimaryKeysW(
  SQLHSTMT        hstmt,
  SQLWCHAR      * szCatalogName,
  SQLSMALLINT     cbCatalogName,
  SQLWCHAR      * szSchemaName,
  SQLSMALLINT     cbSchemaName,
  SQLWCHAR      * szTableName,
  SQLSMALLINT     cbTableName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLPrimaryKeysW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLPrimaryKeysW])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName);
        }
    }

    TRACE_FLEAVE_RC("SQLPrimaryKeysW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLProcedureColumnsW(
  SQLHSTMT        hstmt,
  SQLWCHAR      * szCatalogName,
  SQLSMALLINT     cbCatalogName,
  SQLWCHAR      * szSchemaName,
  SQLSMALLINT     cbSchemaName,
  SQLWCHAR      * szProcName,
  SQLSMALLINT     cbProcName,
  SQLWCHAR      * szColumnName,
  SQLSMALLINT     cbColumnName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLProcedureColumnsW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLProcedureColumnsW])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szProcName, cbProcName, szColumnName, cbColumnName);
        }
    }

    TRACE_FLEAVE_RC("SQLProcedureColumnsW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLProceduresW(
  SQLHSTMT        hstmt,
  SQLWCHAR      * szCatalogName,
  SQLSMALLINT     cbCatalogName,
  SQLWCHAR      * szSchemaName,
  SQLSMALLINT     cbSchemaName,
  SQLWCHAR      * szProcName,
  SQLSMALLINT     cbProcName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLProceduresW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLProceduresW])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szProcName, cbProcName);
        }
    }

    TRACE_FLEAVE_RC("SQLProceduresW",rc);

    return rc;
}

/*
 * Need to handle the case where the HDBC is not yet connected.
 */
SQLRETURN SQL_API
SQLSetConnectOptionW(
  SQLHDBC        hdbc,
  SQLUSMALLINT   fOption,
  SQLULEN        pvParam
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  rc1 = SQL_SUCCESS;
    SQLRETURN  rc2 = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLSetConnectOptionW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            if (  TThDbc->hDbc != SQL_NULL_HDBC  )
                rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLULEN))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectOptionW])))(TThDbc->hDbc, fOption, pvParam);
            else
            {
                if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                    rc1 = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLULEN))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectOptionW])))(TThDbc->directDbc, fOption, pvParam);
                if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                    rc2 = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLUSMALLINT, SQLULEN))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectOptionW])))(TThDbc->clientDbc, fOption, pvParam);
                rc = maxODBCError(rc1, rc2);
            }
        }
    }

    TRACE_FLEAVE_RC("SQLSetConnectOptionW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLSetCursorNameW(
    SQLHSTMT       hstmt,
    SQLWCHAR     * szCursor,
    SQLSMALLINT    cbCursor
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSetCursorNameW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetCursorNameW])))(TThStmt->hStmt,szCursor,cbCursor);
        }
    }

    TRACE_FLEAVE_RC("SQLCursorNameW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLSpecialColumnsW(
  SQLHSTMT       hstmt,
  SQLUSMALLINT   fColType,
  SQLWCHAR     * szCatalogName,
  SQLSMALLINT    cbCatalogName,
  SQLWCHAR     * szSchemaName,
  SQLSMALLINT    cbSchemaName,
  SQLWCHAR     * szTableName,
  SQLSMALLINT    cbTableName,
  SQLUSMALLINT   fScope,
  SQLUSMALLINT   fNullable
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSpecialColumnsW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLUSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLUSMALLINT, SQLUSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSpecialColumnsW])))(TThStmt->hStmt, fColType, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, fScope, fNullable);
        }
    }

    TRACE_FLEAVE_RC("SQLSpecialColumnsW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLStatisticsW(
  SQLHSTMT       hstmt,
  SQLWCHAR     * szCatalogName,
  SQLSMALLINT    cbCatalogName,
  SQLWCHAR     * szSchemaName,
  SQLSMALLINT    cbSchemaName,
  SQLWCHAR     * szTableName,
  SQLSMALLINT    cbTableName,
  SQLUSMALLINT   fUnique,
  SQLUSMALLINT   fAccuracy
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLStatisticsW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLUSMALLINT, SQLUSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLStatisticsW])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, fUnique, fAccuracy);
        }
    }

    TRACE_FLEAVE_RC("SQLStatisticsW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLTablePrivilegesW(
  SQLHSTMT        hstmt,
  SQLWCHAR      * szTableQualifier,
  SQLSMALLINT     cbTableQualifier,
  SQLWCHAR      * szTableOwner,
  SQLSMALLINT     cbTableOwner,
  SQLWCHAR      * szTableName,
  SQLSMALLINT     cbTableName
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLTablePrivilegesW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLTablePrivilegesW])))(TThStmt->hStmt, szTableQualifier, cbTableQualifier, szTableOwner, cbTableOwner, szTableName, cbTableName);
        }
    }

    TRACE_FLEAVE_RC("SQLTablePrivilegesW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLTablesW(
  SQLHSTMT       hstmt,
  SQLWCHAR     * szCatalogName,
  SQLSMALLINT    cbCatalogName,
  SQLWCHAR     * szSchemaName,
  SQLSMALLINT    cbSchemaName,
  SQLWCHAR     * szTableName,
  SQLSMALLINT    cbTableName,
  SQLWCHAR     * szTableType,
  SQLSMALLINT    cbTableType
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLTablesW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLTablesW])))(TThStmt->hStmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, szTableType, cbTableType);
        }
    }

    TRACE_FLEAVE_RC("SQLTablesW",rc);

    return rc;
}

/*
 * This is really 'SQLConnectW' but if we expose that then users will
 * think that we are a fully Unicode capable driver (which we are
 * not - the underlying drivers are not Unicode capable). Normally
 * applications should use SQLDriverConnectW. This function takes
 * the same approach as SQLConnect and delegates the heavy lifting
 * to SQLDriverConnectW.
 */
SQLRETURN SQL_API
ttSQLConnectW(
    SQLHDBC             hdbc, 
    SQLWCHAR          * szDSN,
    SQLSMALLINT         cbDSN,
    SQLWCHAR          * szUID,
    SQLSMALLINT         cbUID,
    SQLWCHAR          * szAuthStr,
    SQLSMALLINT         cbAuthStr
)
{
/* Some UTF-16 constants */
static SQLWCHAR u16DSN[] = { 68, 83, 78, 61, 0 };      /* "DSN=" */
static SQLWCHAR u16UID[] = { 59, 85, 73, 68, 61, 0 };  /* ";UID=" */
static SQLWCHAR u16PWD[] = { 59, 80, 87, 68, 61, 0 };  /* ";PWD=" */
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;
    SQLWCHAR  * cspos = NULL;
    SQLWCHAR  * connStr = NULL;

    TRACE_FENTER("ttSQLConnectW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
	else
	if (  TThDbc->hDbc != SQL_NULL_HDBC  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_CONNUSED,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLConnectW: Already connected",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      TThDbc->connName,
			      TThDbc->serverName
                              );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
	if (  (szDSN == NULL) ||
              ((cbDSN <= 0) && (cbDSN != SQL_NTS)) ||
	      ((szDSN != NULL) && (*szDSN == 0))  )
	{
	    MUTEX_LOCK(TThDbc->mutex);
            pushError(&(TThDbc->errorStack),
                      rc = SQL_ERROR,
		      ENCODING_ANSI,
                      (SQLCHAR *)TT_DM_SQLSTATE_NODSN,
                      0,
                      (SQLCHAR *)TTDMERRPFX "SQLConnectW: No DSN specified",
			      (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			      (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			      TThDbc->connName,
			      TThDbc->serverName
                              );
	    MUTEX_UNLOCK(TThDbc->mutex);
	}
        else
        {
            if (  cbDSN == SQL_NTS  )
                cbDSN = (SQLSMALLINT)utf16len(szDSN);
	    if (  szUID == NULL  )
		cbUID = 0;
	    else
            if (  cbUID < 0  )
                cbUID = (SQLSMALLINT)utf16len(szUID);
	    if (  szAuthStr == NULL  )
		cbAuthStr = 0;
	    else
            if (  cbAuthStr < 0  )
                cbAuthStr = (SQLSMALLINT)utf16len(szAuthStr);
	    connStr = (SQLWCHAR *)calloc(1,2*(20+cbDSN+cbUID+cbAuthStr));
	    if (  connStr == NULL  )
	    {
	        MUTEX_LOCK(TThDbc->mutex);
                pushError(&(TThDbc->errorStack),
                          rc = SQL_ERROR,
		          ENCODING_ANSI,
                          (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                          0,
                          (SQLCHAR *)TTDMERRPFX "SQLConnectW: Memory allocation failed",
			          (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
			          (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
			          TThDbc->connName,
			          TThDbc->serverName
                                  );
	        MUTEX_UNLOCK(TThDbc->mutex);
	    }
	    else
	    {
	        /* Build the connection string */
	        cspos = connStr;

	        utf16cpy(cspos, u16DSN);
	        cspos += utf16len(u16DSN);
	        utf16ncpy(cspos, szDSN, cbDSN);
	        cspos += cbDSN;

		if (  cbUID > 0  )
		{
	            utf16cpy(cspos, u16UID);
	            cspos += utf16len(u16UID);
	            utf16ncpy(cspos, szUID, cbUID);
	            cspos += cbUID;
		}

		if (  cbAuthStr > 0  )
		{
	            utf16cpy(cspos, u16PWD);
	            cspos += utf16len(u16PWD);
	            utf16ncpy(cspos, szAuthStr, cbAuthStr);
	            cspos += cbAuthStr;
		}
                *cspos = 0;

                rc = SQLDriverConnectW(hdbc, NULL, connStr, 
				       (SQLSMALLINT)SQL_NTS,
                                       (SQLWCHAR *)NULL, (SQLSMALLINT)0, NULL,
                                       SQL_DRIVER_NOPROMPT);

	        free( (void *)connStr);
	    }
        }
    }

    TRACE_FLEAVE_RC("ttSQLConnectW",rc);

    return rc;
}

#if defined(EXPORT_SQLCONNECTW)
/*
 * This is the real 'SQLConnectW' which is mapped to ttSQLConnectW.
 */
SQLRETURN SQL_API
SQLConnectW(
    SQLHDBC             hdbc,
    SQLWCHAR          * szDSN,
    SQLSMALLINT         cbDSN,
    SQLWCHAR          * szUID,
    SQLSMALLINT         cbUID,
    SQLWCHAR          * szAuthStr,
    SQLSMALLINT         cbAuthStr
)
{
    return ttSQLConnectW( hdbc, szDSN, cbDSN,
                          szUID, cbUID, szAuthStr, cbAuthStr );
}
#endif /* EXPORT_SQLCONNECTW */

SQLRETURN SQL_API
SQLAllocHandle(
      SQLSMALLINT   HandleType,
      SQLHANDLE     InputHandle,
      SQLHANDLE *   OutputHandlePtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;

    TRACE_FENTER("SQLAllocHandle");
    TRACE_HVAL("InputHandle",InputHandle);

    switch (  HandleType  )
    {
        case SQL_HANDLE_ENV:
	    if (  InputHandle != SQL_NULL_HANDLE  )
                rc = SQL_INVALID_HANDLE;
	    else
	        rc = sqlAllocEnv((SQLHENV)OutputHandlePtr, 1);
	    break;
	case SQL_HANDLE_DBC:
	    rc = sqlAllocConnect((SQLHENV)InputHandle, 
			         (SQLHDBC *)OutputHandlePtr, 1);
	    break;
	case SQL_HANDLE_STMT:
	    rc = sqlAllocStmt((SQLHDBC)InputHandle, 
			      (SQLHSTMT *)OutputHandlePtr, 1);
	    break;
	case SQL_HANDLE_DESC:
	    rc = sqlAllocDesc((SQLHDBC)InputHandle, 0,
			      (SQLHDESC *)OutputHandlePtr);
	    break;
	default:
	    rc = SQL_INVALID_HANDLE;
	    break;
    }

    TRACE_FLEAVE_RC("SQLAllocHandle",rc);

    return rc;
}

SQLRETURN SQL_API
SQLFreeHandle(
      SQLSMALLINT   HandleType,
      SQLHANDLE     Handle
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;

    TRACE_FENTER("SQLFreeHandle");
    TRACE_HVAL("Handle",Handle);

    if (  Handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    switch (  HandleType  )
    {
        case SQL_HANDLE_ENV:
	        rc = sqlFreeEnv((SQLHENV)Handle, 1);
	    break;
	case SQL_HANDLE_DBC:
	    rc = sqlFreeConnect((SQLHDBC)Handle, 1);
	    break;
	case SQL_HANDLE_STMT:
	    rc = sqlFreeStmt((SQLHSTMT)Handle, SQL_DROP, 1);
	    break;
	case SQL_HANDLE_DESC:
	    rc = sqlFreeDesc((SQLHDESC)Handle);
	    break;
	default:
	    rc = SQL_INVALID_HANDLE;
	    break;
    }

    TRACE_FLEAVE_RC("SQLFreeHandle",rc);

    return rc;
}

SQLRETURN SQL_API
SQLFetchScroll(
    SQLHSTMT       hstmt,
    SQLSMALLINT    fOrientation,
    SQLLEN         fOffset
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLFetchScroll");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLSMALLINT, SQLLEN))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLFetchScroll])))(TThStmt->hStmt,fOrientation,fOffset);
        }
    }

    TRACE_FLEAVE_RC("SQLFetchScroll",rc);

    return rc;
}

/*
 * Need to handle the 'not connected' case.
 */
SQLRETURN SQL_API
SQLGetConnectAttr(
  SQLHDBC        hdbc,
  SQLINTEGER     Attribute,
  SQLPOINTER     ValuePtr,
  SQLINTEGER     BufferLength,
  SQLINTEGER *   StringLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLGetConnectAttr");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            if (  Attribute == TTDM_CONNECTION_TYPE  )
            {  /* we must handle this one! */
                if (  TThDbc->hDbc == NULL  )
                    *(SQLINTEGER *)ValuePtr = (SQLINTEGER)TTDM_CONN_NONE;
                else
                if (  TThDbc->hDbc == TThDbc->directDbc  )
                    *(SQLINTEGER *)ValuePtr = (SQLINTEGER)TTDM_CONN_DIRECT;
                else
                    *(SQLINTEGER *)ValuePtr = (SQLINTEGER)TTDM_CONN_CLIENT;
            }
            else
            if (  TThDbc->hDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectAttr])))(TThDbc->hDbc, Attribute, ValuePtr, BufferLength, StringLengthPtr);
            else
            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectAttr])))(TThDbc->directDbc, Attribute, ValuePtr, BufferLength, StringLengthPtr);
            else
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectAttr])))(TThDbc->clientDbc, Attribute, ValuePtr, BufferLength, StringLengthPtr);
        }
    }

    TRACE_FLEAVE_RC("SQLGetConnectAttr",rc);

    return rc;
}

/*
 * Need to handle the 'not connected' case.
 */
SQLRETURN SQL_API
SQLGetConnectAttrW(
  SQLHDBC        hdbc,
  SQLINTEGER     Attribute,
  SQLPOINTER     ValuePtr,
  SQLINTEGER     BufferLength,
  SQLINTEGER *   StringLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLGetConnectAttrW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            if (  Attribute == TTDM_CONNECTION_TYPE  )
            {  /* we must handle this one! */
                if (  TThDbc->hDbc == NULL  )
                    *(SQLINTEGER *)ValuePtr = (SQLINTEGER)TTDM_CONN_NONE;
                else
                if (  TThDbc->hDbc == TThDbc->directDbc  )
                    *(SQLINTEGER *)ValuePtr = (SQLINTEGER)TTDM_CONN_DIRECT;
                else
                    *(SQLINTEGER *)ValuePtr = (SQLINTEGER)TTDM_CONN_CLIENT;
            }
            else
            if (  TThDbc->hDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectAttrW])))(TThDbc->hDbc, Attribute, ValuePtr, BufferLength, StringLengthPtr);
            else
            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectAttrW])))(TThDbc->directDbc, Attribute, ValuePtr, BufferLength, StringLengthPtr);
            else
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLGetConnectAttrW])))(TThDbc->clientDbc, Attribute, ValuePtr, BufferLength, StringLengthPtr);
        }
    }

    TRACE_FLEAVE_RC("SQLGetConnectAttrW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLGetDescField(
     SQLHDESC        hdesc,
     SQLSMALLINT     RecNumber,
     SQLSMALLINT     FieldIdentifier,
     SQLPOINTER      ValuePtr,
     SQLINTEGER      BufferLength,
     SQLINTEGER *    StringLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLGetDescField");
    TRACE_HVAL("hdesc",hdesc);

    rc = CLEAR_ERROR_STACK_LOCK(hdesc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDesc = (TTSQLHDESC)hdesc;
        if (  TThDesc->hDesc == SQL_NULL_HDESC  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLSMALLINT, SQLSMALLINT, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetDescField])))(TThDesc->hDesc, RecNumber, FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr);

	}
    }

    TRACE_FLEAVE_RC("SQLGetDescField",rc);

    return rc;
}

SQLRETURN SQL_API
SQLGetDescFieldW(
     SQLHDESC        hdesc,
     SQLSMALLINT     RecNumber,
     SQLSMALLINT     FieldIdentifier,
     SQLPOINTER      ValuePtr,
     SQLINTEGER      BufferLength,
     SQLINTEGER *    StringLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLGetDescFieldW");
    TRACE_HVAL("hdesc",hdesc);
    rc = CLEAR_ERROR_STACK_LOCK(hdesc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDesc = (TTSQLHDESC)hdesc;
        if (  TThDesc->hDesc == SQL_NULL_HDESC  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLSMALLINT, SQLSMALLINT, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetDescFieldW])))(TThDesc->hDesc, RecNumber, FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr);

	}
    }

    TRACE_FLEAVE_RC("SQLGetDescFieldW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLGetDescRec(
     SQLHDESC        hdesc,
     SQLSMALLINT     RecNumber,
     SQLCHAR *       Name,
     SQLSMALLINT     BufferLength,
     SQLSMALLINT *   StringLengthPtr,
     SQLSMALLINT *   TypePtr,
     SQLSMALLINT *   SubTypePtr,
     SQLLEN *        LengthPtr,
     SQLSMALLINT *   PrecisionPtr,
     SQLSMALLINT *   ScalePtr,
     SQLSMALLINT *   NullablePtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLGetDescRec");
    TRACE_HVAL("hdesc",hdesc);

    rc = CLEAR_ERROR_STACK_LOCK(hdesc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDesc = (TTSQLHDESC)hdesc;
        if (  TThDesc->hDesc == SQL_NULL_HDESC  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDESC, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLSMALLINT *, SQLSMALLINT *, SQLLEN *, SQLSMALLINT *, SQLSMALLINT *, SQLSMALLINT *))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetDescRec])))(TThDesc->hDesc, RecNumber, Name, BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, PrecisionPtr, ScalePtr, NullablePtr);
	}
    }

    TRACE_FLEAVE_RC("SQLGetDescRec",rc);

    return rc;
}

SQLRETURN SQL_API
SQLGetDescRecW(
     SQLHDESC        hdesc,
     SQLSMALLINT     RecNumber,
     SQLWCHAR *      Name,
     SQLSMALLINT     BufferLength,
     SQLSMALLINT *   StringLengthPtr,
     SQLSMALLINT *   TypePtr,
     SQLSMALLINT *   SubTypePtr,
     SQLLEN *        LengthPtr,
     SQLSMALLINT *   PrecisionPtr,
     SQLSMALLINT *   ScalePtr,
     SQLSMALLINT *   NullablePtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLGetDescRecW");
    TRACE_HVAL("hdesc",hdesc);

    rc = CLEAR_ERROR_STACK_LOCK(hdesc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDesc = (TTSQLHDESC)hdesc;
        if (  TThDesc->hDesc == SQL_NULL_HDESC  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDESC, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLSMALLINT *, SQLSMALLINT *, SQLLEN *, SQLSMALLINT *, SQLSMALLINT *, SQLSMALLINT *))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetDescRecW])))(TThDesc->hDesc, RecNumber, Name, BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, PrecisionPtr, ScalePtr, NullablePtr);
	}
    }

    TRACE_FLEAVE_RC("SQLGetDescRecW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLSetDescRec(
      SQLHDESC      hdesc,
      SQLSMALLINT   RecNumber,
      SQLSMALLINT   Type,
      SQLSMALLINT   SubType,
      SQLLEN        Length,
      SQLSMALLINT   Precision,
      SQLSMALLINT   Scale,
      SQLPOINTER    DataPtr,
      SQLLEN *      StringLengthPtr,
      SQLLEN *      IndicatorPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLSetDescRec");
    TRACE_HVAL("hdesc",hdesc);

    rc = CLEAR_ERROR_STACK_LOCK(hdesc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDesc = (TTSQLHDESC)hdesc;
        if (  TThDesc->hDesc == SQL_NULL_HDESC  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDESC, SQLSMALLINT, SQLSMALLINT, SQLSMALLINT, SQLLEN, SQLSMALLINT, SQLSMALLINT, SQLPOINTER, SQLLEN *, SQLLEN *))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetDescRec])))(TThDesc->hDesc, RecNumber, Type, SubType, Length, Precision, Scale, DataPtr, StringLengthPtr, IndicatorPtr);
	}
    }

    TRACE_FLEAVE_RC("SQLSetDescRec",rc);

    return rc;
}

/*
 * Need to handle the 'not connected' case.
 */
SQLRETURN SQL_API
SQLSetConnectAttr(
  SQLHDBC        hdbc,
  SQLINTEGER     Attribute,
  SQLPOINTER     ValuePtr,
  SQLINTEGER     StringLength
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  rc1 = SQL_SUCCESS;
    SQLRETURN  rc2 = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLSetConnectAttr");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            if (  TThDbc->hDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectAttr])))(TThDbc->hDbc, Attribute, ValuePtr, StringLength);
            else
            {
                if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                     rc1 = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectAttr])))(TThDbc->directDbc, Attribute, ValuePtr, StringLength);
                if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                     rc2 = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectAttr])))(TThDbc->clientDbc, Attribute, ValuePtr, StringLength);
                rc = maxODBCError(rc1, rc2);
            }
        }
    }

    TRACE_FLEAVE_RC("SQLSetConnectAttr",rc);

    return rc;
}

/*
 * Need to handle the 'not connected' case.
 */
SQLRETURN SQL_API
SQLSetConnectAttrW(
  SQLHDBC        hdbc,
  SQLINTEGER     Attribute,
  SQLPOINTER     ValuePtr,
  SQLINTEGER     StringLength
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  rc1 = SQL_SUCCESS;
    SQLRETURN  rc2 = SQL_SUCCESS;
    TTSQLHDBC TThDbc = NULL;

    TRACE_FENTER("SQLSetConnectAttrW");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  ((TThDbc->directDbc == SQL_NULL_HDBC) &&
               (TThDbc->clientDbc == SQL_NULL_HDBC))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            if (  TThDbc->hDbc != SQL_NULL_HDBC  )
                 rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectAttrW])))(TThDbc->hDbc, Attribute, ValuePtr, StringLength);
            else
            {
                if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                     rc1 = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThDbc->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectAttrW])))(TThDbc->directDbc, Attribute, ValuePtr, StringLength);
                if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                     rc2 = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThDbc->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLSetConnectAttrW])))(TThDbc->clientDbc, Attribute, ValuePtr, StringLength);
                rc = maxODBCError(rc1, rc2);
            }
        }
    }

    TRACE_FLEAVE_RC("SQLSetConnectAttrW",rc);

    return rc;
}

SQLRETURN SQL_API
SQLSetDescField(
     SQLHDESC        hdesc,
     SQLSMALLINT     RecNumber,
     SQLSMALLINT     FieldIdentifier,
     SQLPOINTER      ValuePtr,
     SQLINTEGER      BufferLength
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLSetDescField");
    TRACE_HVAL("hdesc",hdesc);

    rc = CLEAR_ERROR_STACK_LOCK(hdesc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDesc = (TTSQLHDESC)hdesc;
        if (  TThDesc->hDesc == SQL_NULL_HDESC  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLSMALLINT, SQLSMALLINT, SQLPOINTER, SQLINTEGER))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetDescField])))(TThDesc->hDesc, RecNumber, FieldIdentifier, ValuePtr, BufferLength);

	}
    }

    TRACE_FLEAVE_RC("SQLSetDescField",rc);

    return rc;
}

SQLRETURN SQL_API
SQLSetDescFieldW(
     SQLHDESC        hdesc,
     SQLSMALLINT     RecNumber,
     SQLSMALLINT     FieldIdentifier,
     SQLPOINTER      ValuePtr,
     SQLINTEGER      BufferLength
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHDESC TThDesc = NULL;

    TRACE_FENTER("SQLSetDescFieldW");
    TRACE_HVAL("hdesc",hdesc);

    rc = CLEAR_ERROR_STACK_LOCK(hdesc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDesc = (TTSQLHDESC)hdesc;
        if (  TThDesc->hDesc == SQL_NULL_HDESC  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, SQLSMALLINT, SQLSMALLINT, SQLPOINTER, SQLINTEGER))(TThDesc->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetDescFieldW])))(TThDesc->hDesc, RecNumber, FieldIdentifier, ValuePtr, BufferLength);

	}
    }

    TRACE_FLEAVE_RC("SQLSetDescFieldW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLGetStmtAttr(
  SQLHSTMT        hstmt,
  SQLINTEGER      Attribute,
  SQLPOINTER      ValuePtr,
  SQLINTEGER      BufferLength,
  SQLINTEGER *    StringLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDESC TThDesc = NULL;
    SQLHDESC   hdesc = SQL_NULL_HDESC;

    TRACE_FENTER("SQLGetStmtAttr");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetStmtAttr])))(TThStmt->hStmt, Attribute, ValuePtr, BufferLength, StringLengthPtr);
            if (  rc == SQL_SUCCESS  )
            {
                switch (  Attribute  )
                {
                    case SQL_ATTR_IMP_PARAM_DESC:
                        rc = sqlAllocDesc( TThStmt->TThDbc, 1, &hdesc );
                        if ( rc == SQL_SUCCESS )
                        {
                            TThDesc = (TTSQLHDESC)hdesc;
                            TThDesc->hDesc = TThStmt->IPD;
                            *((SQLHDESC *)(ValuePtr)) = (SQLHDESC)TThDesc;
                        }
                        else
                        {
                            copyErrorStack( &(TThStmt->TThDbc->errorStack),
                                            &(TThStmt->errorStack), 0);
                        }
                        break;
                    case SQL_ATTR_IMP_ROW_DESC:
                        rc = sqlAllocDesc( TThStmt->TThDbc, 1, &hdesc );
                        if ( rc == SQL_SUCCESS )
                        {
                            TThDesc = (TTSQLHDESC)hdesc;
                            TThDesc->hDesc = TThStmt->IRD;
                            *((SQLHDESC *)(ValuePtr)) = (SQLHDESC)TThDesc;
                        }
                        else
                        {
                            copyErrorStack( &(TThStmt->TThDbc->errorStack),
                                            &(TThStmt->errorStack), 0);
                        }
                        break;
                    case SQL_ATTR_APP_PARAM_DESC:
                    case SQL_ATTR_APP_ROW_DESC:
                        rc = sqlAllocDesc( TThStmt->TThDbc, 1, &hdesc );
                        if ( rc == SQL_SUCCESS )
                        {
                            TThDesc = (TTSQLHDESC)hdesc;
                            TThDesc->hDesc = *((SQLHDESC *)(ValuePtr));
                            *((SQLHDESC *)(ValuePtr)) = (SQLHDESC)TThDesc;
                        }
                        else
                        {
                            copyErrorStack( &(TThStmt->TThDbc->errorStack),
                                            &(TThStmt->errorStack), 0);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    TRACE_FLEAVE_RC("SQLGetStmtAttr",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLGetStmtAttrW(
  SQLHSTMT        hstmt,
  SQLINTEGER      Attribute,
  SQLPOINTER      ValuePtr,
  SQLINTEGER      BufferLength,
  SQLINTEGER *    StringLengthPtr
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDESC TThDesc = NULL;
    SQLHDESC   hdesc = SQL_NULL_HDESC;

    TRACE_FENTER("SQLGetStmtAttrW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLGetStmtAttrW])))(TThStmt->hStmt, Attribute, ValuePtr, BufferLength, StringLengthPtr);
            if (  rc == SQL_SUCCESS  )
            {
                switch (  Attribute  )
                {
                    case SQL_ATTR_IMP_PARAM_DESC:
                        rc = sqlAllocDesc( TThStmt->TThDbc, 1, &hdesc );
                        if ( rc == SQL_SUCCESS )
                        {
                            TThDesc = (TTSQLHDESC)hdesc;
                            TThDesc->hDesc = TThStmt->IPD;
                            *((SQLHDESC *)(ValuePtr)) = (SQLHDESC)TThDesc;
                        }
                        else
                        {
                            copyErrorStack( &(TThStmt->TThDbc->errorStack),
                                            &(TThStmt->errorStack), 0);
                        }
                        break;
                    case SQL_ATTR_IMP_ROW_DESC:
                        rc = sqlAllocDesc( TThStmt->TThDbc, 1, &hdesc );
                        if ( rc == SQL_SUCCESS )
                        {
                            TThDesc = (TTSQLHDESC)hdesc;
                            TThDesc->hDesc = TThStmt->IRD;
                            *((SQLHDESC *)(ValuePtr)) = (SQLHDESC)TThDesc;
                        }
                        else
                        {
                            copyErrorStack( &(TThStmt->TThDbc->errorStack),
                                            &(TThStmt->errorStack), 0);
                        }
                        break;
                    case SQL_ATTR_APP_PARAM_DESC:
                    case SQL_ATTR_APP_ROW_DESC:
                        rc = sqlAllocDesc( TThStmt->TThDbc, 1, &hdesc );
                        if ( rc == SQL_SUCCESS )
                        {
                            TThDesc = (TTSQLHDESC)hdesc;
                            TThDesc->hDesc = *((SQLHDESC *)(ValuePtr));
                            *((SQLHDESC *)(ValuePtr)) = (SQLHDESC)TThDesc;
                        }
                        else
                        {
                            copyErrorStack( &(TThStmt->TThDbc->errorStack),
                                            &(TThStmt->errorStack), 0);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    TRACE_FLEAVE_RC("SQLGetStmtAttrW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLSetStmtAttr(
  SQLHSTMT        hstmt,
  SQLINTEGER      Attribute,
  SQLPOINTER      ValuePtr,
  SQLINTEGER      StringLength
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSetStmtAttr");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetStmtAttr])))(TThStmt->hStmt, Attribute, ValuePtr, StringLength);
        }
    }

    TRACE_FLEAVE_RC("SQLSetStmtAttr",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLSetStmtAttrW(
  SQLHSTMT        hstmt,
  SQLINTEGER      Attribute,
  SQLPOINTER      ValuePtr,
  SQLINTEGER      StringLength
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHSTMT TThStmt = NULL;

    TRACE_FENTER("SQLSetStmtAttrW");
    TRACE_HVAL("hstmt",hstmt);

    rc = CLEAR_ERROR_STACK_LOCK(hstmt);
    if (  rc == SQL_SUCCESS  )
    {
        TThStmt = (TTSQLHSTMT)hstmt;
        if (  TThStmt->hStmt == SQL_NULL_HSTMT  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (SQL_API *)(SQLHSTMT, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThStmt->pFuncMap->odbcFuncPtr[F_POS_ODBC_SQLSetStmtAttrW])))(TThStmt->hStmt, Attribute, ValuePtr, StringLength);
        }
    }

    TRACE_FLEAVE_RC("SQLSetStmtAttrW",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLGetEnvAttr(
  SQLHENV         henv,
  SQLINTEGER      Attribute,
  SQLPOINTER      ValuePtr,
  SQLINTEGER      BufferLength,
  SQLINTEGER *    StringLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLINTEGER ttdmcaps = 0;
    TTSQLHENV  TThEnv = NULL;

    TRACE_FENTER("SQLGetEnvAttr");
    TRACE_HVAL("henv",henv);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(henv);
    if (  rc == SQL_SUCCESS  )
    {
        TThEnv = (TTSQLHENV)henv;
        if (  ((TThEnv->directEnv == SQL_NULL_HENV) &&
	       (TThEnv->clientEnv == SQL_NULL_HENV))  )
            rc = SQL_INVALID_HANDLE;
        else
        if (  Attribute == SQL_ATTR_TTDM_VERSION  )
        {
            *(SQLINTEGER *)ValuePtr = (SQLINTEGER)TTDMVERN;
        }
        else
        if (  Attribute == SQL_ATTR_TTDM_CAPABILITIES  )
        {
            if (  DMGlobalData.clientFuncs != NULL  )
            {
                ttdmcaps |= SQL_ATTR_TTDM_CLIENT;
                ttdmcaps |= SQL_ATTR_TTDM_ROUTING;
            }
            if (  DMGlobalData.directFuncs != NULL  )
            {
                ttdmcaps |= SQL_ATTR_TTDM_DIRECT;
                ttdmcaps |= SQL_ATTR_TTDM_ROUTING;
#if defined(ENABLE_XLA_DIRECT)
                ttdmcaps |= SQL_ATTR_TTDM_XLA;
#endif
#if defined(ENABLE_UTIL_LIB)
                if (  DMGlobalData.utilFuncs != NULL  )
                    ttdmcaps |= SQL_ATTR_TTDM_UTILITY;
#endif
            }
            *(SQLINTEGER *)ValuePtr = ttdmcaps;
        }
        else
        {
	    if (  TThEnv->pDirectMap != NULL  )
                rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLGetEnvAttr])))(TThEnv->directEnv, Attribute, ValuePtr, BufferLength, StringLengthPtr);
	    else
                rc = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLGetEnvAttr])))(TThEnv->clientEnv, Attribute, ValuePtr, BufferLength, StringLengthPtr);
        }
    }

    TRACE_FLEAVE_RC("SQLGetEnvAttr",rc);

    return rc;
}

SQLRETURN  SQL_API
SQLSetEnvAttr(
  SQLHENV         henv,
  SQLINTEGER      Attribute,
  SQLPOINTER      ValuePtr,
  SQLINTEGER      StringLength
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  rcDirect = SQL_SUCCESS;
    SQLRETURN  rcClient = SQL_SUCCESS;
    TTSQLHENV  TThEnv = NULL;
    long       pVal = 0;
    SQLINTEGER aVal = 0;

    TRACE_FENTER("SQLSetEnvAttr");
    TRACE_HVAL("henv",henv);

    rc = CLEAR_ERROR_STACK_ALL_LOCK(henv);
    if (  rc == SQL_SUCCESS  )
    {
        TThEnv = (TTSQLHENV)henv;
        if (  ((TThEnv->directEnv == SQL_NULL_HENV) &&
	       (TThEnv->clientEnv == SQL_NULL_HENV))  )
            rc = SQL_INVALID_HANDLE;
        else
        {
	    if (  TThEnv->pDirectMap != NULL  )
                rcDirect = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThEnv->pDirectMap->odbcFuncPtr[F_POS_ODBC_SQLSetEnvAttr])))(TThEnv->directEnv, Attribute, ValuePtr, StringLength);
	    if (  TThEnv->pClientMap != NULL  )
                rcClient = (*((SQLRETURN (SQL_API *)(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER))(TThEnv->pClientMap->odbcFuncPtr[F_POS_ODBC_SQLSetEnvAttr])))(TThEnv->clientEnv, Attribute, ValuePtr, StringLength);
            rc = maxODBCError(rcDirect, rcClient);
            if (  (rc == SQL_SUCCESS) &&
                  (Attribute == SQL_ATTR_ODBC_VERSION)  )
            {
                pVal = (long)ValuePtr;
                aVal = (SQLINTEGER)pVal;
                if (  (aVal == SQL_OV_ODBC3) ||
                      (aVal == SQL_OV_ODBC3_80)  )
                    TThEnv->v3 = 1;
            }
        }
    }

    TRACE_FLEAVE_RC("SQLSetEnvAttr",rc);

    return rc;
}

/*
 * SQLError - ANSI
 *
 * This needs lots of extra work due to the multiple error
 * stacks and encodings that need to be handled. Luckily
 * this function generally should not be in any performance
 * critial path.
 */
SQLRETURN  SQL_API
SQLGetDiagRec(
     SQLSMALLINT     HandleType,
     SQLHANDLE       Handle,
     SQLSMALLINT     RecNumber,
     SQLCHAR *       SQLState,
     SQLINTEGER *    NativeErrorPtr,
     SQLCHAR *       MessageText,
     SQLSMALLINT     BufferLength,
     SQLSMALLINT *   TextLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  errRC = SQL_SUCCESS;
    int        encoding = 0;
    SQLCHAR    sqlState[2*(TT_SQLSTATE_LEN+1)];
    SQLINTEGER nativeError= 0;
    SQLCHAR    errorMsg[SQL_MAX_MESSAGE_LENGTH+1];
    SQLCHAR    tErrorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];
    TTSQLHENV  TThEnv = NULL;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDESC TThDesc = NULL;
    SQLHANDLE  Handle1 = SQL_NULL_HANDLE;
    SQLHANDLE  Handle2 = SQL_NULL_HANDLE;
    SQLINTEGER h1Count = 0;
    SQLINTEGER h2Count = 0;
    void     * gdrFuncPtr1 = NULL;
    void     * gdrFuncPtr2 = NULL;
    void     * gdfFuncPtr1 = NULL;
    void     * gdfFuncPtr2 = NULL;
    int        errMsgLen = 0;
    ODBC_ERR_STACK_t * errorStack = NULL;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t mutex = MUTEX_INITIALISER;
#endif /* ENABLE_THREAD_SAFETY */
    
    TRACE_FENTER("SQLGetDiagRec");
    TRACE_HVAL("handle",Handle);

    /*
     * The logic in here is quite complex since
     *
     * (a) TTDM maintains its own error stack for every handle which may
     *     contain one or more TTDM specific errors
     *
     * (b) For some handles (Environment and Connection) there may be two
     *     underlying driver handles each potentially with its own error
     *     stack
     *
     * The logic has to ensure that it does the right thing for all possible 
     * permutations (crikey!).
     */

    if (  (RecNumber <= 0) || (BufferLength < 0)  )
	rc = SQL_ERROR;
    else
    switch (  HandleType  )
    {
        case SQL_HANDLE_ENV:
	    if (  Handle == SQL_NULL_HENV  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThEnv = (TTSQLHENV)Handle;
		if (  (TThEnv->tag != TTDM_STRUCT_TAG) ||
		      (TThEnv->hid != TTDM_HID_ENV) ||
		      ((TThEnv->directEnv == SQL_NULL_HENV) &&
		       (TThEnv->clientEnv == SQL_NULL_HENV))  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThEnv->mutex);
                    errorStack = &(TThEnv->errorStack);
                    if (  TThEnv->clientEnv != SQL_NULL_HENV  )
                    {
                        Handle1 = TThEnv->clientEnv;
                        gdrFuncPtr1 = TThEnv->pClientMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                        gdfFuncPtr1 = TThEnv->pClientMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                        if (  TThEnv->directEnv != SQL_NULL_HENV  )
                        {
                            Handle2 = TThEnv->directEnv;
                            gdrFuncPtr2 = TThEnv->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                            gdfFuncPtr2 = TThEnv->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                        }
                    }
                    else
                    {
                        Handle1 = TThEnv->directEnv;
                        gdrFuncPtr1 = TThEnv->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                        gdfFuncPtr1 = TThEnv->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                    }
		}
            }
	    break;
	case SQL_HANDLE_DBC:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThDbc = (TTSQLHDBC)Handle;
		if (  (TThDbc->tag != TTDM_STRUCT_TAG) ||
		      (TThDbc->hid != TTDM_HID_DBC) ||
		      ((TThDbc->directDbc == SQL_NULL_HDBC) &&
		       (TThDbc->clientDbc == SQL_NULL_HDBC))  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThDbc->mutex);
                    errorStack = &(TThDbc->errorStack);
                    if (  TThDbc->hDbc != SQL_NULL_HDBC  ) /* connected */
                    {
                        Handle1 = TThDbc->hDbc;
                        gdrFuncPtr1 = TThDbc->pFuncMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                        gdfFuncPtr1 = TThDbc->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                    }
                    else /* not connected */
                    {
                        if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                        {
                            Handle1 = TThDbc->clientDbc;
                            gdrFuncPtr1 = TThDbc->pClientMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                            gdfFuncPtr1 = TThDbc->pClientMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                            {
                                Handle2 = TThDbc->directDbc;
                                gdrFuncPtr2 = TThDbc->pDirectMap->
                                               odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                                gdfFuncPtr2 = TThDbc->pDirectMap->
                                               odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                            }
                        }
                        else
                        {
                            Handle1 = TThDbc->directDbc;
                            gdrFuncPtr1 = TThDbc->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                            gdfFuncPtr1 = TThDbc->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                        }
                    }
		}
	    }
	    break;
	case SQL_HANDLE_STMT:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThStmt = (TTSQLHSTMT)Handle;
		if (  (TThStmt->tag != TTDM_STRUCT_TAG) ||
		      (TThStmt->hid != TTDM_HID_STMT) ||
		      (TThStmt->hStmt == SQL_NULL_HSTMT)  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThStmt->mutex);
                    errorStack = &(TThStmt->errorStack);
                    Handle1 = TThStmt->hStmt;
                    gdrFuncPtr1 = TThStmt->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                    gdfFuncPtr1 = TThStmt->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
		}
	    }
	    break;
	case SQL_HANDLE_DESC:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThDesc = (TTSQLHDESC)Handle;
		if (  (TThDesc->tag != TTDM_STRUCT_TAG) ||
		      (TThDesc->hid != TTDM_HID_DESC) ||
		      (TThDesc->hDesc == SQL_NULL_HDESC)  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThDesc->mutex);
                    errorStack = &(TThDesc->errorStack);
                    Handle1 = TThDesc->hDesc;
                    gdrFuncPtr1 = TThDesc->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagRec];
                    gdfFuncPtr1 = TThDesc->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
		}
	    }
	    break;
	default:
	    rc = SQL_INVALID_HANDLE;
	    break;
    }

    if (  rc == SQL_SUCCESS  )
    {
	if (  RecNumber <= (*errorStack).msgcnt  )
	{   /* Return it from our internal error stack */
            rc = getError(errorStack, (int)RecNumber, &errRC, &encoding, 
		      sqlState, &nativeError, tErrorMsg, 
		      NULL, NULL, NULL, NULL);
            if (  rc == SQL_SUCCESS  )
            {
		if (  NativeErrorPtr != NULL  )
                    *NativeErrorPtr = nativeError;
	        if (  encoding == ENCODING_ANSI  )
	        { /* can copy directly */
		    if (  SQLState != NULL  )
                        strcpy((char *)SQLState,(const char *)sqlState);
		    strcpy((char *)errorMsg,(const char *)tErrorMsg);
	        }
	        else
	        { /* need to convert UTF-16 to ANSI */
		    if (  SQLState != NULL  )
		        translateUTF16toANSI((char *)SQLState,(SQLWCHAR *)sqlState);
		    translateUTF16toANSI((char *)errorMsg,(SQLWCHAR *)tErrorMsg);
                }
	        /* now copy error message to return buffer */
                errMsgLen = strlen((const char *)errorMsg);
		if (  TextLengthPtr != NULL  )
                    *TextLengthPtr = (SQLSMALLINT)errMsgLen;
		if (  MessageText != NULL  )
		{
                    if (  errMsgLen < (int)BufferLength  )
                        strcpy((char *)MessageText, (const char *)errorMsg);
                    else
                    {
                        rc = SQL_SUCCESS_WITH_INFO;
                        if (  BufferLength > 0  )
                        {
                            strncpy((char *)MessageText, (const char *)errorMsg, (BufferLength-1));
                            MessageText[BufferLength-1] = 0;
                        }
                        else
                            MessageText[0] = 0;
                    }
		}
            }
	}
        else
        {   /* get it from the driver stack */
	    /* adjust record number for what is on our stack */
	    RecNumber -= (SQLSMALLINT)((*errorStack).msgcnt);
	    /* get counts for driver handle1 and 2 as appropriate */
            if (  gdfFuncPtr1 != NULL  )
	        rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                    SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
		                    SQLSMALLINT, SQLSMALLINT *))
                                    (gdfFuncPtr1)))
                                    (HandleType, Handle1, 0, SQL_DIAG_NUMBER,
				    (SQLPOINTER)&h1Count, SQL_IS_INTEGER,
                                    NULL);
            if (  (rc == SQL_SUCCESS) && (gdfFuncPtr2 != NULL)  )
		rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                    SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
			            SQLSMALLINT, SQLSMALLINT *))
                                    (gdfFuncPtr2)))
                                    (HandleType, Handle2, 0, SQL_DIAG_NUMBER,
			            (SQLPOINTER)&h2Count, SQL_IS_INTEGER,
                                    NULL);

	    if (  (rc == SQL_SUCCESS) && (RecNumber > (h1Count + h2Count))  )
		rc = SQL_NO_DATA;
	    else
	    if (  rc == SQL_SUCCESS  )
	    {
		/* if all is good so far, try and get diag data from driver */
                if (  (gdrFuncPtr1 != NULL) && (RecNumber <= h1Count)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLCHAR *, SQLINTEGER *, 
				       SQLCHAR *, SQLSMALLINT, SQLSMALLINT *))
                                       (gdrFuncPtr1)))
                                       (HandleType, Handle1, RecNumber,
                                       SQLState, NativeErrorPtr,
				       MessageText, BufferLength,
                                       TextLengthPtr);
		RecNumber -= h1Count;
                if (  (gdrFuncPtr2 != NULL) && (RecNumber > 0)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLCHAR *, SQLINTEGER *, 
				       SQLCHAR *, SQLSMALLINT, SQLSMALLINT *))
                                       (gdrFuncPtr2)))
                                       (HandleType, Handle2, RecNumber,
                                       SQLState, NativeErrorPtr,
				       MessageText, BufferLength,
                                       TextLengthPtr);
	    }
	}

        MUTEX_UNLOCK(mutex);
    }

    TRACE_FLEAVE_RC("SQLGetDiagRec",rc);

    return rc;
}

/*
 * SQLError - UTF16
 *
 * This needs lots of extra work due to the multiple error
 * stacks and encodings that need to be handled. Luckily
 * this function generally should not be in any performance
 * critial path.
 */
SQLRETURN  SQL_API
SQLGetDiagRecW(
     SQLSMALLINT     HandleType,
     SQLHANDLE       Handle,
     SQLSMALLINT     RecNumber,
     SQLWCHAR *      SQLState,
     SQLINTEGER *    NativeErrorPtr,
     SQLWCHAR *      MessageText,
     SQLSMALLINT     BufferLength,
     SQLSMALLINT *   TextLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  errRC = SQL_SUCCESS;
    int        encoding = 0;
    SQLCHAR    sqlState[2*(TT_SQLSTATE_LEN+1)];
    SQLINTEGER nativeError= 0;
    SQLCHAR    errorMsg[SQL_MAX_MESSAGE_LENGTH+1];
    SQLCHAR    tErrorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];
    TTSQLHENV  TThEnv = NULL;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDESC TThDesc = NULL;
    SQLHANDLE  Handle1 = SQL_NULL_HANDLE;
    SQLHANDLE  Handle2 = SQL_NULL_HANDLE;
    SQLINTEGER h1Count = 0;
    SQLINTEGER h2Count = 0;
    void     * gdrFuncPtr1 = NULL;
    void     * gdrFuncPtr2 = NULL;
    void     * gdfFuncPtr1 = NULL;
    void     * gdfFuncPtr2 = NULL;
    int        errMsgLen = 0;
    ODBC_ERR_STACK_t * errorStack = NULL;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t mutex = MUTEX_INITIALISER;
#endif /* ENABLE_THREAD_SAFETY */
    
    TRACE_FENTER("SQLGetDiagRecW");
    TRACE_HVAL("handle",Handle);

    /*
     * The logic in here is quite complex since
     *
     * (a) TTDM maintains its own error stack for every handle which may
     *     contain one or more TTDM specific errors
     *
     * (b) For some handles (Environment and Connection) there may be two
     *     underlying driver handles each potentially with its own error
     *     stack
     *
     * The logic has to ensure that it does the right thing for all possible 
     * permutations (crikey!).
     */

    if (  (RecNumber <= 0) || (BufferLength < 0)  )
	rc = SQL_ERROR;
    else
    switch (  HandleType  )
    {
        case SQL_HANDLE_ENV:
	    if (  Handle == SQL_NULL_HENV  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThEnv = (TTSQLHENV)Handle;
		if (  (TThEnv->tag != TTDM_STRUCT_TAG) ||
		      (TThEnv->hid != TTDM_HID_ENV) ||
		      ((TThEnv->directEnv == SQL_NULL_HENV) &&
		       (TThEnv->clientEnv == SQL_NULL_HENV))  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThEnv->mutex);
                    errorStack = &(TThEnv->errorStack);
                    if (  TThEnv->clientEnv != SQL_NULL_HENV  )
                    {
                        Handle1 = TThEnv->clientEnv;
                        gdrFuncPtr1 = TThEnv->pClientMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                        gdfFuncPtr1 = TThEnv->pClientMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                        if (  TThEnv->directEnv != SQL_NULL_HENV  )
                        {
                            Handle2 = TThEnv->directEnv;
                            gdrFuncPtr2 = TThEnv->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                            gdfFuncPtr2 = TThEnv->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                        }
                    }
                    else
                    {
                        Handle1 = TThEnv->directEnv;
                        gdrFuncPtr1 = TThEnv->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                        gdfFuncPtr1 = TThEnv->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                    }
		}
            }
	    break;
	case SQL_HANDLE_DBC:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThDbc = (TTSQLHDBC)Handle;
		if (  (TThDbc->tag != TTDM_STRUCT_TAG) ||
		      (TThDbc->hid != TTDM_HID_DBC) ||
		      ((TThDbc->directDbc == SQL_NULL_HDBC) &&
		       (TThDbc->clientDbc == SQL_NULL_HDBC))  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThDbc->mutex);
                    errorStack = &(TThDbc->errorStack);
                    if (  TThDbc->hDbc != SQL_NULL_HDBC  ) /* connected */
                    {
                        Handle1 = TThDbc->hDbc;
                        gdrFuncPtr1 = TThDbc->pFuncMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                        gdfFuncPtr1 = TThDbc->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                    }
                    else /* not connected */
                    {
                        if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                        {
                            Handle1 = TThDbc->clientDbc;
                            gdrFuncPtr1 = TThDbc->pClientMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                            gdfFuncPtr1 = TThDbc->pClientMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                            {
                                Handle2 = TThDbc->directDbc;
                                gdrFuncPtr2 = TThDbc->pDirectMap->
                                               odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                                gdfFuncPtr2 = TThDbc->pDirectMap->
                                               odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                            }
                        }
                        else
                        {
                            Handle1 = TThDbc->directDbc;
                            gdrFuncPtr1 = TThDbc->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                            gdfFuncPtr1 = TThDbc->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                        }
                    }
		}
	    }
	    break;
	case SQL_HANDLE_STMT:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThStmt = (TTSQLHSTMT)Handle;
		if (  (TThStmt->tag != TTDM_STRUCT_TAG) ||
		      (TThStmt->hid != TTDM_HID_STMT) ||
		      (TThStmt->hStmt == SQL_NULL_HSTMT)  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThStmt->mutex);
                    errorStack = &(TThStmt->errorStack);
                    Handle1 = TThStmt->hStmt;
                    gdrFuncPtr1 = TThStmt->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                    gdfFuncPtr1 = TThStmt->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
		}
	    }
	    break;
	case SQL_HANDLE_DESC:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThDesc = (TTSQLHDESC)Handle;
		if (  (TThDesc->tag != TTDM_STRUCT_TAG) ||
		      (TThDesc->hid != TTDM_HID_DESC) ||
		      (TThDesc->hDesc == SQL_NULL_HDESC)  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThDesc->mutex);
                    errorStack = &(TThDesc->errorStack);
                    Handle1 = TThDesc->hDesc;
                    gdrFuncPtr1 = TThDesc->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagRecW];
                    gdfFuncPtr1 = TThDesc->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
		}
	    }
	    break;
	default:
	    rc = SQL_INVALID_HANDLE;
	    break;
    }

    if (  rc == SQL_SUCCESS  )
    {
	if (  RecNumber <= (*errorStack).msgcnt  )
	{   /* Return it from our internal error stack */
            rc = getError(errorStack, (int)RecNumber, &errRC, &encoding, 
		      sqlState, &nativeError, tErrorMsg,
		      NULL, NULL, NULL, NULL);
            if (  rc == SQL_SUCCESS  )
            {
		if (  NativeErrorPtr != NULL  )
                    *NativeErrorPtr = nativeError;
	        if (  encoding == ENCODING_UTF16  )
	        { /* can copy directly */
		    if (  SQLState != NULL  )
                        utf16cpy(SQLState,(SQLWCHAR *)sqlState);
		    utf16cpy((SQLWCHAR *)errorMsg,(SQLWCHAR *)tErrorMsg);
	        }
	        else
	        { /* need to convert ANSI to UTF-16 */
		    if (  SQLState != NULL  )
		        translateANSItoUTF16(SQLState,(char *)sqlState);
		    translateANSItoUTF16((SQLWCHAR *)errorMsg,(char *)tErrorMsg);
                }
	        /* now copy error message to return buffer */
                errMsgLen = utf16len((SQLWCHAR *)errorMsg);
		if (  TextLengthPtr != NULL  )
                    *TextLengthPtr = (SQLSMALLINT)errMsgLen;
		if (  MessageText != NULL  )
		{
                    if (  errMsgLen < (int)BufferLength  )
                        utf16cpy(MessageText, (SQLWCHAR *)errorMsg);
                    else
                    {
                        rc = SQL_SUCCESS_WITH_INFO;
                        if (  BufferLength > 0  )
                        {
                            utf16ncpy(MessageText, (SQLWCHAR *)errorMsg, (BufferLength-1));
                            MessageText[BufferLength-1] = 0;
                        }
                        else
                            MessageText[0] = 0;
                    }
		}
            }
	}
        else
        {   /* get it from the driver stack */
	    /* adjust record number for what is on our stack */
	    RecNumber -= (SQLSMALLINT)((*errorStack).msgcnt);
	    /* get counts for driver handle1 and 2 as appropriate */
            if (  gdfFuncPtr1 != NULL  )
	        rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                    SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
		                    SQLSMALLINT, SQLSMALLINT *))
                                    (gdfFuncPtr1)))
                                    (HandleType, Handle1, 0, SQL_DIAG_NUMBER,
				    (SQLPOINTER)&h1Count, SQL_IS_INTEGER,
                                    NULL);
            if (  (rc == SQL_SUCCESS) && (gdfFuncPtr2 != NULL)  )
		rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                    SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
			            SQLSMALLINT, SQLSMALLINT *))
                                    (gdfFuncPtr2)))
                                    (HandleType, Handle2, 0, SQL_DIAG_NUMBER,
			            (SQLPOINTER)&h2Count, SQL_IS_INTEGER,
                                    NULL);

	    if (  (rc == SQL_SUCCESS) && (RecNumber > (h1Count + h2Count))  )
		rc = SQL_NO_DATA;
	    else
	    if (  rc == SQL_SUCCESS  )
	    {
		/* if all is good so far, try and get diag data from driver */
                if (  (gdrFuncPtr1 != NULL) && (RecNumber <= h1Count)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLWCHAR *, SQLINTEGER *, 
				       SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *))
                                       (gdrFuncPtr1)))
                                       (HandleType, Handle1, RecNumber,
                                       SQLState, NativeErrorPtr,
				       MessageText, BufferLength,
                                       TextLengthPtr);
		RecNumber -= h1Count;
                if (  (gdrFuncPtr2 != NULL) && (RecNumber > 0)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLWCHAR *, SQLINTEGER *, 
				       SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *))
                                       (gdrFuncPtr2)))
                                       (HandleType, Handle2, RecNumber,
                                       SQLState, NativeErrorPtr,
				       MessageText, BufferLength,
                                       TextLengthPtr);
	    }
	}

        MUTEX_UNLOCK(mutex);
    }

    TRACE_FLEAVE_RC("SQLGetDiagRecW",rc);

    return rc;
}

/*
 * SQLGetDiagField - ANSI
 *
 * This needs lots of extra work due to the multiple error
 * stacks and encodings that need to be handled. Luckily
 * this function generally should not be in any performance
 * critial path.
 */
SQLRETURN  SQL_API
SQLGetDiagField(
     SQLSMALLINT     HandleType,
     SQLHANDLE       Handle,
     SQLSMALLINT     RecNumber,
     SQLSMALLINT     DiagIdentifier,
     SQLPOINTER      DiagInfoPtr,
     SQLSMALLINT     BufferLength,
     SQLSMALLINT *   TextLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  errRC = SQL_SUCCESS;
    int        encoding = 0;
    SQLCHAR    sqlState[2*(TT_SQLSTATE_LEN+1)];
    SQLINTEGER nativeError= 0;
    SQLCHAR    errorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];
    SQLCHAR  * diagClassOrigin = NULL;
    SQLCHAR  * diagSubclassOrigin = NULL;
    SQLCHAR  * diagConnectionName = NULL;
    SQLCHAR  * diagServerName = NULL;
    TTSQLHENV  TThEnv = NULL;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDESC TThDesc = NULL;
    SQLHANDLE  Handle1 = SQL_NULL_HANDLE;
    SQLHANDLE  Handle2 = SQL_NULL_HANDLE;
    SQLINTEGER h1Count = 0;
    SQLINTEGER h2Count = 0;
    void     * gdfFuncPtr1 = NULL;
    void     * gdfFuncPtr2 = NULL;
    int        errMsgLen = 0;
    int        isStmtHandle = 0;
    int        isHeaderField = 0;
    int        needsSpecialHandling = 0;
    int        stmtOnly = 0;
    int        isString = 0;
    ODBC_ERR_STACK_t * errorStack = NULL;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t mutex = MUTEX_INITIALISER;
#endif /* ENABLE_THREAD_SAFETY */
    
    TRACE_FENTER("SQLGetDiagField");
    TRACE_HVAL("handle",Handle);

    /*
     * The logic in here is quite complex since
     *
     * (a) TTDM maintains its own error stack for every handle which may
     *     contain one or more TTDM specific errors
     *
     * (b) For some handles (Environment and Connection) there may be two
     *     underlying driver handles each potentially with its own error
     *     stack
     *
     * (c) Some fields are 'header' fields which do not refer to a specific
     *     record in the error stack (luckily TTDM does not currently
     *     generate any header fields of its own)
     *
     * (d) Some requests are only valid for certain handle types (primarily
     *     statement handles)
     *
     * The logic has to ensure that it does the right thing for all possible 
     * permutations (crikey!).
     */

    /*
     * First check handle type for validity and setup handle related stuff.
     */
    switch (  HandleType  )
    {
        case SQL_HANDLE_ENV:
	    if (  Handle == SQL_NULL_HENV  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThEnv = (TTSQLHENV)Handle;
		if (  (TThEnv->tag != TTDM_STRUCT_TAG) ||
		      (TThEnv->hid != TTDM_HID_ENV) ||
		      ((TThEnv->directEnv == SQL_NULL_HENV) &&
		       (TThEnv->clientEnv == SQL_NULL_HENV))  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThEnv->mutex);
                    errorStack = &(TThEnv->errorStack);
                    if (  TThEnv->clientEnv != SQL_NULL_HENV  )
                    {
                        Handle1 = TThEnv->clientEnv;
                        gdfFuncPtr1 = TThEnv->pClientMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                        if (  TThEnv->directEnv != SQL_NULL_HENV  )
                        {
                            Handle2 = TThEnv->directEnv;
                            gdfFuncPtr2 = TThEnv->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                        }
                    }
                    else
                    {
                        Handle1 = TThEnv->directEnv;
                        gdfFuncPtr1 = TThEnv->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                    }
		}
            }
	    break;
	case SQL_HANDLE_DBC:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThDbc = (TTSQLHDBC)Handle;
		if (  (TThDbc->tag != TTDM_STRUCT_TAG) ||
		      (TThDbc->hid != TTDM_HID_DBC) ||
		      ((TThDbc->directDbc == SQL_NULL_HDBC) &&
		       (TThDbc->clientDbc == SQL_NULL_HDBC))  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThDbc->mutex);
                    errorStack = &(TThDbc->errorStack);
                    if (  TThDbc->hDbc != SQL_NULL_HDBC  ) /* connected */
                    {
                        Handle1 = TThDbc->hDbc;
                        gdfFuncPtr1 = TThDbc->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                    }
                    else /* not connected */
                    {
                        if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                        {
                            Handle1 = TThDbc->clientDbc;
                            gdfFuncPtr1 = TThDbc->pClientMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                            {
                                Handle2 = TThDbc->directDbc;
                                gdfFuncPtr2 = TThDbc->pDirectMap->
                                               odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                            }
                        }
                        else
                        {
                            Handle1 = TThDbc->directDbc;
                            gdfFuncPtr1 = TThDbc->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
                        }
                    }
		}
	    }
	    break;
	case SQL_HANDLE_STMT:
	    isStmtHandle = 1;
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThStmt = (TTSQLHSTMT)Handle;
		if (  (TThStmt->tag != TTDM_STRUCT_TAG) ||
		      (TThStmt->hid != TTDM_HID_STMT) ||
		      (TThStmt->hStmt == SQL_NULL_HSTMT)  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThStmt->mutex);
                    errorStack = &(TThStmt->errorStack);
                    Handle1 = TThStmt->hStmt;
                    gdfFuncPtr1 = TThStmt->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
		}
	    }
	    break;
	case SQL_HANDLE_DESC:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThDesc = (TTSQLHDESC)Handle;
		if (  (TThDesc->tag != TTDM_STRUCT_TAG) ||
		      (TThDesc->hid != TTDM_HID_DESC) ||
		      (TThDesc->hDesc == SQL_NULL_HDESC)  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThDesc->mutex);
                    errorStack = &(TThDesc->errorStack);
                    Handle1 = TThDesc->hDesc;
                    gdfFuncPtr1 = TThDesc->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagField];
		}
	    }
	    break;
	default:
	    rc = SQL_INVALID_HANDLE;
	    break;
    }

    /*
     * If all okay so far, continue.
     */
    if (  rc == SQL_SUCCESS  )
    {
	/*
	 * Determine some important things based on the request type
	 * and then do some basic validation.
	 */
	classifyDiagField(DiagIdentifier, &isHeaderField, 
			  &needsSpecialHandling, &stmtOnly, &isString);
	if (  (stmtOnly && !isStmtHandle) ||
	      (!isHeaderField && (RecNumber <= 0)) ||
	      (isString && (BufferLength < 0))  )
	    rc = SQL_ERROR;
	else /* still okay so continue */
	if (  isHeaderField  )
	{ 
	    if (  !needsSpecialHandling || ((*errorStack).msgcnt == 0)   )
	    {
		/* get diag data from driver */
                if (  gdfFuncPtr1 != NULL  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLSMALLINT, SQLPOINTER,
				       SQLSMALLINT, SQLSMALLINT *))
                                       (gdfFuncPtr1)))
                                       (HandleType, Handle1, RecNumber,
                                       DiagIdentifier, DiagInfoPtr,
				       BufferLength, TextLengthPtr);
		else
                if (  gdfFuncPtr2 != NULL  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLSMALLINT, SQLPOINTER,
				       SQLSMALLINT, SQLSMALLINT *))
                                       (gdfFuncPtr2)))
                                       (HandleType, Handle2, RecNumber,
                                       DiagIdentifier, DiagInfoPtr,
				       BufferLength, TextLengthPtr);
		else
		    rc = SQL_ERROR;
	    }
	    else
	    /* must be either SQL_DIAG_NUMBER or SQL_DIAG_RETURNCODE */
	    if (  DiagIdentifier == SQL_DIAG_RETURNCODE  )
	    {
                if (  (*errorStack).msgcnt > 0  )
		{
                    rc = getError(errorStack, 1, &errRC, 
				  &encoding, sqlState, &nativeError, 
				  errorMsg, NULL, NULL, NULL, NULL);
		    if (  (rc == SQL_SUCCESS) && (DiagInfoPtr != NULL)  )
			    *((SQLRETURN *)DiagInfoPtr) = errRC; /* FINI */
		 }
             }
	     else
	     { /* SQL_DIAG_NUMBER */
                 if (  gdfFuncPtr1 != NULL  )
	              rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT,
		                                     SQLHANDLE,
                                                     SQLSMALLINT,
			                             SQLSMALLINT,
			                             SQLPOINTER, 
		                                     SQLSMALLINT,
			                             SQLSMALLINT *))
                                                   (gdfFuncPtr1)))
                                                      (HandleType,
			                               Handle1,
			                               0,
			                               SQL_DIAG_NUMBER,
		                                       (SQLPOINTER)&h1Count,
			                               SQL_IS_INTEGER,
                                                       NULL);
                 if (  (rc == SQL_SUCCESS) && (gdfFuncPtr2 != NULL)  )
		     rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT,
		                                    SQLHANDLE,
                                                    SQLSMALLINT,
			                            SQLSMALLINT,
			                            SQLPOINTER, 
		                                    SQLSMALLINT,
		                                    SQLSMALLINT *))
                                               (gdfFuncPtr2)))
                                                  (HandleType,
			                           Handle2,
			                           0,
			                           SQL_DIAG_NUMBER,
		                                   (SQLPOINTER)&h2Count,
			                           SQL_IS_INTEGER,
                                                   NULL);
	         if (  (rc == SQL_SUCCESS) && (DiagInfoPtr != NULL)  )
	              *((SQLINTEGER *)DiagInfoPtr) = h1Count +
	                          h2Count + (*errorStack).msgcnt;
	    }
	}
	else /* not a header field */
	if (  RecNumber <= (*errorStack).msgcnt  )
	{   /* Return it from our internal error stack */
            rc = getError(errorStack, (int)RecNumber, &errRC, &encoding, 
		      sqlState, &nativeError, errorMsg,
                      &diagClassOrigin, &diagSubclassOrigin, 
		      &diagConnectionName, &diagServerName);
            if (  rc == SQL_SUCCESS  )
            {
		switch (  DiagIdentifier  )
		{
	            case SQL_DIAG_CLASS_ORIGIN:
			rc = diagCopy(encoding, diagClassOrigin, 
				      (SQLCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_SUBCLASS_ORIGIN:
			rc = diagCopy(encoding, diagSubclassOrigin, 
				      (SQLCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_CONNECTION_NAME:
			rc = diagCopy(encoding, diagConnectionName, 
				      (SQLCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_SERVER_NAME:
			rc = diagCopy(encoding, diagServerName, 
				      (SQLCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_SQLSTATE:
			rc = diagCopy(encoding, sqlState, 
				      (SQLCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_MESSAGE_TEXT:
			rc = diagCopy(encoding, errorMsg, 
				      (SQLCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_COLUMN_NUMBER:
			if (  DiagInfoPtr != NULL  )
			    *((SQLINTEGER *)DiagInfoPtr) = SQL_NO_COLUMN_NUMBER;
			break;
	            case SQL_DIAG_ROW_NUMBER:
			if (  DiagInfoPtr != NULL  )
			    *((SQLLEN *)DiagInfoPtr) = SQL_NO_ROW_NUMBER;
			break;
	            case SQL_DIAG_NATIVE:
			if (  DiagInfoPtr != NULL  )
			    *((SQLINTEGER *)DiagInfoPtr) = nativeError;
			break;
		    default:
			break;
		}
            }
	}
        else
        {   /* get it from the driver stack */
	    /* adjust record number for what is on our stack */
	    RecNumber -= (SQLSMALLINT)((*errorStack).msgcnt);
	    /* get counts for driver handle1 and 2 as appropriate */
            if (  gdfFuncPtr1 != NULL  )
	        rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                    SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
		                    SQLSMALLINT, SQLSMALLINT *))
                                    (gdfFuncPtr1)))
                                    (HandleType, Handle1, 0, SQL_DIAG_NUMBER,
				    (SQLPOINTER)&h1Count, SQL_IS_INTEGER,
                                    NULL);
            if (  (rc == SQL_SUCCESS) && (gdfFuncPtr2 != NULL)  )
		rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                    SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
			            SQLSMALLINT, SQLSMALLINT *))
                                    (gdfFuncPtr2)))
                                    (HandleType, Handle2, 0, SQL_DIAG_NUMBER,
			            (SQLPOINTER)&h2Count, SQL_IS_INTEGER,
                                    NULL);

	    if (  (rc == SQL_SUCCESS) && (RecNumber > (h1Count + h2Count))  )
		rc = SQL_NO_DATA;
	    else
	    if (  rc == SQL_SUCCESS  )
	    {
		/* if all is good so far, try and get diag data from driver */
                if (  (gdfFuncPtr1 != NULL) && (RecNumber <= h1Count)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
				       SQLSMALLINT, SQLSMALLINT *))
                                       (gdfFuncPtr1)))
                                       (HandleType, Handle1, RecNumber,
                                       DiagIdentifier, DiagInfoPtr,
				       BufferLength, TextLengthPtr);
		RecNumber -= h1Count;
                if (  (gdfFuncPtr2 != NULL) && (RecNumber > 0)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
				       SQLSMALLINT, SQLSMALLINT *))
                                       (gdfFuncPtr2)))
                                       (HandleType, Handle2, RecNumber,
                                       DiagIdentifier, DiagInfoPtr,
				       BufferLength, TextLengthPtr);
	    }
	}

        MUTEX_UNLOCK(mutex);
    }

    TRACE_FLEAVE_RC("SQLGetDiagField",rc);

    return rc;
}

/*
 * SQLGetDiagField - UTF16
 *
 * This needs lots of extra work due to the multiple error
 * stacks and encodings that need to be handled. Luckily
 * this function generally should not be in any performance
 * critial path.
 */
SQLRETURN  SQL_API
SQLGetDiagFieldW(
     SQLSMALLINT     HandleType,
     SQLHANDLE       Handle,
     SQLSMALLINT     RecNumber,
     SQLSMALLINT     DiagIdentifier,
     SQLPOINTER      DiagInfoPtr,
     SQLSMALLINT     BufferLength,
     SQLSMALLINT *   TextLengthPtr
)
{
    SQLRETURN  rc = SQL_SUCCESS;
    SQLRETURN  errRC = SQL_SUCCESS;
    int        encoding = 0;
    SQLCHAR    sqlState[2*(TT_SQLSTATE_LEN+1)];
    SQLINTEGER nativeError= 0;
    SQLCHAR    errorMsg[2*(SQL_MAX_MESSAGE_LENGTH+1)];
    SQLCHAR  * diagClassOrigin = NULL;
    SQLCHAR  * diagSubclassOrigin = NULL;
    SQLCHAR  * diagConnectionName = NULL;
    SQLCHAR  * diagServerName = NULL;
    TTSQLHENV  TThEnv = NULL;
    TTSQLHDBC  TThDbc = NULL;
    TTSQLHSTMT TThStmt = NULL;
    TTSQLHDESC TThDesc = NULL;
    SQLHANDLE  Handle1 = SQL_NULL_HANDLE;
    SQLHANDLE  Handle2 = SQL_NULL_HANDLE;
    SQLINTEGER hCount = 0;
    SQLINTEGER h1Count = 0;
    SQLINTEGER h2Count = 0;
    void     * gdfFuncPtr1 = NULL;
    void     * gdfFuncPtr2 = NULL;
    int        errMsgLen = 0;
    int        isStmtHandle = 0;
    int        isHeaderField = 0;
    int        needsSpecialHandling = 0;
    int        stmtOnly = 0;
    int        isString = 0;
    ODBC_ERR_STACK_t * errorStack = NULL;
#if defined(ENABLE_THREAD_SAFETY)
    tt_mutex_t mutex = MUTEX_INITIALISER;
#endif /* ENABLE_THREAD_SAFETY */
    
    TRACE_FENTER("SQLGetDiagFieldW");
    TRACE_HVAL("handle",Handle);

    /*
     * The logic in here is quite complex since
     *
     * (a) TTDM maintains its own error stack for every handle which may
     *     contain one or more TTDM specific errors
     *
     * (b) For some handles (Environment and Connection) there may be two
     *     underlying driver handles each potentially with its own error
     *     stack
     *
     * (c) Some fields are 'header' fields which do not refer to a specific
     *     record in the error stack (luckily TTDM does not currently
     *     generate any header fields of its own)
     *
     * (d) Some requests are only valid for certain handle types (primarily
     *     statement handles)
     *
     * The logic has to ensure that it does the right thing for all possible 
     * permutations (crikey!).
     */

    /*
     * First check handle type for validity and setup handle related stuff.
     */
    switch (  HandleType  )
    {
        case SQL_HANDLE_ENV:
	    if (  Handle == SQL_NULL_HENV  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThEnv = (TTSQLHENV)Handle;
		if (  (TThEnv->tag != TTDM_STRUCT_TAG) ||
		      (TThEnv->hid != TTDM_HID_ENV) ||
		      ((TThEnv->directEnv == SQL_NULL_HENV) &&
		       (TThEnv->clientEnv == SQL_NULL_HENV))  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThEnv->mutex);
                    errorStack = &(TThEnv->errorStack);
                    if (  TThEnv->clientEnv != SQL_NULL_HENV  )
                    {
                        Handle1 = TThEnv->clientEnv;
                        gdfFuncPtr1 = TThEnv->pClientMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                        if (  TThEnv->directEnv != SQL_NULL_HENV  )
                        {
                            Handle2 = TThEnv->directEnv;
                            gdfFuncPtr2 = TThEnv->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                        }
                    }
                    else
                    {
                        Handle1 = TThEnv->directEnv;
                        gdfFuncPtr1 = TThEnv->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                    }
		}
            }
	    break;
	case SQL_HANDLE_DBC:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThDbc = (TTSQLHDBC)Handle;
		if (  (TThDbc->tag != TTDM_STRUCT_TAG) ||
		      (TThDbc->hid != TTDM_HID_DBC) ||
		      ((TThDbc->directDbc == SQL_NULL_HDBC) &&
		       (TThDbc->clientDbc == SQL_NULL_HDBC))  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThDbc->mutex);
                    errorStack = &(TThDbc->errorStack);
                    if (  TThDbc->hDbc != SQL_NULL_HDBC  ) /* connected */
                    {
                        Handle1 = TThDbc->hDbc;
                        gdfFuncPtr1 = TThDbc->pDirectMap->
                                       odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                    }
                    else /* not connected */
                    {
                        if (  TThDbc->clientDbc != SQL_NULL_HDBC  )
                        {
                            Handle1 = TThDbc->clientDbc;
                            gdfFuncPtr1 = TThDbc->pClientMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                            if (  TThDbc->directDbc != SQL_NULL_HDBC  )
                            {
                                Handle2 = TThDbc->directDbc;
                                gdfFuncPtr2 = TThDbc->pDirectMap->
                                               odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                            }
                        }
                        else
                        {
                            Handle1 = TThDbc->directDbc;
                            gdfFuncPtr1 = TThDbc->pDirectMap->
                                           odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
                        }
                    }
		}
	    }
	    break;
	case SQL_HANDLE_STMT:
	    isStmtHandle = 1;
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThStmt = (TTSQLHSTMT)Handle;
		if (  (TThStmt->tag != TTDM_STRUCT_TAG) ||
		      (TThStmt->hid != TTDM_HID_STMT) ||
		      (TThStmt->hStmt == SQL_NULL_HSTMT)  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThStmt->mutex);
                    errorStack = &(TThStmt->errorStack);
                    Handle1 = TThStmt->hStmt;
                    gdfFuncPtr1 = TThStmt->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
		}
	    }
	    break;
	case SQL_HANDLE_DESC:
	    if (  Handle == SQL_NULL_HDBC  )
                rc = SQL_INVALID_HANDLE;
	    else
	    {
		TThDesc = (TTSQLHDESC)Handle;
		if (  (TThDesc->tag != TTDM_STRUCT_TAG) ||
		      (TThDesc->hid != TTDM_HID_DESC) ||
		      (TThDesc->hDesc == SQL_NULL_HDESC)  )
		    rc = SQL_INVALID_HANDLE;
		else
		{
                    MUTEX_LOCK(mutex = TThDesc->mutex);
                    errorStack = &(TThDesc->errorStack);
                    Handle1 = TThDesc->hDesc;
                    gdfFuncPtr1 = TThDesc->pFuncMap->
                                   odbcFuncPtr[F_POS_ODBC_SQLGetDiagFieldW];
		}
	    }
	    break;
	default:
	    rc = SQL_INVALID_HANDLE;
	    break;
    }

    /*
     * If all okay so far, continue.
     */
    if (  rc == SQL_SUCCESS  )
    {
	/*
	 * Determine some important things based on the request type
	 * and then do some basic validation.
	 */
	classifyDiagField(DiagIdentifier, &isHeaderField, 
			  &needsSpecialHandling, &stmtOnly, &isString);
	if (  (stmtOnly && !isStmtHandle) ||
	      (!isHeaderField && (RecNumber <= 0)) ||
	      (isString && (BufferLength < 0))  )
	    rc = SQL_ERROR;
	else /* still okay so continue */
	if (  isHeaderField  )
	{ 
	    if (  !needsSpecialHandling || ((*errorStack).msgcnt == 0)   )
	    {
		/* get diag data from driver */
                if (  gdfFuncPtr1 != NULL  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLSMALLINT, SQLPOINTER,
				       SQLSMALLINT, SQLSMALLINT *))
                                       (gdfFuncPtr1)))
                                       (HandleType, Handle1, RecNumber,
                                       DiagIdentifier, DiagInfoPtr,
				       BufferLength, TextLengthPtr);
		else
                if (  gdfFuncPtr2 != NULL  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLSMALLINT, SQLPOINTER,
				       SQLSMALLINT, SQLSMALLINT *))
                                       (gdfFuncPtr2)))
                                       (HandleType, Handle2, RecNumber,
                                       DiagIdentifier, DiagInfoPtr,
				       BufferLength, TextLengthPtr);
		else
		    rc = SQL_ERROR;
	    }
	    else
	    /* must be either SQL_DIAG_NUMBER or SQL_DIAG_RETURNCODE */
	    if (  DiagIdentifier == SQL_DIAG_RETURNCODE  )
	    {
                if (  (*errorStack).msgcnt > 0  )
		{
                    rc = getError(errorStack, 1, &errRC, 
			          &encoding, sqlState, &nativeError, 
				  errorMsg, NULL, NULL, NULL, NULL);
		    if (  (rc == SQL_SUCCESS) && (DiagInfoPtr != NULL)  )
			    *((SQLRETURN *)DiagInfoPtr) = errRC; /* FINI */
		}
            }
	    else
	    { /* SQL_DIAG_NUMBER */
                if (  gdfFuncPtr1 != NULL  )
	                 rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT,
						        SQLHANDLE,
                                                        SQLSMALLINT,
							SQLSMALLINT,
							SQLPOINTER, 
		                                        SQLSMALLINT,
							SQLSMALLINT *))
                                                     (gdfFuncPtr1)))
                                                          (HandleType,
							   Handle1,
							   0,
							   SQL_DIAG_NUMBER,
				                           (SQLPOINTER)&h1Count,
							   SQL_IS_INTEGER,
                                                           NULL);
                if (  (rc == SQL_SUCCESS) && (gdfFuncPtr2 != NULL)  )
		         rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT,
						        SQLHANDLE,
                                                        SQLSMALLINT,
							SQLSMALLINT,
							SQLPOINTER, 
			                                SQLSMALLINT,
							SQLSMALLINT *))
                                                     (gdfFuncPtr2)))
                                                          (HandleType,
							   Handle2,
							   0,
							   SQL_DIAG_NUMBER,
			                                   (SQLPOINTER)&h2Count,
							   SQL_IS_INTEGER,
                                                           NULL);
                if (  (rc == SQL_SUCCESS) && (DiagInfoPtr != NULL)  )
		     *((SQLINTEGER *)DiagInfoPtr) = h1Count +
				 h2Count + (*errorStack).msgcnt;
	    }
	}
	else /* not a header field */
	if (  RecNumber <= (*errorStack).msgcnt  )
	{   /* Return it from our internal error stack */
            rc = getError(errorStack, (int)RecNumber, &errRC, &encoding, 
		      sqlState, &nativeError, errorMsg,
                      &diagClassOrigin, &diagSubclassOrigin, 
		      &diagConnectionName, &diagServerName);
            if (  rc == SQL_SUCCESS  )
            {
		switch (  DiagIdentifier  )
		{
	            case SQL_DIAG_CLASS_ORIGIN:
			rc = diagCopyW(encoding, (SQLWCHAR *)diagClassOrigin, 
				      (SQLWCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_SUBCLASS_ORIGIN:
			rc = diagCopyW(encoding, (SQLWCHAR *)diagSubclassOrigin, 
				      (SQLWCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_CONNECTION_NAME:
			rc = diagCopyW(encoding, (SQLWCHAR *)diagConnectionName, 
				      (SQLWCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_SERVER_NAME:
			rc = diagCopyW(encoding, (SQLWCHAR *)diagServerName, 
				      (SQLWCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_SQLSTATE:
			rc = diagCopyW(encoding, (SQLWCHAR *)sqlState, 
				      (SQLWCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_MESSAGE_TEXT:
			rc = diagCopyW(encoding, (SQLWCHAR *)errorMsg, 
				      (SQLWCHAR *)DiagInfoPtr, BufferLength,
				      TextLengthPtr);
			break;
	            case SQL_DIAG_COLUMN_NUMBER:
			if (  DiagInfoPtr != NULL  )
			    *((SQLINTEGER *)DiagInfoPtr) = SQL_NO_COLUMN_NUMBER;
			break;
	            case SQL_DIAG_ROW_NUMBER:
			if (  DiagInfoPtr != NULL  )
			    *((SQLLEN *)DiagInfoPtr) = SQL_NO_ROW_NUMBER;
			break;
	            case SQL_DIAG_NATIVE:
			if (  DiagInfoPtr != NULL  )
			    *((SQLINTEGER *)DiagInfoPtr) = nativeError;
			break;
		    default:
			break;
		}
            }
	}
        else
        {   /* get it from the driver stack */
	    /* adjust record number for what is on our stack */
	    RecNumber -= (SQLSMALLINT)((*errorStack).msgcnt);
	    /* get counts for driver handle1 and 2 as appropriate */
            if (  gdfFuncPtr1 != NULL  )
	        rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                    SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
		                    SQLSMALLINT, SQLSMALLINT *))
                                    (gdfFuncPtr1)))
                                    (HandleType, Handle1, 0, SQL_DIAG_NUMBER,
				    (SQLPOINTER)&h1Count, SQL_IS_INTEGER,
                                    NULL);
            if (  (rc == SQL_SUCCESS) && (gdfFuncPtr2 != NULL)  )
		rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                    SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
			            SQLSMALLINT, SQLSMALLINT *))
                                    (gdfFuncPtr2)))
                                    (HandleType, Handle2, 0, SQL_DIAG_NUMBER,
			            (SQLPOINTER)&h2Count, SQL_IS_INTEGER,
                                    NULL);

	    if (  (rc == SQL_SUCCESS) && (RecNumber > (h1Count + h2Count))  )
		rc = SQL_NO_DATA;
	    else
	    if (  rc == SQL_SUCCESS  )
	    {
		/* if all is good so far, try and get diag data from driver */
                if (  (gdfFuncPtr1 != NULL) && (RecNumber <= h1Count)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
				       SQLSMALLINT, SQLSMALLINT *))
                                       (gdfFuncPtr1)))
                                       (HandleType, Handle1, RecNumber,
                                       DiagIdentifier, DiagInfoPtr,
				       BufferLength, TextLengthPtr);
		RecNumber -= h1Count;
                if (  (gdfFuncPtr2 != NULL) && (RecNumber > 0)  )
                    rc = (*((SQLRETURN (SQL_API *)(SQLSMALLINT, SQLHANDLE,
                                       SQLSMALLINT, SQLSMALLINT, SQLPOINTER, 
				       SQLSMALLINT, SQLSMALLINT *))
                                       (gdfFuncPtr2)))
                                       (HandleType, Handle2, RecNumber,
                                       DiagIdentifier, DiagInfoPtr,
				       BufferLength, TextLengthPtr);
	    }
	}

        MUTEX_UNLOCK(mutex);
    }

    TRACE_FLEAVE_RC("SQLGetDiagFieldW",rc);

    return rc;
}

/*
 ========================================================================
                 Public functions - Scaleout Routing API
 ========================================================================
 */

/*
 * These functions do the bare minimum of work and simply pass the call
 * straight through to the relevant direct or client library function.
 */

SQLRETURN SQL_API 
ttGridDistClear(
  SQLHDBC hdbc,
  TTGRIDDIST dist
)
{
   SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("ttGridDistClear");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, TTGRIDDIST))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_ttGridDistClear])))(TThDbc->hDbc,dist);
    }

    TRACE_FLEAVE_RC("ttGridDistClear",rc);

    return rc;
} // ttGridDistClear

SQLRETURN SQL_API 
ttGridDistCreate(
  SQLHDBC hdbc,
  TTGRIDMAP map, 
  SQLSMALLINT cTypes[],
  SQLSMALLINT sqlTypes[],
  SQLULEN precisions[],
  SQLSMALLINT scales[],
  SQLLEN maxSizes[],
  SQLUSMALLINT ncols,
  TTGRIDDIST *dist
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("ttGridDistCreate");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, TTGRIDMAP, SQLSMALLINT[], SQLSMALLINT[], SQLULEN[], SQLSMALLINT[], SQLLEN[], SQLUSMALLINT, TTGRIDDIST *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_ttGridDistCreate])))(TThDbc->hDbc,map,cTypes,sqlTypes,precisions,scales,maxSizes,ncols,dist);
    }

    TRACE_FLEAVE_RC("ttGridDistCreate",rc);

    return rc;
} // ttGridDistCreate

SQLRETURN SQL_API 
ttGridDistElementGet(
  SQLHDBC hdbc,
  TTGRIDDIST dist,
  SQLSMALLINT elemids[],
  SQLSMALLINT elemidSz
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("ttGridDistElementGet");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, TTGRIDDIST, SQLSMALLINT[], SQLSMALLINT))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_ttGridDistElementGet])))(TThDbc->hDbc,dist,elemids,elemidSz);
    }

    TRACE_FLEAVE_RC("ttGridDistElementGet",rc);

    return rc;
} // ttGridDistElementGet

SQLRETURN SQL_API 
ttGridDistFree(
  SQLHDBC hdbc,
  TTGRIDDIST dist
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("ttGridDistFree");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, TTGRIDDIST))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_ttGridDistFree])))(TThDbc->hDbc,dist);
    }

    TRACE_FLEAVE_RC("ttGridDistFree",rc);

    return rc;
} // ttGridDistFree

SQLRETURN SQL_API 
ttGridDistReplicaGet(
  SQLHDBC hdbc,
  TTGRIDDIST dist,
  SQLSMALLINT *repsetid
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("ttGridDistReplicaGet");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, TTGRIDDIST, SQLSMALLINT *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_ttGridDistReplicaGet])))(TThDbc->hDbc,dist,repsetid);
    }

    TRACE_FLEAVE_RC("ttGridDistReplicaGet",rc);

    return rc;
} // ttGridDistReplicaGet

SQLRETURN SQL_API 
ttGridDistValueSet(
  SQLHDBC hdbc,
  TTGRIDDIST dist,
  SQLSMALLINT pos,
  SQLPOINTER val,
  SQLLEN vallen
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("ttGridDistValueSet");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, TTGRIDDIST, SQLSMALLINT, SQLPOINTER, SQLLEN))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_ttGridDistValueSet])))(TThDbc->hDbc,dist,pos,val,vallen);
    }

    TRACE_FLEAVE_RC("ttGridDistValueSet",rc);

    return rc;
} // ttGridDistValueSet

SQLRETURN SQL_API 
ttGridMapCreate(
  SQLHDBC hdbc,
  TTGRIDMAP *map
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("ttGridMapCreate");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, TTGRIDMAP *))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_ttGridMapCreate])))(TThDbc->hDbc,map);
    }

    TRACE_FLEAVE_RC("ttGridMapCreate",rc);

    return rc;
} // ttGridMapCreate

SQLRETURN SQL_API 
ttGridMapFree(
  SQLHDBC hdbc,
  TTGRIDMAP map
)
{
    SQLRETURN   rc = SQL_SUCCESS;
    TTSQLHDBC   TThDbc = NULL;

    TRACE_FENTER("ttGridMapFree");
    TRACE_HVAL("hdbc",hdbc);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  TThDbc->hDbc == SQL_NULL_HDBC  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (SQL_API *)(SQLHDBC, TTGRIDMAP))(TThDbc->pFuncMap->odbcFuncPtr[F_POS_ODBC_ttGridMapFree])))(TThDbc->hDbc,map);
    }

    TRACE_FLEAVE_RC("ttGridMapFree",rc);

    return rc;
} // ttGridMapFree

#if defined(ENABLE_XLA_DIRECT)

/*
 ========================================================================
                 Public functions - XLA 
 ========================================================================
 */

/*
 * Most of these functions simply do the bare minimum of work and simply
 * pass the call straight through to the relevant direct or client
 * library function. The only function that needs to do any significant
 * additional work is ttXlaPersistOpen.
 */

SQLRETURN  XLA_API
ttXlaPersistOpen(
                 SQLHDBC hdbc, 
                 SQLCHAR *tag, 
                 SQLUINTEGER options, 
                 ttXlaHandle_h *handle
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTSQLHDBC  TThDbc = NULL;
    TTXlaHandle_t * TThXla = NULL;

    TRACE_FENTER("ttXlaPersistOpen");
    TRACE_HVAL("hdbc",hdbc);
    TRACE_HVAL("handle",handle);

    rc = CLEAR_ERROR_STACK_LOCK(hdbc);
    if (  rc == SQL_SUCCESS  )
    {
        TThDbc = (TTSQLHDBC)hdbc;
        if (  (TThDbc->hDbc == SQL_NULL_HDBC) || (handle == NULL) ||
              (TThDbc->pFuncMap->libType != TT_LIB_DM_TYPE) /*not direct mode*/
           )
            rc = SQL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(TThDbc->mutex);
            TThXla = (TTXlaHandle_t *)calloc(1,sizeof(TTXlaHandle_t));
            if (  TThXla == NULL  )
                pushError(&(TThDbc->errorStack),
                          rc = SQL_INVALID_HANDLE,
			  ENCODING_ANSI,
                          (SQLCHAR *)TT_DM_SQLSTATE_NOMEM,
                          tt_ErrDMNoMemory,
                          (SQLCHAR *)"TimesTen Driver Manager: "
                                     "ttXlaPersistOpen: "
                                     "Unable to allocate memory for XLA handle",
	                  (SQLCHAR *)TTDM_DIAG_CLASS_ORIGIN,
	                  (SQLCHAR *)TTDM_DIAG_SUBCLASS_ORIGIN,
	                  TThDbc->connName,
	                  TThDbc->serverName
                          );
            else
            {
                TThXla->tag = TTDM_STRUCT_TAG;
	        TThXla->hid = TTDM_HID_XLA;
                TThXla->hXla = NULL;
                TThXla->TThEnv = SQL_NULL_HENV;
                TThXla->TThDbc = SQL_NULL_HDBC;
                TThXla->pFuncMap = NULL;
#if defined(ENABLE_THREAD_SAFETY)
                TThXla->pMutex = NULL;
#endif /* ENABLE_THREAD_SAFETY */
                rc = (*((SQLRETURN (XLA_API *)(SQLHDBC, SQLCHAR *, SQLUINTEGER, ttXlaHandle_h *))(TThDbc->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaPersistOpen])))((SQLHDBC)(TThDbc->hDbc), tag, options, (ttXlaHandle_h *)&(TThXla->hXla));

                TThDbc->TThXla = TThXla;
                TThXla->TThEnv = TThDbc->TThEnv;
                TThXla->TThDbc = TThDbc;
                TThXla->pFuncMap = TThDbc->pFuncMap;
#if defined(ENABLE_THREAD_SAFETY)
                TThXla->pMutex = TThDbc->mutex;
#endif /* ENABLE_THREAD_SAFETY */
                *handle = (ttXlaHandle_h)TThXla;
            }
            MUTEX_UNLOCK(TThDbc->mutex);
        }
    }

    TRACE_FLEAVE_RC("ttXlaPersistOpen",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaClose(
                 ttXlaHandle_h handle
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaClose");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaClose])))(TThXla->hXla);
            if (  rc == SQL_SUCCESS  )
            {
                MUTEX_LOCK(TThXla->pMutex);
                /* mark invalid */
                TThXla->tag = 0;
		TThXla->hid = 0;

                TThXla->TThDbc->TThXla = NULL;
                MUTEX_UNLOCK(TThXla->pMutex);
                TThXla->TThEnv = SQL_NULL_HENV;
                TThXla->TThDbc = SQL_NULL_HDBC;
                TThXla->hXla = NULL;
#if defined(ENABLE_THREAD_SAFETY)
                TThXla->pMutex = NULL;
#endif /* ENABLE_THREAD_SAFETY */
                free( (void *)TThXla );
            }
        }
    }

    TRACE_FLEAVE_RC("ttXlaClose",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaAcknowledge(
                 ttXlaHandle_h handle
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaAcknowledge");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaAcknowledge])))(TThXla->hXla);
    }

    TRACE_FLEAVE_RC("ttXlaAcknowledge",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaDeleteBookmark(
                 ttXlaHandle_h handle
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaDeleteBookmark");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaDeleteBookmark])))(TThXla->hXla);
    }

    TRACE_FLEAVE_RC("ttXlaDeleteBookmark",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaGetLSN(
                 ttXlaHandle_h handle,
                 tt_XlaLsn_t *LSNP
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaGetLSN");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, tt_XlaLsn_t *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaGetLSN])))(TThXla->hXla, LSNP);
    }

    TRACE_FLEAVE_RC("ttXlaGetLSN",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaSetLSN(
                 ttXlaHandle_h handle,
                 tt_XlaLsn_t *LSNP
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaSetLSN");
    TRACE_HVAL("handle",handle);


    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, tt_XlaLsn_t *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaSetLSN])))(TThXla->hXla, LSNP);
    }

    TRACE_FLEAVE_RC("ttXlaSetLSN",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaGetVersion(
                 ttXlaHandle_h handle,
                 ttXlaVersion_t *configuredVersion,
                 ttXlaVersion_t *actualVersion
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaGetVersion");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaVersion_t *, ttXlaVersion_t *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaGetVersion])))(TThXla->hXla, configuredVersion, actualVersion);
    }

    TRACE_FLEAVE_RC("ttXlaGetVersion",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaSetVersion(
                 ttXlaHandle_h handle,
                 ttXlaVersion_t *version
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaSetVersion");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaVersion_t *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaSetVersion])))(TThXla->hXla, version);
    }

    TRACE_FLEAVE_RC("ttXlaSetVersion",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaVersionCompare(
                 ttXlaHandle_h handle,
                 ttXlaVersion_t *version1,
                 ttXlaVersion_t *version2,
                 SQLINTEGER *result
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaVersionCompare");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaVersion_t *, ttXlaVersion_t *, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaVersionCompare])))(TThXla->hXla, version1, version2, result);
    }

    TRACE_FLEAVE_RC("ttXlaVersionCompare",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaGenerateSQL(
                 ttXlaHandle_h handle,
                 ttXlaUpdateDesc_t *record,
                 char *buffer,
                 SQLINTEGER maxlen,
                 SQLINTEGER *actuallen
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaGenerateSQL");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaUpdateDesc_t *, char *, SQLINTEGER, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaGenerateSQL])))(TThXla->hXla, record, buffer, maxlen, actuallen);
    }

    TRACE_FLEAVE_RC("ttXlaVersionCompare",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaError(
                 ttXlaHandle_h handle,
                 SQLINTEGER *errCode,
                 char *errMessage,
                 SQLINTEGER maxLen,
                 SQLINTEGER *retLen
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaError");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, SQLINTEGER *, char *, SQLINTEGER, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaError])))(TThXla->hXla, errCode, errMessage, maxLen, retLen);
    }

    TRACE_FLEAVE_RC("ttXlaError", rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaErrorRestart(
                 ttXlaHandle_h handle
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaErrorRestart");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaErrorRestart])))(TThXla->hXla);
    }

    TRACE_FLEAVE_RC("ttXlaErrorRestart",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaTableVersionVerify(
                 ttXlaHandle_h handle,
		 ttXlaTblVerDesc_t *tblInfo,
		 ttXlaUpdateDesc_t *record,
		 SQLINTEGER *compat
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaTableVersionVerify");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaTblVerDesc_t *, ttXlaUpdateDesc_t *, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaTableVersionVerify])))(TThXla->hXla, tblInfo, record, compat);
    }

    TRACE_FLEAVE_RC("ttXlaTableVersionVerify",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaVersionTableInfo(
                 ttXlaHandle_h handle,
		 ttXlaUpdateDesc_t *record,
		 ttXlaTblVerDesc_t *tblinfo
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaVersionTableInfo");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaUpdateDesc_t *, ttXlaTblVerDesc_t *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaVersionTableInfo])))(TThXla->hXla, record, tblinfo);
    }

    TRACE_FLEAVE_RC("ttXlaVersionTableInfo",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaVersionColumnInfo(
                 ttXlaHandle_h handle,
		 ttXlaUpdateDesc_t *record,
		 ttXlaColDesc_t *colinfo,
		 SQLINTEGER maxcols,
		 SQLINTEGER *nreturned
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaVersionColumnInfo");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaUpdateDesc_t *, ttXlaColDesc_t *, SQLINTEGER, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaVersionColumnInfo])))(TThXla->hXla, record, colinfo, maxcols, nreturned);
    }

    TRACE_FLEAVE_RC("ttXlaVersionColumnInfo",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaTableStatus(
                 ttXlaHandle_h handle,
                 SQLUBIGINT systemTableID,
                 SQLUBIGINT userTableID,
                 SQLINTEGER *oldStatus,
                 SQLINTEGER *newStatus
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaTableStatus");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, SQLUBIGINT, SQLUBIGINT, SQLINTEGER *, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaTableStatus])))(TThXla->hXla, systemTableID, userTableID, oldStatus, newStatus);
    }

    TRACE_FLEAVE_RC("ttXlaTableStatus",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaGetTableInfo(
                 ttXlaHandle_h handle,
                 SQLUBIGINT systemTableID,
                 SQLUBIGINT userTableID,
                 ttXlaTblDesc_t *tblinfo
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaGetTableInfo");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, SQLUBIGINT, SQLUBIGINT, ttXlaTblDesc_t *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaGetTableInfo])))(TThXla->hXla, systemTableID, userTableID, tblinfo);
    }

    TRACE_FLEAVE_RC("ttXlaGetTableInfo",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaGetColumnInfo(
                 ttXlaHandle_h handle,
                 SQLUBIGINT systemTableID,
                 SQLUBIGINT userTableID,
                 ttXlaColDesc_t *colinfo,
                 SQLINTEGER maxcols,
                 SQLINTEGER *nreturned
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaGetColumnInfo");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, SQLUBIGINT, SQLUBIGINT, ttXlaColDesc_t *, SQLINTEGER, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaGetColumnInfo])))(TThXla->hXla, systemTableID, userTableID, colinfo, maxcols, nreturned);
    }

    TRACE_FLEAVE_RC("ttXlaGetColumnInfo",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNextUpdate(
                 ttXlaHandle_h handle,
		 ttXlaUpdateDesc_t ***records,
		 SQLINTEGER maxrecords,
		 SQLINTEGER *nreturned
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaNextUpdate");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaUpdateDesc_t ***, SQLINTEGER, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaNextUpdate])))(TThXla->hXla, records, maxrecords, nreturned);
    }

    TRACE_FLEAVE_RC("ttXlaNextUpdate",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNextUpdateWait(
                 ttXlaHandle_h handle,
		 ttXlaUpdateDesc_t ***records,
		 SQLINTEGER maxrecords,
		 SQLINTEGER *nreturned,
		 SQLINTEGER seconds
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaNextUpdateWait");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaUpdateDesc_t ***, SQLINTEGER, SQLINTEGER *, SQLINTEGER))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaNextUpdateWait])))(TThXla->hXla, records, maxrecords, nreturned, seconds);
    }

    TRACE_FLEAVE_RC("ttXlaNextUpdateWait",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaTableCheck(
                 ttXlaHandle_h handle,
                 ttXlaTblDesc_t *table,
                 ttXlaColDesc_t *columns,
                 SQLINTEGER *compatP
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaTableCheck");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaTblDesc_t *, ttXlaColDesc_t *, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaTableCheck])))(TThXla->hXla, table, columns, compatP);
    }

    TRACE_FLEAVE_RC("ttXlaTableCheck",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaApply(
                 ttXlaHandle_h handle,
                 ttXlaUpdateDesc_t *record,
                 SQLINTEGER test
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaApply");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaUpdateDesc_t *, SQLINTEGER))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaApply])))(TThXla->hXla, record, test);
    }

    TRACE_FLEAVE_RC("ttXlaApply",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaLookup(
                 ttXlaHandle_h handle,
                 ttXlaTblDesc_t *table,
                 void *keys,
                 void *result,
                 SQLINTEGER maxsize,
                 SQLINTEGER *retsize
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaLookup");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaTblDesc_t *, void *, void *, SQLINTEGER, SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaLookup])))(TThXla->hXla, table, keys, result, maxsize, retsize);
    }

    TRACE_FLEAVE_RC("ttXlaLookup",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaRollback(
                 ttXlaHandle_h handle
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaRollback");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaRollback])))(TThXla->hXla);
    }

    TRACE_FLEAVE_RC("ttXlaRollback",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaCommit(
                 ttXlaHandle_h handle
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaCommit");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaCommit])))(TThXla->hXla);
    }

    TRACE_FLEAVE_RC("ttXlaCommit",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaTableByName(
                 ttXlaHandle_h handle,
                 char *owner,
                 char *name,
                 SQLUBIGINT *sysTableID,
                 SQLUBIGINT *userTableID
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaTableByName");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, char *, char *, SQLUBIGINT *, SQLUBIGINT *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaTableByName])))(TThXla->hXla, owner, name, sysTableID, userTableID);
    }

    TRACE_FLEAVE_RC("ttXlaTableByName",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaDateToODBCCType(
                 void        *fromData, 
                 DATE_STRUCT *returnData
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaDateToODBCCType");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, DATE_STRUCT *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaDateToODBCCType])))(fromData, returnData);
    }

    TRACE_FLEAVE_RC("ttXlaDateToODBCCType",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaTimeToODBCCType(
                 void        *fromData, 
                 TIME_STRUCT *returnData
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaTimeToODBCCType");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, TIME_STRUCT *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaTimeToODBCCType])))(fromData, returnData);
    }

    TRACE_FLEAVE_RC("ttXlaTimeToODBCCType",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaTimeStampToODBCCType(
                 void              *fromData, 
                 TIMESTAMP_STRUCT  *returnData
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaTimeStampToODBCCType");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, TIMESTAMP_STRUCT *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaTimeStampToODBCCType])))(fromData, returnData);
    }

    TRACE_FLEAVE_RC("ttXlaTimeStampToODBCCType",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaDecimalToCString(
                 void         *fromData, 
                 char         *returnData, 
                 SQLSMALLINT   precision, 
                 SQLSMALLINT   scale
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaDecimalToCString");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, char *, SQLSMALLINT, SQLSMALLINT))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaDecimalToCString])))(fromData, returnData, precision, scale);
    }

    TRACE_FLEAVE_RC("ttXlaDecimalToCString",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNumberToCString(
                 ttXlaHandle_h handle,
                 void *fromData,
                 char *buf,
                 int buflen
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaNumberToCString");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, void *, char *, int))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaNumberToCString])))(TThXla->hXla, fromData, buf, buflen);
        }
    }

    TRACE_FLEAVE_RC("ttXlaNumberToCString",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNumberToTinyInt(
                 void *fromData,
                 SQLCHAR *tiny
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaNumberToTinyInt");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, SQLCHAR *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaNumberToTinyInt])))(fromData, tiny);
    }

    TRACE_FLEAVE_RC("ttXlaNumberToTinyInt",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNumberToSmallInt(
                 void *fromData,
                 SQLSMALLINT *smint
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaNumberToSmallInt");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, SQLSMALLINT *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaNumberToSmallInt])))(fromData, smint);
    }

    TRACE_FLEAVE_RC("ttXlaNumberToSmallInt",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNumberToInt(
                 void *fromData,
                 SQLINTEGER *ival
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaNumberToInt");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, SQLINTEGER *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaNumberToInt])))(fromData, ival);
    }

    TRACE_FLEAVE_RC("ttXlaNumberToInt",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNumberToUInt(
                 void *fromData,
                 SQLUINTEGER *ival
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaNumberToUInt");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, SQLUINTEGER *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaNumberToUInt])))(fromData, ival);
    }

    TRACE_FLEAVE_RC("ttXlaNumberToUInt",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNumberToBigInt(
                 void *fromData,
                 SQLBIGINT *bint
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaNumberToBigInt");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, SQLBIGINT *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaNumberToBigInt])))(fromData, bint);
    }

    TRACE_FLEAVE_RC("ttXlaNumberToBigInt",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaNumberToDouble(
                 void *fromData,
                 double *dbl
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaNumberToDouble");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, double *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaNumberToDouble])))(fromData, dbl);
    }

    TRACE_FLEAVE_RC("ttXlaNumberToDouble",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaOraDateToODBCTimeStamp(
                 void *fromData,
                 TIMESTAMP_STRUCT *returnData
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaOraDateToODBCTimeStamp");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, TIMESTAMP_STRUCT *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaOraDateToODBCTimeStamp])))(fromData, returnData);
    }

    TRACE_FLEAVE_RC("ttXlaOraDateToODBCTimeStamp",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaOraTimeStampToODBCTimeStamp(
                 void *fromData,
                 TIMESTAMP_STRUCT  *returnData
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaOraTimeStampToODBCTimeStamp");

    if (  DMGlobalData.directFuncs == NULL  ) 
        rc = SQL_ERROR;
    else
    {
        rc = (*((SQLRETURN (XLA_API *)( void *, TIMESTAMP_STRUCT *))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaOraTimeStampToODBCTimeStamp])))(fromData, returnData);
    }

    TRACE_FLEAVE_RC("ttXlaOraTimeStampToODBCTimeStamp",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaConvertCharType(
                 ttXlaHandle_h handle,
                 ttXlaColDesc_t *colinfo,
                 void*tup,
                 void*buf,
                 size_t buflen
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla;

    TRACE_FENTER("ttXlaConvertCharType");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
        {
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaColDesc_t *, void *, void *, size_t))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaConvertCharType])))(TThXla->hXla, colinfo, tup, buf, buflen);
        }
    }

    TRACE_FLEAVE_RC("ttXlaConvertCharType",rc);

    return rc;
}

/*
 * Undocumented public functions
 */

SQLRETURN  XLA_API
ttXlaEpilog2(
                 ttXlaHandle_h handle
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla = NULL;

    TRACE_FENTER("ttXlaEpilog2");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaEpilog2])))(TThXla->hXla);
    }

    TRACE_FLEAVE_RC("ttXlaEpilog2",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaInvalidateTbl2(
                 ttXlaHandle_h handle,
                 SQLUBIGINT    sysTableID,
                 SQLUBIGINT    userTableID
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla = NULL;

    TRACE_FENTER("ttXlaInvalidateTbl2");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, SQLUBIGINT, SQLUBIGINT))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaInvalidateTbl2])))(TThXla->hXla, sysTableID, userTableID);
    }

    TRACE_FLEAVE_RC("ttXlaConfigBuffer",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaGenerateSQL2(
                 ttXlaHandle_h       handle,
                 ttXlaUpdateDesc_t * record,
                 char              * buffer,
                 SQLINTEGER          maxlen,
                 SQLINTEGER        * actuallen,
                 char              * valptr[],
                 char              * valbuf,
                 SQLINTEGER          vmaxlen,
                 SQLINTEGER        * vactLen,
                 SQLINTEGER          ind[],
                 SQLINTEGER          maxColLen[],
                 SQLINTEGER        * valCount
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla = NULL;

    TRACE_FENTER("ttXlaGenerateSQL2");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, ttXlaUpdateDesc_t *, char *, SQLINTEGER, SQLINTEGER *, char *[], char *, SQLINTEGER, SQLINTEGER *, SQLINTEGER[], SQLINTEGER[], SQLINTEGER *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaGenerateSQL2])))(TThXla->hXla, record, buffer, maxlen, actuallen, valptr, valbuf, vmaxlen, vactLen, ind, maxColLen, valCount);
    }

    TRACE_FLEAVE_RC("ttXlaGenerateSQL2",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaTranslate(
                 ttXlaHandle_h handle,
                 void        * record
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla = NULL;

    TRACE_FENTER("ttXlaTranslate");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, void *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaTranslate])))(TThXla->hXla, record);
    }

    TRACE_FLEAVE_RC("ttXlaTranslate",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaSqlOption(
                 ttXlaHandle_h handle,
                 const char  * option
)
{
    SQLRETURN rc = SQL_SUCCESS;
    TTXlaHandle_t * TThXla = NULL;

    TRACE_FENTER("ttXlaSqlOption");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = SQL_INVALID_HANDLE;
    else
    {
        TThXla = (TTXlaHandle_t *)handle;
        if (  (TThXla->tag != TTDM_STRUCT_TAG) ||
	      (TThXla->hid != TTDM_HID_XLA)   ||
              (TThXla->hXla == NULL)  )
            rc = SQL_INVALID_HANDLE;
        else
            rc = (*((SQLRETURN (XLA_API *)(ttXlaHandle_h, const char *))(TThXla->pFuncMap->xlaFuncPtr[F_POS_XLA_ttXlaSqlOption])))(TThXla->hXla, option);
    }

    TRACE_FLEAVE_RC("ttXlaSqlOption",rc);

    return rc;
}

SQLRETURN  XLA_API
ttXlaRowidToCString(
		 void *fromData, 
		 char *buf, 
		 int buflen
)
{
    SQLRETURN rc = SQL_SUCCESS;

    TRACE_FENTER("ttXlaRowidToCString");
    rc = (*((SQLRETURN (XLA_API *)(void *, char *, int))(DMGlobalData.directFuncs->xlaFuncPtr[F_POS_XLA_ttXlaRowidToCString])))(fromData, buf, buflen);

    TRACE_FLEAVE_RC("ttXlaRowidToCString",rc);

    return rc;
}

#endif /* ENABLE_XLA_DIRECT */

#if defined(ENABLE_UTIL_LIB)

/*
 ========================================================================
                 Public functions - Utility Library
 ========================================================================
 */

/*
 * The TT utllity library has a dependency on the direct mode library.
 * We need to be sure we do not unload the direct mode library while the
 * utility library is loaded and in use. The easiest way to enforce this
 * is to allocate a global, direct mode, ODBC environment and keep this
 * allocated while the Utility library is in use. We will deallocate this
 * environment when the last Utility environment is freed and this will
 * trigger the unloading of the Utility libary in SQLFreeEnv.
*
 * These functions mostly do the bare minimum of work and simply
 * pass the call straight through to the relevant library function.
 * The two exceptions are ttUtilAllocEnv and ttUtilFreeEnv.
 */

int UTIL_API
ttUtilAllocEnv(
                       ttUtilHandle*  handle_ptr,
                       char*          errBuff,
                       unsigned int   buffLen,
                       unsigned int*  errLen
)
{
    SQLRETURN sqlrc = SQL_SUCCESS;
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttUtilAllocEnv");
    TRACE_HVAL("handle_ptr",handle_ptr);

    if (  handle_ptr == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        *handle_ptr = NULL;
        MUTEX_LOCK(DMGlobalData.utilMutex);
        if (  DMGlobalData.utilEnv == NULL  )
        { /* first call */
            sqlrc = SQLAllocEnv((void *)&(DMGlobalData.utilEnv));
            if (  sqlrc != SQL_SUCCESS  )
            {
                postUtilEnvError(TT_DM_UTIL_ENV1ERR,errBuff,buffLen,errLen);
                rc = TTUTIL_ERROR;
            }
            else
            if (  DMGlobalData.utilFuncs == NULL  )
            {
                /* utility library not loaded - maybe not present */
                postUtilEnvError(TT_DM_UTIL_LIBERR,errBuff,buffLen,errLen);
                rc = TTUTIL_ERROR;
                SQLFreeEnv(DMGlobalData.utilEnv);
                DMGlobalData.utilEnv = NULL;
            }
            else
            {
                TThUtil = (TTUtilHandle)calloc(1,sizeof(TTUtilHandle_t));
                if (  TThUtil == NULL  )
                {
                    postUtilEnvError(TT_DM_UTIL_MEMERR,errBuff,
                                          buffLen,errLen);
                    rc = TTUTIL_ERROR;
                    SQLFreeEnv(DMGlobalData.utilEnv);
                    DMGlobalData.utilEnv = NULL;
                }
                else
                {
                    TThUtil->tag = TTDM_STRUCT_TAG;
	            TThUtil->hid = TTDM_HID_UTIL;
                    rc = (*((int (UTIL_API *)(ttUtilHandle *, char *, unsigned int, unsigned int *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttUtilAllocEnv])))(&(TThUtil->hUtil), errBuff, buffLen, errLen);

#if defined(ENABLE_THREAD_SAFETY)
		    if (  rc == TTUTIL_SUCCESS  )
		    {
                        MUTEX_CREATE(TThUtil->mutex);
                        if (  TThUtil->mutex == NULL  )
			{
                            postUtilEnvError(TT_DM_UTIL_MEMERR,errBuff,
					     buffLen,errLen);
                            rc = TTUTIL_ERROR;
		        }
		    }
#endif /* ENABLE_THREAD_SAFETY */

                    if (  rc != TTUTIL_SUCCESS  )
                    {
                        free( (void *)TThUtil);
                        SQLFreeEnv(DMGlobalData.utilEnv);
                        DMGlobalData.utilEnv = NULL;
                    }
                    else
                    {
                        /* link it into global list */
                        if (  DMGlobalData.UThead == NULL  )
                        {
                            DMGlobalData.UThead = DMGlobalData.UTtail = TThUtil;
                            TThUtil->prev = TThUtil->next = NULL;
                        }
                        else
                        {
                            DMGlobalData.UTtail->next = (void *)TThUtil;
                            TThUtil->prev = DMGlobalData.UTtail;
                            TThUtil->next = NULL;
                            DMGlobalData.UTtail = TThUtil;
                        }

                        *handle_ptr = (void *)TThUtil;
                    }
                }
            }
        }
        MUTEX_UNLOCK(DMGlobalData.utilMutex);
    }

    TRACE_FLEAVE_RC("ttUtilAllocEnv",rc);

    return rc;
}

int UTIL_API
ttUtilFreeEnv(
                       ttUtilHandle  handle,
                       char*         errBuff,
                       unsigned int  buffLen,
                       unsigned int* errLen
)
{
    int  rc = TTUTIL_SUCCESS;
    SQLRETURN sqlrc = SQL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttUtilFreeEnv");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
        {
            MUTEX_LOCK(DMGlobalData.utilMutex);
            MUTEX_LOCK(TThUtil->mutex);
            rc = (*((int (UTIL_API *)(ttUtilHandle, char *, unsigned int, unsigned int *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttUtilFreeEnv])))(TThUtil->hUtil, errBuff, buffLen, errLen);
            if (  rc == TTUTIL_SUCCESS  )
            {
                /* mark invalid */
                TThUtil->tag = 0;
		TThUtil->hid = 0;

                /* unlink from master list and free */
                if  (  DMGlobalData.UThead == DMGlobalData.UTtail  )
                {
                    /* Only entry in list */
                    DMGlobalData.UThead = DMGlobalData.UTtail = NULL;
                }
                else
                if (  TThUtil == DMGlobalData.UThead  )
                {
                    /* First entry in list */
                    DMGlobalData.UThead = DMGlobalData.UThead->next;
                    DMGlobalData.UThead->prev = NULL;
                }
                else
                if (  TThUtil == DMGlobalData.UTtail  )
                {
                    /* Last entry in list */
                    DMGlobalData.UTtail = DMGlobalData.UTtail->prev;
                    DMGlobalData.UTtail->next = NULL;
                }
                else
                {
                    /* somewhere else in list */
                    TThUtil->prev->next = TThUtil->next;
                    TThUtil->next->prev = TThUtil->prev;
                }

                TThUtil->hUtil = NULL;
                TThUtil->next = TThUtil->prev = NULL;

                MUTEX_UNLOCK(TThUtil->mutex);
                MUTEX_DESTROY(TThUtil->mutex);
                free( (void *)TThUtil );

                /* check if we should free environment and so unload library */
                if (  DMGlobalData.UThead == NULL  )
                {
                    sqlrc = SQLFreeEnv(DMGlobalData.utilEnv);
                    if (  sqlrc == SQL_SUCCESS  )
                        DMGlobalData.utilEnv = NULL;
                    else
                    {
                        postUtilEnvError(TT_DM_UTIL_ENV2ERR,errBuff,
                                         buffLen,errLen);
                        rc = TTUTIL_ERROR;
                    }
                }
            }
            else
                MUTEX_UNLOCK(TThUtil->mutex);

            MUTEX_UNLOCK(DMGlobalData.utilMutex);
        }
    }

    TRACE_FLEAVE_RC("ttUtilFreeEnv",rc);

    return rc;
}

int UTIL_API
ttUtilGetErrorCount(
                       ttUtilHandle  handle,
                       unsigned int* errCount
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttUtilGetErrorCount");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, unsigned int *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttUtilGetErrorCount])))(TThUtil->hUtil, errCount);
    }

    TRACE_FLEAVE_RC("ttUtilGetErrorCount",rc);

    return rc;
}

int UTIL_API
ttUtilGetError(
                       ttUtilHandle   handle,
                       unsigned int   errIndex,
                       unsigned int*  retCode,
                       ttUtilErrType* retType,
                       char*          errBuff,
                       unsigned int   buffLen,
                       unsigned int*  errLen
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttUtilGetError");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, unsigned int, unsigned int *, ttUtilErrType *, char *, unsigned int, unsigned int *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttUtilGetError])))(TThUtil->hUtil, errIndex, retCode, retType, errBuff, buffLen, errLen);
    }

    TRACE_FLEAVE_RC("ttUtilGetError",rc);

    return rc;
}

int UTIL_API
ttBackup(
                       ttUtilHandle   handle,
                       const char*    connStr,
                       ttBackupType   type,
                       ttBooleanType  atomic,
                       const char*    backupDir,
                       const char*    baseName,
                       ttUtFileHandle stream
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttBackup");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, ttBackupType, ttBooleanType, const char *, const char *, ttUtFileHandle))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttBackup])))(TThUtil->hUtil, connStr, type, atomic, backupDir, baseName, stream);
    }

    TRACE_FLEAVE_RC("ttBackup",rc);

    return rc;
}

int UTIL_API
ttDestroyDataStore(
                       ttUtilHandle   handle,
                       const char*    connStr,
                       unsigned int   timeout
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttDestroyDataStore");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, unsigned int))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttDestroyDataStore])))(TThUtil->hUtil, connStr, timeout);
    }

    TRACE_FLEAVE_RC("ttDestroyDataStore",rc);

    return rc;
}

int UTIL_API
ttDestroyDataStoreForce(
                       ttUtilHandle   handle,
                       const char*    connStr,
                       unsigned int   timeout
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttDestroyDataStoreForce");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, unsigned int))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttDestroyDataStoreForce])))(TThUtil->hUtil, connStr, timeout);
    }

    TRACE_FLEAVE_RC("ttDestroyDataStoreForce",rc);

    return rc;
}

int UTIL_API
ttRamGrace(
                       ttUtilHandle  handle,
                       const char*   connStr,
                       unsigned int  seconds
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRamGrace");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, unsigned int))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRamGrace])))(TThUtil->hUtil, connStr, seconds);
    }

    TRACE_FLEAVE_RC("ttRamGrace",rc);

    return rc;
}

int UTIL_API
ttRamLoad(
                       ttUtilHandle  handle,
                       const char*   connStr
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRamLoad");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRamLoad])))(TThUtil->hUtil, connStr);
    }

    TRACE_FLEAVE_RC("ttRamLoad",rc);

    return rc;
}

int UTIL_API
ttRamPolicy(
                       ttUtilHandle    handle,
                       const char*     connStr,
                       ttRamPolicyType policy
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRamPolicy");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, ttRamPolicyType))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRamPolicy])))(TThUtil->hUtil, connStr, policy);
    }

    TRACE_FLEAVE_RC("ttRamPolicy",rc);

    return rc;
}

int UTIL_API
ttRamUnload(
                       ttUtilHandle  handle,
                       const char*   connStr
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRamUnload");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRamUnload])))(TThUtil->hUtil, connStr);
    }

    TRACE_FLEAVE_RC("ttRamUnload",rc);

    return rc;
}

int UTIL_API
ttRepDuplicateEx(
                       ttUtilHandle handle,
                       const char *destConnStr,
                       const char *srcDatastore,
                       const char *remoteHost,
                       ttRepDuplicateExArg *arg
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRepDuplicateEx");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, const char *, const char *, ttRepDuplicateExArg *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRepDuplicateEx])))(TThUtil->hUtil, destConnStr, srcDatastore, remoteHost, arg);
    }

    TRACE_FLEAVE_RC("ttRepDuplicateEx",rc);

    return rc;
}

int UTIL_API
ttRestore(
                       ttUtilHandle    handle,
                       const char*     connStr,
                       ttRestoreType   type,
                       const char*     backupDir,
                       const char*     baseName,
                       ttUtFileHandle  stream,
                       unsigned int    flags
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRestore");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, ttRestoreType, const char *, const char *, ttUtFileHandle, unsigned int))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRestore])))(TThUtil->hUtil, connStr, type, backupDir, baseName, stream, flags);
    }

    TRACE_FLEAVE_RC("ttRestore",rc);

    return rc;
}

int UTIL_API
ttXactIdRollback(
                       ttUtilHandle handle,
                       const char*  connStr,
                       const char*  xactId
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttXactIdRollback");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
	      (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, const char *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttXactIdRollback])))(TThUtil->hUtil, connStr, xactId);
    }

    TRACE_FLEAVE_RC("ttXactIdRollback",rc);

    return rc;
}

/************* Beyond here are not really public / supported ***********/

#if 0 /* unsupported functions */
int UTIL_API
ttUtilGetDaemonStatus(ttUtilHandle    handle,
                      char*           errBuff,
                      size_t          buffLen
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttUtilGetDaemonStatus");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, char *, size_t))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttUtilGetDaemonStatus])))(TThUtil->hUtil, errBuff, buffLen);
    }

    TRACE_FLEAVE_RC("ttUtilGetDaemonStatus",rc);

    return rc;
} // ttUtilGetDaemonStatus

int UTIL_API
ttRamPersist(ttUtilHandle  handle,
             const char*   connStr
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRamPersist");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRamPersist])))(TThUtil->hUtil, connStr);
    }

    TRACE_FLEAVE_RC("ttRamPersist",rc);

    return rc;
} // ttRamPersist

int UTIL_API
ttRamNoPersist(ttUtilHandle  handle,
               const char*   connStr
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRamNoPersist");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRamNoPersist])))(TThUtil->hUtil, connStr);
    }

    TRACE_FLEAVE_RC("ttRamNoPersist",rc);

    return rc;
} // ttRamNoPersist

int UTIL_API
ttXactIdRollbackExecuting(ttUtilHandle handle,
                          const char*  connStr,
                          const char*  xactId,
                          bool*        rollbackExecuting_ret
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttXactIdRollbackExecuting");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, const char *, bool *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttXactIdRollbackExecuting])))(TThUtil->hUtil, connStr, xactId, rollbackExecuting_ret);
    }

    TRACE_FLEAVE_RC("ttXactIdRollbackExecuting",rc);

    return rc;
} // ttXactIdRollbackExecuting

int UTIL_API
ttXactIdCommit(ttUtilHandle handle,
               const char*  connStr,
               const char*  xactId
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttXactIdCommit");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, const char *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttXactIdCommit])))(TThUtil->hUtil, connStr, xactId);
    }

    TRACE_FLEAVE_RC("ttXactIdCommit",rc);

    return rc;
} // ttXactIdCommit


int UTIL_API
ttRepDuplicateCheck(const char *srcDataStore,
                    const char *remoteHost,
                    int   remoteDaemonPort
)
{
    int  rc = TTUTIL_SUCCESS;

    TRACE_FENTER("ttRepDuplicateCheck");

    rc = (*((int (UTIL_API *)(const char *, const char *, int))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRepDuplicateCheck])))(srcDataStore, remoteHost, remoteDaemonPort);

    TRACE_FLEAVE_RC("ttRepDuplicateCheck",rc);

    return rc;
} // ttRepDuplicateCheck

int UTIL_API
ttReadEpochs(const char* dbName,
             const char* pathname,
              int*        epCntP,
             void*       epArrayP,
             char*       errBuf,
             int         errBufSz
)
{
    int  rc = TTUTIL_SUCCESS;

    TRACE_FENTER("ttReadEpochs");

    rc = (*((int (UTIL_API *)(const char *, const char *, int*, void *, char *, int))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttReadEpochs])))(dbName, pathname, epCntP, epArrayP, errBuf, errBufSz);

    TRACE_FLEAVE_RC("ttReadEpochs",rc);

    return rc;
} // ttReadEpochs

int UTIL_API
ttRepRoleCheck(ttUtilHandle handle,
               const char *destConnStr,
               const char *srcDataStore,
               const char *remoteHost,
               ttRepGetStateRoleArg *arg
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRepRoleCheck");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, const char *, const char *, ttRepGetStateRoleArg *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRepRoleCheck])))(TThUtil->hUtil, destConnStr, srcDataStore, remoteHost, arg);
    }

    TRACE_FLEAVE_RC("ttRepRoleCheck",rc);

    return rc;
} // ttRepRoleCheck

int UTIL_API
ttRepGetStateRole(ttUtilHandle handle,
                  const char *destConnStr,
                  const char *srcDataStore,
                  const char *remoteHost,
                  ttRepGetStateRoleArg *arg
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRepGetStateRole");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, const char *, const char *, ttRepGetStateRoleArg *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRepGetStateRole])))(TThUtil->hUtil, destConnStr, srcDataStore, remoteHost, arg);
    }

    TRACE_FLEAVE_RC("ttRepGetStateRole",rc);

    return rc;
} // ttRepGetStateRole

int UTIL_API
ttRepGetCTNCksum(ttUtilHandle handle,
                 const char *destConnStr,
                 const char *srcDataStore,
                 const char *remoteHost,
                 void* storeId,
                 ttRepGetCTNCksumArg *arg
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRepGetCTNCksum");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, const char *, const char *, void *, ttRepGetCTNCksumArg *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRepGetCTNCksum])))(TThUtil->hUtil, destConnStr, srcDataStore, remoteHost, storeId, arg);
    }

    TRACE_FLEAVE_RC("ttRepGetCTNCksum",rc);

    return rc;
} // ttRepGetCTNCksum


int UTIL_API
ttRepGetSubState(ttUtilHandle handle,
                 const char *destConnStr,
                 const char *srcDataStore,
                 const char *remoteHost,
                 void *storeId,
                 ttRepGetSubStateArg *arg
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttRepGetSubState");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, const char *, const char *, void *, ttRepGetSubStateArg *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttRepGetSubState])))(TThUtil->hUtil, destConnStr, srcDataStore, remoteHost, storeId, arg);
    }

    TRACE_FLEAVE_RC("ttRepGetSubState",rc);

    return rc;
} // ttRepGetSubState

int UTIL_API
ttDisconnect(ttUtilHandle            handle,
             const char*             connStr,
             ttDisconnectUrgency     urgency,
             ttDisconnectGranularity granularity,
             ttBooleanType           wait,
             ttBooleanType           allowGrid
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttDisconnect");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *, ttDisconnectUrgency, ttDisconnectGranularity, ttBooleanType, ttBooleanType))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttDisconnect])))(TThUtil->hUtil, connStr, urgency, granularity, wait, allowGrid);
    }

    TRACE_FLEAVE_RC("ttDisconnect",rc);

    return rc;
} // ttDisconnect

int UTIL_API
ttUtDbOpen(ttUtilHandle  handle,
           const char*   connStr
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttUtDbOpen");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttUtDbOpen])))(TThUtil->hUtil, connStr);
    }

    TRACE_FLEAVE_RC("ttUtDbOpen",rc);

    return rc;
} // ttUtDbOpen

int UTIL_API
ttUtDbClose(ttUtilHandle  handle,
            const char*   connStr
)
{
    int  rc = TTUTIL_SUCCESS;
    TTUtilHandle TThUtil = NULL;

    TRACE_FENTER("ttUtDbClose");
    TRACE_HVAL("handle",handle);

    if (  handle == NULL  )
        rc = TTUTIL_INVALID_HANDLE;
    else
    {
        TThUtil = (TTUtilHandle)handle;
        if (  (TThUtil->tag != TTDM_STRUCT_TAG) ||
              (TThUtil->hid != TTDM_HID_UTIL)   ||
              (TThUtil->hUtil == NULL)  )
            rc = TTUTIL_INVALID_HANDLE;
        else
            rc = (*((int (UTIL_API *)(ttUtilHandle, const char *))(DMGlobalData.utilFuncs->utilFuncPtr[F_POS_UTIL_ttUtDbClose])))(TThUtil->hUtil, connStr);
    }

    TRACE_FLEAVE_RC("ttUtDbClose",rc);

    return rc;
} // ttUtDbClose

#endif /* unsupported functiins */

#endif /* ENABLE_UTIL_LIB */

/**************************************************************************/

