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


static char usageStr[] =
  "\n"
  "Usage:\t<CMD> {-h | -help | -V}\n"
  "\t<CMD> [-proc <nprocs>] [-read <nreads>] [-insert <nins>] [-delete <ndels>]\n"
  "\t\t[-xact <xacts>] [-ops <ops>] [-key <keys>] [-range]\n"
  "\t\t[-iso <level>] [-seed <seed>] [-build] [-nobuild] [-v <level>]\n"
  "\t\t[<DSN> | -connstr <connection-string>]\n\n"
  "  -h                  Prints this message and exits.\n"
  "  -help               Same as -h.\n"
  "  -V                  Prints version number and exits.\n"
  "  -proc    <nprocs>   Specifies that <nprocs> is the number of concurrent\n"
  "                      processes. The default is 1.\n"
  "  -read    <nreads>   Specifies that <nreads> is the percentage of read-only\n"
  "                      transactions. The default is 80.\n"
  "  -insert  <nins>     Specifies that <nins> is the percentage of insert\n"
  "                      transactions. The default is 0.\n"
  "  -delete  <ndels>    Specifies that <ndels> is the percentage of delete\n"
  "                      transactions. The default is 0.\n"
  "  -xact    <xacts>    Specifies that <xacts> is the number of transactions\n"
  "                      that each process should run. The default is 10000.\n"
  "  -ops     <ops>      Operations per transaction.  The default is 1.\n"
  "                      In the special case where 0 is specified, no commit\n"
  "                      is done. This may be useful for read-only testing.\n"
  "  -key     <keys>     Specifies the number of records (squared) to initially\n"
  "                      populate in the data store. The default is 100**2.\n"
  "  -range              Create a range index instead of a hash index.\n"
  "  -iso     <level>    Locking isolation level\n"
  "                         0 = serializable\n"
  "                         1 = read-committed (default)\n"
  "  -seed    <seed>     Specifies that <seed> should be the seed for the\n"
  "                      random number generator.\n"
  "  -throttle <n>       Throttle to <n> operations per second\n"
  "  -build              Only build the database.\n"
  "  -nobuild            Only run the benchmark.\n"
  "  -v <level>          Verbose level\n"
  "                         0 = errors only\n"
  "                         1 = results only\n"
  "                         2 = results and some status messages (default)\n"
  "                         3 = all messages\n"
  "If no DSN or connection string is specified, the default is\n"
  "  \"DSN=sampledb;UID=appuser\".\n\n"
  "This program implements a multi-user throughput benchmark.\n\n";




#define DSNNAME DEMODSN

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



/* function to determine odbc INI parameters */
int getinikey (char*        connStr,
               const char*  sought,
               char*        result);

void ExecuteTptBm (int        seed,
                   int        procId);

void erasePassword(volatile char *buf, size_t len);
void getPassword(const char * prompt, const char * uid, char * pswd, size_t len);
int getUid(const char * dsnOrConnstr, char * uid);
int pwdExists(const char * dsnOrConnstr);


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



/* Global variable declarations */

int        rand_seed = 1;     /* seed for the random numbers */
int        num_processes = 1; /* # of concurrent processes for the test */
int        reads = 80;        /* read percentage */
int        inserts = 0;       /* insert percentage */
int        deletes = 0;       /* delete percentage */
int        num_xacts = 10000; /* # of transactions per process */
int        opsperxact = 1;    /* operations per transaction or 0 for no commit */
int        key_cnt = 100;     /* number of keys (squared) populated in the datastore */
int        isolevel = -1;     /* isolation level (or -1 for no change */
int        rangeFlag = 0;     /* if 1 use range index instead of hash */
int        verbose = VERBOSE_DFLT; /* verbose level */
FILE*      statusfp;          /* File for status messages */
char       dsn[CONN_STR_LEN] = { 0 }; /* ODBC data source */
char       connstr_opt[CONN_STR_LEN] = { 0 }; /* ODBC connStr from cmd line */
char*      input_connStr = NULL; /* connection string to be used */
int        buildOnly = 0;     /* Only create/populate */
int        noBuild = 0;       /* Don't create/populate */
int        replication_ended = 0; /* Indicates when replication ended */
int        replication_begin = 0; /* Indicates when replication may begin */
int        primary = 0;       /* primary datastore for replication */
int        standby = 0;       /* standby datatstore for replication */
int        insert_mod = 1;    /* used to prevent multiple inserts clobber */
int        throttle = 0;      /* throttle to <n> ops / second */

