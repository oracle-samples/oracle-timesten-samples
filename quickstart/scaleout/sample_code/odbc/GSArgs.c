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
 *  GSArgs.c - parse command line arguments
 *
 ****************************************************************************/

#include "GridSample.h"

/****************************************************************************
 *
 * Internal functions
 *
 ****************************************************************************/

/*
 * Disable and enable echo on terminal. Used for password entry.
 *
 * INPUTS:
 *     echo_on - true if echo should be enabled, false to disable it
 *
 * OUTPUTS: None
 *
 * RETURNS: 
 *   True or false to indicate if the requested action succeeded.
 */
static boolean
setEcho(
        boolean echo_on
       )
{
#if defined(WINDOWS)
    HANDLE h;
    DWORD mode;

    h = GetStdHandle( STD_INPUT_HANDLE );
    if (  ! GetConsoleMode( h, &mode )  )
        return false;

    if (  echo_on  )
        mode |= ENABLE_ECHO_INPUT;
    else
        mode &= ~ENABLE_ECHO_INPUT;

    if (  ! SetConsoleMode( h, mode )  )
        return false;

    return true;
#else /* ! WINDOWS */
    struct termios tios;
    int fd = 0;

    if (  tcgetattr( fd, &tios ) == -1  )
        return false;

    if (  echo_on  )
    {
      tios.c_lflag |= ECHO;
      tios.c_lflag &= ~ECHONL;
    } 
    else
    {
      tios.c_lflag &= ~ECHO;
      tios.c_lflag |= ECHONL;
    }

    if (  tcsetattr(  fd, TCSADRAIN, &tios ) == -1  )
        return false;

    return true;
#endif
} // setEcho

/*
 * Prompt for and read a password without echo. Uses 'setEcho()'.
 *
 * INPUTS:
 *     prompt - optional prompt to display (can be NULL)
 *
 * OUTPUTS: None
 *
 * RETURNS: 
 *     Pointer to a static area containing the entered password, or NULL if
 *     an error occurred.
 */
static char *
getPassword(
            char const * prompt
           )
{
    static char pswd[MAX_LEN_PWD+2];
    char  *p;

    /* Display the password prompt */
    if (  prompt != NULL  )
        printf("%s", prompt);
  
    /* Do not echo the password to the console */
    setEcho( false );

    /* get the password */
    fgets(pswd, MAX_LEN_PWD+2, stdin);

    /* Turn back on console output */
    setEcho( true );

    // did we get a newline?
    p = strrchr( pswd, '\n' );
    if (  p == NULL  )
        return NULL; // no, that's an error
    // strip it off
    *p = '\0';

    return pswd;
} // getPassword

/*
 * Parse the -txnmix option, extract the values and save them in the context.
 *
 * INPUTS:
 *     ctxt   - pointer to a valid context_t
       txnmix - the value for the -txnmix option
 *
 * OUTPUTS: None
 *
 * RETURNS: 
 *     True if the vaue was valid or false if it was invalid.
 */
static boolean
parseTxnMix(
            context_t  * ctxt,
            char const * txnmix
           )
{
    char buff[32];
    char * token, * string = buff;
    int i, tot, val[5];

    if (  (ctxt == NULL) || (txnmix == NULL) || (*txnmix == '\0')  )
        return false;
    if (  strlen( txnmix ) > 19  )
        return false;
    strcpy( buff, txnmix );

    // We expect 5 comma separated positive integers in the range 0 to 100.
    // The sum of the integers must be exactly 100.
    for (tot = 0, i = 0; i < 5; i++)
    {
        token = strsep( &string, "," );
        if (  token == NULL  )
            return false;
        if (  ! isSimpleInt( token )  )
            return false;
        val[i] = atoi( token );
        if (  (val[i] < 0) || (val[i] > 100)  )
            return false;
        if (  (tot += val[i]) > 100  )
            return false;
    }
    token = strsep( &string, "," );
    if (  (token != NULL) || (tot != 100)  )
        return false;

    ctxt->pctAuthorize = val[0];
    ctxt->pctCharge = val[1];
    ctxt->pctTopUp = val[2];
    ctxt->pctQuery = val[3];
    ctxt->pctPurge = val[4];

    return true;
} // parseTxnMix

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

