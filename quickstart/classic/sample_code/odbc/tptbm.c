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

#define VERBOSE_NOMSGS   0
#define VERBOSE_RESULTS  1    /* for results (and err msgs) only */
#define VERBOSE_DFLT     2    /* the default for the cmdline demo */
#define VERBOSE_ALL      3
#define VERBOSE_FIRST VERBOSE_NOMSGS
#define VERBOSE_LAST  VERBOSE_ALL

#define XS(str) #str
#define S(str)  XS(str)

#define NO_VALUE         -1
#define MIN_KEY           10
#define M_CLASSIC         0
#ifdef SCALEOUT
#define M_SCALEOUT        1
#define M_SCALEOUT_LOCAL  2
#endif
#define DFLT_XACT         10000
#define DFLT_SEED         1
#define DFLT_PROC         1
#define DFLT_RAMPUP       5
#define DFLT_READS        80
#define DFLT_INSERTS      0
#define DFLT_DELETES      0
#define DFLT_OPS          1
#define DFLT_KEY          100
#define DFLT_ISO          1
#define DFLT_RANGE        0
#define DFLT_BUILDONLY    0
#define DFLT_NOBUILD      0
#define DFLT_THROTTLE     0
#define DFLT_INSERT_MOD   1
#define DFLT_PROCID       0

#define PROC_INITIALIZED 0
#define PROC_READY       1
#define PROC_SET         2
#define PROC_GO          3
#define PROC_STARTBENCH  4
#define PROC_STOP        5
#define PROC_END         6

static char usageStr[] =
  "\n"
  "This program implements a multi-user throughput benchmark.\n\n"
  "Usage:\n\n"
  "  %s {-h | -help}\n"
  "  %s [-proc <nprocs>] [-read <nreads>] [-insert <nins>] [-delete <ndels>]\n"
  "     [-throttle <n>] [{-xact <xacts> | -sec <seconds> [-ramp <rseconds>]}]\n"
  "     [-ops <ops>] [-key <keys>] [-range] [-iso <level>] [-seed <seed>]\n"
  "     [-build] [-nobuild] [-v <level>]"
#ifdef SCALEOUT
  " [-scaleout [local]]"
#endif
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
  "  -sec     <seconds>  Specifies that <seconds> is the test duration.\n"
  "                      that each process should run. Default is to run in\n"
  "                      transaction mode (-xact).\n"
  "  -ramp  <rseconds>   Specifies that <rseconds> is the ramp up time in duration\n"
  "                      mode (-sec). Default is " S(DFLT_RAMPUP) ".\n"
  "  -ops     <ops>      Operations per transaction.  The default is " S(DFLT_OPS) ".\n"
  "                      In the special case where 0 is specified, no commit\n"
  "                      is done. This may be useful for read-only testing.\n"
  "  -key     <keys>     Specifies the number of records (squared) to initially\n"
  "                      populate in the data store. The minimum value is " S(MIN_KEY) "\n"
  "                      and the default is " S(DFLT_KEY) " (" S(DFLT_KEY) "**2 rows).\n"
  "  -range              Create a range index instead of a hash index.\n"
  "  -iso     <level>    Locking isolation level\n"
  "                         0 = serializable\n"
  "                         1 = read-committed (default)\n"
  "  -seed    <seed>     Specifies that <seed> should be the seed for the\n"
  "                      random number generator. Must be > 0, default is " S(DFLT_SEED) ".\n"
  "  -throttle <n>       Throttle each process to <n> operations per second.\n"
  "                      Must be > 0. The default is no throttle.\n"
  "  -build              Only build the database, do not run the benchmark.\n"
  "  -nobuild            Only run the benchmark, do not build the database.\n"
#ifdef SCALEOUT
  "  -scaleout [local]   Run in Scaleout mode (default is Classic). If 'local' is\n"
  "                      specified then use the routing API to ensure all operations\n"
  "                      are (as) local (as possible).\n"
#endif
  "  -v <level>          Verbosity level\n"
  "                         0 = errors only\n"
  "                         1 = results only\n"
  "                         2 = results and some status messages (default)\n"
  "                         3 = all messages\n\n"
  "If no DSN or connection string is specified, the default is\n"
  "  \"DSN=sampledb;UID=appuser\".\n\n"
  "The percentage of update operations is 100 minus the percentages of reads,\n"
  "inserts and deletes.\n\n";

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
void ExecuteTptBm (int        seed,
                   int        procId);

