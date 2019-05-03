/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/* This source is best displayed with a tabstop of 4 */

#ifdef WIN32
  #include <windows.h>
  #include <sys/timeb.h>
  #include <time.h>
  #include <process.h>
  #include <winbase.h>
#else
  #include <sys/types.h>
  #include <sys/ipc.h>
  #include <sys/shm.h>
  #include <time.h>
  #include <sys/time.h>
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <sys/wait.h>
  #include <errno.h>
  #include <sqlunix.h>
#endif

#include <stdlib.h>
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <math.h>
#include "utils.h"
#include "tt_version.h"
#include "timesten.h"
#include "ttgetopt.h"

#if defined(SCALEOUT)
#if ! defined(ROUTINGAPI)
#define ROUTINGAPI
#endif /* ! ROUTINGAPI */
#endif /* SCALEOUT */

#define VERBOSE_NOMSGS   0
#define VERBOSE_RESULTS  1    /* for results (and err msgs) only */
#define VERBOSE_DFLT     2    /* the default for the cmdline demo */
#define VERBOSE_ALL      3
#define VERBOSE_FIRST VERBOSE_NOMSGS
#define VERBOSE_LAST  VERBOSE_ALL

#define XS(str) #str
#define S(str)  XS(str)

#define NO_VALUE           -1
#define MIN_KEY             10
#define DBMODE_ID          -1
#define DBMODE_NB          -1
#define DBMODE_FILLER       " "
#define SEL_DBMODE          "SELECT descr FROM VPN_USERS WHERE vpn_id = -1 AND vpn_nb = -1"
#define M_CLASSIC           0
#define DBMODE_CLASSIC      "CLASSIC"
#if defined(SCALEOUT)
#define MAX_KVALUE          5
#define M_SCALEOUT          1
#define DBMODE_SCALEOUT     "SCALEOUT"
#if defined(ROUTINGAPI)
#define M_SCALEOUT_LOCAL    2
#if defined(TTCLIENTSERVER)
#define M_SCALEOUT_ROUTING  3
#endif /* TTCLIENTSERVER */
#endif /* ROUTINGAPI */
#endif /* SCALEOUT */
#define DFLT_XACT           100000
#define DFLT_SEED           1
#define DFLT_PROC           1
#define DFLT_RAMPTIME       10
#define DFLT_READS          80
#define DFLT_INSERTS        0
#define DFLT_DELETES        0
#define DFLT_OPS            1
#define DFLT_NODBEXEC       0
#define DFLT_KEY            100
#define DFLT_ISO            1
#define DFLT_RANGE          0
#define DFLT_BUILDONLY      0
#define DFLT_NOBUILD        0
#define DFLT_THROTTLE       0
#define DFLT_INSERT_MOD     1
#define DFLT_PROCID         0

#define PROC_INITIALIZED 0
#define PROC_READY       1
#define PROC_SET         2
#define PROC_GO          3
#define PROC_RUNNING     4
#define PROC_STARTBENCH  5
#define PROC_MEASURING   6
#define PROC_STOPBENCH   7
#define PROC_STOPPING    8
#define PROC_STOP        9
#define PROC_END         10

#define TTCSERVERDSN     "TTC_SERVER_DSN="

static char usageStr[] =
  "\n"
  "This program implements a multi-user throughput benchmark using\n"
#ifdef TTCLIENTSERVER
  "client-server connection mode.\n\n"
#else
  "direct connection mode.\n\n"
#endif /* TTCLIENTSERVER */
  "Usage:\n\n"
  "  %s {-h | -help}\n"
  "  %s [-proc <nprocs>] [-read <nreads>] [-insert <nins>] [-delete <ndels>]\n"
  "     [-throttle <n>] [{-xact <xacts> | -sec <seconds> [-ramp <rseconds>]}]\n"
  "     [-ops <ops>] [-key <keys>] [-range] [-iso <level>] [-seed <seed>]\n"
  "     [-build] [-nobuild] [-v <level>]"
#ifdef SCALEOUT
#if defined(TTCLIENTSERVER) && defined(ROUTINGAPI)
  " [-scaleout [local | routing]]"
#elif defined(ROUTINGAPI)
  " [-scaleout [local]]"
#else
  " [-scaleout]"
#endif /* ROUTINGAPI */
#endif /* SCALEOUT */
  "\n     [<DSN> | -connstr <connection-string>]\n\n"
  "  -h                  Prints this message and exits.\n"
  "  -help               Same as -h.\n"
  "  -V                  Prints version number and exits.\n"
  "  -proc    <nprocs>   Specifies that <nprocs> is the number of concurrent\n"
  "                      processes. The default is " S(DFLT_PROC) ".\n"
  "  -read    <nreads>   Specifies that <nreads> is the percentage of read-only\n"
  "                      transactions. The default is " S(DFLT_READS) ".\n"
  "  -insert  <nins>     Specifies that <nins> is the percentage of insert\n"
  "                      transactions. The default is " S(DFLT_INSERTS) ".\n"
  "  -delete  <ndels>    Specifies that <ndels> is the percentage of delete\n"
  "                      transactions. The default is " S(DFLT_DELETES) ".\n"
  "  -xact    <xacts>    Specifies that <xacts> is the number of transactions\n"
  "                      that each process should run. The default is " S(DFLT_XACT) ".\n"
  "  -sec     <seconds>  Specifies that <seconds> is the test measurement duration.\n"
  "                      The default is to run in transaction mode (-xact).\n"
  "  -ramp  <rseconds>   Specifies that <rseconds> is the ramp up & down time in\n"
  "                      duration mode (-sec). Default is " S(DFLT_RAMPTIME) ".\n"
  "  -ops     <ops>      Operations per transaction.  The default is " S(DFLT_OPS) ".\n"
  "                      In the special case where 0 is specified, no commit\n"
  "                      is done. This may be useful for read-only testing.\n"
  "  -key     <keys>     Specifies the number of records (squared) to initially\n"
  "                      populate in the database. The minimum value is " S(MIN_KEY) "\n"
  "                      and the default is " S(DFLT_KEY) " (" S(DFLT_KEY) "**2 rows). The same\n"
  "                      value should be specified at both build and run time.\n"
  "  -range              Use a range index for the primary key instead of a hash\n"
  "                      index.\n"
  "  -iso     <level>    Locking isolation level\n"
  "                         0 = serializable\n"
  "                         1 = read-committed (default)\n"
  "  -seed    <seed>     Specifies that <seed> should be the seed for the\n"
  "                      random number generator. Must be > 0, default is " S(DFLT_SEED) ".\n"
  "  -throttle <n>       Throttle each process to <n> operations per second.\n"
  "                      Must be > 0. The default is no throttle.\n"
  "  -build              Only build the database, do not run the benchmark. Only\n"
#if defined(SCALEOUT)
  "                      the '-key', '-range' and '-scaleout' parameters are\n"
  "                      relevant.\n"
#else /* ! SCALEOUT */
  "                      the '-key' and '-range' parameters are relevant.\n"
#endif /* ! SCALEOUT */
  "  -nobuild            Only run the benchmark, do not build the database.\n"
  "                      The '-range' parameter is not relevant.\n"
#if 0
  "  -nodbexec           Don't perform any of the actual database operations in\n"
  "                      the main benchmark loop. This allows you to get a sense\n"
  "                      for the cost of the 'application overhead'.\n"
#endif
#if defined(SCALEOUT)
  "  -scaleout           Run in Scaleout mode. Craetes the table with a hash\n"
  "                      distribution and adapts runtime behaviour for Scaleout.\n"
  "                      You must use the same value at build time and run-time.\n"
#if defined(ROUTINGAPI)
  "      local           Constrain all data access to be to rows in the locally\n"
  "                      connected database element; each process generates keys\n"
  "                      that it knows refer to rows in the element that it is\n"
  "                      connected to. Only relevant at run-time.\n"
#if defined(TTCLIENTSERVER)
  "      routing         Use the routing API to optimize data access. Each\n"
  "                      process maintains a connection to every database\n"
  "                      element and uses the routing API to direct operations\n"
  "                      to an element that it knows contains the target row.\n"
  "                      Only relevant at run time. Cannot be used with -ops > 1.\n"
#endif /* TTCLIENTSERVER */
#endif /* ROUTINGAPI */
#endif /* SCALEOUT */
  "  -v <level>          Verbosity level\n"
  "                         0 = errors only\n"
  "                         1 = results only\n"
  "                         2 = results and some status messages (default)\n"
  "                         3 = all messages\n\n"
  "If no DSN or connection string is specified, the default is\n"
  "  \"DSN=sampledb;UID=appuser\".\n\n"
  "The percentage of update operations is 100 minus the percentages of reads,\n"
  "inserts and deletes.\n\n"
  "For the most accurate results, use duration mode (-sec) with a measurement\n"
  "time of at least several minutes and a ramp time of at least 30 seconds.\n\n";

#define DFLT_DSN DEMODSN
#define DFLT_UID UIDNAME

/* message macros used for all conditional non-error output */

#define tptbm_msg0(str) \
{ \
  if (verbose >= VERBOSE_DFLT) \
  { \
    fprintf (statusfp, str); \
    fflush (statusfp); \
  } \
}

#define tptbm_msg1(str, arg1) \
{ \
  if (verbose >= VERBOSE_DFLT) \
  { \
    fprintf (statusfp, str, arg1); \
    fflush (statusfp); \
  } \
}

#define tptbm_msg2(str, arg1, arg2) \
{ \
  if (verbose >= VERBOSE_DFLT) \
  { \
    fprintf (statusfp, str, arg1, arg2); \
    fflush (statusfp); \
  } \
}

#define tptbm_msg5(str, arg1, arg2, arg3, arg4, arg5) \
{ \
  if (verbose >= VERBOSE_DFLT) \
  { \
    fprintf (statusfp, str, arg1, arg2, arg3, arg4, arg5); \
    fflush (statusfp); \
  } \
}

/* Forward declarations */
void ExecuteTptBm (int seed, int procId);

void erasePassword(volatile char *buf, size_t len);

void getPassword(const char * prompt, const char * uid, char * pswd, size_t len);

int parseConnectionString(
                      char * connstr,
                      char * pDSN,
                      int    sDSN,
                      char * pUID,
                      int    sUID,
                      char * pPWD,
                      int    sPWD
                     );

#ifdef WIN32
  #define strncasecmp     _strnicmp
  #define snprintf        _snprintf

  typedef struct _timeb   ttTime;

  #define ttGetTime(p)    _ftime(p)

  tt_ptrint diff_time (ttTime*  start,
                       ttTime*  end)
  {
    return ((end->time - start->time) * 1000 +
            (end->millitm - start->millitm));
  }
  #define tt_yield() Sleep(0)
#else
  typedef struct timeval  ttTime;

  #define ttGetTime(p)    gettimeofday(p, NULL)

  tt_ptrint diff_time (ttTime*  start,
                       ttTime*  end)
  {
    return ((end->tv_sec - start->tv_sec) * 1000 +
            (end->tv_usec - start->tv_usec) / 1000);
  }
#include <sched.h>
#define tt_yield() sched_yield()
#endif

#define CACHELINE_SIZE 128 /* ideal for x8664 and big enough for every platform except Itanium */

typedef struct procinfo {
  volatile int       state;
  volatile int       pid;
  volatile UBIGINT   xacts;
  char      pad[CACHELINE_SIZE - sizeof(int) - sizeof(UBIGINT)];
} procinfo_t;

#if defined(SCALEOUT) && defined(ROUTINGAPI)
/* ODBC routing API */
TTGRIDMAP   routingGridMap;
TTGRIDDIST  routingHDist;
/* The distribution key has two columns */
SQLSMALLINT routingCTypes[]   = { SQL_C_SLONG, SQL_C_SLONG };
SQLSMALLINT routingSQLTypes[] = { SQL_INTEGER, SQL_INTEGER };
SQLLEN      routingMaxSizes[] = { sizeof(int), sizeof(int) };

int         nElements = 0;
int         kFactor = 0;

#ifdef TTCLIENTSERVER
#define MAX_CLIENT_DSN_LEN  128
typedef struct _ttclientdsn {
  int       elementid;
  int       repset;
  int       dataspace;
  SQLHDBC   hdbc;
  SQLHSTMT  selstmt;
  SQLHSTMT  updstmt;
  SQLHSTMT  insstmt;
  SQLHSTMT  delstmt;
  SQLCHAR   clientdsn[MAX_CLIENT_DSN_LEN+1];
} cCliDSN_t;

cCliDSN_t*  routingDSNs = NULL;
#endif /* TTCLIENTSERVER */

/* End of ODBC routing API */
#endif /* SCALEOUT && ROUTINGAPI*/

/* Global variable declarations */