SQLHENV    henv;              /* ODBC environment handle */
SQLHDBC    ghdbc;             /* global ODBC connection handle */

#if defined(WIN32)
HANDLE     shmHndl;           /* handle to shared memory segment */
#else
int        shmid;             /* shared memory segment id  */
#endif
int*       shmhdr;            /* shared memory segment to sync processes */
int        procId = 0;        /* process # in shared memory array */
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
  int ac;
  char errbuf[80];

  ttc_getcmdname(argv[0], cmdname, sizeof(cmdname));

  ac = ttc_getoptions(argc, argv, TTC_OPT_NO_CONNSTR_OK,
                    errbuf, sizeof(errbuf),
                    "<HELP>",         usageStr,
                    "<VERSION>",      NULL,
                    "<CONNSTR>",      connstr_opt, sizeof(connstr_opt),
                    "<DSN>",          dsn, sizeof(dsn),
                    "proc=i",         &num_processes,
                    "xact=i",         &num_xacts,
                    "seed=i",         &rand_seed,
                    "ops=i",          &opsperxact,
                    "read=i",         &reads,
                    "insert=i",       &inserts,
                    "delete=i",       &deletes,
                    "key=i",          &key_cnt,
                    "insertmod=i",    &insert_mod, /* undocumented */
                    "range",          &rangeFlag,
                    "build",          &buildOnly,
                    "nobuild",        &noBuild,
                    "primary",        &primary,    /* undocumented */
                    "standby",        &standby,    /* undocumented */
                    "v=i",            &verbose,
                    "iso=i",          &isolevel,
                    "throttle=i",     &throttle,
#if defined(WIN32)
                    "procid=i",       &procId,     /* undocumented */
#endif
                    NULL);

  if (ac == -1) {
    fprintf(stderr, "%s\n", errbuf);
    fprintf(stderr, "Type '%s -help' for more information.\n", cmdname);
    exit(-1);
  }
  if (ac != argc) {
    ttc_dump_help(stderr, cmdname, usageStr);
    exit(-1);
  }

  /* Various checks on inputs */
  if (num_processes <= 0) {
    fprintf(stderr, "%s: -proc value must be > 0.\n", cmdname);
    exit(-1);
  }
  if (num_xacts <= 0) {
    fprintf(stderr, "%s: -xact value must be > 0.\n", cmdname);
    exit(-1);
  }
  if (rand_seed <= 0) {
    fprintf(stderr, "%s: -seed value must be > 0.\n", cmdname);
    exit(-1);
  }
  if (opsperxact < 0) {
    fprintf(stderr, "%s: -ops value must be >= 0.\n", cmdname);
    exit(-1);
  }
  if (reads < 0 || reads > 100) {
    fprintf(stderr, "%s: -read value must be in range [0..100].\n", cmdname);
    exit(-1);
  }
  if (inserts < 0 || inserts > 100) {
    fprintf(stderr, "%s: -insert value must be in range [0..100].\n", cmdname);
    exit(-1);
  }
  if (deletes < 0 || deletes > 100) {
    fprintf(stderr, "%s: -delete value must be in range [0..100].\n", cmdname);
    exit(-1);
  }
  if (key_cnt <= 0) {
    fprintf(stderr, "%s: -key value must be > 0.\n", cmdname);
    exit(-1);
  }
  if (insert_mod <= 0) {
    fprintf(stderr, "%s: -insertmod value must be > 0.\n", cmdname);
    exit(-1);
  }
  if (verbose < VERBOSE_FIRST || verbose > VERBOSE_LAST) {
    fprintf(stderr, "%s: -v value must be in range [%d .. %d].\n",
            cmdname, VERBOSE_FIRST, VERBOSE_LAST);
    exit(-1);
  }
  if (isolevel < 0 || isolevel > 1) {
    if (isolevel == -1) {
      /* initialized to this to mean no change */
    } else {
      fprintf(stderr, "%s: -iso value must be in range [0..1].\n", cmdname);
      exit(-1);
    }
  }
#if defined(WIN32)
  if (procId < 0) {
    fprintf(stderr, "%s: -procid value must be >= 0.\n", cmdname);
    exit(-1);
  }