void erasePassword(volatile char *buf, size_t len);
void getPassword(const char * prompt, const char * uid, char * pswd, size_t len);
int
parseConnectionString(
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
  volatile UBIGINT   xacts;
  char      pad[CACHELINE_SIZE - sizeof(int) - sizeof(UBIGINT)];
} procinfo_t;

#ifdef SCALEOUT
/* ODBC routing API */
TTGRIDMAP   routingGridMap;
TTGRIDDIST  routingHDist;
/* The distribution key has two columns */
SQLSMALLINT routingCTypes[]   = { SQL_C_SLONG, SQL_C_SLONG };
SQLSMALLINT routingSQLTypes[] = { SQL_INTEGER, SQL_INTEGER };
SQLLEN      routingMaxSizes[] = { sizeof(int), sizeof(int) };

#ifdef TTCLIENTSERVER
typedef struct _ttclientdsn {
  int       elementid;
  int       repset;
  int       dataspace;
  SQLCHAR   clientdsn[64+1];
} cCliDSN_t;

cCliDSN_t*  routingDSNs = NULL;
int*        routingShmposMap = NULL;
int         nElements = 0;
int         kFactor = 0;
int         routingDSToUse = 1; /* Data Space */
#endif /* TTCLIENTSERVER */
/* End of ODBC Routing API */
#endif /* SCALEOUT */


/* Global variable declarations */

int        rand_seed = NO_VALUE;       /* seed for the random numbers */
int        num_processes = NO_VALUE;   /* # of concurrent processes for the test */
int        duration = NO_VALUE;        /* test duration */
int        rampup = NO_VALUE;          /* rampup in the duration mode */
int        reads = NO_VALUE;           /* read percentage */
int        inserts = NO_VALUE;         /* insert percentage */
int        deletes = NO_VALUE;         /* delete percentage */
int        num_xacts = NO_VALUE;       /* # of transactions per process */
int        opsperxact = NO_VALUE;      /* operations per transaction or 0 for no commit */
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

SQLHENV    henv;              /* ODBC environment handle */
SQLHDBC    ghdbc;             /* global ODBC connection handle */

#if defined(WIN32)
HANDLE     shmHndl;           /* handle to shared memory segment */
#else
int        shmid;             /* shared memory segment id  */
#endif
// int*       shmhdr;            /* shared memory segment to sync processes */
volatile procinfo_t *shmhdr = NULL;   /* shared memory segment to sync processes */
int        procId = NO_VALUE; /* process # in shared memory array */
int        sigReceived;       /* zero if no signal received or signum */
tt_ptrint  *pidArr;           /* array to store list of pids [and DSN for Windows] */
                              /* How this is laid out in memory: */
                              /*   pidArr[0] = num_processes, needed in the child */
                              /*   pidArr[1] = pid 1 */
                              /*   pidArr[2] = pid 2 */
                              /*   ... */
                              /*   pidArr[num_processes] = pid num_processes */
                              /*   pidArr[num_processes + 1] = Length of Connect String */
                              /*   pidArr[num_processes + 2] = ConnectString[0] */
                              /*   pidArr[num_processes + 3] = ConnectString[1] */
                              /*   ... */
                              /*   pidArr[num_processes + lenConnStr] = ConnectString[1] */

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
char* creat_stmnt = "CREATE TABLE vpn_users("
                    "vpn_id             TT_INT   NOT NULL,"
                    "vpn_nb             TT_INT   NOT NULL,"
                    "directory_nb       CHAR(10)  NOT NULL,"
                    "last_calling_party CHAR(10)  NOT NULL,"
                    "descr              CHAR(100) NOT NULL,"
                    "PRIMARY KEY (vpn_id,vpn_nb)) "
                    "unique hash on (vpn_id,vpn_nb) pages = %d";

char* create_stmnt_nohash = "CREATE TABLE vpn_users("
                            "vpn_id             TT_INT   NOT NULL,"
                            "vpn_nb             TT_INT   NOT NULL,"
                            "directory_nb       CHAR(10)  NOT NULL,"
                            "last_calling_party CHAR(10)  NOT NULL,"
                            "descr              CHAR(100) NOT NULL)";

char* drop_stmnt = "DROP TABLE vpn_users";