int        rand_seed = NO_VALUE;       /* seed for the random numbers */
int        num_processes = NO_VALUE;   /* # of concurrent processes for the test */
int        duration = NO_VALUE;        /* test duration */
int        ramptime = NO_VALUE;        /* ramp time in the duration mode */
int        reads = NO_VALUE;           /* read percentage */
int        inserts = NO_VALUE;         /* insert percentage */
int        deletes = NO_VALUE;         /* delete percentage */
int        num_xacts = NO_VALUE;       /* # of transactions per process */
int        opsperxact = NO_VALUE;      /* operations per transaction or 0 for no commit */
int        nodbexec = NO_VALUE;        /* don't do actual db work in main benchmark loop */
int        key_cnt = NO_VALUE;         /* number of keys (squared) populated in the datastore */
int        isolevel = NO_VALUE;        /* isolation level */
int        rangeFlag = NO_VALUE;       /* if 1 use range index instead of hash */
int        verbose = NO_VALUE;         /* verbose level */
FILE*      statusfp;                   /* File for status messages */
char       dsn[CONN_STR_LEN] = "";     /* ODBC data source */
char*      connstr_opt = NULL;         /* ODBC connStr from cmd line */
char*      input_connStr = NULL;       /* connection string to be used */
char       connstr[CONN_STR_LEN] = "";
int        buildOnly = NO_VALUE;       /* Only create/populate */
int        noBuild = NO_VALUE;         /* Don't create/populate */
int        throttle = NO_VALUE;        /* throttle to <n> ops / second */
int        mode = M_CLASSIC;           /* classic vs. scaleout mode */
int        insert_mod = NO_VALUE;      /* used to prevent multiple inserts clobber */
#if defined(SCALEOUT) && defined(ROUTINGAPI)
int        elementid = 0;              /* locally connected element id */
int        replicasetid = 0;           /* locally connected replicaset id */
int        dataspaceid = 0;            /* locally connected dataspace id */
#if defined(TTCLIENTSERVER)
char *     serverdsn = NULL;           /* server DSN for routing mode */
#endif /* TTCLIENTSERVER */
#endif /* SCALEOUT && ROUTINGAPI */

SQLHENV    henv = SQL_NULL_HENV;  /* ODBC environment handle */
SQLHDBC    ghdbc = SQL_NULL_HDBC; /* global ODBC connection handle */
#if defined(SCALEOUT) && defined(ROUTINGAPI)
SQLHDBC    rhdbc = SQL_NULL_HDBC; /* routing API connection handle */
#endif /* SCALEOUT && ROUTINGAPI */

#if defined(WIN32)
HANDLE     shmHndl;           /* handle to shared memory segment */
#else
int        shmid;             /* shared memory segment id  */
#endif
volatile procinfo_t *shmhdr = NULL;   /* shared memory segment to sync processes */
int        procId = NO_VALUE; /* process # in shared memory array */
int        sigReceived;       /* zero if no signal received or signum */

/* tmp file used for shmget() key generation */
char keypath[] = "/tmp/tptbmXXXXXX";

#ifdef WIN32
STARTUPINFO         sInfo = {8L, NULL, NULL, NULL,
                             0L, 0L, 0L, 0L, 0L, 0L, 0L,
                             STARTF_USESHOWWINDOW, SW_HIDE, 0L,
                             NULL, NULL, NULL, NULL};

PROCESS_INFORMATION pInfo;
#endif


/* The select statement used for the read transaction */
char* select_stmnt = "select directory_nb,last_calling_party,descr from vpn_users "
                     "where vpn_id = ? and vpn_nb= ?";

/* The update statement used for the write transaction */
char* update_stmnt = "update vpn_users set last_calling_party = ? "
                     "where vpn_id = ? and vpn_nb = ?";

/* The create table statement */
char* create_stmnt = "CREATE TABLE vpn_users("
                     "vpn_id             TT_INT   NOT NULL,"
                     "vpn_nb             TT_INT   NOT NULL,"
                     "directory_nb       CHAR(10)  NOT NULL,"
                     "last_calling_party CHAR(10)  NOT NULL,"
                     "descr              CHAR(100) NOT NULL,"
                     "PRIMARY KEY (vpn_id,vpn_nb))";

char * hash_clause = " unique hash on (vpn_id,vpn_nb) pages = %d";

char * dist_clause = " distribute by hash(vpn_id, vpn_nb)";

char* drop_stmnt = "DROP TABLE vpn_users";

/* The insert statement used to populate the datastore */
char * insert_stmnt = "insert into vpn_users values (?,?,?,?,?)";

/* The delete statement */
char * delete_stmnt = "delete from vpn_users where vpn_id = ? and vpn_nb = ?";

static char cmdname[80];      /* stripped command name */

char connstr_real[CONN_STR_LEN]; /* ODBC Connection String used */
char connstr_no_password[CONN_STR_LEN]; /* Hide password in childs command-line parameters */
char username[MAX_USERNAME_SIZE];
char password[MAX_PASSWORD_SIZE];
char * passwordPrompt = "Enter password for ";

/*********************************************************************
 *
 *  FUNCTION:       usage
 *
 *  DESCRIPTION:    This function displays the programme usage info.
 *
 *  PARAMETERS:     char * progname - program name
 *
 *  RETURNS:        Nothing, exits
 *
 *********************************************************************/

void usage( char * progname )
{
  fprintf( stderr, usageStr, progname, progname );
  exit( 10 );
}

/*********************************************************************
 *
 *  FUNCTION:       nameOfMode
 *
 *  DESCRIPTION:    Maps a numeric run-time mode to its name.
 *
 *  PARAMETERS:     int mode - the runtime mode
 *                  char ** subname - pointer to receive submode name
 *
 *  RETURNS:        Name of mode or NULL.
 *
 *********************************************************************/

char * nameOfMode( int mode, char ** subname )
{
    char * tname = NULL;
    char * tsubname = NULL;

    switch ( mode )
    {
        case M_CLASSIC:
            tname = "CLASSIC";
            break;
#if defined(SCALEOUT)
        case M_SCALEOUT:
            tname = "SCALEOUT";
            break;
#if defined(ROUTINGAPI)
        case M_SCALEOUT_LOCAL:
            tname = "SCALEOUT";
            tsubname = "LOCAL";
            break;
#if defined(TTCLIENTSERVER)
        case M_SCALEOUT_ROUTING:
            tname = "SCALEOUT";
            tsubname = "ROUTING";
            break;
#endif /* TTCLIENTSERVER */
#endif /* ROUTINGAPI */
#endif /* SCALEOUT */
        default:
            break;
    }
    if (  subname != NULL  )
        *subname = tsubname;

    return tname;
}

/*********************************************************************
 *
 *  FUNCTION:       isnumeric
 *
 *  DESCRIPTION:    Checks if a string represents a valid unsigned
 *                  integer value in the range 0 to 999999999.
 *
 *  PARAMETERS:     char * str
 *
 *  RETURNS:        -1 = invalid value otherwise the value of the integer
 *
 *********************************************************************/

int isnumeric( char * str )
{
    int val = 0;

    if (  (str == NULL) || (*str == '\0')  )
        return -1;
    if (  strlen( str ) > 9  )
        return -1;
    while (  *str  ) {
        if (  (*str < '0') || (*str > '9')  )
            return -1;
        val = (val * 10) + (*str++ - '0');
    }

    return val;
}

/*********************************************************************
 *
 *  FUNCTION:       getServerDSN
 *
 *  DESCRIPTION:    Extracts the value of TTC_SERVER_DSN from a
 *                  full connection string.
 *
 *  PARAMETERS:     char * connstr - the connection string
 *                  char ** dsnval - returned pointer to a malloc'd string
 *
 *  RETURNS:        0 - failure; 1 - success;
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int getServerDSN( char * connstr, char ** dsnval )
{
    char * p;
    char * dsn;

    p = strstr( connstr, TTCSERVERDSN );
    if (  p == NULL  )
        return 0;
    dsn = p + strlen(TTCSERVERDSN);
    if (  (*dsn == '\0') || (*dsn == ';')  )
        return 0;
    p = strchr( dsn, ';' );
    if (  p != NULL  )
        *p = '\0';
    *dsnval = dsn;

    return 1;
}

/*********************************************************************
 *
 *  FUNCTION:       parse_args
 *
 *  DESCRIPTION:    This function parses the command line arguments
 *                  passed to main(), setting the appropriate global
 *                  variables and issuing a usage message for
 *                  invalid arguments.
 *
 *  PARAMETERS:     int argc        # of arguments from main()
 *                  char *argv[]    arguments from main()
 *
 *  RETURNS:        0 - failure; 1 - success;
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int parse_args(int      argc,
               char**   argv)
{
  int argno = 1;
  int pos = 0;

  ttc_getcmdname(argv[0], cmdname, sizeof(cmdname));

  while (  argno < argc )
  {
    if (  (strcmp(argv[argno], "-h") == 0) || 
          (strcmp(argv[argno], "-help") == 0)  )
        usage( cmdname );
    else
    if (  strcmp(argv[argno], "-proc") == 0  ) {
      if (  (++argno >= argc) || (num_processes != NO_VALUE)  )
          usage( cmdname );
      num_processes = isnumeric( argv[argno] );
      if (  num_processes <= 0  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-read") == 0  ) {
      if (  (++argno >= argc) || (reads != NO_VALUE)  )
          usage( cmdname );
      reads = isnumeric( argv[argno] );
      if (  (reads < 0) || (reads > 100)  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-insert") == 0  ) {
      if (  (++argno >= argc) || (inserts != NO_VALUE)  )
          usage( cmdname );
      inserts = isnumeric( argv[argno] );
      if (  (inserts < 0) || (inserts > 100)  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-delete") == 0  ) {
      if (  (++argno >= argc) || (deletes != NO_VALUE)  )
          usage( cmdname );
      deletes = isnumeric( argv[argno] );
      if (  (deletes < 0) || (deletes > 100)  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-xact") == 0  ) {
      if (  (++argno >= argc) || ( num_xacts != NO_VALUE) || 
            (duration != NO_VALUE ) || (ramptime != NO_VALUE)  )
          usage( cmdname );
      num_xacts = isnumeric( argv[argno] );
      if (  num_xacts <= 0  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-sec") == 0  ) {
      if (  (++argno >= argc) || ( duration != NO_VALUE) || 
            (num_xacts != NO_VALUE )  )
          usage( cmdname );
      duration = isnumeric( argv[argno] );
      if (  duration <= 0  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-ramp") == 0  ) {
      if (  (++argno >= argc) || ( ramptime != NO_VALUE) || 
            (num_xacts != NO_VALUE )  )
          usage( cmdname );
      ramptime = isnumeric( argv[argno] );
      if (  ramptime < 0  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-ops") == 0  ) {
      if (  (++argno >= argc) || (opsperxact != NO_VALUE)  )
          usage( cmdname );
      opsperxact = isnumeric( argv[argno] );
      if (  opsperxact < 0  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-key") == 0  ) {
      if (  (++argno >= argc) || (key_cnt != NO_VALUE)  )
          usage( cmdname );
      key_cnt = isnumeric( argv[argno] );
      if (  key_cnt < MIN_KEY  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-range") == 0  ) {
      if (  rangeFlag != NO_VALUE  )
          usage( cmdname );
      rangeFlag = 1;
    }
    else
    if (  strcmp(argv[argno], "-nodbexec") == 0  ) {
      if (  nodbexec != NO_VALUE  )
          usage( cmdname );
      nodbexec = 1;
    }
    else
    if (  strcmp(argv[argno], "-iso") == 0  ) {
      if (  (++argno >= argc) || (isolevel != NO_VALUE)  )
          usage( cmdname );
      isolevel = isnumeric( argv[argno] );
      if (  (isolevel < 0) || (isolevel > 1)  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-seed") == 0  ) {
      if (  (++argno >= argc) || (rand_seed != NO_VALUE)  )
          usage( cmdname );
      rand_seed = isnumeric( argv[argno] );
      if (  rand_seed <= 0  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-throttle") == 0  ) {
      if (  (++argno >= argc) || (throttle != NO_VALUE)  )
          usage( cmdname );
      throttle = isnumeric( argv[argno] );
      if (  throttle <= 0  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-v") == 0  ) {
      if (  (++argno >= argc) || (verbose != NO_VALUE)  )
          usage( cmdname );
      verbose = isnumeric( argv[argno] );
      if (  (verbose < VERBOSE_FIRST) || (verbose > VERBOSE_LAST)  )
          usage( cmdname );
    }
    else
    if (  strcmp(argv[argno], "-build") == 0  ) {
      if (  (buildOnly != NO_VALUE) || (noBuild != NO_VALUE)  )
          usage( cmdname );
      buildOnly = 1;
    }
    else
    if (  strcmp(argv[argno], "-nobuild") == 0  ) {
      if (  (buildOnly != NO_VALUE) || (noBuild != NO_VALUE)  )
          usage( cmdname );
      noBuild = 1;
    }
    else
    if (  strcmp(argv[argno], "-insertmod") == 0  ) {
      if (  (++argno >= argc) || (insert_mod != NO_VALUE)  ) {
          usage( cmdname );
      }
      insert_mod = isnumeric( argv[argno] );
      if (  insert_mod <= 0  ) {
          fprintf( stderr, "Value for '-insertmod' must be > 0.\n");
          exit(1);
      }
    }
#ifdef WIN32
    else
    if (  strcmp(argv[argno], "-procid") == 0  ) {
      if (  (++argno >= argc) || (procId != NO_VALUE)  ) {
          usage( cmdname );
      }
      procId = isnumeric( argv[argno] );
      if (  procId <= 0  ) {
          fprintf( stderr, "Value for '-procid' must be > 0.\n");
          exit(1);
      }
    }
#endif /* WIN32 */
#if defined(SCALEOUT)
    else
    if (  strcmp(argv[argno], "-scaleout") == 0  ) {
      if (  mode != M_CLASSIC  )
          usage( cmdname );
      mode = M_SCALEOUT;
#if defined(ROUTINGAPI)
      if (  ((argno+1) < argc) && (strcmp(argv[argno+1],"local") == 0)  ) {
          mode = M_SCALEOUT_LOCAL;
          argno += 1;
      }
#if defined(TTCLIENTSERVER)
      else
      if (  ((argno+1) < argc) && (strcmp(argv[argno+1],"routing") == 0)  ) {
          mode = M_SCALEOUT_ROUTING;
          argno += 1;
      }
#endif /* TTCLIENTSERVER */
#endif /* ROUTINGAPI */
    }
