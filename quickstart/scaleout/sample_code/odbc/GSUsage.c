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
 *  GSUsage.c - usage information
 *
 ****************************************************************************/

#include "GridSample.h"

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

/*
 * Display program usage information.
 *
 * INPUTS:
 *     ctxt - pointer to a valid context_t.
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
displayUsage(
             context_t const * ctxt
            )
{
    char const * progName;

    progName = (ctxt == NULL)?DFLT_PROGNAME:ctxt->progName;

    printf(
    "\nUsage:\n\n"
    "    %s [-help] [-dsn dsname] [-uid username] [-pwd password]\n"
    "        [-txnmix A,C,T,Q,P] [-nocleanup] [{-numtxn t | -duration s}]\n"
#if defined(DEBUG)
    "        [{-silent | -verbose [n]}] [{-log logpath | -debug logpath}]\n\n"
#else
    "        [{-silent | -verbose [n]}] [-log logpath]\n\n"
#endif // DEBUG
    "Parameters:\n\n"
    "  -help      - Displays this usage information and then exits. Any\n"
    "               other options specified are ignored.\n\n"
    "  -dsn       - Connect to 'dsname' instead of the default DSN (%s).\n"
    "               The DSN must be of the correct type (direct or client/server).\n\n"
    "  -uid       - Connect as user 'username' instead of the default user\n"
    "               (%s).\n\n"
    "  -pwd       - Connect using 'password'. If omitted then the user will\n"
    "               be prompted for the password.\n\n"
    "  -txnmix    - Sets the transaction mix for the workload; A = percentage\n"
    "               of Authorize, C = percentage of Charge, T = percentage of\n"
    "               TopUp, Q = percentage of Query and P = percentage of Purge.\n"
    "               All values are integers >= 0 and <= 100 and the sum of the\n"
    "               values must equal 100. If not specified, the default mix\n"
    "               is A=%d, C=%d, T=%d, Q=%d and P=%d.\n\n"
    "  -nocleanup - The transaction history table will not be truncated prior\n"
    "               to starting the workload.\n\n"
    "  -numtxn    - The workload will run until it has completed 't' transactions\n"
    "               (t > 0) and then the program will terminate.\n\n"
    "  -duration  - The workload will run for approximately 's' seconds (s > 0)\n"
    "               and then the program will terminate.\n\n"
    "  -silent    - The program will not produce any output other than to report\n"
    "               errors. Normally the program will report significant events\n"
    "               (connect, failover, ...) as it runs plus a workload summary\n"
    "               when it terminates.\n\n"
    "  -verbose   - Execution statistics will be reported every 'n' seconds\n"
    "               (n > 0, default is %d) as the program runs, in addition to\n"
    "               the normal reporting.\n\n"
    "  -log       - Write an execution log to 'logpath'.\n\n"
#if defined(DEBUG)
    "  -debug     - Write a debug log to 'logpath'. This log includes much\n"
    "               more detailed information than the regular log; the\n"
    "               extra lines include the flag 'DEBUG:' in the text.\n\n"

    "  The '-log' and '-debug' options are mutually exclusive.\n\n"
#endif // DEBUG
    "  The '-numtxn' and '-duration' options are mutually exclusive. If neither\n"
    "  is specified then the program will run until it is manually terminated\n"
    "  using Ctrl-C, SIGTERM etc.\n\n"
    "  The '-silent' and '-verbose' options are mutually exclusive.\n\n"
    "Exit status:\n\n"
    "  The exit code reflects the outcome of execution as follows:\n"
    "    0  - Success\n"
    "    1  - Parameter error\n"
    "    2  - Help requested\n"
    "   >2  - Fatal error encountered\n\n"
    , progName, DFLT_DSN, DFLT_UID, DFLT_PCT_AUTHORIZE, DFLT_PCT_CHARGE,
    DFLT_PCT_TOPUP, DFLT_PCT_QUERY, DFLT_PCT_PURGE, VB_DFLT_RPT_INTERVAL );
} // displayUsage