char* create_index = "create index tindex on vpn_users(vpn_id,vpn_nb)";

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
 *  RETURNS:        0 - failure; 1 - success; 2 - success (caller exits)
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int parse_args (int      argc,
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
            (duration != NO_VALUE ) || (rampup != NO_VALUE)  )
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
      if (  (++argno >= argc) || ( rampup != NO_VALUE) || 
            (num_xacts != NO_VALUE )  )
          usage( cmdname );
      rampup = isnumeric( argv[argno] );
      if (  rampup < 0  )
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
          fprintf( stderr, "Multiple '-insertmod' parameters are not allowed.\n");
          exit(1);
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
          fprintf( stderr, "Multiple '-procid' parameters are not allowed.\n");
          exit(1);
      }
      procId = isnumeric( argv[argno] );
      if (  procId <= 0  ) {
          fprintf( stderr, "Value for '-procid' must be > 0.\n");
          exit(1);
      }
    }
#endif /* WIN32 */
#ifdef SCALEOUT
    else
    if (  strcmp(argv[argno], "-scaleout") == 0  ) {
      if (  ((argno+1) >= argc) && (strcmp(argv[argno+1],"local") == 0)  ) {
          mode = M_SCALEOUT_LOCAL;
          argno += 1;
      }
      else
          mode = M_SCALEOUT;
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
      if (  rampup == NO_VALUE  )
          rampup = DFLT_RAMPUP;
      num_xacts = 0;
  }
  else {
    if (  rampup != NO_VALUE  )
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
  if ((reads + inserts + deletes) > 100)
  {
    fprintf(stderr, "insert + read + delete must not exceed 100. \n");
    return 0;
  }

  if ((insert_mod * (num_xacts / 100 * (float)inserts)) >
      (key_cnt * key_cnt))
  {
    fprintf(stderr, "Inserts as part of transaction mix exceed number\n"
                    "of rows initially populated into database.\n");
    return 0;
  }

  if ((insert_mod * (num_xacts / 100 * (float)deletes)) >
      (key_cnt * key_cnt))
  {
    fprintf(stderr, "Deletes as part of transaction mix exceed number\n"
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
      fprintf(stderr, "DSN '%s' too long.\n", dsn);
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
    if ( username[0] && !password[0] && (procId == 0)  )
      getPassword(passwordPrompt, username, (char *) password, sizeof(password) );

    pos = 1 + strlen( "DSN=" ) + strlen(dsn);
    if (  username[0]  )
       pos += strlen( ";UID=" ) + strlen( username );
    if (  password[0]  )
       pos += strlen( ";PWD=" ) + strlen( password );
    if (  pos > sizeof(connstr)  ) {
      fprintf(stderr, "Connection string is too long.\n");
      exit(-1);
    }

    pos = 0;
    pos += sprintf( connstr+pos, "DSN=%s", dsn );
    if (  username[0]  )
        pos += sprintf( connstr+pos, ";UID=%s", username );
    strcpy( connstr_no_password, connstr );
    if (  password[0]  ) 
        pos += sprintf( connstr+pos, ";PWD=%s", password );

#ifndef WIN32
    /* now erase the password */
    erasePassword(password, strlen(password));
#endif
  } 

  return 1;
}

/*********************************************************************
 *
 *  FUNCTION:       populate
 *
 *  DESCRIPTION:    This function populates the data source,
 *                  specified by name, with key_cnt**2 records.
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
  int        i, j;
  int        id;
  int        nb;
  SQLCHAR    directory   [11];
  SQLCHAR    last_calling_party[11] = "0000000000";
  SQLCHAR    descr       [101];
  char       buff1       [1024]; /* Connection flags */
  char       buff2       [1024];
  char*      pStmt;


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

  /* NOTE, Cannot erase the connstr_real yet as it may be used by child processes */
  
  /* Trying to connect output */
  sprintf (buff2, "Connecting to driver (connect string: %s)\n",
           connstr_no_password);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, buff2, __FILE__, __LINE__);

  /* Connected output */
  sprintf (buff2, "Connected using %s\n",
           connstr_no_password);
  printf("%s\n", buff2);

  if (!noBuild)
  {
    /* allocate and execute a create statement */
    rc = SQLAllocStmt (hdbc, &hstmt);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "allocating a create statement handle",
                   __FILE__, __LINE__);

    rc = SQLSetConnectOption (hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                   "turning off AUTO_COMMIT option",
                   __FILE__, __LINE__);

    /* Drop the VPN_USERS table if it already exists */
    /* Ignore the error is the VPN_USERS table did not exist   */
    rc = SQLExecDirect (hstmt, (SQLCHAR*) drop_stmnt, SQL_NTS);


    /* make create statement */
    sprintf (buff1, creat_stmnt, (key_cnt * key_cnt)/256);
    pStmt = buff1;

    if (rangeFlag)
      pStmt = create_stmnt_nohash;

    rc = SQLExecDirect (hstmt, (SQLCHAR*) pStmt, SQL_NTS);
    handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                   "executing create table",
                   __FILE__, __LINE__);

    /* Create an index */
    if (rangeFlag)
    {
      rc = SQLExecDirect (hstmt, (SQLCHAR*) create_index, SQL_NTS);
      handle_errors (hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                     "executing create table",
                     __FILE__, __LINE__);
    }

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

    /* commit the transaction */
    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction",
                   __FILE__, __LINE__);

    tptbm_msg0("Populating benchmark data store\n");

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
  }

}