#endif /* SCALEOUT */
    else
    if (  strcmp(argv[argno], "-connstr") == 0  ) {
        if (  (++argno >= argc) || (connstr_opt != NULL) || (dsn[0] != '\0')  )
            usage( cmdname );
        connstr_opt = strdup( argv[argno] );
    }
    else {
        if (  (connstr_opt != NULL) || (dsn[0] != '\0')  )
            usage( cmdname );
        if (  (strlen(argv[argno])+1) > sizeof(dsn)  )
            usage( cmdname );
        strcpy( dsn, argv[argno] );
    }
    argno += 1;
  }

  /* Check if we have a DSN which is really a connection string */
  if ( dsn[0] )
  {
      if (  (strchr(dsn,';') != NULL) || (strncasecmp(dsn, "DSN=", 4) == 0)  )
      {
          connstr_opt = strdup( dsn );
          dsn[0] = '\0';
      }
  }

  /* Assign defaults and computed values as required */
  if (  duration != NO_VALUE  ) {
      if (  ramptime == NO_VALUE  )
          ramptime = DFLT_RAMPTIME;
      num_xacts = 0;
  }
  else {
    if (  ramptime != NO_VALUE  )
        usage( cmdname );
    if (  num_xacts == NO_VALUE  )
      num_xacts = DFLT_XACT;
    duration = 0;
  }
  if (  rand_seed == NO_VALUE  )
    rand_seed = DFLT_SEED;
  if (  num_processes == NO_VALUE  )
     num_processes = DFLT_PROC;
  if (  reads == NO_VALUE  )
      reads = DFLT_READS;
  if (  inserts == NO_VALUE  )
      inserts = DFLT_INSERTS;
  if (  deletes == NO_VALUE  )
      deletes = DFLT_DELETES;
  if (  opsperxact == NO_VALUE  )
      opsperxact = DFLT_OPS;
  if (  key_cnt == NO_VALUE  )
      key_cnt = DFLT_KEY;
  if (  isolevel == NO_VALUE  )
      isolevel = DFLT_ISO;
  if (  verbose == NO_VALUE  )
      verbose = VERBOSE_DFLT;
  if (  rangeFlag == NO_VALUE  )
      rangeFlag = DFLT_RANGE;
  if (  nodbexec == NO_VALUE  )
      nodbexec = DFLT_NODBEXEC;
  if (  throttle == NO_VALUE  )
      throttle = DFLT_THROTTLE;
  if (  buildOnly == NO_VALUE  )
      buildOnly = DFLT_BUILDONLY;
  if (  noBuild == NO_VALUE  )
      noBuild = DFLT_NOBUILD;
  if (  insert_mod == NO_VALUE  )
      insert_mod = DFLT_INSERT_MOD;
  if (  procId == NO_VALUE  )
      procId = DFLT_PROCID;
  
  if (num_processes != 1)
    insert_mod = num_processes;

  /* Various checks on inputs */
#if defined(SCALEOUT) && defined(ROUTINGAPI) && defined(TTCLIENTSERVER)
  if (  mode == M_SCALEOUT_ROUTING  )
  {
      if (  opsperxact > 1  )
      {
        err_msg0("Cannot use -ops > 1 with -scaleout routing.\n");
        return 0;
      }
      if (  (reads < 100) && (opsperxact == 0)  )
      {
        err_msg0("Cannot use -ops = 0 with -scaleout routing and reads < 100.\n");
        return 0;
      }
  }
#endif /* SCALEOUT && ROUTINGAPI && TTCLIENTSERVER */

  if ((reads + inserts + deletes) > 100)
  {
    err_msg0("insert + read + delete must not exceed 100.\n");
    return 0;
  }

  if ((insert_mod * (num_xacts / 100 * (float)inserts)) >
      (key_cnt * key_cnt))
  {
    err_msg0("Inserts as part of transaction mix exceed number\n"
             "of rows initially populated into database.\n");
    return 0;
  }

  if ((insert_mod * (num_xacts / 100 * (float)deletes)) >
      (key_cnt * key_cnt))
  {
    err_msg0("Deletes as part of transaction mix exceed number\n"
             "of rows initially populated into database.\n");
    return 0;
  }

  if ( !dsn[0] && (connstr_opt == NULL)  ) { // apply defaults
    /* No connection string or DSN given, use the default. */
      connstr_opt = strdup( "DSN=" DFLT_DSN ";UID=" DFLT_UID );
    }

  /* process connection string or DSN */
  if (dsn[0]) {
    /* Got a DSN, turn it into a connection string. */
    if (strlen(dsn)+5 >= sizeof(connstr)) {
      err_msg1("DSN '%s' too long.\n", dsn);
      exit(-1);
    }
    sprintf(connstr, "DSN=%s", dsn);
    strcpy(connstr_no_password, connstr);
  } else if (connstr_opt != NULL) {
    /* Got a connection string. */
    if (  ! parseConnectionString( connstr_opt,
                                   dsn, sizeof(dsn),
                                   username, sizeof(username),
                                   password, sizeof(password) )  )
        usage( cmdname );
    if (  procId == 0  )
    {
        if ( username[0] && !password[0] )
          getPassword(passwordPrompt, username, (char *) password, sizeof(password) );
#ifdef TTCLIENTSERVER
        if (  !username[0] || !password[0]  )
        {
            err_msg0("Client/server mode requires both a username and a password.\n");
            return 0;
        }
#endif
    }

    pos = 1 + strlen( "DSN=" ) + strlen(dsn);
    if (  username[0]  )
       pos += strlen( ";UID=" ) + strlen( username );
    if (  password[0]  )
       pos += strlen( ";PWD=" ) + strlen( password );
    if (  pos > sizeof(connstr)  ) {
      err_msg0("Connection string is too long.\n");
      exit(-1);
    }

    pos = 0;
    pos += sprintf( connstr+pos, "DSN=%s", dsn );
    if (  username[0]  )
        pos += sprintf( connstr+pos, ";UID=%s", username );
    strcpy( connstr_no_password, connstr );
    if (  password[0]  ) 
        pos += sprintf( connstr+pos, ";PWD=%s", password );
  } 

  return 1;
}

#if defined(SCALEOUT) && defined(ROUTINGAPI)
/*********************************************************************
 *
 *  FUNCTION:       routingAPI_checkLocal
 *
 *  DESCRIPTION:    Checks if a key corresponds to a row in the
 *                  locally connected element.
 *
 *  PARAMETERS:     int * k1 - vpn_id value
 *                  int * k2 - vpn_nb value
 *
 *  RETURNS:        1 if key is local, 0 if not
 *
 *********************************************************************/

int routingAPI_isLocal(int* k1, int* k2)
{
  int         i;
  SQLSMALLINT elems[MAX_KVALUE];

  /* Clear any existing key values */
  ttGridDistClear(rhdbc, routingHDist);
  /* Set values for the two key columns */
  ttGridDistValueSet(rhdbc, routingHDist, 1, k1, sizeof(int));
  ttGridDistValueSet(rhdbc, routingHDist, 2, k2, sizeof(int));
  /* Get the corresponding element IDs using the key values */
  ttGridDistElementGet(rhdbc, routingHDist, elems, nElements);

  /* The matching function should take replica and database sets into account
   * for now use a trivial match, that may not use all connected processes
   */
  if (  mode == M_SCALEOUT_LOCAL  )
  {
      for (i = 0; i < kFactor; i++)
          if (  elementid == elems[i]  )
              return 1;
  }

  return 0;
}

#if defined(TTCLIENTSERVER)
/*********************************************************************
 *
 *  FUNCTION:       routingAPI_getConn
 *
 *  DESCRIPTION:    Gets the connection (HDBC) for an element
 *                  containing a specific key.
 *
 *  PARAMETERS:     int * k1 - vpn_id value
 *                  int * k2 - vpn_nb value
 *
 *  RETURNS:        1 if key is local, 0 if not
 *
 *********************************************************************/

SQLHDBC routingAPI_getConn(int* k1, int* k2,
                           SQLHSTMT *sel, SQLHSTMT* upd,
                           SQLHSTMT *ins, SQLHSTMT* del )
{
  int         i;
  double      dElements = (double)nElements;
  SQLSMALLINT elems[MAX_KVALUE];
  cCliDSN_t*  dsn;

  /* Clear any existing key values */
  ttGridDistClear(rhdbc, routingHDist);
  /* Set values for the two key columns */
  ttGridDistValueSet(rhdbc, routingHDist, 1, k1, sizeof(int));
  ttGridDistValueSet(rhdbc, routingHDist, 2, k2, sizeof(int));
  /* Get the corresponding element IDs using the key values */
  ttGridDistElementGet(rhdbc, routingHDist, elems, nElements);

  /* The matching function should take replica and database sets into account
   * for now use a trivial match, that may not use all connected processes
   */
  if (  mode == M_SCALEOUT_ROUTING  )
  {
      i = (int)((kFactor * rand()) / ((unsigned int)RAND_MAX + 1));
      dsn = &(routingDSNs[elems[i]]);
      *sel = dsn->selstmt;
      *upd = dsn->updstmt;
      *ins = dsn->insstmt;
      *del = dsn->delstmt;
//fprintf(stderr,"DEBUG: %d,%d: %d=%d\n", *k1, *k2, i, elems[i]);
      return dsn->hdbc;
  }
  return SQL_NULL_HDBC;
}
#endif /* TTCLIENTSERVER */

/*********************************************************************
 *
 *  FUNCTION:       freeRouting
 *
 *  DESCRIPTION:    Free resources used by routing API.
 *
 *  PARAMETERS:     hdbc - an open database connection
 *
 *  RETURNS:        Nothing.
 *
 *********************************************************************/

void freeRouting( void )
{
    int i, rc;

#if defined(TTCLIENTSERVER)
    if (  routingDSNs != NULL  )
    {
        for (i = 1; i <= nElements; i++)
        {
            /* commit, just in case */
            rc = SQLTransact(henv, routingDSNs[i].hdbc, SQL_COMMIT);
            handle_errors(routingDSNs[i].hdbc, NULL, rc, DISCONNECT_EXIT, 
                           "Unable to commit transaction",
                           __FILE__, __LINE__);
            /* drop and free the statement handles */
            rc = SQLFreeStmt(routingDSNs[i].updstmt, SQL_DROP);
            handle_errors (routingDSNs[i].hdbc, routingDSNs[i].updstmt, rc, ABORT_DISCONNECT_EXIT,
                           "freeing statement handle (routing)",
                           __FILE__, __LINE__);
            routingDSNs[i].updstmt = SQL_NULL_HSTMT;
            rc = SQLFreeStmt(routingDSNs[i].selstmt, SQL_DROP);
            handle_errors (routingDSNs[i].hdbc, routingDSNs[i].selstmt, rc, ABORT_DISCONNECT_EXIT,
                           "freeing statement handle (routing)",
                           __FILE__, __LINE__);
            routingDSNs[i].selstmt = SQL_NULL_HSTMT;
            rc = SQLFreeStmt(routingDSNs[i].insstmt, SQL_DROP);
            handle_errors (routingDSNs[i].hdbc, routingDSNs[i].insstmt, rc, ABORT_DISCONNECT_EXIT,
                           "freeing statement handle (routing)",
                           __FILE__, __LINE__);
            routingDSNs[i].insstmt = SQL_NULL_HSTMT;
            rc = SQLFreeStmt(routingDSNs[i].delstmt, SQL_DROP);
            handle_errors (routingDSNs[i].hdbc, routingDSNs[i].delstmt, rc, ABORT_DISCONNECT_EXIT,
                           "freeing statement handle (routing)",
                           __FILE__, __LINE__);
            routingDSNs[i].delstmt = SQL_NULL_HSTMT;
            /* disconnect and free connection handle */
            rc = SQLDisconnect(routingDSNs[i].hdbc);
            handle_errors (routingDSNs[i].hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                           "disconnecting from database (routing)",
                           __FILE__, __LINE__);
            rc = SQLFreeConnect(routingDSNs[i].hdbc);
            handle_errors (routingDSNs[i].hdbc, routingDSNs[i].delstmt, rc, 
                           ABORT_DISCONNECT_EXIT,
                           "freeing statement handle (routing)",
                           __FILE__, __LINE__);
            routingDSNs[i].hdbc = SQL_NULL_HDBC;
        }
        free( (void *)routingDSNs );
        routingDSNs = NULL;
    }
#endif /* TTCLIENTSERVER */
    rc = ttGridDistFree(rhdbc, routingHDist);
    handle_errors(rhdbc, NULL, rc, JUST_DISCONNECT_EXIT,
                  "Error from ttGridDistFree", __FILE__, __LINE__);
    rc = ttGridMapFree(rhdbc, routingGridMap);
    handle_errors(rhdbc, NULL, rc, JUST_DISCONNECT_EXIT,
                  "Error from ttClientGridMap", __FILE__, __LINE__);
}

