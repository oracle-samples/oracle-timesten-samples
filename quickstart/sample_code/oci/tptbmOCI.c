/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*

  NAME
    tptbmOCI.c - An OCI implementation of the TimesTen TPTBM workload

  DESCRIPTION
    This program implements the Transaction Processing Throughput BenchMark
    using OCI.  This program will run against TimesTen 11.2.1+ and 
    Oracle 11.1.0.7+. The instructions to build and run this program are 
    in the Quick Start documentation.
   

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


/* end of file tptbmOCI.c */

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
#include <sys/stat.h>
#include <sys/wait.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <oci.h>
#include <oratypes.h>
#include <ocidfn.h>

#include "tt_version.h" /* Defines the default username and TNS Service name  */

extern  int chg_echo(int echo_on);
void usage(char *prog);
void parse_args(int argc,char **argv);
void populate( void );

/* Common error constants */
#define DB_OUT_OF_SPACE 2356
#define LOCK_TIMEOUT    4021

#define SERVICE_SIZE      256

char username[ MAX_USERNAME_SIZE ];
char password[ MAX_PASSWORD_SIZE ];
char service[  SERVICE_SIZE ];

int init = 0;

#define SB_SUCCESS 0
#define SB_FAIL 1
#define INTP_FMT "ld"
typedef long sb_intp;

#define MAXRAND (0x7fffffff)
#define MAXRANDP1 (0x80000000U)

int         seed = 1;                  /* seed for the random numbers */
int         num_processes = 1;         /* number of concurrent processes
                                        * for the test */
int         num_xacts = 10000;        /* number of transactions per process */
int         dummy = 0;                 /* dummy variable used in the NT case */
int         key_cnt = 100;             /* number of keys (squared) populated */
int         min_xact = 1;              /* Min. SQL ops per transaction */ 
int         max_xact = 1;              /* Max. SQL ops per transaction */ 
int         reads   = 0;                 
int         inserts = 0;              
int         deletes = 0;
int         updates = 0;
int         insert_mod = 1;                                             
int         hash = 1;
int         procId  = 0;               /* process # in shared memory array */
long       *pidArr  = NULL;            /* array of pids for the parent process */

int         *shmhdr = NULL;    /* Shared memory segment */
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


int         think_time = 0;           /* average think time for a transaction*/
int         srvlen = 0;
int         usernamelen = 0;
int         build_only = 0;
int         no_build = 0;
int         multiop = 0;             /* 1 insert, 3 reads, 1 update / xact */
int         repl = 0;
int         isTimesTen = 0;          /* Whether Connected to TimesTen or Oracle DB */

OraText       dbServerVersion[256];
sword         major_version   = 0;
sword         minor_version   = 0;
sword         update_num      = 0;
sword         patch_num       = 0;
sword         port_update_num = 0;
sword         retCode         = 0;


OCIServer *pop_srvhp = NULL;
OCISvcCtx *pop_svchp = NULL;
OCISession *pop_authp = (OCISession *) NULL;

sword ITA_OCIStmtExecute  (OCISvcCtx *svchp, OCIStmt *stmtp, OCIError *errhp, 
                         ub4 iters, ub4 rowoff, const OCISnapshot *snap_in, 
                         OCISnapshot *snap_out, ub4 mode)
{
  return (OCIStmtExecute(svchp, stmtp, errhp, iters, rowoff, snap_in,
			 snap_out, mode));
}

sword ITA_OCITransCommit  (OCISvcCtx *svchp, OCIError *errhp, ub4 flags)
{
  return (OCITransCommit(svchp, errhp, flags));
}

sword ITA_OCITransRollback  (OCISvcCtx *svchp, OCIError *errhp, ub4 flags)
{
  return (OCITransRollback  (svchp, errhp, flags));
}

sword ITA_OCIStmtFetch   (OCIStmt *stmtp, OCIError *errhp, ub4 nrows, 
                        ub2 orientation, ub4 mode)
{
  return (OCIStmtFetch   (stmtp, errhp, nrows, orientation, mode));
}

sword ITA_OCIStmtFetch2   (OCIStmt *stmtp, OCIError *errhp, ub4 nrows, 
                        ub2 orientation, sb4 scrollOffset, ub4 mode)
{
  return (OCIStmtFetch2   (stmtp, errhp, nrows, 
			   orientation,  scrollOffset, mode));
}

sword ITA_OCIAttrGet (const void  *trgthndlp, ub4 trghndltyp, 
                    void  *attributep, ub4 *sizep, ub4 attrtype, 
                    OCIError *errhp)
{
  return (OCIAttrGet (trgthndlp, trghndltyp, 
		      attributep, sizep, attrtype, 
		      errhp));
}

sword ITA_OCIAttrSet (void  *trgthndlp, ub4 trghndltyp, void  *attributep,
                    ub4 size, ub4 attrtype, OCIError *errhp)
{
  return (OCIAttrSet (trgthndlp, trghndltyp, attributep,
		      size, attrtype, errhp));
}

#ifndef _WIN32
sb_intp diff_time(struct timeval * start,struct timeval *end)
{
  return ((end->tv_sec - start->tv_sec)*1000 +
          (end->tv_usec - start->tv_usec)/1000);
}

#else
sb_intp diff_time(struct _timeb * start,struct _timeb *end)
{
  return ((end->time - start->time)*1000 +
          (end->millitm - start->millitm));
}
#endif /* _WIN32 */

/*
 *  Check for errors.
 */