#if defined(WIN32)
void OpenShmSeg (int  shmSize)
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
      // for (i = 0; i < (shmSize / sizeof(int)); i++) {
        // shmhdr [i] = 0; 
      // }
      memset( (void *)shmhdr, 0, shmSize );

      /* Set the number of processes for the children to get the correct offsets */
      shmhdr[0].state = num_processes;

      /* How long is the "real" Connect String */
      // shmhdr[(sizeofnum_processes + 1] = strlen(connstr_real);

      /* Save the connection string for the children to read */
      // for (i = num_processes + 2, j = 0; i <= num_processes + strlen(password) + 3; i++, j++)
        // shmhdr [i] = password[j]; 
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
void OpenShmSeg (int  shmSize)
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
        /*    Used for array of PIDs and DSN to pass to child processes */
        // for (i = 0; i < (shmSize / sizeof(int)); i++) {
          // shmhdr [i] = 0; 
        // }
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



void CreateChildProcs (char*    progName)
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

    pidArr [i] = (tt_ptrint) pid;

#else /* NT */

    /* in this case spawn more processes with a different value for procid
     * the variable procid corresponds to the position of the
     * process in the shared memory array - it is 0 in the parent case */

    {
      /* spawnlp requires quoting the connection string
       *  in case it has embedded spaces
       */
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
#ifdef SCALEOUT
      if (  mode > M_CLASSIC  )
          pos += sprintf( cmdLine+pos, "-scaleout " );
      if (  mode == M_CLASSIC_LOCAL  )
          pos += sprintf( cmdLine+pos, "local " );
#endif /* SCALEOUT */
      if (  num_xacts > 0  )
          pos += sprintf( cmdLine+pos, "-xact %d ", num_xacts );
      else
          pos += sprintf( cmdLine+pos, "-sec %d -ramp %d ", duration, rampup );

      if (CreateProcess (NULL, cmdLine, NULL, NULL, FALSE,
                         CREATE_DEFAULT_ERROR_MODE, NULL,
                         NULL, &sInfo, &pInfo) == 0)
      {
        err_msg0("Failure spawning a process\n");
        exit (1);
      }

      pidArr [i] = (tt_ptrint) pInfo.hProcess;
    }

    pid = 1;

#endif /* NT */
  }
}

/*********************************************************************
 *
 *  FUNCTION:       main
 *
 *  DESCRIPTION:    This is the main function of the tpt benchmark.
 *                  It connects to an ODBC data source, creates and
 *                  populates a table, spawns num_processes
 *                  processes (each performing xacts transactions
 *                  of which 80% are selects and 20% are updates),
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

int main (int argc, char** argv)
{
  int         retval;
  SQLRETURN   rc;

  retval = 1; /* Assume failure */
#if defined(TTCLIENTSERVER) && defined(__hppa) && !defined(__LP64__)
  /* HP requires this for C main programs that call aC++ shared libs */
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
  }

  if (buildOnly)
  {
    rc = SQLDisconnect (ghdbc);
    handle_errors (ghdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "disconnecting from data source",
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
  }

  pidArr = malloc(sizeof(tt_ptrint) * (num_processes + strlen(connstr) + 4));
  if (pidArr == NULL)
  {
    fprintf(stderr, "Failed to allocate memory for pidArr\n");
    exit(-1);
  }

#if 0
  if ((num_processes > 1) || (procId > 0))
    OpenShmSeg ( (sizeof(procinfo_t) * num_processes) + strlen(connstr) + 4);

  CreateChildProcs (argv[0]);
#else
  if (  procId == 0  )
  {
      OpenShmSeg( (sizeof(procinfo_t) * num_processes) + strlen(connstr) + 4);
      if ( num_processes > 1 )
          CreateChildProcs(argv[0]);
  }
  if (  (num_processes > 1) && (procId > 0) )
      OpenShmSeg( (sizeof(procinfo_t) * num_processes) + strlen(connstr) + 4);
#endif

  ExecuteTptBm (rand_seed, procId);
  retval = 0;

 leaveMain:
  return retval;
}