/*********************************************************************
 *
 *  FUNCTION:       initRouting
 *
 *  DESCRIPTION:    Initialise everything needed for -scaleout local
 *                  and/or -scaleout routing.
 *
 *  PARAMETERS:     hdbc - an open database connection
 *
 *  RETURNS:        Nothing.
 *
 *********************************************************************/

void initRouting( void )
{
    int i, rc;
    SQLHSTMT hstmt;
#if defined(TTCLIENTSERVER)
    SQLULEN tIso = SQL_TXN_READ_COMMITTED;
    char tmpdsn[MAX_CLIENT_DSN_LEN+1];
    char buff1[1024];
#endif /* TTCLIENTSERVER */

    /* Create a Grid Map object */
    rc = ttGridMapCreate(rhdbc, &routingGridMap);
    handle_errors(rhdbc, NULL, rc, ABORT_DISCONNECT_EXIT,
                  "ttGridMapCreate() failed retrieving ttGridMap handle",
                  __FILE__, __LINE__);

    /* Create a Grid Distribution object from the map */
    rc = ttGridDistCreate(rhdbc, routingGridMap, routingCTypes, routingSQLTypes,
                     NULL, NULL, routingMaxSizes, 2, &routingHDist);
    handle_errors(rhdbc, NULL, rc, ABORT_DISCONNECT_EXIT,
                  "ttGridDistCreate() failed retrieving distribution object",
                  __FILE__, __LINE__);

    /* determine how many elements */
    rc = SQLAllocStmt(rhdbc, &hstmt);
    handle_errors(rhdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                  "Unable to allocate statement handle",
                  __FILE__, __LINE__);

    rc = SQLPrepare(hstmt, (SQLCHAR *)
"select count(distinct(mappedelementid)),count(distinct(dataspace)) from v$distribution_current;", SQL_NTS);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Unable to prepare count elementid statement",
                  __FILE__, __LINE__);
    rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &nElements, sizeof(int), NULL);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column",
                  __FILE__, __LINE__);
    rc = SQLBindCol(hstmt, 2, SQL_C_SLONG, &kFactor, sizeof(int), NULL);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column",
                  __FILE__, __LINE__);
    rc = SQLExecute(hstmt);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Unable to execute count elementid retrieval statement",
                  __FILE__, __LINE__);
    rc = SQLFetch(hstmt);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to fetch count elementid",
                  __FILE__, __LINE__);
    rc = SQLFreeStmt(hstmt, SQL_CLOSE);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to close select",
                  __FILE__, __LINE__);

    rc = SQLPrepare(hstmt, (SQLCHAR *)
"select elementid#, replicasetid#, dataspaceid# from dual;", SQL_NTS);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Unable to prepare elementid, replicasetid, dataspaceid statement",
                  __FILE__, __LINE__);
    rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &elementid, sizeof(int), NULL);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column",
                  __FILE__, __LINE__);
    rc = SQLBindCol(hstmt, 2, SQL_C_SLONG, &replicasetid, sizeof(int), NULL);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column",
                  __FILE__, __LINE__);
    rc = SQLBindCol(hstmt, 3, SQL_C_SLONG, &dataspaceid, sizeof(int), NULL);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column",
                  __FILE__, __LINE__);
    rc = SQLExecute(hstmt);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Unable to execute elementid retrieval statement",
                  __FILE__, __LINE__);
    rc = SQLFetch(hstmt);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to fetch elementid",
                  __FILE__, __LINE__);
    rc = SQLFreeStmt(hstmt, SQL_CLOSE);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to close select",
                  __FILE__, __LINE__);
    rc = SQLFreeStmt(hstmt, SQL_DROP);
    handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to drop select",
                  __FILE__, __LINE__);

#if defined(TTCLIENTSERVER)
    if (  mode == M_SCALEOUT_ROUTING  )
    {
        /* create DSNs for connection mapping element ids to dsns */

        routingDSNs = (cCliDSN_t*)calloc(nElements+1, sizeof (cCliDSN_t));
        if (routingDSNs == (cCliDSN_t*) NULL) {
          status_msg0 ("Unable to allocate memory for client DSNs\n\n");
          handle_errors(rhdbc, NULL, SQL_ERROR, JUST_DISCONNECT_EXIT, NULL, __FILE__, __LINE__);
        }
    
        /* create element, repset, dsn map */
        rc = SQLAllocStmt(rhdbc, &hstmt);
        handle_errors(rhdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                      "Unable to allocate statement handle",
                      __FILE__, __LINE__);
        rc = SQLPrepare(hstmt, (SQLCHAR *)
         "select 'ttc_redirect=0;ttc_server='||hostexternaladdress||'/'||serverport,mappedelementid,repset,dataspace "
           "from sys.v$distribution_current "
          "order by mappedelementid asc",SQL_NTS);
        handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                      "Unable to prepare count elementid statement",
                      __FILE__, __LINE__);
        rc = SQLExecute(hstmt);
        handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                      "Unable to execute query on v$distribution_current",
                      __FILE__, __LINE__);
    
       for (i = 1; i <= nElements; i++) {
          SQLLEN slen = MAX_CLIENT_DSN_LEN;
          rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, &tmpdsn, slen, NULL);
          handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column 1; clientdsn",
                        __FILE__, __LINE__);
          rc = SQLBindCol(hstmt, 2, SQL_C_SLONG, &routingDSNs[i].elementid, sizeof(int), NULL);
          handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column 2; elementid",
                        __FILE__, __LINE__);
          rc = SQLBindCol(hstmt, 3, SQL_C_SLONG, &routingDSNs[i].repset, sizeof(int), NULL);
          handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column 3; repset",
                        __FILE__, __LINE__);
          rc = SQLBindCol(hstmt, 4, SQL_C_SLONG, &routingDSNs[i].dataspace, sizeof(int), NULL);
          handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to bind column 4; dataspace",
                        __FILE__, __LINE__);
          rc = SQLFetch(hstmt);
          handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to fetch elementid",
                        __FILE__, __LINE__);
          sprintf(routingDSNs[i].clientdsn, "TTC_SERVER_DSN=%s;UID=%s;PWD=%s;%s", "sampledb", username, password, tmpdsn);

          /* Allocate connection handle for this element */
          rc = SQLAllocConnect (henv, &routingDSNs[i].hdbc);
          handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                         "allocating a connection handle",
                         __FILE__, __LINE__);

          /* open the connection */
          rc = SQLDriverConnect (routingDSNs[i].hdbc, NULL,
                                 (SQLCHAR*) routingDSNs[i].clientdsn, SQL_NTS,
                                 (SQLCHAR*) buff1, sizeof (buff1),
                                 NULL, SQL_DRIVER_NOPROMPT);
          handle_errors (routingDSNs[i].hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                         "connecting to database (routing)",
                         __FILE__, __LINE__);
          rc = SQLSetConnectOption (routingDSNs[i].hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
          handle_errors (routingDSNs[i].hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                         "turning off AUTO_COMMIT option",
                         __FILE__, __LINE__);
        if (  isolevel == 0  )
            tIso = SQL_TXN_SERIALIZABLE;
        rc = SQLSetConnectOption (routingDSNs[i].hdbc, SQL_TXN_ISOLATION, tIso);
        handle_errors (routingDSNs[i].hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                       "setting connection option",
                       __FILE__, __LINE__);

          /* Allocate statement handles */
          rc = SQLAllocStmt(routingDSNs[i].hdbc, &routingDSNs[i].updstmt);
          handle_errors(routingDSNs[i].hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                        "Unable to allocate statement handle (routing)",
                        __FILE__, __LINE__);
          rc = SQLAllocStmt(routingDSNs[i].hdbc, &routingDSNs[i].selstmt);
          handle_errors(routingDSNs[i].hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                        "Unable to allocate statement handle (routing)",
                        __FILE__, __LINE__);
          rc = SQLAllocStmt(routingDSNs[i].hdbc, &routingDSNs[i].insstmt);
          handle_errors(routingDSNs[i].hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                        "Unable to allocate statement handle (routing)",
                        __FILE__, __LINE__);
          rc = SQLAllocStmt(routingDSNs[i].hdbc, &routingDSNs[i].delstmt);
          handle_errors(routingDSNs[i].hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                        "Unable to allocate statement handle (routing)",
                        __FILE__, __LINE__);
        }

        rc = SQLFreeStmt(hstmt, SQL_CLOSE);
        handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to close select",
                      __FILE__, __LINE__);
        rc = SQLFreeStmt(hstmt, SQL_DROP);
        handle_errors(rhdbc, hstmt, rc, ABORT_DISCONNECT_EXIT, "Unable to close select",
                      __FILE__, __LINE__);
    
        rc = SQLTransact(henv, rhdbc, SQL_COMMIT);
        handle_errors(rhdbc, NULL, rc, ABORT_DISCONNECT_EXIT, "Unable to commit transaction",
                      __FILE__, __LINE__);
    } /* M_SCALEOUT_ROUTING */
#endif /* TTCLIENTSERVER */
}
#endif /* SCALEOUT && ROUTINGAPI */

/*********************************************************************
 *
 *  FUNCTION:       populate
 *
 *  DESCRIPTION:    This function populates the database
 *                  with key_cnt**2 records.
 *
 *  PARAMETERS:     NONE
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void populate(void)
{
  SQLRETURN  rc;
  SQLHDBC    hdbc;
  SQLHSTMT   hstmt;
  int        i, j, pos;
  int        id;
  int        nb;
  int        dbkey = 0;
  char *     p;
  char *     q;
  SQLCHAR    directory   [11];
  SQLCHAR    last_calling_party[11] = "0000000000";
  SQLCHAR    descr       [101];
  char       dbmode      [101] = { '\0' };
  char       buff1       [1024]; /* Connection flags */
  char       buff2       [1024];


  /* Connect to data store */

  rc = SQLAllocEnv (&henv);
  if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
  {
    /* error occurred -- don't bother calling handle_errors, since handle
     * is not valid so SQLError won't work */
    err_msg3("ERROR in %s, line %d: %s\n",
             __FILE__, __LINE__, "allocating an environment handle");
    exit(1);
  }

  /* call this in case of warning */
  handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, NO_EXIT,
                 "allocating an environment handle",
                 __FILE__, __LINE__);

  rc = SQLAllocConnect (henv, &hdbc);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "allocating a connection handle",
                 __FILE__, __LINE__);

  ghdbc = hdbc;

  /* connect */
  printf("\nConnecting to the database as %s\n", connstr_no_password);

  rc = SQLDriverConnect (hdbc, NULL,
                         (SQLCHAR*) connstr_real, SQL_NTS,
                         (SQLCHAR*) buff1, sizeof (buff1),
                         NULL, SQL_DRIVER_NOPROMPT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "connecting to database",
                 __FILE__, __LINE__);

  /* Trying to connect output */
  sprintf (buff2, "Connecting to driver (connect string: %s)\n",
           connstr_no_password);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, buff2, __FILE__, __LINE__);

#if defined(SCALEOUT) && defined(ROUTINGAPI) && defined(TTCLIENTSERVER)
  if (  ! getServerDSN(buff1, &serverdsn)  )
  {
      err_msg0("Unable to retrieve value for TTC_SERVER_DSN");
      handle_errors(hdbc, SQL_NULL_HSTMT, SQL_ERROR, JUST_DISCONNECT_EXIT, NULL, __FILE__, __LINE__);
  }
#endif /* SCALEOUT && ROUTINGAPI && TTCLIENTSERVER */

  /* Connected output */
  sprintf (buff2, "Connected using %s\n",
           connstr_no_password);
  printf("%s\n", buff2);

  rc = SQLSetConnectOption (hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                 "turning off AUTO_COMMIT option",
                 __FILE__, __LINE__);

  if (!noBuild)
  {
    /* allocate a statement */
    rc = SQLAllocStmt (hdbc, &hstmt);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating a statement handle",
                   __FILE__, __LINE__);

    /* Drop the VPN_USERS table if it already exists */
    /* Ignore any error as the VPN_USERS table might not exist   */
    rc = SQLExecDirect (hstmt, (SQLCHAR*) drop_stmnt, SQL_NTS);

    /* make create statement */
    pos = sprintf( buff1, create_stmnt );
    if (  ! rangeFlag  )
        pos += sprintf (buff1+pos, hash_clause, ((key_cnt * key_cnt)/256)+1);
#if defined(SCALEOUT)
    if (  mode != M_CLASSIC )
        pos += sprintf( buff1+pos, dist_clause );