#endif

  if ((reads + inserts + deletes) > 100)
  {
    fprintf(stderr, "insert + read + delete should not exceed 100. \n");
    fprintf(stderr, "For more information, run \"%s -help\"\n",
            cmdname);
    return 0;
  }

  if (num_processes != 1)
    insert_mod = num_processes;

  if (num_processes > 1)
    throttle /= num_processes;
  
  if ((insert_mod * (num_xacts / 100 * (float)inserts)) >
      (key_cnt * key_cnt))
  {
    fprintf(stderr, "Inserts as part of transaction mix exceed\n"
                     "number initially populated into data store.\n");
    fprintf(stderr, "For more information, run \"%s -help\"\n",
            cmdname);
    return 0;
  }
  if ((insert_mod * (num_xacts / 100 * (float)deletes)) >
      (key_cnt * key_cnt))
  {
    fprintf(stderr, "Deletes as part of transaction mix exceed\n"
                     "number initially populated into data store.\n");
    fprintf(stderr, "For more information, run \"%s -help\"\n",
            cmdname);
    return 0;
  }

  /* check for connection string or DSN */
  if (dsn[0] && connstr_opt[0]) {

    /* Got both a connection string and a DSN. */
    fprintf(stderr, "%s: Both DSN and connection string were given.\n",
            cmdname);
    exit(-1);

  } else if (dsn[0]) {

    /* Got a DSN, turn it into a connection string. */
    if (strlen(dsn)+6 >= sizeof(connstr_opt)) {
      fprintf(stderr, "%s: DSN '%s' too long.\n", cmdname, dsn);
      exit(-1);
    }

    /* does the DSN have a UID */
    if ( getUid( dsn, username) ) {

      /* Only check for the password if the UID exists */
      if (pwdExists(dsn) ) {        

        /* use the password in the DSN as is */
        sprintf(connstr_opt, "DSN=%s", dsn);

        /* What will be displayed and on the CmdLine */
        sprintf(connstr_no_password, "DSN=%s", dsn);
      }
      else {

        /* Get the password once in the parent process */
        if (procId == 0) {
        
          /* get the password and store it in password */
          getPassword(passwordPrompt, username, (char *) password, sizeof(password) );          

          /* Add the password to the DSN */
          sprintf(connstr_opt, "DSN=%s;pwd=%s", dsn, password);

          /* now erase the password */
          erasePassword(password, strlen(password));
        }

        /* What will be displayed and on the CmdLine */
        sprintf(connstr_no_password, "DSN=%s", dsn);
      }
    } 
    else {

      /* use the DSN as is */
      sprintf(connstr_opt, "DSN=%s", dsn);

      /* What will be displayed and on the CmdLine */
      sprintf(connstr_no_password, "%s", dsn);
    }

  } else if (connstr_opt[0]) {

    /* Got a connection string. */
    if (strlen(connstr_opt)+2 >= sizeof(connstr_opt)) {
      fprintf(stderr, "%s: connection string '%s' too long.\n",
              cmdname, connstr_opt);
      app_exit(-1);
    }

    /* does the Connect String have a UID */
    if (getUid( connstr_opt, username) ) {

      /* Only check for the password if the UID exists */
      if (pwdExists(connstr_opt) ) {

        /* use the connstr string as is */
        sprintf(connstr_no_password, "%s", connstr_opt);

      }
      else {

        /* Get the password once in the parent process */
        if (procId == 0) {

          getPassword(passwordPrompt, username, (char *) password, sizeof(password) );

          /* Use the DSN as a temp string */
          strcpy(dsn, connstr_opt);

          /* Add the password to the Connect String */
          sprintf(connstr_opt, "%s;pwd=%s", dsn, password);

          /* now erase the password */
          erasePassword(password, strlen(password));
        }

        /* What will be displayed and on the CmdLine */
        sprintf(connstr_no_password, "%s", dsn);
      }
    } 
    else {
      /* use the DSN as is */

      /* What will be displayed and on the CmdLine */
      sprintf(connstr_no_password, "%s", dsn);
    }

  } else {
    /* No connection string or DSN given, use the default. */
    if (primary)
      sprintf (dsn, "%sRepSrc", DSNNAME);
    else if (standby)
      sprintf (dsn, "%sRepDst", DSNNAME);
    else {

      /* Get the password once in the parent process */
      if (procId == 0) {

        getPassword(passwordPrompt, UIDNAME, (char *) password, sizeof(password) );

        /* Use the DSN as a temp string */
        strcpy(dsn, connstr_opt);

        /* Add the UID and password to the DSN */
        sprintf(dsn, "%s;%s;pwd=%s", DSNNAME, UID, password);

        /* now erase the password */
        erasePassword(password, strlen(password));
      }
    }

    if (strlen(dsn)+6 >= sizeof(connstr_opt)) {
      fprintf(stderr, "%s: DSN '%s' too long.\n", cmdname, dsn);
      exit(-1);
    }
    sprintf(connstr_opt, "DSN=%s", dsn);

    /* What will be displayed and on the CmdLine */
    sprintf(connstr_no_password, "DSN=%s;%s", DSNNAME, UID);
  }

  return 1;
}

