/*
* Copyright (c) 1999, 2025, Oracle and/or its affiliates.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*

  NAME
    TptbmODPI.c - An ODPI-C implementation of the TimesTen TPTBM workload

  DESCRIPTION
    This program implements the Transaction Processing Throughput BenchMark
    using ODPI-C.


  EXPORT FUNCTION(S)
    None

  INTERNAL FUNCTION(S)
    <other external functions defined - one-line descriptions>

  STATIC FUNCTION(S)
    <static functions defined - one-line descriptions>

  NOTES
    <other useful comments, qualifications, etc.>

*/


/*---------------------------------------------------------------------------
                    PRIVATE TYPES AND CONSTANTS
 ---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
                    STATIC FUNCTION DECLARATIONS
 ---------------------------------------------------------------------------*/
static int ttRandom(void);


#ifdef _WIN32
#include <windows.h>
#include <sys/timeb.h>
#include <time.h>
#include <process.h>
#include <winbase.h>
#define sleep(x) Sleep(1000*(x))

#else

#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#endif

#include <dpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define NO_EXIT            0
#define DISCONNECT_EXIT   -2
#define JUST_EXIT         -5

/* definitions for the DSNs */
#define CONNSTR           "localhost/sampledb:timesten_direct"
#define UIDNAME           "appuser"

#define MAX_USERNAME_SIZE 31
#define MAX_PASSWORD_SIZE 31
#define CONN_STR_LEN      256

#define MAXRAND   32767
#define MAXRANDP1 32768

void dpiPrintError(int lineno);
void dpiHandleError(dpiConn *conn, int abort, int lineno);
void usage(char *prog);
void parse_args(int argc,char **argv);
void OpenShmSeg (int shmSize);
void CreateChildProcs (char* progName);
void populate();
void ExecuteTptBm (int seed, int procId);

char username[ MAX_USERNAME_SIZE ];
char password[ MAX_PASSWORD_SIZE ];
char connstr [ CONN_STR_LEN ];


/*
 *  * Platform specific data types for integers that scale
 *  * with pointer size
*/
#ifdef _WIN32
typedef signed   __int64 sb_intp;
#else
typedef signed   long    sb_intp;
#endif

int         seed = 1;                  /* seed for the random numbers */
int         num_processes = 1;         /* number of concurrent processes
                                        * for the test */
int         num_xacts = 10000;         /* number of transactions per process */
int         opsperxact = 1;            /* operations per transaction */
int         shmpos = 1;                /* position of the process in the
                                        * shared memory array */
int         dummy = 0;                 /* dummy variable used in the NT case */
int         key_cnt = 100;             /* number of keys (squared) populated */
int         reads   = 0;               /* if 100% reads then no commit is executed */
int         inserts = 0;
int         deletes = 0;
int         updates = 0;
int         insert_mod = 1;
int         hash = 1;
int         procId  = 0;               /* process # in shared memory array */
sb_intp    *pidArr  = NULL;            /* array of pids for the parent process */

int        *shmhdr = NULL;             /* Shared memory segment */
                                       /* Children use shmhdr for PIDs and password */
                                       /* How shmhdr is laid out in memory : */
          /* shmhdr[0] = num_processes, needed in the child processes */
          /* shmhdr[1] = pid 1 */
          /* shmhdr[2] = pid 2 */
          /* ... */
          /* shmhdr[num_processes] = pid num_processes */
          /* shmhdr[num_processes + 1] = Length of password */
          /* shmhdr[num_processes + 2] = password[0] */
          /* shmhdr[num_processes + 3] = password[1] */
          /* ... */
          /* shmhdr[num_processes + lenPassword] = password[lenPassword - 1] */
          /* The password MUST be in a single byte characterset */

int         shmSize = 0;
int         shmid = 0;                 /* shared memory segment id */
char        keypath[] = "/tmp/tptbmXXXXXX"; /* tmp file used for shmget() key generation */

int         think_time = 0;            /* average think time for a transaction*/
int         connstrlen = 0;
int         usernamelen = 0;
int         pwdlen = 0;
int         build_only = 0;
int         no_build = 0;
int         multiop = 0;               /* 1 insert, 3 reads, 1 update / xact */
int         isTimesTen = 1;            /* is TimesTen or Oracle DB */

dpiContext  *gContext = NULL;

#ifndef _WIN32
#define ttGetTime(p)    gettimeofday(p, NULL)

sb_intp diff_time(struct timeval * start,struct timeval *end)
{
  return ((end->tv_sec - start->tv_sec)*1000 +
          (end->tv_usec - start->tv_usec)/1000);
}

#else
HANDLE han; /* handle to shared memory segment */

#define ttGetTime(p)    ftime(p)

sb_intp diff_time(struct timeb * start,struct timeb *end)
{
  return ((end->time - start->time)*1000 +
          (end->millitm - start->millitm));
}
#endif /* _WIN32 */


/*
 *  Prints the last ODPI error
 */
void dpiPrintError(int lineno)
{
    dpiErrorInfo info;
    dpiContext_getError(gContext, &info);

    printf("ERROR %s:%d - %s\n", info.fnName, lineno, info.message);
}


/*
 *  Handles errors and warnings for the last dpi function called
 */
void dpiHandleError(dpiConn *conn, int abort, int lineno)
{
  dpiPrintError(lineno);

  if (abort == NO_EXIT) {
    dpiPrintError(__LINE__);
  }
  else if (abort == DISCONNECT_EXIT) {
    if (dpiConn_release(conn) == DPI_FAILURE) {
      dpiPrintError(__LINE__);
    }
    exit(1);
  }
  else if (abort == JUST_EXIT) {
    dpiPrintError(__LINE__);
    exit(1);
  }
}