#endif /* SCALEOUT */

    /* create the table */
    rc = SQLExecDirect (hstmt, (SQLCHAR*) buff1, SQL_NTS);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "executing create table",
                   __FILE__, __LINE__);

    /* drop and free the create statement */
    rc = SQLFreeStmt (hstmt, SQL_DROP);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "freeing statement handle",
                   __FILE__, __LINE__);

    /* allocate an insert statement */
    rc = SQLAllocStmt (hdbc, &hstmt);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating an insert statement handle",
                   __FILE__, __LINE__);

    /* prepare the insert */
    rc = SQLPrepare (hstmt, (SQLCHAR*) insert_stmnt, SQL_NTS);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "preparing insert",
                   __FILE__, __LINE__);

    /* bind the input parameters */
    rc = SQLBindParameter (hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                           SQL_INTEGER, 0, 0, &id, 0, NULL);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "binding parameter",
                   __FILE__, __LINE__);

    rc = SQLBindParameter (hstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                           SQL_INTEGER, 0, 0, &nb, 0, NULL);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "binding parameter",
                   __FILE__, __LINE__);

    rc = SQLBindParameter (hstmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR,
                           SQL_CHAR, 11, 0, directory, 0, NULL);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "binding parameter",
                   __FILE__, __LINE__);

    rc = SQLBindParameter (hstmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR,
                           SQL_CHAR, 11, 0, last_calling_party, 0, NULL);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "binding parameter",
                   __FILE__, __LINE__);

    rc = SQLBindParameter (hstmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR,
                           SQL_CHAR, 101, 0, descr, 0, NULL);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "binding parameter",
                   __FILE__, __LINE__);

    /* Store the database mode  and key value */
    id = DBMODE_ID;
    nb = DBMODE_NB;
    strcpy(directory, DBMODE_FILLER);
    strcpy(last_calling_party, DBMODE_FILLER);
    dbkey = key_cnt;
#if defined(SCALEOUT)
    if (  mode == M_CLASSIC  )
#endif /* SCALEOUT */
        strcpy(dbmode, DBMODE_CLASSIC);
#if defined(SCALEOUT)
    else
        strcpy(dbmode, DBMODE_SCALEOUT);
#endif /* SCALEOUT */
    sprintf(descr, "%s/%d", dbmode, dbkey);
    rc = SQLExecute (hstmt);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "executing insert",
                   __FILE__, __LINE__);

    /* commit the transaction */
    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction",
                   __FILE__, __LINE__);

    tptbm_msg0("Populating benchmark database\n");

    /* insert key_cnt**2 records */
    for (i = 0; i < key_cnt; i++)
    {
      for (j = 0; j < key_cnt; j++)
      {
        id = i;
        nb = j;
        sprintf ((char*) directory, "55%d%d", id, nb);
        sprintf ((char*) descr,
                 "<place holder for description of VPN %d extension %d>",
                 id, nb);

        /* execute insert statement */
        rc = SQLExecute (hstmt);
        handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                       "executing insert",
                       __FILE__, __LINE__);
      }

      /* commit the transaction */
      rc = SQLTransact (henv, hdbc, SQL_COMMIT);
      handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction",
                     __FILE__, __LINE__);
    }

    /* commit the transaction */
    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction",
                   __FILE__, __LINE__);

    /* drop and free the insert statement */
    rc = SQLFreeStmt (hstmt, SQL_DROP);
    handle_errors (hdbc, hstmt, rc, DISCONNECT_EXIT,
                   "freeing statement handle",
                   __FILE__, __LINE__);

    tptbm_msg0("Population complete\n\n");
  } /* ! noBuild */
  else
  { /* retrieve the database mode and key and check it against the run-time values */
    /* allocate a statement */
    rc = SQLAllocStmt(hdbc, &hstmt);
    handle_errors(hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating a statement handle",
                   __FILE__, __LINE__);

    rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, &descr, sizeof(descr), NULL);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "binding column",
                   __FILE__, __LINE__);
        
    rc = SQLExecDirect(hstmt, (SQLCHAR*)SEL_DBMODE, SQL_NTS);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "retrieving database mode",
                   __FILE__, __LINE__);

    rc = SQLFetch (hstmt);
    if ( rc == SQL_NO_DATA_FOUND  )
    {
        strcpy( dbmode, DBMODE_CLASSIC);
        dbkey = DFLT_KEY;
    }
    else
    if (  rc == SQL_SUCCESS  )
    {
        p = strchr( descr, '/' );
        if (  p != NULL  )
        {
            *p++ = '\0';
            strcpy( dbmode, descr );
            q = strchr( p, ' ' );
            if (  q != NULL  )
            {
                *q = '\0';
                if (  isnumeric( p )  )
                    dbkey = atoi(p);
            }
        }
    }
    else
        handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                       "fetching select",
                       __FILE__, __LINE__);
    
    /* close the statement */
    rc = SQLFreeStmt (hstmt,SQL_CLOSE);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "closing select",
                   __FILE__, __LINE__);

    /* drop and free the statement */
    rc = SQLFreeStmt(hstmt, SQL_DROP);
    handle_errors(hdbc, hstmt, rc, DISCONNECT_EXIT,
                   "freeing statement handle",
                   __FILE__, __LINE__);

    if (  ! dbmode[0] || (dbkey == 0)  )
    {
        tptbm_msg0("Warning: unable to parse database creation data from VPN_USERs table\n");
    }
    else
    {
        if (  ((strncmp(dbmode, DBMODE_CLASSIC, strlen(DBMODE_CLASSIC)) == 0) && (mode != M_CLASSIC)) ||
              ((strncmp(dbmode, DBMODE_CLASSIC, strlen(DBMODE_CLASSIC)) != 0) && (mode == M_CLASSIC))  )
        {
            tptbm_msg2("Warning: database build mode (%s) is different to run-time mode (%s)\n\n", 
                        dbmode, nameOfMode(mode, NULL));
        }
        if (  dbkey > key_cnt  )
        {
            tptbm_msg2("Warning: database build key (%d) greater than run-time value (%d)\n\n", 
                        dbkey, key_cnt);
        }
        else
        if (  dbkey < key_cnt  ) {
            err_msg2("Database build key (%d) less than run-time value (%d)\n\n", dbkey, key_cnt);
            handle_errors(hdbc, SQL_NULL_HSTMT, SQL_ERROR, JUST_DISCONNECT_EXIT,
                   NULL, __FILE__, __LINE__);
        }
    }
  } /* noBuild */
}

/*********************************************************************
 *
 *  FUNCTION:       OpenShmSeg
 *
 *  DESCRIPTION:    Creates+initializes/attaches the shared memory 
 *                  segment used for inter-process co-ordination and
 *                  data exchange.
 *
 *  PARAMETERS:     int shmSize - the size of segment required.
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

#if defined(WIN32)
void OpenShmSeg(int  shmSize)
{
    int  i;
    int  j;
    char * p;

    /* procId is 0 only for the parent process */
    if (procId == 0)
    {
      /* create shared memory segment */
      shmHndl = CreateFileMapping (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                          shmSize, "TTenMultiUserTestsOnNT");

      if (shmHndl == NULL)
      {
        err_msg1("could not map shm segment\nError number: %d\n", GetLastError());
        exit (1);
      }

      shmhdr = (procinfo_t *)MapViewOfFile (shmHndl, FILE_MAP_WRITE, 0, 0, 
                                     shmSize);
      if (shmhdr == NULL)
      {
        err_msg1("could not map shm segment\nError number: %d\n", GetLastError());
        exit (1);
      }

      /* initialize shared memory segment */
      /*    Used for array of PIDs and DSN to pass to child processes */
      memset( (void *)shmhdr, 0, shmSize );

      /* Set the number of processes for the children to get the correct offsets */
      shmhdr[0].state = num_processes;

      /* Save the connection string for the children to read */
      p = (char *)shmhdr + ((sizeof(procinfo_t) * num_processes);
      strcpy( p, connstr );

    }
    else
    {
      /* connect to shared memory segment */
      shmHndl = OpenFileMapping (FILE_MAP_WRITE, TRUE, 
          "TTenMultiUserTestsOnNT");
      if (shmHndl == NULL)
      {
        err_msg0("could not open shm segment\n");
        exit (1);
      }

      shmhdr = (procinfo_t *)MapViewOfFile (shmHndl, FILE_MAP_WRITE, 
                                     0, 0, shmSize);
      if (shmhdr == NULL)
      {
        err_msg0("could not map shm segment\n");
        exit (1);
      }
    }
}
#else /* UNIX */
void OpenShmSeg(int  shmSize)
{
    int i;
    int fd;
    char * p;

    if ( procId == 0  ) {
        /* create a file for key generation */
        fd = mkstemp(keypath);
        if (fd != -1)
        {
          /* success - close the file */
          close(fd);
        }
        /* if mkstemp() failed, then ftok() will return -1 to use as the key */
    
        /* create shared memory segment */
        shmid = shmget (ftok (keypath, rand_seed),
                               shmSize, IPC_CREAT | 0666);
        if (shmid == -1)
        {
          err_msg1("could not create shm segment, errno = %d\n", errno);
          exit (1);
        }
    }

    shmhdr = (procinfo_t *)shmat (shmid, 0, 0);
    if ((tt_ptrint) shmhdr == -1)
    {
      err_msg1("could not attach to shm segment, errno = %d\n", errno);
      exit (1);
    }

    if ( procId == 0  ) {
        /* initialize shared memory segment */
        memset( (void *)shmhdr, 0, shmSize );
    
        /* Set the number of processes for the children to get the correct offsets */
        shmhdr[0].state = num_processes;
    
        /* remove shared memory segment so it disappears after detach */
        shmctl (shmid, IPC_RMID, NULL);
        /* remove the key file */
        unlink(keypath);
    }
}
#endif

/*********************************************************************
 *
 *  FUNCTION:       CreateChildProcs
 *
 *  DESCRIPTION:    Spawns the required number of child processes.
 *
 *  PARAMETERS:     char * progname - executable path
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void CreateChildProcs(char*    progName)
{
  int   i;
  int   pid;

  /* spawn the child processes */

  for (i = 1; i < num_processes; i++)
  {
    rand_seed += 5;

#if !defined(WIN32) /* UNIX */

    pid = fork();
    if (pid == 0)
    {
      rand_seed += 2;
      procId = i;
      break;
    }

    if (pid < 0)
    {
      err_msg0("Failure spawning a process\n");
      exit (1);
    }

    shmhdr[i].pid = (tt_ptrint) pid;

#else /* Windows */

    /* in this case spawn more processes with a different value for procid
     * the variable procid corresponds to the position of the
     * process in the shared memory array - it is 0 in the parent case */

    {
      char cmdLine      [2000];
      int  pos = 0;

      pos = sprintf( cmdLine+pos, 
                 "%s -connstr \"%s\" -proc 1 -seed %d -procid %d -key %d -iso %d "
                 "-read %d -insert %d -delete %d -insertmod %d -ops %d ",
                 progName, connstr_no_password, rand_seed + 1, i, key_cnt, isolevel,
                 reads, inserts, deletes, insert_mod, opsperxact);
      if (  rangeFlag  )
          pos += sprintf( cmdLine+pos, "-range " );
      if (  throttle > 0  )
          pos += sprintf( cmdLine+pos, "-throttle %d ", throttle );
#if defined(SCALEOUT)
      if (  mode > M_CLASSIC  )
          pos += sprintf( cmdLine+pos, "-scaleout " );
#if defined(ROUTINGAPI)
      if (  mode == M_SCALEOUT_LOCAL  )
          pos += sprintf( cmdLine+pos, "local " );
#if defined(TTCLIENTSERVER)
      else
      if (  mode == M_SCALEOUT_ROUTING  )
          pos += sprintf( cmdLine+pos, "routing " );
#endif /* TTCLIENTSERVER */
#endif /* ROUTINGAPI */
#endif /* SCALEOUT */
      if (  num_xacts > 0  )
          pos += sprintf( cmdLine+pos, "-xact %d ", num_xacts );
      else
          pos += sprintf( cmdLine+pos, "-sec %d -ramp %d ", duration, ramptime );

      if (CreateProcess (NULL, cmdLine, NULL, NULL, FALSE,
                         CREATE_DEFAULT_ERROR_MODE, NULL,
                         NULL, &sInfo, &pInfo) == 0)
      {
        err_msg0("Failure spawning a process\n");
        exit (1);
      }

      shmdhr[i].pid = (tt_ptrint) pInfo.hProcess;
    }

    pid = 1;

#endif /* Windows */
  }
}

/*********************************************************************
 *
 *  FUNCTION:       main
 *
 *  DESCRIPTION:    This is the main function of the tptbm benchmark.
 *                  It connects to an ODBC data source, creates and
 *                  populates a table, spawns num_processes
 *                  processes (each performing xacts transactions),
 *                  and then reports timing statistics.
 *
 *  PARAMETERS:     int argc        # of command line arguments
 *                  char *argv[]    command line arguments
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int main(int argc, char** argv)
{
  int         retval;
  SQLRETURN   rc;

  retval = 1; /* Assume failure */
#if defined(TTCLIENTSERVER) && defined(__hppa) && !defined(__LP64__)
  /* HP/UX requires this for C main programs that call aC++ shared libs */
  _main();