/* Find the UID in a DSN or Connect String */
/* assign the username to the INOUT uid param */
int getUid(const char * dsnOrConnstr, char * uid)
{
  int  lenDsnOrConnstr = 0;
  int i = 0;
  const char * uideq = "UID=";
  const char * semicolon = ";";
  int lenUideq = strlen(uideq);
  int startUIDeq = 0;
  int foundUID = 0;
  int done = 0;
  int lenUid = 0;

  lenDsnOrConnstr = strlen(dsnOrConnstr);
  for (i = 0; i < lenDsnOrConnstr; i++)
  {
      if (!strncasecmp( &dsnOrConnstr[i], uideq, lenUideq) ) {
        startUIDeq = i;
        foundUID = 1;
        break;
      }
  }

  /* Was there a UID in the DSN or Connect String */
  if (!foundUID) {
     foundUID = 0;
     return foundUID;

  } else {  /* now get the username length */

      for (i = startUIDeq + lenUideq; i < lenDsnOrConnstr; i++) {

        /* Is the UID=? embedded within the connect string */
        if (!strncasecmp( &dsnOrConnstr[i], semicolon, 1) ) {

          /* The username length must be >= 1 */
          if (i > startUIDeq + lenUideq) {
            done = 1;
            break;
          } 
          else {

            /* fail */
            foundUID = 0;
            return foundUID;
          }           
        }  /* if */

      }  /* for */

      /* Was the UID=? at the end of the connect string */
      if (!done) {

         /* The username length must be >= 1 */
         if (lenDsnOrConnstr <= (startUIDeq + lenUideq) ) {

            /* fail */
            foundUID = 0;
            return foundUID;
         }
      }

      /* The username length */
      lenUid = i - (startUIDeq + lenUideq);

      /* Assign the username to the OUT param */
      strncpy(uid, &dsnOrConnstr[startUIDeq + lenUideq], lenUid);
  }  /* foundUID */

  return foundUID;
}


/* Find if a PWD exists in the DSN or Connect String */
int pwdExists(const char * dsnOrConnstr)
{
  int  lenDsnOrConnstr = 0;
  int i = 0;
  const char * pwdEq = "PWD=";
  const char * semicolon = ";";
  int lenPwdEq = strlen(pwdEq);
  int startPWDeq = 0;
  int foundPWD = 0;
  int done = 0;

  lenDsnOrConnstr = strlen(dsnOrConnstr);
  for (i = 0; i < lenDsnOrConnstr; i++)
  {
      if (!strncasecmp( &dsnOrConnstr[i], pwdEq, lenPwdEq) ) {
        startPWDeq = i;
        foundPWD = 1;
        break;
      }
  }

  /* Was there a PWD in the DSN or Connect String */
  if (!foundPWD) {
     foundPWD = 0;
     return foundPWD;

  } else {  /* now get the password length */

      for (i = startPWDeq + lenPwdEq; i < lenDsnOrConnstr; i++) {

        /* Is the PWD=? embedded within the connect string */
        if (!strncasecmp( &dsnOrConnstr[i], semicolon, 1) ) {       

          /* The password name length must be >= 1 */
          if (i > startPWDeq + lenPwdEq) {
            done = 1;
            break;
          } 
          else {

            /* fail */
            foundPWD = 0;
            return foundPWD;
          }           
        }  /* if */

      }  /* for */

      /* Was the PWD=? at the end of the connect string */
      if (!done) {

         /* The password length must be >= 1 */
         if (lenDsnOrConnstr == (startPWDeq + lenPwdEq) ) {

            /* fail */
            foundPWD = 0;
            return foundPWD;
         }
      }

  }  /* foundPWD */

  return foundPWD;
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

void populate (char*  name,
               int    overwrite)
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
  }
}