/* SQL statements used in this program */

/* The select statement used for the read transaction */

char * select_stmnt =
"select directory_nb,last_calling_party,descr from vpn_users \
where vpn_id = :1 and vpn_nb = :2";

/* The update statement used for the write transaction */

char * update_stmnt =
"update vpn_users set last_calling_party = :1 \
where vpn_id = :2 and vpn_nb = :3";


/* The destroy table statement */

char * drop_stmnt = "DROP TABLE vpn_users";

/* The create table statement */

char * creat_stmnt = "CREATE TABLE vpn_users(\
vpn_id             TT_INTEGER   NOT NULL,\
vpn_nb             TT_INTEGER   NOT NULL,\
directory_nb       CHAR(20)  NOT NULL,\
last_calling_party CHAR(20)  NOT NULL,\
descr              CHAR(100) NOT NULL,\
                   PRIMARY KEY (vpn_id,vpn_nb))";

char * creat_stmnt_hash = "CREATE TABLE vpn_users(\
vpn_id             TT_INTEGER   NOT NULL,\
vpn_nb             TT_INTEGER   NOT NULL,\
directory_nb       CHAR(20)  NOT NULL,\
last_calling_party CHAR(20)  NOT NULL,\
descr              CHAR(100) NOT NULL,\
                   PRIMARY KEY (vpn_id,vpn_nb))\
UNIQUE HASH ON (vpn_id,vpn_nb) PAGES = %d";

char * creat_stmnt_rdbms = "CREATE TABLE vpn_users(\
vpn_id             INT       NOT NULL,\
vpn_nb             INT       NOT NULL,\
directory_nb       CHAR(20)  NOT NULL,\
last_calling_party CHAR(20)  NOT NULL,\
descr              CHAR(100) NOT NULL,\
                   PRIMARY KEY (vpn_id,vpn_nb))";

/* The insert statement used to populate the datastore */

char * insert_stmnt = "insert into vpn_users values (:1,:2,:3,:4,:5)";

char * delete_stmnt = "delete from vpn_users where vpn_id = :1 and vpn_nb = :2";

char * stat_stmnt = "CALL ttOptUpdateStats('vpn_users')";