#endif

  /* Set up default signal handlers */
  if (HandleSignals() != 0) {
    err_msg0("Unable to set signal handlers\n");
    goto leaveMain;
  }
  StopRequestClear();

  /* initialize the file for status messages */
  statusfp = stdout;

  
  /* parse the command line arguments */
  if (  ! parse_args(argc, argv)  )
      goto leaveMain;

  retval = 0;

  if (StopRequested()) {
    err_msg1("Signal %d received. Stopping\n", SigReceived());
    goto leaveMain;
  }

  /* the parent/main process must prepare datastore for the benchmark */
  strcpy(connstr_real, connstr);
  if (procId == 0) {
    populate();
    if (buildOnly)
    {
      rc = SQLDisconnect (ghdbc);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "disconnecting from database",
                     __FILE__, __LINE__);
    
      rc = SQLFreeConnect (ghdbc);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "freeing connection handle",
                     __FILE__, __LINE__);

      rc = SQLFreeEnv (henv);
      handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "freeing environment handle",
                     __FILE__, __LINE__);
      retval = 0;
      goto leaveMain;
    } /* buildOnly */
  }

  if (  procId == 0  )
  {
      OpenShmSeg( (sizeof(procinfo_t) * num_processes) + strlen(connstr) + 4);
      if ( num_processes > 1 )
          CreateChildProcs(argv[0]);
  }
  else
  if (  num_processes > 1  )
      OpenShmSeg( (sizeof(procinfo_t) * num_processes) + strlen(connstr) + 4);

  ExecuteTptBm (rand_seed, procId);
  retval = 0;

leaveMain:
  return retval;
}

/*********************************************************************
 *
 *  FUNCTION:       ExecuteTptBm
 *
 *  DESCRIPTION:    The main benachmark logic. Executed by each
 *                  process.
 *
 *  PARAMETERS:     int seed - random number seed
 *                  int procId - process inde into shared memory array
 *                               0 is parent
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void ExecuteTptBm(int          seed,
                  int          procId)
{
  SQLHSTMT    updstmt = SQL_NULL_HSTMT;
  SQLHSTMT    selstmt = SQL_NULL_HSTMT;
  SQLHSTMT    insstmt = SQL_NULL_HSTMT;
  SQLHSTMT    delstmt = SQL_NULL_HSTMT;
  SQLRETURN   rc;

  ttTime    main_start;       /* variable to measure time */
  ttTime    main_end;         /* variable to measure time */

  time_t    interval_start;   /* variable to measure interval time */
  time_t    interval_cur;     /* variable to measure interval time */
  time_t    interval_diff;    /* variable to measure interval time */
  time_t    duration_start, duration_cur, duration_diff;
  int       duration_target = 0;
  UBIGINT   duration_est = 10000;    /* duration estimate; initial guess = 10000 xacts / sec */
  int       rampingup = 0;    /* currently in ramp-up */
  int       rampingdown = 0;  /* currently in ramp-down */
  int       rand_int;         /* rand integer */
  int       insert_start;     /* start mark for inserts */
  int       insert_present;   /* present mark for inserts */
  int       delete_start = 0; /* start mark for deletes */
  int       delete_present=0; /* present mark for deletes */

  tt_ptrint time_diff;        /* difference in time between
                               * start and finish */
  double    tps;              /* compute transactions per second */
  SQLULEN   tIso = SQL_TXN_READ_COMMITTED;
  int       i;
  int       op_count=0;
  char      errstr [4096];

  /* the following two buffers are used for all statements */
  int       id;
  int       nb;
  int       path = 0;         /* type of transaction */

  SQLHDBC   thdbc = SQL_NULL_HDBC; // temp HDBC

  int the_num_processes = 0;
  int connStrLen ;
  int k;
  int j;
  int child;
  double dkey_cnt = (double)key_cnt;
  char * p;

  /* the following three buffers are for the select statement */
  SQLCHAR   directory [11];
  SQLCHAR   last [11];
  SQLCHAR   descr [101];
  SQLCHAR   last_calling_party[11] = "0000000000";

  srand(seed);

  /* initialize the select statement buffers */
  directory [10]  = '\0';
  last      [10]  = '\0';
  descr     [100] = '\0';

  if (procId == 0) {
#if defined(SCALEOUT) && defined (ROUTINGAPI)
    if (  mode > M_SCALEOUT  )
        rhdbc = ghdbc;
#endif /* SCALEOUT && ROUTINGAPI */
    the_num_processes = num_processes;
  } else {
    ghdbc = SQL_NULL_HDBC;
    /* Get the num_processes from SHM */
    the_num_processes = shmhdr[0].state;

  /* Connect to data store */
#ifdef WIN32  /* Only needed on Windoze */
    p = (char *)shmhdr + (sizeof(procinfo_t) * the_num_processes);
    strcpy( connstr, p );
    connStrLen = strlen( connstr );
    strcpy( connstr_real, connstr );
#endif /* WIN32 */

    rc = SQLAllocEnv (&henv);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
    {
      /* error occurred -- don't bother calling handle_errors, since handle
       * is not valid so SQLError won't work */
      err_msg3("ERROR in %s, line %d: %s\n",
               __FILE__, __LINE__, "allocating an environment handle");
      exit(1);
    }
    /* call this in case of warning */
    handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, NO_EXIT,
                   "allocating an environment handle",
                   __FILE__, __LINE__);
    
    rc = SQLAllocConnect (henv, &ghdbc);
    handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "allocating a connection handle",
                   __FILE__, __LINE__);
    
    sprintf (errstr, "Connecting to database (connect string %s)\n",
             connstr_no_password);
    rc = SQLDriverConnect (ghdbc, NULL, (SQLCHAR*) connstr_real, SQL_NTS,
                           NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "connecting to the database",
                   __FILE__, __LINE__);

#if defined(SCALEOUT) && defined (ROUTINGAPI)
    if (  mode > M_SCALEOUT  )
        rhdbc = ghdbc;
#endif /* SCALEOUT && ROUTINGAPI */
   
    /* Now erase the connect strings and password to get rid of the password */
    erasePassword(connstr_real, strlen(connstr_real));
    erasePassword(connstr, strlen(connstr));
  }

#if defined(SCALEOUT) && defined(ROUTINGAPI)
  if (  mode > M_SCALEOUT  )
      initRouting();
#endif /* SCALEOUT && ROUTING */
  erasePassword(password, strlen(password));
#if defined(SCALEOUT) && defined (ROUTINGAPI) && defined(TTCLIENTSERVER)
  if (  mode != M_SCALEOUT_ROUTING  )
  {
#endif /* SCALEOUT && ROUTINGAPI && TTCLIENTSERVER */
      /* turn off autocommit */
      rc = SQLSetConnectOption (ghdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                     "turning AUTO_COMMIT option OFF",
                     __FILE__, __LINE__);
    
      /* explicitly set the locking isolation level */
      if (isolevel != -1)
      {
        switch (isolevel)
        {
          case 0: tIso = SQL_TXN_SERIALIZABLE;
                  break;
    
          case 1: tIso = SQL_TXN_READ_COMMITTED;
                  break;
        /* value was error checked above */
        }

        rc = SQLSetConnectOption (ghdbc, SQL_TXN_ISOLATION, tIso);
        handle_errors (ghdbc, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                       "setting connection option",
                       __FILE__, __LINE__);
      }

      /* allocate an update statement */
      rc = SQLAllocStmt (ghdbc, &updstmt);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                     "allocating an update statement handle",
                     __FILE__, __LINE__);

      /* allocate a select statement */
      rc = SQLAllocStmt (ghdbc, &selstmt);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                     "allocating a select statement handle",
                     __FILE__, __LINE__);
    
      /* allocate an insert statement */
      rc = SQLAllocStmt (ghdbc, &insstmt);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                     "allocating an insert statement handle",
                     __FILE__, __LINE__);
    
      /* allocate a  delete statement */
      rc = SQLAllocStmt (ghdbc, &delstmt);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                     "allocating a delete statement handle",
                     __FILE__, __LINE__);
