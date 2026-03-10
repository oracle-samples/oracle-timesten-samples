/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

#ifndef RUNTEST_UTILS
#define RUNTEST_UTILS

#if defined(WIN32)
# include <windows.h>
#else
# include <unistd.h>
# include <termios.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <tt_errCode.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Signal handling */
#include <signal.h>

typedef void (*handler_t) (int);

/* Call this function to setup ctrl-c handlers and the like
   Returns 0 on success.
 */
extern int HandleSignals(void);

/* Clears the stoprequest flag
   Call this at program initialization */
extern void StopRequestClear(void);

/* Polls the stoprequest flag.
   Non-zero return means an application should stop
  (e.g. when a signal has been received) */
extern int  StopRequested(void);

/* Returns the signal number that was received
   Zero means no signal was received */
extern int SigReceived(void);

/* DBMS types (used to enable DBMS-specific features) */

#define DBMS_UNKNOWN    0
#define DBMS_TIMESTEN   1
#define DBMS_MSSQL      2

/* Special Native Error codes used for simulating WaitForConnect semantics */

#define PANIC_RECOVERY_ERROR        704
#define DB_CREATE_IN_PROGRESS       711
#define DB_IN_USE_ERROR             839
#define DB_EXISTS                   854
#define INVALIDATE_IN_PROGRESS      9991
#define FINAL_CKPT_IN_PROGRESS      9993
#define RECOVERY_IN_PROGRESS        9994
#define FORCED_ROLLBACK_IN_PROGRESS 9995
#define EXAMINE_IN_PROGRESS         9996
#define REQUIRES_MASTER_CATCHUP     8110
#define MASTER_CATCHUP_IN_PROGRESS  8111

/* Special Native Error codes used for determing corrupt data store */
#define BAD_CONNECT            846
#define DIRTY_BYTE             994



/* exit macro */
/* NOTE: UNIX macro forces sleep before exiting if
 *       error occurred to give the calling process
 *       time to finish reading before program exits
 */
#ifdef WIN32
#define app_exit(x) \
exit(x)
#else
#define app_exit(x) \
do { \
  fflush(stdout); \
  fflush(stderr); \
  if (!isatty(fileno(stderr))) \
    tt_sleep(2); \
  if (1) /* fool the compiler to avoid "statement not reached" warnings */ \
    exit(x); \
} while (0)
#endif

/*
 *              Exit flags for handle_errors routine
 *
 * These flags are passed to the handle_errors routine to indicate how
 * various kinds of errors should be handled.
 *
 * JUST_EXIT: print a short error message (do not call SQLError) and exit
 *     the application without disconnecting.  This code is typically used
 *     after an unsuccessful call to SQLAllocEnv, after which you cannot
 *     call SQLError (and you are not yet connected to a datastore).
 *
 * JUST_DISCONNECT_EXIT: print a short error message (do not call
 *     SQLError), disconnect from the datastore and exit the application.
 *     This code is used when an application encounters an error that is
 *     not ODBC-related after connecting to a datastore.
 *
 * ERROR_EXIT: print a full error message (calls SQLError) and exit
 *     without disconnecting from the datastore.  This code is used when an
 *     ODBC error occurs before a connection has been made (eg. an
 *     unsuccessful call to SQLAllocConnect or SQLDriverConnect).
 *
 * DISCONNECT_EXIT: print a full error message (calls SQLError),
 *     disconnect from the datastore and exit the application.  This code
 *     is used after an unsuccessful ODBC call when a connection is active
 *     but no transaction is active (it does not attempt to roll back).
 *
 * ABORT_DISCONNECT_EXIT: print a full error message (calls SQLError),
 *     rollback the current transaction, disconnect from the datastore, and
 *     exit the application.  This is the most commonly used flag.
 *
 * NO_EXIT: print a full error message (calls SQLError) and return the
 *     native error code.
 */

#define NO_EXIT                 0
#define ERROR_EXIT              1
#define DISCONNECT_EXIT         -2
#define ABORT_DISCONNECT_EXIT   -3
#define JUST_DISCONNECT_EXIT    -4
#define JUST_EXIT               -5