/*
 * Parse and validate the command line arguments. 
 *
 * Stores the extracted values in the context.
 *
 * INPUTS:
 *     ctxt   - pointer to a valid context_t
 *     argc   - numbver of command line arguments
 *     argv   - array of pointers to the arguments
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code. Also displays error messages as required.
 */
int
parseArgs(
          context_t  * ctxt,
          int          argc,
          char const * argv[]
         )
{
    int argno = 1;
    boolean foundDSN = false;
    boolean foundUID = false;
    boolean foundPWD = false;
    boolean foundTxnMix = false;
    boolean foundNoCleanup = false;
    boolean foundNumTxn = false;
    boolean foundDuration = false;
    boolean foundSilent = false;
    boolean foundVerbose = false;
    boolean foundLog = false;
    boolean foundDebug = false;
#if defined(SPECIAL_FEATURES)
    boolean foundCommitrotxn = false;
#endif // SPECIAL_FEATURES
    int rptIntvl;
    long numTxn = 0L;
    long duration = 0L;
    int l;
    char * p;
    char prompt[50+MAX_LEN_UID];

    if (  (ctxt == NULL) || (argc < 1) || (argv == NULL)  )
    {
        fprintf(stderr, "*** %s", ERRM_PARAM_INTERNAL );
        return ERR_PARAM_INTERNAL;
    }

    // extract program name
    p = strrchr( argv[0], '/' );
    if (  p != NULL  )
        ctxt->progName = p + 1;
    else
        ctxt->progName = argv[0];
    
    // parse command line options
    while (  argno < argc  )
    {
        // -help
        if (  strcmp( argv[argno], OPT_HELP ) == 0  )
        {
            displayUsage( ctxt );
            return ERR_HELP;
        }
        else

        // -verbose
        if (  strcmp( argv[argno], OPT_VERBOSE ) == 0  )
        {
            if (  foundVerbose  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_VERBOSE );
                return ERR_PARAM;
            }
            if (  foundSilent  )
            {
                fprintf( stderr,
                         "*** Cannot specify both '%s' and '%s' options\n",
                         OPT_SILENT, OPT_VERBOSE );
                return ERR_PARAM;
            }
            foundVerbose = true;
            ctxt->vbLevel = VB_VERBOSE;
            if (  ++argno < argc  )
            {
                if ( isSimpleInt( argv[argno] )  )
                {
                    rptIntvl = atoi(argv[argno]);
                    if ( rptIntvl< VB_MIN_RPT_INTERVAL  )
                    {
                        fprintf( stderr,
                          "*** Value for '%s' option must be at least %d\n",
                                  OPT_VERBOSE, VB_MIN_RPT_INTERVAL);
                        return ERR_PARAM;
                    }
                    ctxt->vbInterval = rptIntvl;
                }
                else
                    argno -= 1;
            }
        }
        else

        // -silent
        if (  strcmp( argv[argno], OPT_SILENT ) == 0  )
        {
            if (  foundSilent  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_SILENT );
                return ERR_PARAM;
            }
            if (  foundVerbose  )
            {
                fprintf( stderr,
                         "*** Cannot specify both '%s' and '%s' options\n",
                         OPT_VERBOSE, OPT_SILENT );
                return ERR_PARAM;
            }
            foundSilent = true;
            ctxt->vbLevel = VB_SILENT;
        }
        else

        // -nocleanup
        if (  strcmp( argv[argno], OPT_NOCLEANUP ) == 0  )
        {
            if (  foundNoCleanup  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_NOCLEANUP );
                return ERR_PARAM;
            }
            foundNoCleanup = true;
            ctxt->doCleanup = false;
        }
        else

        // -txnmix
        if (  strcmp( argv[argno], OPT_TXNMIX ) == 0  )
        {
            if (  foundTxnMix  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_TXNMIX );
                return ERR_PARAM;
            }
            foundTxnMix = true;
            if (  ++argno >= argc  )
            {
                fprintf( stderr,
                         "*** Missing value for '%s' option\n",
                         OPT_TXNMIX );
                return ERR_PARAM;
            }
            if (  ! parseTxnMix( ctxt, argv[argno] )  )
            {
                fprintf( stderr,
                         "*** Invalid value for '%s' option\n",
                         OPT_TXNMIX );
                return ERR_PARAM;
            }
        }
        else

        // -dsn
        if (  strcmp( argv[argno], OPT_DSN ) == 0  )
        {
            if (  foundDSN  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_DSN );
                return ERR_PARAM;
            }
            foundDSN = true;
            if (  ++argno >= argc  )
            {
                fprintf( stderr,
                         "*** Missing value for '%s' option\n",
                         OPT_DSN );
                return ERR_PARAM;
            }
            l = strlen(argv[argno]);
            if (  (l < 1) || (l > MAX_LEN_DSN)  )
            {
                fprintf( stderr,
                         "*** Invalid value for '%s' option\n",
                         OPT_DSN );
                return ERR_PARAM;
            }
            ctxt->dbDSN = argv[argno];
        }
        else

        // -uid
        if (  strcmp( argv[argno], OPT_UID ) == 0  )
        {
            if (  foundUID  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_UID );
                return ERR_PARAM;
            }
            foundUID = true;
            if (  ++argno >= argc  )
            {
                fprintf( stderr,
                         "*** Missing value for '%s' option\n",
                         OPT_UID );
                return ERR_PARAM;
            }
            l = strlen(argv[argno]);
            if (  (l < 1) || (l > MAX_LEN_UID)  )
            {
                fprintf( stderr,
                         "*** Invalid value for '%s' option\n",
                         OPT_UID );
                return ERR_PARAM;
            }
            ctxt->dbUID = argv[argno];
        }
        else

        // -pwd
        if (  strcmp( argv[argno], OPT_PWD ) == 0  )
        {
            if (  foundPWD  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_PWD );
                return ERR_PARAM;
            }
            foundPWD = true;
            if (  ++argno >= argc  )
            {
                fprintf( stderr,
                         "*** Missing value for '%s' option\n",
                         OPT_PWD );
                return ERR_PARAM;
            }
            l = strlen(argv[argno]);
            if (  l > MAX_LEN_PWD  )
            {
                fprintf( stderr,
                         "*** Invalid value for '%s' option\n",
                         OPT_PWD );
                return ERR_PARAM;
            }
            ctxt->dbPWD = argv[argno];
        }
        else

        // -log
        if (  strcmp( argv[argno], OPT_LOG ) == 0  )
        {
            if (  foundLog  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_LOG );
                return ERR_PARAM;
            }
            if (  foundDebug  )
            {
                fprintf( stderr,
                         "*** Cannot specify both '%s' and '%s' options\n",
                         OPT_DEBUG, OPT_LOG );
                return ERR_PARAM;
            }
            foundLog = true;
            if (  ++argno >= argc  )
            {
                fprintf( stderr,
                         "*** Missing value for '%s' option\n",
                         OPT_LOG );
                return ERR_PARAM;
            }
            ctxt->logPath = argv[argno];
        }
        else

        // -debug
        if (  strcmp( argv[argno], OPT_DEBUG ) == 0  )
        {
#if defined(DEBUG)
            if (  foundDebug  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_DEBUG );
                return ERR_PARAM;
            }
            if (  foundLog  )
            {
                fprintf( stderr,
                         "*** Cannot specify both '%s' and '%s' options\n",
                         OPT_LOG, OPT_DEBUG );
                return ERR_PARAM;
            }
            foundDebug = true;
            if (  ++argno >= argc  )
            {
                fprintf( stderr,
                         "*** Missing value for '%s' option\n",
                         OPT_DEBUG );
                return ERR_PARAM;
            }
            ctxt->logPath = argv[argno];
            ctxt->debugMode = true;