#if defined(SCALEOUT) && defined(ROUTINGAPI) && defined(TTCLIENTSERVER)
  }

  if (  mode == M_SCALEOUT_ROUTING  )
  {
      for (i = 1; i <= nElements; i++)
      {
          thdbc = routingDSNs[i].hdbc;
          updstmt = routingDSNs[i].updstmt;
          selstmt = routingDSNs[i].selstmt;
          delstmt = routingDSNs[i].delstmt;
          insstmt = routingDSNs[i].insstmt;
    
          /* prepare the select statement */
          rc = SQLPrepare (selstmt, (SQLCHAR*) select_stmnt, SQL_NTS);
          handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                         "preparing select",
                         __FILE__, __LINE__);

          /* Bind the selected columns  */
          rc = SQLBindCol (selstmt, 1, SQL_C_CHAR, &directory, 11, NULL);
          handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding column",
                     __FILE__, __LINE__);
        
          rc = SQLBindCol (selstmt, 2, SQL_C_CHAR, &last, 11, NULL);
          handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding column",
                         __FILE__, __LINE__);
        
          rc = SQLBindCol (selstmt, 3, SQL_C_CHAR, &descr, 101, NULL);
          handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding column",
                         __FILE__, __LINE__);
        
          /* bind the input parameters */
          rc = SQLBindParameter (selstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, 
                                 SQL_INTEGER, 0, 0, &id, 0, NULL);
          handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          rc = SQLBindParameter (selstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, 
                                 SQL_INTEGER, 0, 0, &nb, 0, NULL);
          handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          /* prepare the update statement */
          rc = SQLPrepare (updstmt, (SQLCHAR*) update_stmnt, SQL_NTS);
          handle_errors (thdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                         "preparing update",
                         __FILE__, __LINE__);
        
          /* bind the input parameters */
          rc = SQLBindParameter (updstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                                 11, 0, last, 0, NULL);
          handle_errors (thdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          rc = SQLBindParameter (updstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                                 0, 0, &id, 0, NULL);
          handle_errors (thdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          rc = SQLBindParameter (updstmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                                 0, 0, &nb, 0, NULL);
          handle_errors (thdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          /* prepare the delete statement */
        
          rc = SQLPrepare(delstmt,(SQLCHAR *) delete_stmnt, SQL_NTS);
          handle_errors(thdbc, delstmt, rc, 1,
                        "Unable to prepare delete",
                        __FILE__, __LINE__);
        
          /* bind the input parameters */
        
          rc = SQLBindParameter(delstmt,1,SQL_PARAM_INPUT,SQL_C_SLONG,SQL_INTEGER,
                                0,0,&id,0,NULL);
          handle_errors(thdbc, delstmt, rc, 1, "Unable to bind parameter",
                        __FILE__, __LINE__);
        
          rc = SQLBindParameter(delstmt,2,SQL_PARAM_INPUT,SQL_C_SLONG,SQL_INTEGER,
                                0,0,&nb,0,NULL);
          handle_errors(thdbc, delstmt, rc, 1, "Unable to bind parameter",
                        __FILE__, __LINE__);
        
        
          /* prepare the insert */
          rc = SQLPrepare (insstmt, (SQLCHAR*) insert_stmnt, SQL_NTS);
          handle_errors (thdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                         "preparing insert",
                         __FILE__, __LINE__);
        
          /* bind the input parameters */
          rc = SQLBindParameter (insstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                                 0, 0, &id, 0, NULL);
          handle_errors (thdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          rc = SQLBindParameter (insstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                                 0, 0, &nb, 0, NULL);
          handle_errors (thdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          rc = SQLBindParameter (insstmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                                 11, 0, directory, 0, NULL);
          handle_errors (thdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          rc = SQLBindParameter (insstmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR,
                                 SQL_CHAR, 11, 0, last_calling_party, 0, NULL);
          handle_errors (thdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          rc = SQLBindParameter (insstmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                                 101, 0, descr, 0, NULL);
          handle_errors (thdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
          /* commit the transaction */
          rc = SQLTransact (henv, thdbc, SQL_COMMIT);
          handle_errors (thdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                         "committing transaction",
                         __FILE__, __LINE__);
      } 
  }
  else {
#endif /* SCALEOUT && ROUTINGAPI && TTCLIENTSERVER */
      /* prepare the select statement */
      rc = SQLPrepare (selstmt, (SQLCHAR*) select_stmnt, SQL_NTS);
      handle_errors (ghdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                     "preparing select",
                     __FILE__, __LINE__);

      /* Bind the selected columns  */
      rc = SQLBindCol (selstmt, 1, SQL_C_CHAR, &directory, 11, NULL);
      handle_errors (ghdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding column",
                 __FILE__, __LINE__);
        
      rc = SQLBindCol (selstmt, 2, SQL_C_CHAR, &last, 11, NULL);
      handle_errors (ghdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding column",
                     __FILE__, __LINE__);
        
      rc = SQLBindCol (selstmt, 3, SQL_C_CHAR, &descr, 101, NULL);
      handle_errors (ghdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding column",
                     __FILE__, __LINE__);
        
      /* bind the input parameters */
      rc = SQLBindParameter (selstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, 
                             SQL_INTEGER, 0, 0, &id, 0, NULL);
      handle_errors (ghdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      rc = SQLBindParameter (selstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, 
                             SQL_INTEGER, 0, 0, &nb, 0, NULL);
      handle_errors (ghdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      /* prepare the update statement */
      rc = SQLPrepare (updstmt, (SQLCHAR*) update_stmnt, SQL_NTS);
      handle_errors (ghdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                     "preparing update",
                     __FILE__, __LINE__);
        
      /* bind the input parameters */
      rc = SQLBindParameter (updstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                             11, 0, last, 0, NULL);
      handle_errors (ghdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      rc = SQLBindParameter (updstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                             0, 0, &id, 0, NULL);
      handle_errors (ghdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      rc = SQLBindParameter (updstmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                             0, 0, &nb, 0, NULL);
      handle_errors (ghdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      /* prepare the delete statement */
      rc = SQLPrepare(delstmt,(SQLCHAR *) delete_stmnt, SQL_NTS);
      handle_errors(ghdbc, delstmt, rc, 1,
                    "Unable to prepare delete",
                    __FILE__, __LINE__);
        
      /* bind the input parameters */
      rc = SQLBindParameter(delstmt,1,SQL_PARAM_INPUT,SQL_C_SLONG,SQL_INTEGER,
                            0,0,&id,0,NULL);
      handle_errors(ghdbc, delstmt, rc, 1, "Unable to bind parameter",
                    __FILE__, __LINE__);
        
      rc = SQLBindParameter(delstmt,2,SQL_PARAM_INPUT,SQL_C_SLONG,SQL_INTEGER,
                            0,0,&nb,0,NULL);
      handle_errors(ghdbc, delstmt, rc, 1, "Unable to bind parameter",
                    __FILE__, __LINE__);
        
      /* prepare the insert */
      rc = SQLPrepare (insstmt, (SQLCHAR*) insert_stmnt, SQL_NTS);
      handle_errors (ghdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                     "preparing insert",
                     __FILE__, __LINE__);
        
      /* bind the input parameters */
      rc = SQLBindParameter (insstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                             0, 0, &id, 0, NULL);
      handle_errors (ghdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      rc = SQLBindParameter (insstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                             0, 0, &nb, 0, NULL);
      handle_errors (ghdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      rc = SQLBindParameter (insstmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                             11, 0, directory, 0, NULL);
      handle_errors (ghdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      rc = SQLBindParameter (insstmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_CHAR, 11, 0, last_calling_party, 0, NULL);
      handle_errors (ghdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                     "binding parameter",
                     __FILE__, __LINE__);
        
      rc = SQLBindParameter (insstmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                             101, 0, descr, 0, NULL);
      handle_errors (ghdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                         "binding parameter",
                         __FILE__, __LINE__);
        
      /* commit the transaction */
      rc = SQLTransact (henv, ghdbc, SQL_COMMIT);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction",
                     __FILE__, __LINE__);
#if defined(SCALEOUT) && defined(ROUTINGAPI) && defined(TTCLIENTSERVER)
  }
#endif /* SCALEOUT && ROUTINGAPI && TTCLIENTSERVER */

  /* print this starting message only in the main/parent process */
  if (procId == 0)
  {
    if (the_num_processes > 1) {
      /* wait for the children to finish initialization */
      tptbm_msg1("Waiting for %d processes to initialize\n", the_num_processes);
      // ready all the children
      for ( i = 1; i < the_num_processes; i++ )
          shmhdr[i].state = PROC_READY;
      do {
          if (StopRequested())
            goto cleanup;
          j = 1; k= 0;
          for ( i = 1; i < the_num_processes; i++ ) {
              if ( shmhdr[i].state != PROC_SET  )
                  j = 0, k++;
          }
          tt_sleep (1);
      } while ( ! j );
      // start all the children
      for ( i = 1; i < the_num_processes; i++ )
          shmhdr[i].state = PROC_GO;
    }

    out_msg6("Beginning execution with %d process%s: "
                 "%d%% read, %d%% update, %d%% insert, %d%% delete\n",
                 the_num_processes, (the_num_processes > 1) ? "es" : "",
                 reads,
                 100 - (reads + inserts + deletes),
                 inserts, deletes);
  } else {
      // wait to be readied
      while (shmhdr[procId].state < PROC_READY)
      {
        if (StopRequested())
            goto cleanup;
        tt_sleep(1)
      }
      shmhdr[procId].state = PROC_SET; // indicate ready
      // wait to be started
      while (shmhdr[procId].state < PROC_GO)
      {
        if (StopRequested())
            goto cleanup;
        tt_yield();
      }
      shmhdr[procId].state = PROC_RUNNING; // indicate running
  }

  insert_start = key_cnt + procId;   /* start mark for this process */
  insert_present = 0;
  delete_start = procId;

  long delete_max = (key_cnt * key_cnt) / the_num_processes;
  long deleted = 0;

  ttGetTime (&main_start); /* start timing */
  time(&interval_start);
  if ( (procId == 0) && duration) {
    duration_start = interval_start;
    if (ramptime > 0) {
      duration_target = ramptime;
      rampingup = 1;
    } else {
      duration_target = duration;
      rampingup = 0;
    }
  }

    for (i = 0; duration || (i < num_xacts); i++)
    {
      if (duration) {
        /* no more rows to delete */
        if ( deletes && (deleted >= delete_max) ) {
          if (the_num_processes > 1) {
            shmhdr[procId].xacts = i; /* store the number of xacts */
            shmhdr[procId].state = PROC_STOP;
          } else {
            /* i am the only process */
            num_xacts = i;
          }
          if (verbose >= VERBOSE_ALL)
            status_msg2("Process %d deleted %.0f rows\n",
                        procId, (double)deleted);
          goto finish_loop;
        }

        /* child */
        if ( procId > 0 ) {
          /* check this first for performance */
          if (shmhdr[procId].state > PROC_RUNNING) {
            if (shmhdr[procId].state == PROC_STARTBENCH) {
              i = 0; /* reset the counter */
              shmhdr[procId].state = PROC_MEASURING;
            }
            else if (shmhdr[procId].state == PROC_STOPBENCH) {
              if (verbose >= VERBOSE_ALL)
                status_msg2("Process %d finished %.0f xacts\n",
                            procId, (double)i);
              shmhdr[procId].xacts = i; /* store the number of xacts */
              shmhdr[procId].state = PROC_STOPPING;
            }
            else if (shmhdr[procId].state == PROC_STOP) {
              shmhdr[procId].state = PROC_STOPPING;
              goto finish_loop;
            }
          }
        }
        /* parent */
        else {
          /* check if duration exceeded */
          (void)time(&duration_cur);
          duration_diff = (duration_cur - duration_start);
          if (  duration_diff >= (time_t)duration_target  )
          {
              if (  rampingup  ) {
                /* RAMP-UP done */
                status_msg0("Ramp-up done...measuring begins\n");
                ttGetTime(&main_start); /* reset the start time */
                if (the_num_processes > 1) {
                  for (child = 1; child < the_num_processes; child++) {
                    /* tell children to start timing */
                    shmhdr[child].state = PROC_STARTBENCH;
                  }
                }
                duration_start = duration_cur;
                duration_target = duration;
                rampingup = 0;
                i = 0;
              } else {
                /* Benchmark finished */
                if (  rampingdown  ) {
                    /* RAMP-UP done */
                    status_msg0("Ramp-down done...terminating\n");
                    rampingdown = 0;
                    if (the_num_processes > 1) {
                      for (child = 1; child < the_num_processes; child++) {
                        /* tell children to stop */
                        shmhdr[child].state = PROC_STOP;
                      }
                    }
                    goto finish_loop;
                } else {
                    /* record time at the end of the measured part of the run */
                    ttGetTime (&main_end);
                    shmhdr[procId].xacts = i; /* store the number of xacts */

                    if (the_num_processes > 1) {
                      for (child = 1; child < the_num_processes; child++) {
                        /* tell children to stop measuring */
                        shmhdr[child].state = PROC_STOPBENCH;
                      }
                        /* and wait for them to acknowledge */
                      do {
                          tt_yield();
                          j = 1;
                          for ( child = 1; child < the_num_processes; child++ ) {
                              if ( shmhdr[child].state != PROC_STOPPING  )
                                  j = 0;
                          }
                      } while ( j == 0 );
                    }
                    if (verbose >= VERBOSE_ALL)
                      status_msg2("Process %d finished %.0f xacts\n",
                                  procId, (double)i);
                    if (  ramptime  ) {
                        duration_start = duration_cur;
                        duration_target = ramptime;
                        rampingdown = 1;
                        status_msg0("Measuring finished...ramping down\n");
                    } else {
                        if (the_num_processes > 1) {
                          for (child = 1; child < the_num_processes; child++) {
                            /* tell children to stop */
                            shmhdr[child].state = PROC_STOP;
                          }
                        }
                        goto finish_loop;
                    }
                }
              }
          }
        } /* end of parent */
      } /* end of duration */
    else
        shmhdr[procId].xacts = i;

    throttle:      
      if (throttle && ((i % throttle) == (throttle-1))) {
        (void) time (&interval_cur);
        interval_diff = (interval_cur - interval_start);
        if (interval_diff > (time_t)0) {
          interval_start = interval_cur;
        }
        else {
          tt_yield();
          goto throttle;
        }
      }

      op_count++;
      if (reads != 100)
      {
        rand_int = rand();

        if (rand_int < ((float)(reads + inserts + deletes) / 100) * ((unsigned int)RAND_MAX + 1)) {
          if (rand_int < ((float)(reads + inserts)/ 100) * ((unsigned int)RAND_MAX + 1)) {
            if (rand_int < ((float)(reads)/ 100) * ((unsigned int)RAND_MAX + 1)) {
              path = 1;    /* read */
            }
            else {
              path = 2;    /* insert */
            }
          }
          else {
            path = 3;      /* delete */
          }
        }
        else
          path = 0;        /* update */
      }
      else
        path = 1;          /* read */

      if (path == 1)                                /* select xact */
      {
        /* randomly pick argument values */
        id = (int) (dkey_cnt * rand() / ((unsigned int)RAND_MAX + 1));
        nb = (int) (dkey_cnt * rand() / ((unsigned int)RAND_MAX + 1));
#if defined(SCALEOUT) && defined(ROUTINGAPI)
        if (  mode == M_SCALEOUT_LOCAL  )
            while (  !  routingAPI_isLocal(&id,&nb)  )
            {
                id = (int) (dkey_cnt * rand() / ((unsigned int)RAND_MAX + 1));
                nb = (int) (dkey_cnt * rand() / ((unsigned int)RAND_MAX + 1));
            }
#if defined(TTCLIENTSERVER)
        if (  mode == M_SCALEOUT_ROUTING  )
            thdbc = routingAPI_getConn(&id,&nb,&selstmt,&updstmt,&insstmt,&delstmt);
        else
#endif /* TTCLIENTSERVER */
#endif /* SCALEOUT && ROUTINGAPI */
            thdbc = ghdbc;

        if ( ! nodbexec )
        {
            /* execute, fetch, free and commit */
            rc = SQLExecute (selstmt);
            handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                           "executing select",
                           __FILE__, __LINE__);
    
            rc = SQLFetch (selstmt);
            handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                           "fetching select",
                           __FILE__, __LINE__);
    
            rc = SQLFreeStmt (selstmt,SQL_CLOSE);
            handle_errors (thdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                           "closing select",
                           __FILE__, __LINE__);
    
            if (op_count == opsperxact) {
              /* TimesTen doesn't require reads to be committed          */
              rc = SQLTransact (henv, thdbc, SQL_COMMIT);
              handle_errors (thdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                             "committing transaction",
                             __FILE__, __LINE__);
              op_count = 0;
            }
        }
      }
      else if (path == 0)                           /* update xact */
      {
        /* randomly pick argument values */
        id = (int) (dkey_cnt * rand() / ((unsigned int)RAND_MAX + 1));
        nb = (int) (dkey_cnt * rand() / ((unsigned int)RAND_MAX + 1));
#if defined(SCALEOUT) && defined(ROUTINGAPI)
        if (  mode == M_SCALEOUT_LOCAL  )
            while (  !  routingAPI_isLocal(&id,&nb)  )
            {
                id = (int) (dkey_cnt * rand() / ((unsigned int)RAND_MAX + 1));
                nb = (int) (dkey_cnt * rand() / ((unsigned int)RAND_MAX + 1));
            }
#if defined(TTCLIENTSERVER)
        if (  mode == M_SCALEOUT_ROUTING  )
            thdbc = routingAPI_getConn(&id,&nb,&selstmt,&updstmt,&insstmt,&delstmt);
        else
#endif /* TTCLIENTSERVER */
#endif /* SCALEOUT && ROUTINGAPI */
            thdbc = ghdbc;

        sprintf ((char*)last, "%dx%d", id, nb);
        last[10] = 0;

        if ( ! nodbexec )
        {
            /* execute and commit */
            rc = SQLExecute (updstmt);
            handle_errors (thdbc, updstmt, rc, ABORT_DISCONNECT_EXIT,
                           "executing update",
                           __FILE__, __LINE__);
    
            if (op_count == opsperxact) {
              rc = SQLTransact (henv, thdbc, SQL_COMMIT);
              handle_errors (thdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                             "committing transaction",
                             __FILE__, __LINE__);
              op_count = 0;
            }
        }
      }
      else if (path == 3)                           /* delete xact */
      {
        id = delete_start;
        nb = delete_present++;
        if (delete_present == key_cnt){
          delete_present = 0;
          delete_start += insert_mod;
        }
#if defined(SCALEOUT) && defined(ROUTINGAPI)
        if (  mode == M_SCALEOUT_LOCAL  )
            while (  !  routingAPI_isLocal(&id,&nb)  )
            {
                id = delete_start;
                nb = delete_present++;
                if (delete_present == key_cnt){
                  delete_present = 0;
                  delete_start += insert_mod;
                }
            }
#if defined(TTCLIENTSERVER)
        if (  mode == M_SCALEOUT_ROUTING  )
            thdbc = routingAPI_getConn(&id,&nb,&selstmt,&updstmt,&insstmt,&delstmt);
        else
#endif /* TTCLIENTSERVER */
#endif /* SCALEOUT && ROUTINGAPI */
            thdbc = ghdbc;

        if ( ! nodbexec )
        {
            rc = SQLExecute (delstmt);
            handle_errors (thdbc, delstmt, rc, ABORT_DISCONNECT_EXIT,
                           "executing delete",
                           __FILE__, __LINE__);

            if (op_count == opsperxact) {
              rc = SQLTransact (henv, thdbc, SQL_COMMIT);
              handle_errors (thdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                             "committing delete transaction",
                             __FILE__, __LINE__);
              op_count = 0;
            }
        }
      }
      else                                          /* insert xact */
      {
        id = insert_start;
        nb = insert_present++;
        if (insert_present == key_cnt)
        {
          insert_present = 0;
          insert_start += insert_mod;
        }
#if defined(SCALEOUT) && defined(ROUTINGAPI)
        if (  mode == M_SCALEOUT_LOCAL  )
            while (  !  routingAPI_isLocal(&id,&nb)  )
            {
                id = insert_start;
                nb = insert_present++;
                if (insert_present == key_cnt)
                {
                  insert_present = 0;
                  insert_start += insert_mod;
                }
            }
#if defined(TTCLIENTSERVER)
        if (  mode == M_SCALEOUT_ROUTING  )
            thdbc = routingAPI_getConn(&id,&nb,&selstmt,&updstmt,&insstmt,&delstmt);
        else
#endif /* TTCLIENTSERVER */
#endif /* SCALEOUT && ROUTINGAPI */
            thdbc = ghdbc;

        snprintf ((char*) directory, 10, "55%d%d", id, nb);
        sprintf ((char*) descr,
               "<place holder for description of VPN %d extension %d>", id, nb);

        if ( ! nodbexec )
        {
            /* execute and commit */
            rc = SQLExecute (insstmt);
            handle_errors (thdbc, insstmt, rc, ABORT_DISCONNECT_EXIT,
                           "executing insert",
                           __FILE__, __LINE__);
    
            if (op_count == opsperxact) {
              rc = SQLTransact (henv, thdbc, SQL_COMMIT);
              handle_errors (thdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                             "committing transaction",
                             __FILE__, __LINE__);
              op_count = 0;
            }
        }
      }
    }
finish_loop:
  /* the following is only for multi-process */
  if ((the_num_processes > 1) || (procId > 0))
  {
    /* each process indicates completion of run */
    shmhdr[procId].state = PROC_END;

    if (procId == 0)   /* parent process */
    {
      for (i = 1; i < the_num_processes; i++)
      {
        /* wait for all children to finish */
#if defined(WIN32)
        DWORD exitcode;

        while ((GetExitCodeProcess((HANDLE) shmhdr[i].pid, &exitcode) 
             && (exitcode == STILL_ACTIVE))
           && (shmhdr[i].state < PROC_END))
#else
        while ( (waitpid((pid_t)shmhdr[i].pid, NULL, WNOHANG) != (pid_t)shmhdr[i].pid) &&
                (shmhdr[i].state < PROC_END))
#endif
        {
            /* main/parent  process waits for all sub-processes to complete */
            tt_sleep (1);
        }
      }
    }
  }

  if (  ! duration ) {
      /* time at the end of the measured part of the run */
      ttGetTime (&main_end);
  }

  /* Only calculate stats in main process */
  if ( (procId == 0) && verbose)
  {
  /* compute transactions per second, handling zero time difference case */

      time_diff = diff_time (&main_start, &main_end);
      if (time_diff == 0)
        time_diff = 1000; /* 1 second minimum difference */

      num_xacts = shmhdr[0].xacts;
      if ( the_num_processes > 1 )
          for (i = 1; i < the_num_processes; i++)
               num_xacts += shmhdr[i].xacts;
      tps = ((double) num_xacts / time_diff) * 1000;
      if (opsperxact > 1)
        tps /= (double)opsperxact;

      if (time_diff >= 1000) {
        out_msg1("\nElapsed time:     %10.1f seconds \n", (double)time_diff/1000.0);
      }
      else {
        out_msg1("\nElapsed time:     %10.1f msec \n", (double)time_diff);
      }
      if (opsperxact == 0) {
        out_msg1("Operation rate:   %10.1f operations/second\n", tps);
      }
      else {
        out_msg1("Transaction rate: %10.1f transactions/second\n", tps);
      }
      if (opsperxact > 1) {
        out_msg2("Operation rate:   %10.1f operations/second (%d ops/transaction)\n",
                 tps * opsperxact, opsperxact);
      }
  }

cleanup:
#if defined(SCALEOUT) && defined(ROUTINGAPI)
  if (  mode > M_SCALEOUT  )
      freeRouting();
#if defined(TTCLIENTSERVER)
  if (  mode != M_SCALEOUT_ROUTING  )
  {
#endif /* TTCLIENTSERVER */
#endif /* SCALEOUT && ROUTING */
      rc = SQLTransact (henv, ghdbc, SQL_COMMIT);
      handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction",
                     __FILE__, __LINE__);

      /* drop and free the update statement */
      rc = SQLFreeStmt (updstmt, SQL_DROP);
      handle_errors (ghdbc, updstmt, rc, DISCONNECT_EXIT,
                     "freeing statement handle",
                     __FILE__, __LINE__);

      /* drop and free the select statement */
      rc = SQLFreeStmt (selstmt, SQL_DROP);
      handle_errors (ghdbc, selstmt, rc, DISCONNECT_EXIT,
                     "freeing statement handle",
                     __FILE__, __LINE__);

      /* drop and free the insert statement */
      rc = SQLFreeStmt (insstmt, SQL_DROP);
      handle_errors (ghdbc, insstmt, rc, DISCONNECT_EXIT,
                     "freeing statement handle",
                     __FILE__, __LINE__);

      /* drop and free the delete statement */
      rc = SQLFreeStmt (delstmt, SQL_DROP);
      handle_errors (ghdbc, delstmt, rc, DISCONNECT_EXIT,
                     "freeing delete statement handle",
                     __FILE__, __LINE__);
#if defined(SCALEOUT) && defined(ROUTINGAPI) && defined(TTCLIENTSERVER)
  }
#endif /* SCALEOUT && ROUTINGAPI && TTCLIENTSERVER */

  rc = SQLDisconnect(ghdbc);
  handle_errors(ghdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                      "disconnecting from database",
                         __FILE__, __LINE__);
  rc = SQLFreeConnect(ghdbc);
  handle_errors(ghdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                       "freeing connection handle",
                         __FILE__, __LINE__);

  rc = SQLFreeEnv(henv);
  handle_errors(SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "freeing environment handle",
                 __FILE__, __LINE__);

#if defined(WIN32)
    UnmapViewOfFile(shmhdr);
    CloseHandle(shmHndl);
    DeleteFile("CloseHandle");
#else
    shmdt ((char*) shmhdr);
#endif
}

/* Turn on and off echoing text to the console */
int chg_echo(int echo_on)
{
#ifdef WIN32

   HANDLE h;
   DWORD mode;

   h = GetStdHandle(STD_INPUT_HANDLE);
   if (! GetConsoleMode(h, &mode)) {
     return 0;
   }
   if (echo_on) {
     mode |= ENABLE_ECHO_INPUT;
   } else {
     mode &= ~ENABLE_ECHO_INPUT;
   }
   if (! SetConsoleMode(h, mode)) {
     return 0;
   }
   return 1;

#else
   
   struct termios tios;

   int fd = 0;
   if (tcgetattr(fd, &tios) == -1) {
     return 0;
   }
   if (echo_on) {
     tios.c_lflag |= ECHO;
     tios.c_lflag &= ~ECHONL;
   } else {
     tios.c_lflag &= ~ECHO;
     tios.c_lflag |= ECHONL;
   }
   if (tcsetattr(fd, TCSADRAIN, &tios) == -1) {
     perror("");
     return 0;
   }
   return 0;
#endif
} /* chg_echo */


void getPassword(const char * prompt, const char * uid, char * pswd, size_t len)
{
  int retCode = 0;

  /* Display the password prompt */
  printf("\n%s%s : ", prompt, uid);

  /* Do not echo the password to the console */
  retCode = chg_echo(0);

  /* get the password */
  fgets(pswd, len, stdin);
  
  /* Turn back on console output */
  retCode = chg_echo(1);

  /* Get rid of any CR or LF */
  pswd[strlen((char *) pswd) - 1] = '\0';

  printf("\n");
}

/*********************************************************************
 *
 *  FUNCTION:       erasePassword
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
erasePassword(volatile char *buf, size_t len)
{
   if (buf != NULL) {

     volatile char* p = buf;
     size_t i;
     for (p = buf, i = 0; i < len; ++p, ++i) {
       *p = 0;
     }
   }
}  /* erasePassword */

/*********************************************************************
 *
 *  FUNCTION:       parseConnectionString
 *
 *  DESCRIPTION:    Parses a connection string, verifying it conforms to
 *                  the allowed format, and extracts the DSN (mandatory),
 *                  username (optional) and password (optional, only if 
 *                  username is present).
 *
 *  PARAMETERS:     char * connstr   The connection string
 *                  char * pDSN      Buffer to receive DSN
 *                  int    sDSN      Size of DSN buffer
 *                  char * pUID      Buffer to receive username
 *                  int    sUID      Size of username buffer
 *                  char * pPWD      Buffer to receive password
 *                  int    sPWD      Size of password buffer
 *
 *  RETURNS:        True if successful, false if some error.
 *
 *  NOTES:          Required format for a connection strign is:
 *                  [DSN=]dsname[;UID=username[;PWD=password]]
 *
 *********************************************************************/

int
parseConnectionString(
                      char * connstr,
                      char * pDSN,
                      int    sDSN,
                      char * pUID,
                      int    sUID,
                      char * pPWD,
                      int    sPWD
                     )
{
    enum {eDSN, eUID, ePWD} aname;
    char * p, * q, *avalue;

    if (  (connstr == NULL) || 
          (pDSN == NULL) || (sDSN <= 0) ||
          (pUID == NULL) || (sUID <= 0) ||
          (pPWD == NULL) || (sPWD <= 0)  )
        return 0; /* invalid */
    *pDSN = *pUID = *pPWD = '\0';
    p = connstr;

    while (  (p != NULL) && (*p != '\0')  )
    {
        if (  strncasecmp( p, "UID=", strlen("UID=") ) == 0  )
        {
            aname = eUID;
            p += (strlen("UID=")-1);
            *p++ = '\0';
            avalue = p;
        }
        else
        if (  strncasecmp( p, "PWD=", strlen("PWD=") ) == 0  )
        {
            aname = ePWD;
            p += (strlen("PWD=")-1);
            *p++ = '\0';
            avalue = p;
        }
        else
        if (  strncasecmp( p, "DSN=", strlen("DSN=") ) == 0  )
        {
            aname = eDSN;
            p += (strlen("DSN=")-1);
            *p++ = '\0';
            avalue = p;
        }
        else
        {
            aname = eDSN;
            avalue = p;
        }
        if (  *p == '\0'  )
            return 0; /* invalid */
        q = strchr( p, ';' );
        if (  q != NULL  )
            *q++ = '\0';

        switch ( aname ) 
        {
            case eDSN:
                if (  *pDSN != '\0'  )
                    return 0; /* invalid */
                if (  (strlen(avalue)+1) > sDSN  )
                    return 0; /* invalid */
                strcpy( pDSN, avalue );
                break;
            case eUID:
                if (  *pUID != '\0'  )
                    return 0; /* invalid */
                if (  (strlen(avalue)+1) > sUID  )
                    return 0; /* invalid */
                strcpy( pUID, avalue );
                break;
            case ePWD:
                if (  *pPWD != '\0'  )
                    return 0; /* invalid */
                if (  (strlen(avalue)+1) > sPWD  )
                    return 0; /* invalid */
                strcpy( pPWD, avalue );
                break;
        }

        p = q;
    }

    /* validation */
    if (  *pDSN == '\0'  )
        return 0; /* invalid */
    if (  (*pPWD != '\0') && (*pUID == '\0')  )
        return 0; /* invalid */

    return 1;
}

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