void ExecuteTptBm (int          seed,
                   int          procId)
{
  SQLHSTMT    upstmt,selstmt,instmt,delstmt;
  SQLRETURN   rc;

  ttTime    main_start;       /* variable to measure time */
  ttTime    main_end;         /* variable to measure time */

  time_t    interval_start;   /* variable to measure interval time */
  time_t    interval_cur;     /* variable to measure interval time */
  time_t    interval_diff;    /* variable to measure interval time */
  time_t    duration_start, duration_cur, duration_diff;
  int       duration_target = 0;
  UBIGINT   duration_est = 10000;    /* duration estimate; initial guess = 10000 xacts / sec */
  int       rampingup;
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

  /* the following three buffers are for the select statement */
  SQLCHAR   directory [11];
  SQLCHAR   last [11];
  SQLCHAR   descr [101];
  SQLCHAR   last_calling_party[11] = "0000000000";

  /* the following two buffers are used for all statements */
  int       id;
  int       nb;
  int       path = 0;         /* type of transaction */

  SQLHDBC   hdbc;

  int the_num_processes = 0;
  int connStrLen ;
  int k;
  int j;
  char * p;


  srand(seed);

  /* initialize the select statement buffers */
  directory [10]  = '\0';
  last      [10]  = '\0';
  descr     [100] = '\0';



  /* Connect to data store */
  if (procId == 0) {
    hdbc = ghdbc;
    the_num_processes = num_processes;
  } else {
    /* Get the num_processes from SHM */
    the_num_processes = shmhdr[0].state;

#ifdef WIN32  /* Only needed on Windoze */
    p = (char *)shmhdr + (sizeof(procinfo_t) * the_num_processes);
    strcpy( connstr, p );
    connStrLen = strlen( connstr );
    strcpy( connstr_real, connstr );
#if 0
    /* Get the length of the connect string from SHM */
    connStrLen = shmhdr[the_num_processes + 1];

    k = 0;

    /* Get the connect string from the correct SHM offsets */
    for (j = the_num_processes + 2; j <= the_num_processes + connStrLen + 1; j++) {
      connstr_real[k] = shmhdr[j];
      k++;
    }
#endif
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
    
    rc = SQLAllocConnect (henv, &hdbc);
    handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "allocating a connection handle",
                   __FILE__, __LINE__);
    

    // printf("In ExecuteTptBm (Child process = %d), waiting to connect\n", procId);

    rc = SQLDriverConnect (hdbc, NULL, (SQLCHAR*) connstr_real, SQL_NTS,
                           NULL, 0, NULL, SQL_DRIVER_NOPROMPT);

    /* Now erase the connect string to get rid of the password */
    erasePassword(connstr_real, strlen(connstr_real));
    erasePassword(connstr, strlen(connstr));

    sprintf (errstr, "connecting to driver (connect string %s)\n",
             connstr_no_password);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT, errstr, __FILE__, __LINE__);

  }
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

    rc = SQLSetConnectOption (hdbc, SQL_TXN_ISOLATION, tIso);
    handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, DISCONNECT_EXIT,
                   "setting connection option",
                   __FILE__, __LINE__);
  }

  /* allocate an update statement */
  rc = SQLAllocStmt (hdbc, &upstmt);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                 "allocating an update statement handle",
                 __FILE__, __LINE__);

  /* allocate a select statement */
  rc = SQLAllocStmt (hdbc, &selstmt);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                 "allocating a select statement handle",
                 __FILE__, __LINE__);

  /* allocate an insert statement */
  rc = SQLAllocStmt (hdbc, &instmt);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                 "allocating an insert statement handle",
                 __FILE__, __LINE__);

  /* allocate a  delete statement */
  rc = SQLAllocStmt (hdbc, &delstmt);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                 "allocating a delete statement handle",
                 __FILE__, __LINE__);

  /* turn off autocommit */
  rc = SQLSetConnectOption (hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ABORT_DISCONNECT_EXIT,
                 "turning AUTO_COMMIT option OFF",
                 __FILE__, __LINE__);

  /* prepare the select statement */
  rc = SQLPrepare (selstmt, (SQLCHAR*) select_stmnt, SQL_NTS);
  handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                 "preparing select",
                 __FILE__, __LINE__);

  /* Bind the selected columns  */
  rc = SQLBindCol (selstmt, 1, SQL_C_CHAR, &directory, 11, NULL);
  handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding column",
                 __FILE__, __LINE__);

  rc = SQLBindCol (selstmt, 2, SQL_C_CHAR, &last, 11, NULL);
  handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding column",
                 __FILE__, __LINE__);

  rc = SQLBindCol (selstmt, 3, SQL_C_CHAR, &descr, 101, NULL);
  handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding column",
                 __FILE__, __LINE__);

  /* bind the input parameters */
  rc = SQLBindParameter (selstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, 
                         SQL_INTEGER, 0, 0, &id, 0, NULL);
  handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  rc = SQLBindParameter (selstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, 
                         SQL_INTEGER, 0, 0, &nb, 0, NULL);
  handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction",
                 __FILE__, __LINE__);

  /* prepare the update statement */
  rc = SQLPrepare (upstmt, (SQLCHAR*) update_stmnt, SQL_NTS);
  handle_errors (hdbc, upstmt, rc, ABORT_DISCONNECT_EXIT,
                 "preparing update",
                 __FILE__, __LINE__);

  /* bind the input parameters */
  rc = SQLBindParameter (upstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                         11, 0, last, 0, NULL);
  handle_errors (hdbc, upstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  rc = SQLBindParameter (upstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                         0, 0, &id, 0, NULL);
  handle_errors (hdbc, upstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  rc = SQLBindParameter (upstmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                         0, 0, &nb, 0, NULL);
  handle_errors (hdbc, upstmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  /* prepare the delete statement */

  rc = SQLPrepare(delstmt,(SQLCHAR *) delete_stmnt, SQL_NTS);
  handle_errors(hdbc, delstmt, rc, 1,
                "Unable to prepare delete",
                __FILE__, __LINE__);

  /* bind the input parameters */

  rc = SQLBindParameter(delstmt,1,SQL_PARAM_INPUT,SQL_C_SLONG,SQL_INTEGER,
                        0,0,&id,0,NULL);
  handle_errors(hdbc, delstmt, rc, 1, "Unable to bind parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(delstmt,2,SQL_PARAM_INPUT,SQL_C_SLONG,SQL_INTEGER,
                        0,0,&nb,0,NULL);
  handle_errors(hdbc, delstmt, rc, 1, "Unable to bind parameter",
                __FILE__, __LINE__);


  /* prepare the insert */
  rc = SQLPrepare (instmt, (SQLCHAR*) insert_stmnt, SQL_NTS);
  handle_errors (hdbc, instmt, rc, ABORT_DISCONNECT_EXIT,
                 "preparing insert",
                 __FILE__, __LINE__);

  /* bind the input parameters */
  rc = SQLBindParameter (instmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                         0, 0, &id, 0, NULL);
  handle_errors (hdbc, instmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  rc = SQLBindParameter (instmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                         0, 0, &nb, 0, NULL);
  handle_errors (hdbc, instmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  rc = SQLBindParameter (instmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                         11, 0, directory, 0, NULL);
  handle_errors (hdbc, instmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  rc = SQLBindParameter (instmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_CHAR, 11, 0, last_calling_party, 0, NULL);
  handle_errors (hdbc, instmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  rc = SQLBindParameter (instmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
                         101, 0, descr, 0, NULL);
  handle_errors (hdbc, instmt, rc, ABORT_DISCONNECT_EXIT,
                 "binding parameter",
                 __FILE__, __LINE__);

  /* commit the transaction */
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction",
                 __FILE__, __LINE__);

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
                 the_num_processes, the_num_processes > 1 ? "es" : "",
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
  }

  time(&interval_start);

  if ( (procId == 0) && duration) {
    duration_start = interval_start;
    if (rampup > 0) {
      duration_target = rampup;
      rampingup = 1;
    } else {
      duration_target = duration;
      rampingup = 0;
    }
  }

  ttGetTime (&main_start);

  insert_start = key_cnt + procId;   /* start mark for this process */
  insert_present = 0;
  delete_start = procId;

    long delete_max = (key_cnt * key_cnt) / the_num_processes;
    long deleted = 0;
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
          if (shmhdr[procId].state != PROC_GO) {
            if (shmhdr[procId].state == PROC_STARTBENCH) {
              i = 0; /* reset the counter */
              shmhdr[procId].state = PROC_GO;
            }
            else if (shmhdr[procId].state == PROC_STOP) {
              if (verbose >= VERBOSE_ALL)
                status_msg2("Process %d finished %.0f xacts\n",
                            procId, (double)i);
              shmhdr[procId].xacts = i; /* store the number of xacts */
              goto finish_loop;
            }
          }
        }
        /* parent */
        else {
          if (i > duration_est) {
            int child;
            (void)time(&duration_cur);
            duration_diff = (duration_cur - duration_start);
            /* recalculate the estimation */
            if (duration_diff == 0) duration_diff = 1; /* avoid divide-by-zero */
            if (duration_diff < 10) /* too short to determine the estimation */
              duration_est = (i / duration_diff) * (duration_diff + 10);
            else if (duration_diff < duration_target - 5) /* jump to 5 seconds before the end */
              duration_est = (i / duration_diff) * (duration_target - 5);
            else /* for last 5 seconds, try second by second */
              duration_est = (i / duration_diff) * (duration_diff + 1);

            /* duration passed */
            if (duration_diff >= (time_t)duration_target) {
              if (rampingup) {
                /* RAMP-UP done */
                status_msg0("Ramp-up done...benchmark begins\n");
                ttGetTime(&main_start); /* reset the start time */
                if (the_num_processes > 1) {
                  for (child = 1; child < the_num_processes; child++) {
                    shmhdr[child].state = PROC_STARTBENCH;
                  }
                }
                duration_start = duration_cur;
                duration_target = duration;
                rampingup = 0;
                i = 0;
              } else {
                /* Benchmark finished */
                if (the_num_processes > 1) {
                  for (child = 1; child < the_num_processes; child++) {
                    /* tell children to stop */
                    shmhdr[child].state = PROC_STOP;
                  }
                }
                shmhdr[procId].xacts = i; /* store the number of xacts */
                if (verbose >= VERBOSE_ALL)
                  status_msg2("Process %d finished %.0f xacts\n",
                              procId, (double)i);
                goto finish_loop;
              }
            }

          } /* i > duration_est */
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
        id = (int) ((double)key_cnt * rand() / ((unsigned int)RAND_MAX + 1));
        nb = (int) ((double)key_cnt * rand() / ((unsigned int)RAND_MAX + 1));

        /* execute, fetch, free and commit */
        rc = SQLExecute (selstmt);
        handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                       "executing select",
                       __FILE__, __LINE__);

        rc = SQLFetch (selstmt);
        handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                       "fetching select",
                       __FILE__, __LINE__);

        rc = SQLFreeStmt (selstmt,SQL_CLOSE);
        handle_errors (hdbc, selstmt, rc, ABORT_DISCONNECT_EXIT,
                       "closing select",
                       __FILE__, __LINE__);

        if (op_count == opsperxact) {
          /* TimesTen doesn't require reads to be committed          */
          rc = SQLTransact (henv, hdbc, SQL_COMMIT);
          handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                         "committing transaction",
                         __FILE__, __LINE__);
          op_count = 0;
        }
      }
      else if (path == 0)                           /* update xact */
      {
        /* randomly pick argument values */
        id = (int) ((double)key_cnt * rand() / ((unsigned int)RAND_MAX + 1));
        nb = (int) ((double)key_cnt * rand() / ((unsigned int)RAND_MAX + 1));

        sprintf ((char*)last, "%dx%d", id, nb);
        last[10] = 0;

        /* execute and commit */
        rc = SQLExecute (upstmt);
        handle_errors (hdbc, upstmt, rc, ABORT_DISCONNECT_EXIT,
                       "executing update",
                       __FILE__, __LINE__);

        if (op_count == opsperxact) {
          rc = SQLTransact (henv, hdbc, SQL_COMMIT);
          handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                         "committing transaction",
                         __FILE__, __LINE__);
          op_count = 0;
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
        rc = SQLExecute (delstmt);
        handle_errors (hdbc, delstmt, rc, ABORT_DISCONNECT_EXIT,
                       "executing delete",
                       __FILE__, __LINE__);

        if (op_count == opsperxact) {
          rc = SQLTransact (henv, hdbc, SQL_COMMIT);
          handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                         "committing delete transaction",
                         __FILE__, __LINE__);
          op_count = 0;
        }
      }
      else                                          /* insert xact */
      {
        id = insert_start;
        nb = insert_present++;
        snprintf ((char*) directory, 10, "55%d%d", id, nb);
        sprintf ((char*) descr,
               "<place holder for description of VPN %d extension %d>", id, nb);

        if (insert_present == key_cnt)
        {
          insert_present = 0;
          insert_start += insert_mod;
        }

        /* execute and commit */
        rc = SQLExecute (instmt);
        handle_errors (hdbc, instmt, rc, ABORT_DISCONNECT_EXIT,
                       "executing insert",
                       __FILE__, __LINE__);


        if (op_count == opsperxact) {
          rc = SQLTransact (henv, hdbc, SQL_COMMIT);
          handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                         "committing transaction",
                         __FILE__, __LINE__);
          op_count = 0;
        }
      }
    }