#else
            fprintf( stderr,
                     "*** The '%s' option is not enabled\n",
                     OPT_DEBUG );
            return ERR_PARAM;
#endif // DEBUG
        }
        else

        // -numtxn
        if (  strcmp( argv[argno], OPT_NUMTXN ) == 0  )
        {
            if (  foundNumTxn  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_NUMTXN );
                return ERR_PARAM;
            }
            if (  foundDuration  )
            {
                fprintf( stderr,
                         "*** Cannot specify both '%s' and '%s' options\n",
                         OPT_DURATION, OPT_NUMTXN );
                return ERR_PARAM;
            }
            if (  ++argno >= argc  )
            {
                fprintf( stderr,
                         "*** Missing value for '%s' option\n",
                         OPT_NUMTXN );
                return ERR_PARAM;
            }
            if ( ! isSimpleLong( argv[argno] )  )
            {
                fprintf( stderr,
                         "*** Invalid value for '%s' option\n",
                         OPT_NUMTXN );
                return ERR_PARAM;
            }
            numTxn = atol(argv[argno]);
            if (  numTxn <= 0L  )
            {
                fprintf( stderr,
                         "*** Invalid value for '%s' option\n",
                         OPT_NUMTXN );
                return ERR_PARAM;
            }
            foundNumTxn = true;
            ctxt->limitExecution = true;
            ctxt->runNumTxn = numTxn;
        }
        else

        // -duration
        if (  strcmp( argv[argno], OPT_DURATION ) == 0  )
        {
            if (  foundDuration  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_DURATION );
                return ERR_PARAM;
            }
            if (  foundNumTxn  )
            {
                fprintf( stderr,
                         "*** Cannot specify both '%s' and '%s' options\n",
                         OPT_NUMTXN, OPT_DURATION );
                return ERR_PARAM;
            }
            if (  ++argno >= argc  )
            {
                fprintf( stderr,
                         "*** Missing value for '%s' option\n",
                         OPT_DURATION );
                return ERR_PARAM;
            }
            if ( ! isSimpleLong( argv[argno] )  )
            {
                fprintf( stderr,
                         "*** Invalid value for '%s' option\n",
                         OPT_DURATION );
                return ERR_PARAM;
            }
            duration = atol(argv[argno]);
            if (  duration <= 0L  )
            {
                fprintf( stderr,
                         "*** Invalid value for '%s' option\n",
                         OPT_DURATION );
                return ERR_PARAM;
            }
            foundDuration = true;
            ctxt->limitExecution = true;
            ctxt->runDuration = duration;
        }
        else