/*
 *              Abort flags for the QuitTest routine
 *
 * NO_XA_ABORT: do not attempt a rollback before disconnecting from the
 *              datastore.
 *
 * XA_ABORT:    roll back the current transaction before disconnecting from
 *              the datastore.
 */

#define NO_XA_ABORT             0
#define XA_ABORT                1

/* message macros for all normal output */

#define out_msg0(str) \
{fprintf(stdout,str);fflush(stdout);}

#define out_msg1(str,arg1) \
{fprintf(stdout,str,arg1);fflush(stdout);}

#define out_msg2(str,arg1,arg2) \
{fprintf(stdout,str,arg1,arg2);fflush(stdout);}

#define out_msg3(str,arg1,arg2,arg3) \
{fprintf(stdout,str,arg1,arg2,arg3);fflush(stdout);}

#define out_msg4(str,arg1,arg2,arg3,arg4) \
{fprintf(stdout,str,arg1,arg2,arg3,arg4);fflush(stdout);}

#define out_msg5(str,arg1,arg2,arg3,arg4,arg5) \
{fprintf(stdout,str,arg1,arg2,arg3,arg4,arg5);fflush(stdout);}

#define out_msg6(str,arg1,arg2,arg3,arg4,arg5,arg6) \
{fprintf(stdout,str,arg1,arg2,arg3,arg4,arg5,arg6);fflush(stdout);}

#define out_msg7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7) \
{fprintf(stdout,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7);fflush(stdout);}

#define out_msg8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8) \
{fprintf(stdout,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8);fflush(stdout);}


/* message macros used for all conditional non-error output */

#define status_msg0(str) \
{if (verbose >= VERBOSE_DFLT) {fprintf(statusfp, str); fflush(statusfp);}}

#define status_msg1(str, arg1) \
{if (verbose >= VERBOSE_DFLT) {fprintf(statusfp, str, arg1); fflush(statusfp);}}

#define status_msg2(str, arg1, arg2) \
{if (verbose >= VERBOSE_DFLT) {fprintf(statusfp, str, arg1, arg2); fflush(statusfp);}}

#define status_msg3(str, arg1, arg2, arg3) \
{if (verbose >= VERBOSE_DFLT) {fprintf(statusfp, str, arg1, arg2, arg3); fflush(statusfp);}}

/* message macros used for error output */

#define err_msg0(str) \
{fflush(stdout); fprintf(stderr, "*** " str); fflush(stderr);}

#define err_msg0b(str) \
{fflush(stdout); fprintf(stderr, str); fflush(stderr);}

#define err_msg1(str, arg1) \
{fflush(stdout); fprintf(stderr, "*** " str, arg1); fflush(stderr);}

#define err_msg2(str, arg1, arg2) \
{fflush(stdout); fprintf(stderr, "*** " str, arg1, arg2); fflush(stderr);}

#define err_msg3(str, arg1, arg2, arg3) \
{fflush(stdout); fprintf(stderr, "*** " str, arg1, arg2, arg3); fflush(stderr);}

#define err_msg7(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
{fflush(stdout); fprintf(stderr, "*** " str, arg1, arg2, arg3, arg4, arg5, arg6, arg7); fflush(stderr);}

#define CONN_STR_LEN    512     /* Length of connection string */
#define MSG_LNG         512     /* Maximum message length */

SQLUINTEGER display_size (SQLSMALLINT coltype, SQLUINTEGER collen, SQLCHAR* colname);

int handle_errors(
                  SQLHDBC       hdbc,
                  SQLHSTMT      hstmt,
                  SQLRETURN     rc,
                  int           abort,
                  char*         msg,
                  char          *filename,
                  int           lineno);

extern void QuitTest(
                     SQLHENV    henv,
                     SQLHDBC    hdbc,
                     int        exitStatus,
                     int        abortXA);

extern void
tt_MemErase(volatile void *buf, 
                        size_t        len);


extern int IsGridEnable(SQLHDBC hdbc);

#if defined(WIN32)
  #define tt_sleep(sec)   Sleep((sec) * 1000)
#else
  #define tt_sleep(sec)   sleep(sec);
#endif

#endif /* RUNTEST_UTILS */

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