finish_loop:
   rc = SQLTransact(henv, hdbc, SQL_COMMIT);
   handle_errors(hdbc, NULL, rc, -3, "Unable to commit last transaction",
                    __FILE__, __LINE__);

  /* the following is only for multi-process */
  if ((the_num_processes > 1) || (procId > 0))
  {
    /* each process indicates completion of run */
    shmhdr[procId].state = PROC_END;

    if (procId == 0)   /* parent process */
    {
      for (i = 1; i < the_num_processes; i++)
      {
        /* code to check if any children are dead */
#if defined(WIN32)
        DWORD exitcode;

        while ((GetExitCodeProcess ((HANDLE) pidArr [i], &exitcode) 
             && (exitcode == STILL_ACTIVE))
           && (shmhdr[i].state < PROC_END))
#else
        while ((waitpid (pidArr [i], NULL, WNOHANG) != pidArr [i])
           && (shmhdr[i].state < PROC_END))
#endif
        {
          /* main/parent  process waits for all sub-processes to complete */
            tt_sleep (1);
        }
      }
    }
  }

  /* time at the end of the run */
  ttGetTime (&main_end);

  /* compute transactions per second, handling zero time difference case */

  time_diff = diff_time (&main_start, &main_end);
  if (time_diff == 0)
    time_diff = 1000; /* 1 second minimum difference */

  num_xacts = shmhdr[0].xacts;
  if ( the_num_processes > 1 )
      for (i = 1; i < the_num_processes; i++)
           num_xacts += shmhdr[i].xacts;
  tps = ((double) num_xacts / time_diff) * 1000;
  if (opsperxact > 1) {
    tps /= (double)opsperxact;
  }

  /* print this message only in the main/parent process */
  if (procId == 0 && verbose)
  {
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
  rc = SQLTransact (henv, hdbc, SQL_COMMIT);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "committing transaction",
                 __FILE__, __LINE__);

  /* drop and free the update statement */
  rc = SQLFreeStmt (upstmt, SQL_DROP);
  handle_errors (hdbc, upstmt, rc, DISCONNECT_EXIT,
                 "freeing statement handle",
                 __FILE__, __LINE__);

  /* drop and free the select statement */
  rc = SQLFreeStmt (selstmt, SQL_DROP);
  handle_errors (hdbc, selstmt, rc, DISCONNECT_EXIT,
                 "freeing statement handle",
                 __FILE__, __LINE__);

  /* drop and free the insert statement */
  rc = SQLFreeStmt (instmt, SQL_DROP);
  handle_errors (hdbc, instmt, rc, DISCONNECT_EXIT,
                 "freeing statement handle",
                 __FILE__, __LINE__);

  /* drop and free the delete statement */
  rc = SQLFreeStmt (delstmt, SQL_DROP);
  handle_errors (hdbc, delstmt, rc, DISCONNECT_EXIT,
                 "freeing delete statement handle",
                 __FILE__, __LINE__);

  rc = SQLDisconnect (hdbc);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "disconnecting from data source",
                 __FILE__, __LINE__);

  rc = SQLFreeConnect (hdbc);
  handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "freeing connection handle",
                 __FILE__, __LINE__);

  rc = SQLFreeEnv (henv);
  handle_errors (SQL_NULL_HDBC, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                 "freeing environment handle",
                 __FILE__, __LINE__);


  /* the following is only for multi-process */
  if ((the_num_processes > 1) || (procId > 0))
  {
#if defined(WIN32)

    UnmapViewOfFile (shmhdr);
    CloseHandle (shmHndl);
    DeleteFile("CloseHandle");
    

#else
    shmdt ((char*) shmhdr);

#endif
  }
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