/*********************************************************************
 *
 *  FUNCTION:       usage
 *
 *  DESCRIPTION:    This function prints a usage message describing
 *                  the command line options of the program.
 *
 *  PARAMETERS:     char* prog    full program path name
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
usage(char *prog)
{
  /* Get the name of the program (sans path). */

  /* Print the usage message */

  printf( "\nUsage:\n"
          "  %s [-h] [-help] \n"
          "  %s -pwd <password> [-user <username>] [-connstr <connstr>]  \n"
          "             [-build] [-key <key>] [-range]"
          "  %s -pwd <password> [-user <username>] [-connstr <connstr>]\n"
          "             [-read <reads>] [-insert <ins>] [-update <ups>] [-delete <dels>]\n"
          "             [-xact <xacts>] [-ops <ops_per_txn>]\n"
          "             [-proc <nprocs>] [-key <key>] [-range] [-thinkTime <sleep_in_ms>]\n"
          "             [-seed <seed>] [-nobuild] [-multiop]\n\n"
          "  -h                     Prints this message and exits.\n"
          "  -help                  Same as -h.\n"
          "  -user      <username>  Specify DB username.\n"
          "                         Default username is %s\n"
          "  -pwd       <password>  Specify DB password.\n"
          "  -connstr   <connstr>   The connect string identifying the database to which\n"
          "                         a connection is to be established.\n"
          "                         The connection string can point to Direct Linked or\n"
          "                         Client/Server TimesTen DSNs.\n"
          "                         Default connection string is %s\n"
          "  -build                 Create and load VPN_USERS table and then exit.\n"
          "  -nobuild               Skip creating and loading table VPN_USERS,\n"
          "                         but run the workload.\n"
          "  -range                 Build a range index.\n"
          "                         Default is hash index.\n"
          "  -key       <key>       Specifies the number of records (squared) to initially\n"
          "                         populate in the VPN_USERS table.\n"
          "                         Default is 100^2 or 10,000 rows\n"
          "  -xact      <xacts>     Specifies that <xacts> is the number of transactions\n"
          "                         that each process should run. The default is 10000.\n"
          "  -ops       <ops>       Operations per transaction.  The default is 1.\n"
          "  -read      <reads>     Specifies that <nreads> is the percentage of read-only\n"
          "                         transactions. The default is 80.\n"
          "                         In the special case where 100 is specified, no commit\n"
          "                         is done. This may be useful for read-only testing.\n"

          "  -insert    <ins>       Specifies that <nins> is the percentage of insert\n"
          "                         transactions. The default is 0.\n"
          "  -update    <ups>       Specifies that <ups> is the percentage of update\n"
          "                         transactions. The default is 20.\n"
          "  -delete    <del>       Specifies that <ndel> is the percentage of delete\n"
          "                         transactions. The default is 0.\n"
          "  -multiop               SQL workload with 1 insert, 3 reads, 1 update per txn.\n"
          "                         When using multiop, the -ops parameters mean\n"
          "                         that many iterations of the multiop workload..\n"
          "  -thinkTime <ms>        Specifies the 'think time' in milli-seconds per txn.\n"
          "  -proc      <nprocs>    Specifies that <nprocs> is the number of concurrent\n"
          "                         processes. The default is 1.\n"
          "  -seed      <seed>      Specifies that <seed> should be the seed for the\n"
          "                         random number generator.\n",
                  prog, prog, prog, UIDNAME, CONNSTR
                  );

  printf("\nExample :\n");
  printf("  70%% reads, 30%% updates for appuser@sampledb with 100K txns\n\n");
  printf("  tptbmODPI -read 70 -update 30 -connstr localhost/sampledb:timesten_direct -user appuser -pwd appuser -xact 100000\n\n");

  exit(1);
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
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
parse_args(int argc,char **argv)
{

  int i = 1;

  /* Initialize the connection string */
  memset(connstr, 0, sizeof(connstr));

  /* Use the default Service Name, can override from command-line */
  strcpy(connstr, CONNSTR);
  connstrlen = strlen(connstr);

  /* Use the default userame, can override from command-line */
  strcpy(username, UIDNAME);
  usernamelen = strlen(username);

  while(i < argc) {
    if ( !strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") ) {
      usage(argv[0]);
      exit(1);
    }
    else if (strcmp("-proc",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &num_processes) == -1 ||
          num_processes <= 0) {
        printf("-proc flag requires a positive integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-build",argv[i]) == 0) {
      build_only = 1;
      i += 1;
    }
    else if (strcmp("-nobuild",argv[i]) == 0) {
      no_build = 1;
      i += 1;
    }
    else if (strcmp("-range",argv[i]) == 0) {
      hash = 0;
      i += 1;
    }
    else if (strcmp("-multiop",argv[i]) == 0) {
      multiop = 1;
      reads   = 60;
      inserts = 20;
      updates = 20;
      i += 1;
    }
    else if (strcmp("-ops",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &opsperxact) == -1 ||
          opsperxact <= 0) {
        printf("-ops flag requires a positive integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if ( (strcmp("-thinktime",argv[i]) == 0) || (strcmp("-thinkTime",argv[i]) == 0) ) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &think_time) == -1 ||
          think_time < 0) {
        printf("-thinkTime flag requires a non-negative integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-xact",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &num_xacts) == -1 ||
          num_xacts <= 0) {
        printf("-xact flag requires a positive integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-seed",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &seed) == -1 ||
          seed < 0) {
        printf("-seed flag requires a non-negative integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-read",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &reads) == -1 ||
          (reads < 0) || (reads > 100)) {
        printf("-read flag requires an integer between 0 and 100\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-insert",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &inserts) == -1 ||
          (inserts < 0) || (inserts > 100)) {
        printf("-insert flag requires an integer between 0 and 100\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-update",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &updates) == -1 ||
          (updates < 0) || (updates > 100)) {
        printf("-update flag requires an integer between 0 and 100\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-delete",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &deletes) == -1 ||
          (deletes < 0) || (deletes > 100)) {
        printf("-delete flag requires an integer between 0 and 100\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-key",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &key_cnt) == -1 ||
          key_cnt <= 0) {
        printf("-key flag requires a positive integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-connstr",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      connstrlen = strlen (argv[i+1]);

      if (connstrlen >= CONN_STR_LEN - 1) {
        printf("\nThe Oracle Net Servicename [%d characters] was too long\n", connstrlen);
        usage(argv[0]);
        exit(1);

      } else {

        /* Initialize the connection string */
        memset(connstr, 0, sizeof(connstr));

        /* Get the Oracle Net Service Name */
        strcpy(connstr, argv[i+1]);

      }
      i += 2;
    }
    else if (strcmp("-user",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      usernamelen = strlen (argv[i+1]);

      if (usernamelen >= MAX_USERNAME_SIZE - 1) {
        printf("\nThe username [%d characters] was too long\n", usernamelen);
        usage(argv[0]);
        exit(1);

      } else {

        /* Initialize the username */
        memset(username, 0, sizeof(username));

        /* Get the username */
        strcpy(username, argv[i+1]);

      }
      i += 2;
    }
    else if (strcmp("-pwd",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      pwdlen = strlen (argv[i+1]);

      if (pwdlen >= MAX_PASSWORD_SIZE - 1) {
        printf("\nThe password [%d characters] was too long\n", pwdlen);
        usage(argv[0]);
        exit(1);

      } else {

        /* Initialize the password */
        memset(password, 0, sizeof(password));

        /* Get the password */
        strcpy(password, argv[i+1]);

      }
      i += 2;
    }
    else if (strcmp("-insertmod",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &insert_mod) == -1 ||
          insert_mod <= 0) {
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }

    else if (strcmp("-dummy",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &dummy) == -1 || dummy < 0) {
        printf("-dummy flag requires a non-negative integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }

    else if (strcmp("-procId",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &procId) == -1 || dummy < 0) {
        printf("-procId flag requires a non-negative integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }

    else {
      usage(argv[0]);
    }
  }

  /* check for wrong number of arguments */
  if (i != argc) {
    usage(argv[0]);
  }

  /* check a password was provided on the parent process */
  if (strlen(password) == 0 && dummy == 0) {
    usage(argv[0]);
  }

  if ((reads + inserts + updates + deletes) == 0){
     reads   = 80;
     updates = 20;
  }

  if ((reads + inserts + deletes + updates) != 100){
        printf("\nreads + inserts + updates + deletes should equal 100%%. \n");
    usage(argv[0]);
  }

  if (num_processes != 1)
        insert_mod = num_processes;

  if ( !multiop &&
       ((insert_mod * (num_xacts/100*(float)inserts)) > (key_cnt*key_cnt)) ){

    printf("\nTrying to add more rows (%d) than the current DB size (%d rows).\n",
                     num_xacts, key_cnt*key_cnt );
    printf("For the tptbm workload, either decrease the '-xacts' parameter\nand/or increase the '-key' parameter\n\n");
    printf("For example:\n");
    printf("-xacts <= %9d  [for a -key size of %d]\n", key_cnt*key_cnt, key_cnt);
    printf("-key   >= %9d  [for %d -xacts]\n\n", 1 + ((int) num_xacts * num_xacts), num_xacts);
    exit(1);

  }

}

#if defined(_WIN32)
void OpenShmSeg (int  shmSize)
{

  int i, j, k;

  if ( (dummy == 0) && (num_processes > 1) ) {

    /* create shared memory segment */
    han = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,
                            shmSize,
                            "TimesTenMultiUserTestsOnWindowsNT");

    if (han == NULL){
      printf("could not create shm segment\n");
      exit(1);
    }

    if ((shmhdr = MapViewOfFile(han,FILE_MAP_WRITE,0,0,
                                shmSize)) == NULL) {
      printf("could not create shm segment\n");
      exit(1);
    }

    /* initialize shared memory segment */
    for (i = 0; i < num_processes + strlen(password) + 4; i++)
      shmhdr[i] = 0;

    /* Set the number of processes for the children to get the correct offsets */
    shmhdr[0] = num_processes;

    /* How long is the password */
    shmhdr[num_processes + 1] = strlen(password);

    /* Copy the password to shared memory for Windows children to read */
    j = num_processes + 2; /* password offset */
    for (k = 0; k < strlen(password); k++) {
       shmhdr[j] = password[k];
       j++;
    }

  }
  else if (dummy != 0) {

    /* connect to shared memory segment */

    han = OpenFileMapping(FILE_MAP_WRITE,TRUE,
                          "TimesTenMultiUserTestsOnWindowsNT");

    if (han == NULL){
      printf("could not open shm segment\n");
      exit(1);
    }

    if ((shmhdr = MapViewOfFile(han,FILE_MAP_WRITE,0,0,shmSize)) == NULL) {
      printf("could not open shm segment\n");
      exit(1);
    }
  }
}
#else /* UNIX */
void OpenShmSeg (int  shmSize)
{

  int i;
  int fd;

  if (num_processes > 1) {

    /* create a file for key generation */
    fd = mkstemp(keypath);

    if (fd != -1)
    {
      /* success - close the file */
      close(fd);
    }
    /* if mkstemp() failed, then ftok() will return -1 to use as the key */

    /* create shared memory segment */
    shmid = shmget(ftok(keypath,seed),
                   shmSize,
                   IPC_CREAT | 0666);

    if (shmid == -1) {
      printf("could not create shm segment\n");

      if (pidArr)
        free(pidArr);
      exit(1);
    }

    shmhdr = (int *) shmat(shmid,0,0);

    if ((sb_intp)shmhdr == -1) {
      printf("could not create shm segment\n");
      if (pidArr)
        free(pidArr);
      exit(1);
    }

    /* initialize shared memory segment */
    for (i = 0; i < num_processes + strlen(password) + 4; i++) {
      shmhdr[i] = 0;
    }

    /* Set the number of processes for the children to get the correct offsets */
    shmhdr[0] = num_processes;

    /* remove access to shared memory segment */
    shmctl (shmid, IPC_RMID,NULL);
    /* remove the key file */
    unlink(keypath);
  }

}
#endif


void CreateChildProcs (char* progName)
{

#ifdef _WIN32
  char           seed_string[10];         /* buffer used to recursively invoke
                                           * this program */
  char           insert_string[10];       /* buffer used to recursively invoke
                                           * this program */
  char           update_string[10];       /* buffer used to recursively invoke
                                           * this program */
  char           delete_string[10];       /* buffer used to recursively invoke
                                           * this program */
  char           inmod_string[10];         /* buffer used to recursively invoke
                                           * this program */
  char           xact_string[10];         /* buffer used to recursively invoke
                                           * this program */
  char           shmpos_string[10];       /* buffer used to recursively invoke
                                           * this program */
  char           key_string[10];          /* buffer used to recursively invoke
                                           * this program */
  char           read_string[10];         /* buffer used to recursively invoke
                                           * this program */
  char           cmdLine[4000];

  STARTUPINFO    sInfo = {8L, NULL, NULL, NULL,
                          0L, 0L, 0L, 0L, 0L, 0L, 0L,
                          STARTF_USESHOWWINDOW, SW_HIDE, 0L,
                          NULL, NULL, NULL, NULL};

  PROCESS_INFORMATION pInfo;
#endif

  int    i;
  int    pid;

  if (num_processes == 2)
    printf("Waiting for %d child process to synchronize\n", num_processes - 1);
  else
    printf("Waiting for %d child processes to synchronize\n", num_processes - 1);

  for(i = 1; i < num_processes; i++) {

    seed += 5;

#ifndef _WIN32

    fflush(stdout);
    fflush(stderr);
    pid = fork();
    if (pid == 0) {
      seed += 2;
      procId = i;
      break;
    }
    if (pid == -1) {
      printf("Failure spawning a process\n");
      exit(1);
    }
    pidArr[i] = pid;
    shmpos++;

#else

    /* in this case spawn more processes with a different value for dummy */
    /* the variable dummy corresponds to shmpos, the position of the process
 *        in the shared memory array - it is 0 in the parent case */

    sprintf(seed_string,"%d",seed + 1 );
    sprintf(xact_string,"%d",num_xacts );
    sprintf(shmpos_string,"%d",shmpos);
    sprintf(key_string,"%d",key_cnt);
    sprintf(read_string,"%d",reads);
    sprintf(insert_string,"%d",inserts);
    sprintf(update_string,"%d",updates);
    sprintf(delete_string,"%d",deletes);
    sprintf(inmod_string,"%d",insert_mod);

    {
      /* spawnlp has a bug that requires quoting szConnStrIn if it has embedded spaces
 *        */

      sprintf(cmdLine,
"%s -proc 1 -xact %s -seed %s -dummy %s -procId %s -key %s -read %s -insert %s -update %s -delete %s -insertmod %s -connstr %s -user %s",
                progName,
                xact_string,
                seed_string,
                shmpos_string,
                shmpos_string,
                key_string,
                read_string,
                insert_string,
                update_string,
                delete_string,
                inmod_string,
                connstr,
                username);

      if (multiop)
        strcat (cmdLine, " -multiop");

      if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &sInfo, &pInfo) == 0){
        printf("Failure spawning a process\n");
        exit(1);
      }
      pidArr[i] = (sb_intp) pInfo.hProcess;
    }

    shmpos++;
    pid = 1;

#endif
  }
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

void populate( )
{

  dpiConn    *conn;
  dpiStmt    *stmt;
  dpiData    intColValue, stringColValue;
  int        i, j;
  int        id;
  int        nb;
  char       directory[11];
  char       last_calling_party[11] = "0000000000";
  char       descr [100];
  char       buff1 [1024];
  char       *pStmt;
  int        pages = ((key_cnt * key_cnt) / 256) + 1;

  /* Initialize database connection */
  if (dpiConn_create(gContext,
      username,  usernamelen,
      password,  pwdlen,
      connstr,   connstrlen,
      NULL, NULL, &conn) == DPI_FAILURE) {
    dpiHandleError(conn, JUST_EXIT, __LINE__);
  }

  if (NULL == strstr(connstr, "timesten_direct") && NULL == strstr(connstr, "timesten_client"))
    isTimesTen = 0;

  /* drop the table if it is already created */
  if (dpiConn_prepareStmt(conn, 0, drop_stmnt, strlen(drop_stmnt), NULL, 0, &stmt) == DPI_FAILURE)
      dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  dpiStmt_execute(stmt, DPI_MODE_EXEC_DEFAULT, NULL);

  /* make create statement */
  pStmt = creat_stmnt;

  if (hash) {
    sprintf (buff1, creat_stmnt_hash, pages);
    pStmt = buff1;
  }

  if (!isTimesTen) {
    pStmt = creat_stmnt_rdbms;
  }

  /* prepare and execute the create statement */
  if (dpiConn_prepareStmt(conn, 0, pStmt, strlen(pStmt), NULL, 0, &stmt) == DPI_FAILURE)
      dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_execute(stmt, DPI_MODE_EXEC_DEFAULT, NULL) == DPI_FAILURE)
      dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  /* commit the transaction */
  dpiConn_commit(conn);

  /* prepare the insert statement */
  if (dpiConn_prepareStmt(conn, 0, insert_stmnt, strlen(insert_stmnt), NULL, 0, &stmt) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  printf("Populating benchmark data store\n");

  /* insert key_cnt**2 records */
  for (i = 0; i < key_cnt; i++)
  {
    for (j = 0; j < key_cnt; j++)
    {
      id = i;
      nb = j;
      sprintf ((char*) directory, "55%d%d", (unsigned int) (id % 10000), (unsigned int) (nb % 10000));
      sprintf ((char*) descr,
               "<place holder for description of VPN %d extension %d>",
               id, nb);

      /* bind the input parameters */
      dpiData_setInt64(&intColValue, id);
      if (dpiStmt_bindValueByPos(stmt, 1, DPI_NATIVE_TYPE_INT64, &intColValue) == DPI_FAILURE)
        dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
      dpiData_setInt64(&intColValue, nb);
      if (dpiStmt_bindValueByPos(stmt, 2, DPI_NATIVE_TYPE_INT64, &intColValue) == DPI_FAILURE)
        dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
      dpiData_setBytes(&stringColValue, directory, strlen(directory));
      if (dpiStmt_bindValueByPos(stmt, 3, DPI_NATIVE_TYPE_BYTES, &stringColValue) == DPI_FAILURE)
        dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
      dpiData_setBytes(&stringColValue, last_calling_party, strlen(last_calling_party));
      if (dpiStmt_bindValueByPos(stmt, 4, DPI_NATIVE_TYPE_BYTES, &stringColValue) == DPI_FAILURE)
        dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
      dpiData_setBytes(&stringColValue, descr, strlen(descr));
      if (dpiStmt_bindValueByPos(stmt, 5, DPI_NATIVE_TYPE_BYTES, &stringColValue) == DPI_FAILURE)
        dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

      /* execute insert statement */
      if (dpiStmt_execute(stmt, DPI_MODE_EXEC_DEFAULT, NULL) == DPI_FAILURE)
        dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
    }

    /* commit the transaction */
    if ( dpiConn_commit(conn) == DPI_FAILURE ) {
      dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
    }
  }

  /* drop and free the insert statement */
  dpiStmt_release(stmt);

  /* enable TimesTen statistics */
  if (isTimesTen) {
    if (dpiConn_prepareStmt(conn, 0, stat_stmnt, strlen(stat_stmnt), NULL, 0, &stmt) == DPI_FAILURE)
      dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

    if (dpiStmt_execute(stmt, DPI_MODE_EXEC_DEFAULT, NULL) == DPI_FAILURE)
      dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

    if (dpiConn_commit(conn) == DPI_FAILURE)
      dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

    dpiStmt_release(stmt);
  }

  /* release database connection */
  if (dpiConn_release(conn) == DPI_FAILURE)
    dpiHandleError(conn, JUST_EXIT, __LINE__);

  /* close the connection */
  if (dpiConn_close(conn, DPI_MODE_CONN_CLOSE_DEFAULT, NULL, 0) == DPI_FAILURE)
    dpiHandleError(conn, JUST_EXIT, __LINE__);
}

/*********************************************************************
 *
 *  FUNCTION:       ExecuteTptBm
 *
 *  DESCRIPTION:    Performs the tptbm benchmark (by default 80% are selects and
 *                  20% are updates) and then reports timing statistics.
 *
 *  PARAMETERS:     int seed        seed for the random number generation
 *                  int procId      process # in shared memory array
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void ExecuteTptBm (int seed,
                   int procId)
{
  dpiConn   *conn;
  dpiStmt   *selStmt, *insStmt, *updStmt, *delStmt, *pStmt;
  dpiVar    *idVar, *nbVar, *directoryVar, *lastVar, *descrVar;
  dpiData   *idData, *nbData, *directoryData, *lastData, *descrData;

  #ifdef _WIN32
  struct timeb main_start;    /* variable to measure time */
  struct timeb main_end;      /* variable to measure time */
  int          exitcode;
  #else
  struct timeval main_start;  /* variable to measure time */
  struct timeval main_end;    /* variable to measure time */
  #endif

  int       rand_int;         /* rand integer */
  int       insert_start;     /* start mark for inserts */
  int       insert_present;   /* present mark for inserts */
  int       delete_start = 0; /* start mark for deletes */
  int       delete_present=0; /* present mark for deletes */

  sb_intp   time_diff;        /* difference in time between
                               * start and finish */
  double    tps;              /* compute transactions per second */
  int       i;
  int       op_count=0;

  /* the following three buffers are for the select statement */
  char      directory [11];
  char      last [11];
  char      descr [101];

  /* the following two buffers are used for all statements */
  int       id;
  int       nb;

  int       path = 0;         /* type of transaction */

  srand (seed);

  /* initialize the select statement buffers */
  directory [10] = '\0';
  last      [10] = '\0';
  descr     [100]= '\0';

  /* Initialize database connection */
  if (dpiConn_create(gContext,
      username,  usernamelen,
      password,  pwdlen,
      connstr,   connstrlen,
      NULL, NULL, &conn) == DPI_FAILURE) {
    dpiHandleError(conn, JUST_EXIT, __LINE__);
  }

  /* prepare statements */
  if (dpiConn_prepareStmt(conn, 0, select_stmnt, strlen(select_stmnt), NULL, 0, &selStmt) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiConn_prepareStmt(conn, 0, insert_stmnt, strlen(insert_stmnt), NULL, 0, &insStmt) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiConn_prepareStmt(conn, 0, delete_stmnt, strlen(delete_stmnt), NULL, 0, &delStmt) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiConn_prepareStmt(conn, 0, update_stmnt, strlen(update_stmnt), NULL, 0, &updStmt) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  /* create dpi variables */
  if (dpiConn_newVar(conn, DPI_ORACLE_TYPE_NUMBER, DPI_NATIVE_TYPE_INT64,
                     1, 0, 0, 0, NULL, &idVar, &idData) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiConn_newVar(conn, DPI_ORACLE_TYPE_NUMBER, DPI_NATIVE_TYPE_INT64,
                     1, 0, 0, 0, NULL, &nbVar, &nbData) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiConn_newVar(conn, DPI_ORACLE_TYPE_VARCHAR, DPI_NATIVE_TYPE_BYTES,
                     1, 11, 0, 0, NULL, &directoryVar, &directoryData) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiConn_newVar(conn, DPI_ORACLE_TYPE_VARCHAR, DPI_NATIVE_TYPE_BYTES,
                     1, 11, 0, 0, NULL, &lastVar, &lastData) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiConn_newVar(conn, DPI_ORACLE_TYPE_VARCHAR, DPI_NATIVE_TYPE_BYTES,
                     1, 100, 0, 0, NULL, &descrVar, &descrData) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  /* bind select parameters */
  if (dpiStmt_bindByPos(selStmt, 1, idVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_bindByPos(selStmt, 2, nbVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  /* bind insert parameters */
  if (dpiStmt_bindByPos(insStmt, 1, idVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_bindByPos(insStmt, 2, nbVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_bindByPos(insStmt, 3, directoryVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_bindByPos(insStmt, 4, lastVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_bindByPos(insStmt, 5, descrVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  /* bind delete parameters */
  if (dpiStmt_bindByPos(delStmt, 1, idVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_bindByPos(delStmt, 2, nbVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  /* bind update parameters */
  if (dpiStmt_bindByPos(updStmt, 1, lastVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_bindByPos(updStmt, 2, idVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
  if (dpiStmt_bindByPos(updStmt, 3, nbVar) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  /* print this starting message only in the main/parent process */
  if (procId == 0)
  {
    if (num_processes > 1) {
      /* wait for the children to finish initialization */

      printf("Waiting for %d processes to initialize\n",
                 num_processes);
      sleep (10);
    }

    printf("Beginning execution with %d process%s: "
               "%d%% read, %d%% update, %d%% insert, %d%% delete\n",
               num_processes, num_processes > 1 ? "es" : "",
               reads,
               100 - (reads + inserts + deletes),
               inserts, deletes);
  }

  ttGetTime (&main_start);

  /* the following is only for multi-process */
  if ((num_processes > 1) || (procId > 0))
  {
    /* parent sets  the entire array to 1 */
    if (procId == 0)
    {
      for (i = 0; i < num_processes; i++)
        shmhdr [i] = 1;
    }

    /* the child polls for a value of 1 */
    while (shmhdr [procId] == 0)
      sleep (1);
  }

  insert_start = key_cnt + procId;   /* start mark for this process */
  insert_present = 0;
  delete_start = procId;

  /* execute num_xacts transactions */
  for (i = 0; i < num_xacts; i++)
  {
    /* calculate the number of operations per xact */
    if (multiop) {
      /* By default opsperxact = 1 */
      op_count = 5 * opsperxact;
    }
    else
      op_count = opsperxact;

    /* exeute operations */
    while (op_count > 0) {
      op_count--;

      if (think_time > 0 && op_count == 0) {
#ifdef _WIN32
        /* Sleep in milli-seconds */
        Sleep(think_time);
#else
        struct timeval waitT;

        /* Make it in milli-seconds */
        waitT.tv_sec = 0;
        waitT.tv_usec = think_time * 1000;
        select(0,0,0,0,&waitT);
#endif
      }

      if (multiop) {
        switch (op_count  % 5) {
          case 4: path = 2; break;
          case 3:
          case 2:
          case 1: path = 1; break;
          case 0: path = 0; break;
          default: printf("unknown path for multiop\n");
        }
      }
      else {
        if (reads != 100)
        {
          rand_int = ttRandom() % MAXRANDP1;
          if (rand_int < ((float)(reads + inserts +deletes) / 100) * MAXRANDP1) {
            if (rand_int < ((float)(reads + inserts)/ 100) * MAXRANDP1) {
              if (rand_int < ((float)(reads)/ 100) * MAXRANDP1) {
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
      }

      if (path == 1)                                /* select xact */
      {
        /* randomly pick argument values */
        id = (int) ((key_cnt - 1) * ((ttRandom() % MAXRANDP1) / (double)MAXRAND));
        nb = (int) ((key_cnt - 1) * ((ttRandom() % MAXRANDP1) / (double)MAXRAND));

        /* set values */
        pStmt = selStmt;
        dpiData_setInt64(idData, id);
        dpiData_setInt64(nbData, nb);
      }
      else if (path == 0)                           /* update xact */
      {
        /* randomly pick argument values */
        id = (int) ((key_cnt - 1) * ((ttRandom() % MAXRANDP1) / (double)MAXRAND));
        nb = (int) ((key_cnt - 1) * ((ttRandom() % MAXRANDP1) / (double)MAXRAND));
        sprintf(last, "%d", procId);

        /* set values */
        pStmt = updStmt;
        dpiVar_setFromBytes(lastVar, 0, last, strlen(last));
        dpiData_setInt64(idData, id);
        dpiData_setInt64(nbData, nb);
      }
      else if (path == 3)                           /* delete xact */
      {
        id = delete_start;
        nb = delete_present++;

        if (delete_present == key_cnt){
          delete_present = 0;
          delete_start += insert_mod;
        }

        /* set values */
        pStmt = delStmt;
        dpiData_setInt64(idData, id);
        dpiData_setInt64(nbData, nb);
      }
      else                                          /* insert xact */
      {
        id = insert_start;
        nb = insert_present++;
        sprintf ((char*) directory, "55%d%d", (unsigned int) (id % 10000), (unsigned int) (nb % 10000));
        sprintf ((char*) last, "0000000000");
        sprintf ((char*) descr,
               "<place holder for description of VPN %d extension %d>", id, nb);

        if (insert_present == key_cnt)
        {
          insert_present = 0;
          insert_start += insert_mod;
        }

        /* set values */
        pStmt = insStmt;
        dpiData_setInt64(idData, id);
        dpiData_setInt64(nbData, nb);
        dpiVar_setFromBytes(directoryVar, 0, directory, strlen(directory));
        dpiVar_setFromBytes(lastVar, 0, last, strlen(last));
        dpiVar_setFromBytes(descrVar, 0, descr, strlen(descr));
      }

      /* execute the statement  */
      if (dpiStmt_execute(pStmt, DPI_MODE_EXEC_DEFAULT, NULL) == DPI_FAILURE)
        dpiHandleError(conn, NO_EXIT, __LINE__);

      /* commit xact once all operations have finished */
      if (reads != 100 && op_count == 0) {
        if (dpiConn_commit(conn) == DPI_FAILURE)
          dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);
      }
    }
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
#if defined(_WIN32)
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
            sleep (1);
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


  tps = num_processes * ((double) num_xacts / time_diff) * 1000;
  if (opsperxact > 1) {
    tps /= (double)opsperxact;
  }

  /* print this message only in the main/parent process */
  if (procId == 0)
  {
    if (time_diff >= 1000) {
      printf("\nElapsed time:     %10.1f seconds \n", (double)time_diff/1000.0);
    }
    else {
      printf("\nElapsed time:     %10.1f msec \n", (double)time_diff);
    }
    if (opsperxact == 0) {
      printf("Operation rate:   %10.1f operations/second\n", tps);
    }
    else {
      printf("Transaction rate: %10.1f transactions/second\n", tps);
    }
    if (opsperxact > 1) {
      printf("Operation rate:   %10.1f operations/second (%d ops/transaction)\n",
               tps * opsperxact, opsperxact);
    }
  }

  /* commit any remaining transactions */
  if (dpiConn_commit(conn) == DPI_FAILURE)
    dpiHandleError(conn, DISCONNECT_EXIT, __LINE__);

  /* clean up */
  dpiVar_release(idVar);
  dpiVar_release(nbVar);
  dpiVar_release(directoryVar);
  dpiVar_release(lastVar);
  dpiVar_release(descrVar);
  dpiStmt_release(selStmt);
  dpiStmt_release(insStmt);
  dpiStmt_release(updStmt);
  dpiStmt_release(delStmt);

  /* release database connection */
  if (dpiConn_release(conn) == DPI_FAILURE)
    dpiHandleError(conn, JUST_EXIT, __LINE__);

  /* the following is only for multi-process */
  if ((num_processes > 1) || (procId > 0))
  {
#if defined(_WIN32)
    UnmapViewOfFile (shmhdr);
    CloseHandle (han);
#else
    shmdt ((char*) shmhdr);

#endif
  }
}

/*********************************************************************
 *
 *  FUNCTION:       main
 *
 *  DESCRIPTION:    This is the main function of the tpt benchmark.
 *                  It connects to a data source, creates and
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

  int            passwordOffset = 0;
  int            passwordLen    = 0;
  int            numProcesses   = 0;
  int            j, k           = 0;

  dpiErrorInfo   errorInfo;

  /* Clear the password */
  memset(password, 0, sizeof(password));

  /* parse the command line arguments */
  parse_args(argc,argv);

  if (dummy > 0) shmpos = dummy;     /* this happens only in the NT case */
  /* on unix dummy is always 0 */

  /* Initilize global context */
  if (dpiContext_create(DPI_MAJOR_VERSION, DPI_MINOR_VERSION, &gContext, &errorInfo) < 0)
    dpiHandleError(NULL, JUST_EXIT, __LINE__);

  /* the parent/main process must prepare VPN_USERS table for the benchmark */
  if (dummy <= 0 && !no_build) {
    populate();
  }

  if (build_only) {
    exit(0);
  }

  /* Local memory for PIDs and password */
  pidArr = malloc(sizeof(long) * (num_processes + 1));
  if (pidArr == NULL)
  {
    printf("Failed to allocate memory for pidArr\n");
    exit(-1);
  }

  /* Calculate the size of the shared memory segment */
  shmSize = sizeof(int) * (num_processes + strlen(password) + 4);

  if (num_processes > 1 || procId > 0) {
    OpenShmSeg(shmSize);
  }

  /* Don't prompt for the password in child processes on Windows */
  if (dummy != 0) {

    numProcesses   = shmhdr[ 0 ];
    passwordLen    = shmhdr[ numProcesses + 1];
    passwordOffset = numProcesses + 2;
    pwdlen         = passwordLen;

    /* Copy password from shared memory */
    j = 0;
    for (k = passwordOffset; k < passwordOffset + passwordLen; k++)
    {
       password[j] = shmhdr[ k ];
       j++;
    }

  }

  /* spawn the child processes */
  if (num_processes > 1)
    CreateChildProcs(argv[0]);

  /* start workload */
  ExecuteTptBm(seed, procId);

  return 0;
}

/* Use a common random number generator rather than relying on platform */
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define TYPE_3          3               /* x**31 + x**3 + 1 */
#define BREAK_3         128
#define DEG_3           31
#define SEP_3           3

static unsigned int randtbl[DEG_3 + 1] = {
        TYPE_3,
        0x9a319039, 0x32d9c024, 0x9b663182, 0x5da1f342, 0xde3b81e0, 0xdf0a6fb5,
        0xf103bc02, 0x48f340fb, 0x7449e56b, 0xbeb1dbb0, 0xab5c5918, 0x946554fd,
        0x8c2e680f, 0xeb3d799f, 0xb11ee0b7, 0x2d436b86, 0xda672e2a, 0x1588ca88,
        0xe369735d, 0x904f35f7, 0xd7158fd6, 0x6fa6f051, 0x616e6b96, 0xac94efdc,
        0x36413f93, 0xc622c298, 0xf5a42ab8, 0x8a88d77b, 0xf5ad9d0e, 0x8999220b,
        0x27fb47b9,
};

static int *fptr = (int *) &randtbl[SEP_3 + 1];
static int *rptr = (int *) &randtbl[1];

static int *state = (int *) &randtbl[1];
static int *end_ptr = (int *) &randtbl[DEG_3 + 1];

/*
 * Returns a 31-bit random number.
 */
static int
ttRandom(void)
{
  int i;

  *fptr += *rptr;
  i = (*fptr >> 1) & 0x7fffffff;        /* chucking least random bit */
  if (++fptr >= end_ptr) {
    fptr = state;
    ++rptr;
  } else if (++rptr >= end_ptr)
    rptr = state;
  return(i);
}


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */

/* end of file TptbmODPI.c */