#if defined(SPECIAL_FEATURES)
        // -commitrotxn
        if (  strcmp( argv[argno], OPT_COMMITROTXN ) == 0  )
        {
            if (  foundCommitrotxn  )
            {
                fprintf( stderr,
                         "*** Multiple '%s' options not permitted\n",
                         OPT_COMMITROTXN );
                return ERR_PARAM;
            }
            foundCommitrotxn = true;
            ctxt->commitReadTxn = true;
        }
        else
#endif // SPECIAL_FEATURES

        // Invalid!
        {
            fprintf( stderr, "*** Invalid option '%s'\n", argv[argno] );
            return ERR_PARAM;
        }

        argno += 1;
    }

    // If no password specified, then prompt for one
    if (  ! foundPWD  )
    {
        sprintf( prompt, "%s: Password for '%s'? ",
                 getTS(), ctxt->dbUID );
        ctxt->dbPWD = getPassword( prompt );
    }

    // Check for 'null' password (empty string)
    if (  (ctxt->dbPWD != NULL) && (*(ctxt->dbPWD) == '\0')  )
        ctxt->dbPWD = NULL;

    // If logging or debug was specified, open the file
    if (  ctxt->logPath != NULL  )
    {
        ctxt->logOut = fopen( ctxt->logPath, "w" );
        if (  ctxt->logOut == NULL  )
        {
            fprintf( stderr,
                     "*** Unable to open log file '%s'\n",
                     ctxt->logPath );
            return ERR_PARAM;
        }
    }

    return SUCCESS;
} // parseArgs