#if defined(WIN32)
void OpenShmSeg (int  shmSize)
{
    int  i;
    int  j;

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

      shmhdr = MapViewOfFile (shmHndl, FILE_MAP_WRITE, 0, 0, 
                                     shmSize);
      if (shmhdr == NULL)
      {
        err_msg1("could not map shm segment\nError number: %d\n", GetLastError());
        exit (1);
      }

      /* initialize shared memory segment */
      /*    Used for array of PIDs and DSN to pass to child processes */
      for (i = 0; i < num_processes + strlen(connstr_real) + 4; i++) {
        shmhdr [i] = 0; 
      }

      /* Set the number of processes for the children to get the correct offsets */
      shmhdr[0] = num_processes;

      /* How long is the "real" Connect String */
      shmhdr[num_processes + 1] = strlen(connstr_real);

      j = 0;
      /* Assign the connect string for the children to read */    
      for (i = num_processes + 2; i <= num_processes + strlen(connstr_real) + 4; i++) {
        shmhdr [i] = connstr_real[j]; 
        j++;
      }

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

      shmhdr = MapViewOfFile (shmHndl, FILE_MAP_WRITE, 
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

    shmhdr = (int*) shmat (shmid, 0, 0);
    if ((tt_ptrint) shmhdr == -1)
    {
      err_msg1("could not attach to shm segment, errno = %d\n", errno);
      exit (1);
    }

    /* initialize shared memory segment */
    /*    Used for array of PIDs and DSN to pass to child processes */
    for (i = 0; i < num_processes + strlen(connstr_real) + 4; i++) {
      shmhdr [i] = 0; 
    }

    /* Set the number of processes for the children to get the correct offsets */
    shmhdr[0] = num_processes;

    /* remove access to shared memory segment */
    shmctl (shmid, IPC_RMID,NULL);
    /* remove the key file */
    unlink(keypath);
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

      if (isolevel >= 0)
        sprintf (cmdLine, 
                 "%s -connstr \"%s\" -proc 1 -xact %d -seed %d -procid %d "
                 "-key %d -read %d -insert %d -insertmod %d -iso %d",
                 progName, connstr_no_password, num_xacts, rand_seed + 1,
                 i, key_cnt, reads, inserts, insert_mod, isolevel);
      else
        sprintf (cmdLine, 
                 "%s -connStr \"%s\" -proc 1 -xact %d -seed %d -procid %d "
                 "-key %d -read %d -insert %d -insertmod %d",
                 progName, connstr_no_password, num_xacts, rand_seed + 1,
                 i, key_cnt, reads, inserts, insert_mod);


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
  switch (parse_args(argc, argv))
  {
    case 1:
       break;

    case 2:
      retval = 0;
      /* Fall through */
    default:
      goto leaveMain;
  }

  if (StopRequested()) {
    err_msg1("Signal %d received. Stopping\n", SigReceived());
    goto leaveMain;
  }

  /* the parent/main process must prepare datastore for the benchmark */
  if (procId == 0) {

    sprintf(connstr_real, "%s", connstr_opt);

    populate (dsn, !noBuild); 

  } else {

    strcpy(connstr_real, connstr_opt);
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

  pidArr = malloc(sizeof(tt_ptrint) * (num_processes + strlen(password) + 4));
  if (pidArr == NULL)
  {
    fprintf(stderr, "Failed to allocate memory for pidArr\n");
    exit(-1);
  }

  if ((num_processes > 1) || (procId > 0))
    OpenShmSeg (sizeof(int) * (num_processes + strlen(password) + 4));

  CreateChildProcs (argv[0]);

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


  srand(seed);

  /* initialize the select statement buffers */
  directory [10]  = '\0';
  last      [10]  = '\0';
  descr     [100] = '\0';



  /* Connect to data store */
  if (procId == 0)
    hdbc = ghdbc;
  else
  {
    /* Get the num_processes from SHM to enable the offsets to work */
    the_num_processes = shmhdr[0];


#ifdef WIN32  /* Only needed on Windoze */
    /* Get the length of the connect string from SHM */
    connStrLen = shmhdr[the_num_processes + 1];

    k = 0;

    /* Get the connect string from the correct SHM offsets */
    for (j = the_num_processes + 2; j <= the_num_processes + connStrLen + 1; j++) {
      connstr_real[k] = shmhdr[j];
      k++;
    }
#endif

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
    

    printf("In ExecuteTptBm (Child process = %d), waiting to connect\n", procId);

    rc = SQLDriverConnect (hdbc, NULL, (SQLCHAR*) connstr_real, SQL_NTS,
                           NULL, 0, NULL, SQL_DRIVER_NOPROMPT);

    /* Now erase the connect string to get rid of the password */
    erasePassword(connstr_real, strlen(connstr_real));
    erasePassword(connstr_opt, strlen(connstr_opt));

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
    if (num_processes > 1) {
      /* wait for the children to finish initialization */
      
      tptbm_msg1("\nWaiting for %d processes to initialize\n",
                 num_processes);
      tt_sleep (10);
    }

    if (!standby) {
      out_msg6("\nBeginning execution with %d process%s: "
                 "%d%% read, %d%% update, %d%% insert, %d%% delete\n",
                 num_processes, num_processes > 1 ? "es" : "",
                 reads,
                 100 - (reads + inserts + deletes),
                 inserts, deletes);
    }
    else {
      tptbm_msg0("Beginning execution in standby mode.\n");
    }
  }

  /* For replication demo, otherwise not used
     In case of the primary replication datastore wait for
     the standby to be ready 
  */
  if (primary)
  {
    id = 0;
    nb = 0;

    /* wait for replication begin */
    while (!replication_begin)
    {
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

      rc = SQLTransact (henv, hdbc, SQL_COMMIT);
      handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction",
                     __FILE__, __LINE__);

      /* has the standby updated the record */
      if (strncmp ((char*)last, "begin", 5) == 0)
      {
        /* received the begin transmission record */
        replication_begin = 1;
      }
      else 
      {
        tt_sleep (1);
      }
    }
  }

  time(&interval_start);
  ttGetTime (&main_start);

  /* the following is only for multi-process */
  if ((num_processes > 1) || (procId > 0))
  {
    /* parent sets  the entire array to 1 */
    if (procId == 0)
    {
      for (i = 1; i < num_processes + 1; i++)
        shmhdr [i] = 1;      
    }

    /* the child polls for a value of 1 */
    while (shmhdr [procId] == 0)
      tt_sleep (1);
  }

  insert_start = key_cnt + procId;   /* start mark for this process */
  insert_present = 0;
  delete_start = procId;

  /* execute num_xacts transactions except in replication standby datastore */
  if (!standby)
  {
    for (i = 0; i < num_xacts; i++)
    {
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
  }
  else  /* Used only for replication demo */
  {
    /* Standby datastore. Indicate replication may begin  */
    id = 0;
    nb = 0;
    sprintf ((char*) last, "begin"); /* replication demo may begin */

    /* execute and commit */
    rc = SQLExecute (upstmt);
    handle_errors (hdbc, upstmt, rc, ABORT_DISCONNECT_EXIT,
                   "executing update",
                   __FILE__, __LINE__);


    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction",
                   __FILE__, __LINE__);

    /* wait for transmission to end from primary */
    while (!replication_ended)
    {
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

      rc = SQLTransact (henv, hdbc, SQL_COMMIT);
      handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction",
                     __FILE__, __LINE__);

      if (strncmp ((char*) last, "check", 5) == 0)
      {
        /* received the end of transmission record */
        replication_ended = 1; 
      }
      else
      {
        tt_sleep (1);
      }
    }

    /* Standby datastore. Insert replication end record */
    id = 0;
    nb = 0;
    sprintf ((char*) last, "end"); /* replication demo ended */

    /* execute and commit */
    rc = SQLExecute (upstmt);
    handle_errors (hdbc, upstmt, rc, ABORT_DISCONNECT_EXIT,
                   "executing update",
                   __FILE__, __LINE__);


    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction",
                   __FILE__, __LINE__);
  }


  /* the following is only for multi-process */
  if ((num_processes > 1) || (procId > 0))
  {
    /* each process indicates completion of run */
    shmhdr [procId] = 0;

    if (procId == 0)   /* parent process */
    {
      for (i = 1; i < num_processes; i++)
      {
        /* code to check if any children are dead */
#if defined(WIN32)
        DWORD exitcode;

        while ((GetExitCodeProcess ((HANDLE) pidArr [i], &exitcode) 
             && (exitcode == STILL_ACTIVE))
           && (shmhdr [i] == 1))
#else
        while ((waitpid (pidArr [i], NULL, WNOHANG) != pidArr [i])
           && (shmhdr [i] == 1))
#endif
        {
          /* main/parent  process waits for all sub-processes to complete */
            tt_sleep (1);
        }
      }
    }
  }

  /* Primary datastore. Insert replication end record */
  if ((primary) && (procId == 0))
  {
    id = 0;
    nb = 0;
    sprintf ((char*) last, "check"); /* replication demo ended */

    /* execute and commit */
    rc = SQLExecute (upstmt);
    handle_errors (hdbc, upstmt, rc, ABORT_DISCONNECT_EXIT,
                   "executing update",
                   __FILE__, __LINE__);

    rc = SQLTransact (henv, hdbc, SQL_COMMIT);
    handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                   "committing transaction",
                   __FILE__, __LINE__);

    while (!replication_ended)
    {
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

      rc = SQLTransact (henv, hdbc, SQL_COMMIT);
      handle_errors (hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                     "committing transaction",
                     __FILE__, __LINE__);

      if (strncmp ((char*) last, "end", 3) == 0)
        replication_ended = 1; /* received the end record */
      else
        tt_sleep (1);
    }
  }


  /* time at the end of the run */
  ttGetTime (&main_end);

  /* compute transactions per second, handling zero time difference case */

  time_diff = diff_time (&main_start, &main_end);
  if (time_diff == 0)
    time_diff = 1000; /* 1 second minimum difference */

  
  tps = num_processes * ((double) num_xacts / time_diff) * 1000;
  if (opsperxact > 1) {
    tps /= (double)opsperxact;
  }

  /* print this message only in the main/parent process */
  if (procId == 0 && verbose)
  {
    if (!standby) {
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
    else {
      out_msg0("Standby complete. All updates received.\n");
    }
  }

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
  if ((num_processes > 1) || (procId > 0))
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



/*********************************************************************
 *
 *  FUNCTION:       getinikey
 *
 *  DESCRIPTION:    Retrieve the string value of a key from connStr.
 *
 *  PARAMETERS:     char* connStr   input connStr to search
                    char *sought    Parameter whose value required.
 *                  char *result    The value of the parameter.
 *
 *  RETURNS:        int             0  success, -1 failure.
 *
 *  NOTES:          NONE
 *
 *********************************************************************/
int getinikey (char*        connStr,
               const char*  sought,
               char*        result)
{
  const char* cp;
  const char* cp2;
  int         kl;

  kl = strlen (sought);
  cp = connStr;
  while (*cp)
  {
    /* Starting on an attribute name, read through until we hit an = or ; */
    cp2 = cp;

    while (*cp2)
    {
      if (*cp2 == '=' || *cp2 == ';')
        break;

      cp2++;
    }

    /* If hit end of string, funny attribute with no value attached,
     * so we can't use it.  Give up. */
    if (*cp2 == 0)
      break;

    /* If we ended up finding nothing, just advance cp by one and try
     * again.  Could happen due to adjacent ;'s or ='s in string */
    if (cp2 == cp)
    {
      cp++;
      continue;
    }

    /* Compare keyword (case insensitive) to the one we're looking for */
    if ((kl == cp2 - cp) && strncasecmp (sought, cp, cp2 - cp) == 0)
    {
      /* Extract the desired data store name */
      cp = cp2 + 1;
      cp2 = cp;

      while (*cp2 && *cp2 != ';')
        cp2++;

      memcpy (result, cp, cp2 - cp);
      result [cp2 - cp] = 0;
      return 0;
    }

    /* No match.  Advance cp to beginning of next possible keyword */
    cp = cp2 + 1;

    while (*cp && *cp != ';')
      cp++;

    if (*cp != 0)
      cp++;
  }

  /* Never found what we wanted, return failure */
  return -1;
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

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