static int  error_count = 0;
static int checkerrReally(int line, OCIError *errhp, sword status)
{
  text errbuf[512];
  sb4 errcode;

#ifdef _WIN32
  DWORD  myPid = 0;
#else
  pid_t  myPid = 0;
#endif

  switch (status) {
    case OCI_SUCCESS:
      break;
    case OCI_SUCCESS_WITH_INFO:
      (void) printf("Error - OCI_SUCCESS_WITH_INFO %d\n", line);
      break;
    case OCI_NEED_DATA:
      (void) printf("Error - OCI_NEED_DATA %d\n", line);
      break;
    case OCI_NO_DATA:
	  /* For this workload, a missing row is valid as it may have been deleted  */
      /* (void) printf("Error - OCI_NODATA, line %d\n", line); */
      break;

	case OCI_ERROR:
      (void) OCIErrorGet(errhp, (ub4) 1, (text *) NULL, &errcode,
                         errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
      
	  if (LOCK_TIMEOUT == errcode) {
        
		/* For this workload, lock timeouts can be common */          
/*
                printf("Got a lock timeout for the update or delete:\n");
                printf("- Multiple connections were trying to modify the same row(s).\n");
                printf("- Increase the possible rows (-key) and/or decrease the concurrency (-proc).\n\n");
*/
		break;

	  } else if (DB_OUT_OF_SPACE == errcode) {
		(void) printf("\n%d rows of data will not fit in table %s.vpn_users\n", key_cnt*key_cnt, username); 
		(void) printf("\nPlease do one or more of the following:\n");
                (void) printf("  1. Make the -key parameter smaller\n");          
                (void) printf("  2. Increase the database size by making the PermSize attribute bigger\n");
                (void) printf("     The PermSize (in MB) must fit within physical RAM\n\n");
	  } else {

	    /* Show the error */
	    (void) printf("line %d: Error - %s\n", line, (char *) errbuf );			  
	  }
      break;
    case OCI_INVALID_HANDLE:
      (void) printf("Error - OCI_INVALID_HANDLE %d\n", line);
      break;
    case OCI_STILL_EXECUTING:
      (void) printf("Error - OCI_STILL_EXECUTE %d \n", line);
      break;
    case OCI_CONTINUE:
      (void) printf("Error - OCI_CONTINUE %d\n", line);
      break;
	default:
      break;
  }
  if ( ! (   status == OCI_SUCCESS 
	      || status == OCI_SUCCESS_WITH_INFO 
		  || status == OCI_NO_DATA 
		  || LOCK_TIMEOUT == errcode ) ) {
    error_count++;

#ifdef _WIN32
    myPid = GetCurrentProcessId();
#else
    myPid = getpid();
#endif

    if (error_count > (num_processes * 1)) {

      printf("-1 ALERT error_count = %d, num_processes = %d\n", error_count, num_processes);
      return -1;
    }
    else {
      printf("Process %ld, exiting after too many errors (%d)\n\n", (long) myPid, error_count);
      exit(1);
    }
  }
  return 0;
}

#define checkerr(x,y) \
do {\
  if (checkerrReally(__LINE__,x,y)) { goto failExit; } \
} while (0);



/*
 *  Exit program with an exit code.
 */
void do_exit(sword exit_code)
{
 
  exit(exit_code);
}



/* SQL statements used in this program */

/* The select statement used for the read transaction */

char * select_stmnt =
"select directory_nb,last_calling_party,descr from vpn_users \
where vpn_id = :1 and vpn_nb= :2";

/* The update statement used for the write transaction */

char * update_stmnt =
"update vpn_users set last_calling_party = :1 \
where vpn_id = :2 and vpn_nb = :3";


/* The destroy table statement */

char * drop_stmnt = "DROP TABLE vpn_users";

/* The create table statement */ 

char * creat_stmnt = "CREATE TABLE vpn_users(\
vpn_id             NUMBER(5)   NOT NULL,\
vpn_nb             NUMBER(5)   NOT NULL,\
directory_nb       CHAR(10)  NOT NULL,\
last_calling_party CHAR(10)  NOT NULL,\
descr              CHAR(100) NOT NULL,\
                   PRIMARY KEY (vpn_id,vpn_nb)) ";

char * creat_stmnt_hash = "CREATE TABLE vpn_users(\
vpn_id             NUMBER(5)   NOT NULL,\
vpn_nb             NUMBER(5)   NOT NULL,\
directory_nb       CHAR(10)  NOT NULL,\
last_calling_party CHAR(10)  NOT NULL,\
descr              CHAR(100) NOT NULL,\
                   PRIMARY KEY (vpn_id,vpn_nb))\
UNIQUE HASH ON (vpn_id,vpn_nb) PAGES = %d";

/* constraint constr1 PRIMARY KEY (vpn_id,vpn_nb)) tablespace MYDATA";*/

/* organization index \ tablespace EXAMPLE"; */

/* The insert statement used to populate the datastore */

char * insert_stmnt = "insert into vpn_users values (:1,:2,:3,:4,:5)";

char * delete_stmnt = "delete from vpn_users where vpn_id = :1 and vpn_nb = :2";

/* char * stat_stmnt = "ANALYZE TABLE vpn_users COMPUTE STATISTICS"; */
char * stat_stmnt = "call ttOptUpdateStats('',1)";

/* Set the session lock timeout */
char * lock_timeout_stmnt = "call ttlockwait(0)";

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
	  "  %s [-h] [-help] [-V] \n"
          "  %s [-read <reads>] [-insert <ins>] [-update <ups>] [-delete <dels>]\n"
          "             [-xact <xacts>] [-min <ops_per_xact>] [-max <ops_per_xact>]\n"
          "             [-proc <nprocs>] [-key <key>] [-range] [-thinkTime <sleep_in_ms>]\n"
          "             [-seed <seed>] [-build] [-nobuild]\n"
          "             [-multiop] [-service <tnsServiceName>] [-user <user>]\n\n"
          "  -h                     Prints this message and exits.\n"
          "  -help                  Same as -h.\n"
          "  -V                     Prints the version number and exits.\n"
          "  -build                 Create and load VPN_USERS table and then exit.\n"
          "  -delete    <del>       Specifies that <ndel> is the percentage of delete\n"
          "                         transactions. The default is 0.\n"
          "  -range                 Build a range index.\n"
          "                         Default is hash index.\n"
          "  -insert    <ins>       Specifies that <nins> is the percentage of insert\n"
          "                         transactions. The default is 0.\n"
          "  -key       <key>       Specifies the number of records (squared) to initially\n"
          "                         populate in the VPN_USERS table.\n"
          "                         Default is 100^2 or 10,000 rows\n"
          "  -max       <max_xacts> Maximum number of SQL operations per transaction.\n"
          "                         Default is 1.\n"
          "  -min       <min_xacts> Minimum number of SQL operations per transaction.\n"
          "                         Default is 1.\n"
          "                         The number of SQL operations in a transaction\n"
	  "                         is randomly chosen between -min and -max.\n"
          "  -multiop               SQL workload with 1 insert, 3 reads, 1 update per txn.\n"
          "                         When using multiop, the -min and -max parameters mean\n"
          "                         that many iterations of the multiop workload.\n"
          "  -nobuild               Skip creating and loading table VPN_USERS,\n"
          "                         but run the workload.\n"          
          "  -proc      <nprocs>    Specifies that <nprocs> is the number of concurrent\n"
          "                         processes. The default is 1.\n"

          "  -read      <reads>     Specifies that <nreads> is the percentage of read-only\n"
          "                         transactions. The default is 80.\n"
          "  -seed      <seed>      Specifies that <seed> should be the seed for the\n"
          "                         random number generator.\n"
          "  -service               Specify the Oracle Net service name.\n"
          "                         The service name can point to Direct Linked or\n"
          "                         Client/Server TimesTen DSNs or to an Oracle database.\n"
          "                         Default service name is %s\n"
          "  -thinkTime <ms>        Specifies the 'think time' in milli-seconds per txn.\n"
          "  -update    <ups>       Specifies that <ups> is the percentage of update\n"
          "                         transactions. The default is 20.\n"
          "  -user                  Specify DB username.\n"
          "                         Default username is %s\n"
          "  -xact      <xacts>     Specifies that <xacts> is the number of transactions\n"
          "                         that each process should run. The default is 10000.\n",
		  prog, prog, DEMODSN, UIDNAME
		  );

  printf("\nExample :\n");
  printf("  70%% reads, 30%% updates for appuser@sampledb with 100K txns\n\n");
  printf("  tptbmOCI -read 70 -update 30 -service sampledb -user appuser -xact 100000\n\n");

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

  int           i              = 1;
  int           temp           = 0;


  /* Initialize the service name */
  memset(service, 0, sizeof(service));

  /* Use the default Service Name, can override from command-line */
  strcpy(service, DEMODSN);
  srvlen = strlen(service);

  /* Use the default userame, can override from command-line */
  strcpy(username, UIDNAME);
  usernamelen = strlen(username);

  while(i < argc) {
    if ( !strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") ) {
      usage(argv[0]);
      exit(1);
    }
    else if (strcmp("-V",argv[i]) == 0) {

      printf("%s\n", TTVERSION_STRING);
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
    else if (strcmp("-min",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &min_xact) == -1 ||
          min_xact <= 0) {
        printf("-min flag requires a positive integer argument\n");
        usage(argv[0]);
        exit(1);
      }
      i += 2;
    }
    else if (strcmp("-max",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      if (sscanf(argv[i+1], "%d", &max_xact) == -1 ||
          max_xact <= 0) {
        printf("-max flag requires a positive integer argument\n");
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
    else if (strcmp("-service",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      srvlen = strlen (argv[i+1]);

      if (srvlen >= SERVICE_SIZE - 1) {
        printf("\nThe Oracle Net Servicename [%d characters] was too long\n", srvlen);
        usage(argv[0]);
        exit(1);

      } else {

        /* Initialize the service name */
        memset(service, 0, sizeof(service));

        /* Get the Oracle Net Service Name */
        strcpy(service, argv[i+1]);

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
    printf("-key   >= %9d  [for %d -xacts]\n\n", 1 + ((int) sqrt(num_xacts)), num_xacts);
    exit(1);

  }
 
  /* Fix min and max if necessary */
  if (min_xact > max_xact) {
    temp = max_xact;
    max_xact = min_xact;
    min_xact = temp;
  }  

  /* Don't prompt for the password in child processes on Windows */
  if (0 == dummy) {

    printf("\nEnter password for %s : ", username);

    /* Turn off echo */
    chg_echo(0);

    /* Get the password */
    fgets(password, sizeof(password), stdin);

    /* Turn on echo */
    chg_echo(1);
    password[strlen((char *) password)-1] = '\0';

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
void
populate( void )
{

  /* buffers to insert data */

  text       directory[11];
  text       descr[101];
  text       last[11];
  int        id;
  int        nb;
  char       errstr[4096];
  int        i, j;
  
  OCIEnv    *envhp = NULL;
  OCIError  *errhp = NULL;
  OCIStmt *stmthp = NULL;
  OCIBind *bnd1p  = NULL,*bnd2p = NULL, *bnd3p = NULL, *bnd4p = NULL, *bnd5p = NULL;


  /* Use OCIEnvCreate() instead of the older OCIInitialize() and OCIEnvInit() functions */
  retCode = OCIEnvCreate( (OCIEnv **) &envhp,
                          (ub4) OCI_DEFAULT | OCI_ENV_NO_MUTEX, 
                          (dvoid *) 0,
                          (dvoid * (*)(dvoid *,size_t)) 0,
                          (dvoid * (*)(dvoid *, dvoid *, size_t)) 0,
                          (void (*)(dvoid *, dvoid *)) 0, (size_t) 0,
                          (dvoid **) 0); 

  if (retCode != OCI_SUCCESS) {
     printf("\nOCIEnvCreate failed at line %d, retCode = %d", __LINE__, retCode);
     exit(1);
  }


  if  (OCIHandleAlloc((dvoid *) envhp, (dvoid **) &errhp,OCI_HTYPE_ERROR, 0, (dvoid **) NULL) != OCI_SUCCESS){
	  printf("OCIHandleAlloc failed at %d",__LINE__);
	  exit(1);
  }

  checkerr(errhp,OCIHandleAlloc((dvoid *) envhp, (dvoid **) &pop_srvhp,OCI_HTYPE_SERVER, 0, (dvoid **) NULL) );

  checkerr(errhp,OCIHandleAlloc((dvoid *) envhp, (dvoid **) &pop_svchp,OCI_HTYPE_SVCCTX, 0, (dvoid **) NULL) );


  checkerr(errhp,OCIServerAttach(pop_srvhp, errhp, (const OraText *)service, (sb4)srvlen,(ub4)OCI_DEFAULT));

  checkerr(errhp,OCIAttrSet((dvoid *)pop_svchp,OCI_HTYPE_SVCCTX,pop_srvhp,(ub4)0,OCI_ATTR_SERVER,(OCIError *)errhp));

  checkerr(errhp,OCIHandleAlloc((dvoid *)envhp,(dvoid **) &pop_authp,(ub4)OCI_HTYPE_SESSION,(size_t)0,(dvoid **) 0));


  checkerr(errhp,OCIAttrSet((dvoid *)pop_authp,(ub4)OCI_HTYPE_SESSION,(dvoid *)username,(ub4)strlen((char *)username),
                    OCI_ATTR_USERNAME, errhp));

  checkerr(errhp,OCIAttrSet((dvoid *)pop_authp,(ub4)OCI_HTYPE_SESSION,(dvoid *)password,(ub4)strlen((char *)password),
                    OCI_ATTR_PASSWORD, errhp));

  checkerr(errhp, OCISessionBegin(pop_svchp,errhp,pop_authp,OCI_CRED_RDBMS,(ub4)OCI_DEFAULT));


  checkerr(errhp,OCIAttrSet((dvoid *)pop_svchp,(ub4)OCI_HTYPE_SVCCTX,(dvoid *)pop_authp,(ub4)0,
                    (ub4) OCI_ATTR_SESSION, errhp));


  /* Get the DB Server version (Oracle or TimesTen) */
  checkerr(errhp, OCIServerVersion(pop_srvhp, errhp, dbServerVersion, sizeof(dbServerVersion), (ub1) OCI_HTYPE_SERVER));

  /* Get the Oracle Client Version */
  OCIClientVersion(&major_version, &minor_version, &update_num, &patch_num, &port_update_num);

  if (NULL == strstr((const char *) dbServerVersion, "TimesTen")) {

    isTimesTen = 0;
    printf("\n\nConnected using %s@%s\n", username, service);
    printf("\nUsing %s\n", (char *) dbServerVersion);

    printf("and Oracle Client %d.%d.%d.%d.%d\n", 
           major_version,
           minor_version,
           update_num,
           patch_num,
           port_update_num);

  } else {
    
     isTimesTen = 1;
  }




  checkerr(errhp, OCIHandleAlloc((dvoid *)envhp,(dvoid **)&stmthp,OCI_HTYPE_STMT,0,(dvoid **)NULL));

  checkerr(errhp, OCIStmtPrepare(stmthp, errhp, (text *) drop_stmnt,
                    (ub4) strlen(drop_stmnt), OCI_NTV_SYNTAX, OCI_DEFAULT));

  /* ignore errors from drop */
  OCIStmtExecute(pop_svchp, stmthp, errhp, 1, 0, 0, 0, OCI_DEFAULT);


  if (hash && isTimesTen) {
    sprintf(errstr,creat_stmnt_hash,(key_cnt*key_cnt)/256);
  }
  else {
    strcpy(errstr, creat_stmnt);
  }

  checkerr(errhp, OCIStmtPrepare(stmthp, errhp, (text *) errstr,
                    (ub4) strlen(errstr), OCI_NTV_SYNTAX, OCI_DEFAULT));

  checkerr(errhp, OCIStmtExecute(pop_svchp, stmthp, errhp, 1, 0, 0, 0, OCI_DEFAULT));

  checkerr(errhp,OCIStmtPrepare(stmthp,errhp,(text *)insert_stmnt,(ub4)strlen(insert_stmnt),OCI_NTV_SYNTAX,OCI_DEFAULT));

  /*  checkerr(errhp, OCIHandleAlloc((dvoid *) stmthp, (dvoid **) &bnd1p,
                    OCI_HTYPE_BIND, 0, (dvoid **) NULL));

  checkerr(errhp, OCIHandleAlloc((dvoid *) stmthp, (dvoid **) &bnd2p,
                    OCI_HTYPE_BIND, 0, (dvoid **) NULL));

  checkerr(errhp, OCIHandleAlloc((dvoid *) stmthp, (dvoid **) &bnd3p,
                    OCI_HTYPE_BIND, 0, (dvoid **) NULL));

  checkerr(errhp, OCIHandleAlloc((dvoid *) stmthp, (dvoid **) &bnd4p,
  OCI_HTYPE_BIND, 0, (dvoid **) NULL));
  */

  checkerr(errhp, OCIBindByPos(stmthp, &bnd1p, errhp,1, &id,sizeof(id),SQLT_INT, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp, &bnd2p, errhp,2, &nb,sizeof(nb),SQLT_INT, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp, &bnd3p, errhp,3, directory,sizeof(directory),SQLT_STR, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp, &bnd4p, errhp,4, last,sizeof(last),SQLT_STR, (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));
  checkerr(errhp, OCIBindByPos(stmthp, &bnd5p, errhp,5, descr,sizeof(descr),SQLT_STR, (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  printf("\nLoad the %s.vpn_users table with %d rows of data\n", 
		  username, key_cnt * key_cnt);

  memset(last, 0, sizeof(last));

  for (i=0; i < key_cnt; i++){
    for(j=0; j < key_cnt; j++){
      
      id = i;
      nb = j;
      sprintf((char *)last,"000000000");
      sprintf((char *)directory,"55%d%d",id,nb);
      sprintf((char *)descr,
              "<place holder for description of VPN %d extension %d>",id,nb);
      
      /* execute insert statement */
      checkerr(errhp, ITA_OCIStmtExecute(pop_svchp, stmthp, errhp, 1, 0, 0, 0, OCI_DEFAULT));

    }
    checkerr(errhp, ITA_OCITransCommit(pop_svchp, errhp, 0));

  }
  printf("\n");

  /* 
  checkerr(errhp, OCIStmtPrepare(stmthp, errhp, (text *) stat_stmnt,
                    (ub4) strlen(stat_stmnt), OCI_NTV_SYNTAX, OCI_DEFAULT));

  checkerr(errhp, OCIStmtExecute(pop_svchp, stmthp, errhp, 1, 0, 0, 0, OCI_DEFAULT));
  */


 failExit:

  checkerr(errhp, OCITransCommit(pop_svchp, errhp, 0));

  checkerr(errhp, OCIHandleFree((dvoid *) stmthp, OCI_HTYPE_STMT));

  if (build_only) {
     checkerr(errhp, OCISessionEnd(pop_svchp,errhp,pop_authp,OCI_DEFAULT));

     checkerr(errhp, OCIServerDetach(pop_srvhp, errhp, OCI_DEFAULT));


     checkerr(errhp, OCIHandleFree((dvoid *) pop_authp, OCI_HTYPE_SESSION));

     checkerr(errhp, OCIHandleFree((dvoid *) pop_svchp, OCI_HTYPE_SVCCTX));

     checkerr(errhp, OCIHandleFree((dvoid *) pop_srvhp, OCI_HTYPE_SERVER));

     OCIHandleFree((dvoid *) pop_srvhp, OCI_HTYPE_ERROR);

     OCIHandleFree((dvoid *) pop_srvhp, OCI_HTYPE_ENV);
  }
  
  return;
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

int main(int argc, char** argv)
{


  
#ifdef _WIN32
  HANDLE         han;                     /* handle to shared memory segment */
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
  char           key_string[10];       /* buffer used to recursively invoke
                                           * this program */
  char           read_string[10];       /* buffer used to recursively invoke
                                           * this program */
  struct _timeb  main_start,main_end;     /* variables to measure time */

  char           nameBuffer[FILENAME_MAX];/* buffer to hold short (8.3) name
                                           * for argv[0] */                       
  DWORD          nameBufferLen = FILENAME_MAX - 1;   /* capacity of the short
                                                      * name buffer */
  char           cmdLine[4000];

  STARTUPINFO    sInfo = {8L, NULL, NULL, NULL,
                          0L, 0L, 0L, 0L, 0L, 0L, 0L,
                          STARTF_USESHOWWINDOW, SW_HIDE, 0L,
                          NULL, NULL, NULL, NULL};

  PROCESS_INFORMATION pInfo;

#else
  int            shmid = 0;                   /* shared memory segment id */
  struct timeval main_start,main_end;     /* variables to measure time */
  struct tms tstart, tfinish;
  double ticks, utime, stime, cutime, cstime;    

#endif

  int            rand_int;                /* rand integer */
  int            rand_wait = 0;               /* rand wait interval */

  int            insert_start;            /* start mark for inserts */
  int            insert_present;          /* present mark for inserts */
  int            delete_start = 0;        /* start mark for deletes */
  int            delete_present = 0;      /* present mark for deletes */

  sb_intp        time_diff;               /* difference in time between
                                           * start and finish */
  double         duration_secs;           /* Duration of workload in seconds */
  double         duration;                /* Duration of workload in seconds
                                             or milli-seconds */
  char           duration_units[20];
  double         tps;                     /* to compute transactions per
                                           * second */

  int            i,jj;
  int            ops_in_xact = 1;         /* SQL operations in a transaction */
  int            err = 0;

#ifdef _WIN32
  int            exitcode;
#endif /* _WIN32 */

  /* the following three buffers are for the select statement */

  text    directory[11];
  text    last[11];
  text    descr[101];
  int     id;
  int     nb;

  OCIEnv    *envhp = NULL;
  OCIServer *srvhp = NULL;
  OCISvcCtx *svchp = NULL;
  OCIError  *errhp = NULL;
  OCISession *authp = (OCISession *) NULL; 

  OCIStmt *stmthp1 = NULL,*stmthp2 = NULL,*stmthp3 = NULL, *stmthp4 = NULL, *stmthp5 = NULL;
  OCIBind *bnd11p  = NULL,*bnd12p = NULL;
  OCIBind *bnd21p  = NULL,*bnd22p = NULL, *bnd23p = NULL;
  OCIBind *bnd31p  = NULL,*bnd32p = NULL, *bnd33p = NULL, *bnd34p = NULL,*bnd35p = NULL;
  OCIBind *bnd41p = NULL, *bnd42p = NULL;
  OCIDefine *defnp1 = (OCIDefine *) 0,*defnp2 = (OCIDefine *) 0,*defnp3 = (OCIDefine *) 0;

  int            shmpos = 1;              /* position of the process in the
                                           * shared memory array */
  int            ami_parent = 0;          /* parent process */
  int            path = 0;                /* whether update or select
                                           * transaction */ 
  int           passwordOffset = 0;
  int           passwordLen    = 0;
  int           numProcesses   = 0;
  int           j, k           = 0;

  int           minSqlPerTxn   = 0;
  int           maxSqlPerTxn   = 0;


  /* initialize the selectstatement buffers */
  
  directory[10] = '\0';
  last[10] = '\0';
  descr[100]= '\0';
  
  /* Clear the password */
  memset(password, 0, sizeof(password));

  /* parse the command line arguments */
  parse_args(argc,argv);

  if (dummy > 0) shmpos = dummy;     /* this happens only in the NT case */
  /* on unix dummy is always 0 */

  /* the parent/main process must prepare VPN_USERS table for the benchmark */
  if (dummy <= 0 && !no_build) {
    populate();
  }

  if (build_only)
    exit(0);  

  /* Local memory for PIDs and password */
  pidArr = malloc(sizeof(long) * (num_processes + 1));
  if (pidArr == NULL)
  {
    printf("Failed to allocate memory for pidArr\n");
    exit(-1);
  }

  /* Calculate the size of the shared memory segment */
  shmSize = sizeof(int) * (num_processes + strlen(password) + 4);

  /* if there are more than 1 processes specified and the platform in
   * non-Windows, initialize the shared memory segment */

#ifndef _WIN32

  if (num_processes > 1) {

    /* create shared memory segment */
    shmid = shmget(ftok("ctest",seed),
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

    /* Password not needed in shared memory on non-Windows systems */
  }

#else

  /* dummy is 0 only for the parent process */

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


#endif

  /* Don't prompt for the password in child processes on Windows */
  if (dummy != 0) {


    numProcesses   = shmhdr[ 0 ];
    passwordLen    = shmhdr[ numProcesses + 1];
    passwordOffset = numProcesses + 2;

    /* Copy password from shared memory */
    j = 0;
    for (k = passwordOffset; k < passwordOffset + passwordLen; k++)
    {
       password[j] = shmhdr[ k ];
       j++;
    }

  }


  /* spawn the child processes */

  if (dummy == 0 && num_processes > 1) {
    if (num_processes == 2)
      printf("Waiting for %d child process to synchronize\n", num_processes - 1);
	else
      printf("Waiting for %d child processes to synchronize\n", num_processes - 1);
  }

  {

  for(i = 1; i < num_processes; i++) {

    seed += 5;

#ifndef _WIN32

    fflush(stdout);
    fflush(stderr);
    ami_parent = fork();
    if (ami_parent == 0){
      seed += 2;
      break;
    }
    if (ami_parent == -1){
      printf("Failure spawning a process\n");
      exit(1);
      
    }
    pidArr[i] = ami_parent;
    shmpos++;

#else

    /* in this case spawn more processes with a different value for dummy */
    /* the variable dummy corresponds to shmpos, the position of the process
       in the shared memory array - it is 0 in the parent case */

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
       */
      
      sprintf(cmdLine, 
"%s -proc 1 -xact %s -seed %s -dummy %s -key %s -read %s -insert %s -update %s -delete %s -insertmod %s -service %s -user %s",
                argv[0],
                xact_string,
                seed_string,
                shmpos_string,
                key_string,
                read_string,
                insert_string,
                update_string,
                delete_string,
                inmod_string,
                service,
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
    ami_parent = 1;
    
#endif
  }
  }

  srand(seed);

  if (OCIInitialize(OCI_DEFAULT, (dvoid *) 0, 0, 0, 0) != OCI_SUCCESS){
	  printf("OCIInitialize failed at %d",__LINE__);
	  exit(1);
  }

  if  (OCIEnvInit((OCIEnv **) &envhp,OCI_DEFAULT, 0, (dvoid **) NULL) != OCI_SUCCESS){
	  printf("OCIEnvInit failed at %d",__LINE__);
	  exit(1);
  }

  if  (OCIHandleAlloc((dvoid *) envhp, (dvoid **) &errhp,OCI_HTYPE_ERROR, 0, (dvoid **) NULL) != OCI_SUCCESS){
	  printf("OCIHandleAlloc failed at %d",__LINE__);
	  exit(1);
  }

  checkerr(errhp,OCIHandleAlloc((dvoid *) envhp, (dvoid **) &srvhp,OCI_HTYPE_SERVER, 0, (dvoid **) NULL) );

  checkerr(errhp,OCIHandleAlloc((dvoid *) envhp, (dvoid **) &svchp,OCI_HTYPE_SVCCTX, 0, (dvoid **) NULL) );

  checkerr(errhp,OCIServerAttach(srvhp, errhp, (text *)service, (sb4)srvlen,(ub4)OCI_DEFAULT));

  checkerr(errhp,OCIAttrSet((dvoid *)svchp,OCI_HTYPE_SVCCTX,srvhp,(ub4)0,OCI_ATTR_SERVER,(OCIError *)errhp));

  checkerr(errhp,OCIHandleAlloc((dvoid *)envhp,(dvoid **) &authp,(ub4)OCI_HTYPE_SESSION,(size_t)0,(dvoid **) 0));


  checkerr(errhp,OCIAttrSet((dvoid *)authp,(ub4)OCI_HTYPE_SESSION,(dvoid *)username,(ub4)strlen((char *)username),
                    OCI_ATTR_USERNAME, errhp));

  checkerr(errhp,OCIAttrSet((dvoid *)authp,(ub4)OCI_HTYPE_SESSION,(dvoid *)password,(ub4)strlen((char *)password),
                    OCI_ATTR_PASSWORD, errhp));

  checkerr(errhp, OCISessionBegin(svchp,errhp,authp,OCI_CRED_RDBMS,(ub4)OCI_DEFAULT));


  checkerr(errhp,OCIAttrSet((dvoid *)svchp,(ub4)OCI_HTYPE_SVCCTX,(dvoid *)authp,(ub4)0,
                    (ub4) OCI_ATTR_SESSION, errhp));
  
  checkerr(errhp, OCIHandleAlloc((dvoid *)envhp,(dvoid **)&stmthp1,OCI_HTYPE_STMT,0,(dvoid **)NULL));

  checkerr(errhp, OCIHandleAlloc((dvoid *)envhp,(dvoid **)&stmthp2,OCI_HTYPE_STMT,0,(dvoid **)NULL));

  checkerr(errhp, OCIHandleAlloc((dvoid *)envhp,(dvoid **)&stmthp3,OCI_HTYPE_STMT,0,(dvoid **)NULL));
  checkerr(errhp, OCIHandleAlloc((dvoid *)envhp,(dvoid **)&stmthp4,OCI_HTYPE_STMT,0,(dvoid **)NULL));
  checkerr(errhp, OCIHandleAlloc((dvoid *)envhp,(dvoid **)&stmthp5,OCI_HTYPE_STMT,0,(dvoid **)NULL));

  checkerr(errhp, OCIStmtPrepare(stmthp1, errhp, (text *) select_stmnt,
                    (ub4) strlen(select_stmnt), OCI_NTV_SYNTAX, OCI_DEFAULT));

  checkerr(errhp, OCIStmtPrepare(stmthp2, errhp, (text *) update_stmnt,
                    (ub4) strlen(update_stmnt), OCI_NTV_SYNTAX, OCI_DEFAULT));

  checkerr(errhp, OCIStmtPrepare(stmthp3, errhp, (text *) insert_stmnt,
                    (ub4) strlen(insert_stmnt), OCI_NTV_SYNTAX, OCI_DEFAULT));

  checkerr(errhp, OCIStmtPrepare(stmthp4, errhp, (text *) delete_stmnt,
                    (ub4) strlen(delete_stmnt), OCI_NTV_SYNTAX, OCI_DEFAULT));
  
  checkerr(errhp, OCIStmtPrepare(stmthp5, errhp, (text *) lock_timeout_stmnt,
                    (ub4) strlen(lock_timeout_stmnt), OCI_NTV_SYNTAX, OCI_DEFAULT));
  
  checkerr(errhp, OCIBindByPos(stmthp3, &bnd31p, errhp,1, &id,sizeof(id),SQLT_INT, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp3, &bnd32p, errhp,2, &nb,sizeof(nb),SQLT_INT, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp3, &bnd33p, errhp,3, directory,sizeof(directory),SQLT_STR, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp3, &bnd34p, errhp,4, last,sizeof(last),SQLT_STR, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp3, &bnd35p, errhp,5, descr,sizeof(descr),SQLT_STR, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));
  
  checkerr(errhp, OCIBindByPos(stmthp2, &bnd21p, errhp,1, last,sizeof(last),SQLT_STR, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp2, &bnd22p, errhp,2, &id,sizeof(id),SQLT_INT, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp2, &bnd23p, errhp,3, &nb,sizeof(nb),SQLT_INT, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp1, &bnd11p, errhp,1, &id,sizeof(id),SQLT_INT, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));
  
  checkerr(errhp, OCIBindByPos(stmthp1, &bnd12p, errhp,2, &nb,sizeof(nb),SQLT_INT, (dvoid *) 0,
                    (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));
  
  checkerr(errhp, OCIDefineByPos(stmthp1, &defnp1, errhp, 1,(dvoid *) directory, sizeof(directory), SQLT_STR,
                             (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIDefineByPos(stmthp1, &defnp2, errhp, 2,(dvoid *) last, sizeof(last), SQLT_STR,
                             (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIDefineByPos(stmthp1, &defnp3, errhp, 3,(dvoid *) descr, sizeof(descr), SQLT_STR,
                             (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp4, &bnd41p, errhp,1, &id,sizeof(id),SQLT_INT, (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmthp4, &bnd42p, errhp,2, &nb,sizeof(nb),SQLT_INT, (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT));

    /* Get the DB Server version (Oracle or TimesTen) */
/*  checkerr(errhp, OCIServerVersion(svchp, errhp, dbServerVersion, sizeof(dbServerVersion), (ub1) OCI_HTYPE_SERVER)); */

  /* Get the Oracle Client Version */
/*  OCIClientVersion(&major_version, &minor_version, &update_num, &patch_num, &port_update_num); */

  if (NULL == strstr((const char *) dbServerVersion, "TimesTen")) {

    isTimesTen = 0;

  } else {
    
	isTimesTen = 1;
  }

  if ( isTimesTen ) {

    /* Set the lock timeout */
    checkerr(errhp, ITA_OCIStmtExecute(svchp, stmthp5, errhp, 1, 0, 0, 0, OCI_DEFAULT));
  }


  checkerr(errhp, OCITransCommit(svchp, errhp, 0));
  
  if ( (ami_parent) || ((num_processes == 1) && (dummy <= 0)) ) {

	if (1 == num_processes) {
	  printf("Run %d txns with 1 process: %d%% read, %d%% update, %d%% insert, %d%% delete\n",
		    num_xacts, reads, updates, inserts, deletes);
	} else {

		printf("\nRun %d txns with %d processes: %d%% read, %d%% update, %d%% insert, %d%% delete\n",
		    num_xacts, num_processes, reads, updates, inserts, deletes);
	}

  }
#ifndef _WIN32
  err = gettimeofday(&main_start,NULL);
  (void) times (&tstart);
#else
  _ftime(&main_start);
#endif
  
#ifndef _WIN32

  if (num_processes > 1) {

    /* parent sets the PIDs in array to 1 */

    if (ami_parent) {
	
      for(i=1; i <= num_processes;i++)
        shmhdr[i] = 1;
    }

    /* the child polls for a value of 1 */

    while(shmhdr[shmpos] == 0) {
/*      printf("Child %d not yet synchronized\n", shmpos);   */
      sleep(1);
    }
  }

#else
  if ((num_processes > 1) || (dummy != 0)) {

    /* parent sets  the entire array to 1 */

    if ((ami_parent) || ((dummy == 0) && (num_processes != 1))) {

		for(i=1; i <= num_processes; i++)
        shmhdr[i] = 1;
    }

    /* the child polls for a value of 1 */

	while(shmhdr[shmpos] == 0) {
/*	  printf("Child %d not yet synchronized\n", shmpos); */ 
          sleep(1);
	}
  }

#endif

  insert_start = (key_cnt-1) + shmpos;   /* start mark for this process */
  insert_present = 0;
  delete_start = shmpos-1;

  /* execute num_xacts transactions */
  for (i = 0; i < num_xacts;i++){
    ops_in_xact = min_xact; /* default operations per xact */
    if (min_xact != max_xact){ /* Get a random value */
      
      ops_in_xact = ((int)
                     ((1+max_xact - min_xact) *
                      (ttRandom()/((double)MAXRANDP1)))) + min_xact;
    }
    
    if (multiop) {

      /* By default ops_in_xact = 1 */
      jj = 5 * ops_in_xact;
    } else
      jj = ops_in_xact;

    while(jj > 0){
      jj--;

      if (think_time > 0){
#ifdef _WIN32

        /* Sleep in milli-seconds */
        Sleep(think_time);
#else
	struct timeval waitT;

        /* Make it in milli-seconds */
        rand_wait = think_time * 1000;

	waitT.tv_sec = 0;
	waitT.tv_usec = rand_wait;
	select(0,0,0,0,&waitT);
#endif
      }
      

      if (multiop) {

        switch (jj % 5) {
          case 4: path = 2; break;
          case 3:
          case 2:
          case 1: path = 1; break;
          case 0: path = 0; break;
          default: printf("unknown path for multiop\n");
        }
      }
      else {
       if (reads != 100) {
          rand_int = ttRandom() % MAXRANDP1;
          if (rand_int < ((float)(reads+inserts+deletes)/100)*MAXRANDP1){
            if (rand_int < ((float)(reads+inserts)/100)*MAXRANDP1){
              if (rand_int < ((float)(reads)/100)*MAXRANDP1) {
                path = 1; /* read */
              }
              else {
                path = 2; /* insert */
              }
            }
            else {
              path = 3; /* delete */
            }
          }
          else {
            path = 0; /* update */
          }
        }
        else {
          path = 1;  /* read */
        }
      }
    
      if (path == 1) { /* select xact */
        /* randomly pick argument values */ 
	id = (int)((key_cnt - 1)*(ttRandom()/(double)MAXRAND));
	nb = (int)((key_cnt - 1)*(ttRandom()/(double)MAXRAND));

        checkerr(errhp,ITA_OCIStmtExecute(svchp,stmthp1,errhp,1,0,0,0,OCI_DEFAULT));

      }
      else if (path == 0){ /* update xact */
	/* randomly pick argument values */ 
	
	id = (int)((key_cnt - 1)*(ttRandom()/(double)MAXRAND));
	nb = (int)((key_cnt - 1)*(ttRandom()/(double)MAXRAND));
        if (id == 2 && nb == 2) { /* don't overwrite start/done msg */
          nb = 1;
        }
	
	sprintf((char *)last,"%dx%d",id,nb);
      
	/* execute and commit if required */


	if (jj ==0) {
	  checkerr(errhp, ITA_OCIStmtExecute(svchp, stmthp2, errhp, 1, 0, 0, 0, OCI_COMMIT_ON_SUCCESS));
        }
	else {
	  checkerr(errhp, ITA_OCIStmtExecute(svchp, stmthp2, errhp, 1, 0, 0, 0, OCI_DEFAULT));
        }


      }
      else if (path == 3) { /* delete transaction */
        id = delete_start;
        nb = delete_present++;

        if (delete_present == key_cnt){
          delete_present = 0;
          delete_start += insert_mod;
        }
	if (jj ==0) {
	  checkerr(errhp, ITA_OCIStmtExecute(svchp, stmthp4, errhp, 1, 0, 0, 0, OCI_COMMIT_ON_SUCCESS)); 
        }
	else {
	  checkerr(errhp, ITA_OCIStmtExecute(svchp, stmthp4, errhp, 1, 0, 0, 0, OCI_DEFAULT)); 
        }
      }

      else{/* insert transaction */
	
	id = insert_start;
	nb = insert_present++;
        sprintf((char *)last,"000000000");
	sprintf((char *)directory,"55%d%d",id,nb);
	sprintf((char *)descr,
		"<place holder for description of VPN %d extension %d>",id,nb);
	
	if (insert_present == key_cnt){
	  insert_present = 0;
	  insert_start += insert_mod;
	}
	
	if (jj ==0) {
	  checkerr(errhp, ITA_OCIStmtExecute(svchp, stmthp3, errhp, 1, 0, 0, 0, OCI_COMMIT_ON_SUCCESS)); 
        }
	else {
	  checkerr(errhp, ITA_OCIStmtExecute(svchp, stmthp3, errhp, 1, 0, 0, 0, OCI_DEFAULT)); 
        }
      }
    }		
  }

#ifndef _WIN32
    if (num_processes > 1) {
      
    /* each process indicates completion of run */ 
      shmhdr[shmpos] = 0;
      
    if (ami_parent) {
	  for(i=1; i<num_processes; i++) {
	  
	    /* code to check if any children are dead */
	    if (waitpid(pidArr[i],NULL,WNOHANG) != pidArr[i]){
	  
	    /* main/parent  process waits for all sub-processes to complete */
	    
	    while(shmhdr[i] == 1)
	      sleep(1);
	  }
	}
      }
    }
    
#else
    if ( (num_processes > 1) || (dummy != 0) ){
    /* each process indicates completion of run */ 
      shmhdr[shmpos] = 0;
      
      if ((dummy == 0) && (num_processes != 1)) {
	    for(i = 1; i < num_processes;i++) {
	    /* code to check if any children are dead */
	  
	  if (   GetExitCodeProcess( (HANDLE)pidArr[i], &exitcode)
              && (exitcode == STILL_ACTIVE)
             ) 
          {
	    /* main/parent  process waits for all sub-processes to complete */
	    while(shmhdr[i] == 1)
	      sleep(1);
	  }
	}
      }
    }

#endif

  /* time at the end of the run */

#ifdef _WIN32
  _ftime(&main_end);
#else
  err = gettimeofday(&main_end,NULL);
  (void) times (&tfinish);

  ticks = (double)sysconf(_SC_CLK_TCK);   
  stime = (tfinish.tms_stime - tstart.tms_stime) / ticks;
  utime = (tfinish.tms_utime - tstart.tms_utime) / ticks;
  cutime = (tfinish.tms_cutime - tstart.tms_cutime) / ticks;
  cstime = (tfinish.tms_cstime - tstart.tms_cstime) / ticks;
#endif

  if ((repl) && ((ami_parent) || ((num_processes == 1) && (dummy <= 0)))) {
    id = key_cnt;
    nb = key_cnt;
    printf("done delete\n");fflush(stderr);
    checkerr(errhp, OCIStmtExecute(svchp, stmthp4, errhp, 1, 0, 0, 0, OCI_COMMIT_ON_SUCCESS));
    id = key_cnt;
    nb = key_cnt;
    sprintf((char *)directory,"%d",num_xacts*num_processes);
    sprintf((char *)descr,"%d@%d",num_xacts,num_processes);
    sprintf((char *)last,"done");
    checkerr(errhp, OCIStmtExecute(svchp, stmthp3, errhp, 1, 0, 0, 0, OCI_COMMIT_ON_SUCCESS));
    num_xacts += 2;
  }

  /* compute transactions per second, handling zero time difference case */
  time_diff = diff_time(&main_start,&main_end);
  if (time_diff == 0) 
    time_diff = 1000; /* 1 second minimum difference */
  tps = num_processes * ((double)num_xacts/time_diff) * 1000;

  duration_secs = time_diff / (double) 1000;

  if ( time_diff < 1000 ) {
    duration = time_diff;
    strcpy(duration_units, "milli-seconds");
  } else {
    duration = duration_secs;
    strcpy(duration_units, "seconds");
  }


  /* print this message only in the main/parent process */
  if ((ami_parent) || ((num_processes == 1) && (dummy <= 0))) {

    if (multiop) {

      minSqlPerTxn = 5 * min_xact;
      maxSqlPerTxn = 5 * max_xact;

    } else {

      minSqlPerTxn = min_xact;
      maxSqlPerTxn = max_xact;
    }
	  if (min_xact == max_xact) {

             printf("\nTransactions:     %12.1d <%d> SQL operations per txn\n"
		        "Elapsed time:     %12.1f %s\n"
                "Transaction rate: %12.1f transactions/second\n"
			    "Transaction rate: %12.1f transactions/minute\n\n"
	            , num_xacts, maxSqlPerTxn,
	            duration, duration_units, tps, tps*60
               );

	  } else {

             printf("\nTransactions:     %12.1d <%d to %d> SQL operations per txn\n"
		        "Elapsed time:     %12.1f %s\n"
                "Transaction rate: %12.1f transactions/second\n"
			    "Transaction rate: %12.1f transactions/minute\n\n"
	            , num_xacts, minSqlPerTxn, maxSqlPerTxn,
	            duration, duration_units, tps, tps*60
               );

	  }

  }
  
 failExit:

#ifndef _WIN32
  /* parent process removes shared memory segment */
  if (ami_parent) {
    shmdt((char *)shmhdr);
    shmctl(shmid,IPC_RMID,NULL);
  }
  else
    shmdt((char *)shmhdr);
#else
  /* in the NT case, if more than one process unmap view */

  if ((dummy == 0) && (num_processes > 1)) {
    UnmapViewOfFile(shmhdr);
    CloseHandle(han);
  }
#endif

  checkerr(errhp, OCITransCommit(svchp, errhp, 0));

  if (!no_build && ami_parent) {
     checkerr(errhp, OCISessionEnd(pop_svchp,errhp,pop_authp,OCI_DEFAULT));

     checkerr(errhp, OCIServerDetach(pop_srvhp, errhp, OCI_DEFAULT));


     checkerr(errhp, OCIHandleFree((dvoid *) pop_authp, OCI_HTYPE_SESSION));

     checkerr(errhp, OCIHandleFree((dvoid *) pop_svchp, OCI_HTYPE_SVCCTX));

     checkerr(errhp, OCIHandleFree((dvoid *) pop_srvhp, OCI_HTYPE_SERVER));

     OCIHandleFree((dvoid *) pop_srvhp, OCI_HTYPE_ERROR);

     OCIHandleFree((dvoid *) pop_srvhp, OCI_HTYPE_ENV);
  }

  checkerr(errhp, OCIHandleFree((dvoid *) stmthp1, OCI_HTYPE_STMT));

  checkerr(errhp, OCIHandleFree((dvoid *) stmthp2, OCI_HTYPE_STMT));

  checkerr(errhp, OCIHandleFree((dvoid *) stmthp3, OCI_HTYPE_STMT));
  checkerr(errhp, OCIHandleFree((dvoid *) stmthp4, OCI_HTYPE_STMT));

  checkerr(errhp, OCISessionEnd(svchp,errhp,authp,OCI_DEFAULT));

  checkerr(errhp, OCIServerDetach(srvhp, errhp, OCI_DEFAULT));

  checkerr(errhp, OCIHandleFree((dvoid *) authp, OCI_HTYPE_SESSION));

  checkerr(errhp, OCIHandleFree((dvoid *) svchp, OCI_HTYPE_SVCCTX));
  
  checkerr(errhp, OCIHandleFree((dvoid *) srvhp, OCI_HTYPE_SERVER));
  
  OCIHandleFree((dvoid *) srvhp, OCI_HTYPE_ERROR);

  OCIHandleFree((dvoid *) srvhp, OCI_HTYPE_ENV);
  
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#define	TYPE_3		3		/* x**31 + x**3 + 1 */
#define	BREAK_3		128
#define	DEG_3		31
#define	SEP_3		3

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
  i = (*fptr >> 1) & 0x7fffffff;	/* chucking least random bit */
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

/* end of file tptbmOCI.c */
