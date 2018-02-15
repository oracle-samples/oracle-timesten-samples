/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*

NAME
plsqlOCI.c - OCI demo that uses PLSQL

DESCRIPTION
This OCI program does the following:
1. Uses a PLSQL package procedure
2. Uses a PLSQL package function
3. Uses a PLSQL anonymous block with host variables
4. Opens a Ref cursor in a PLSQL procedure
   and processes the resultset in OCI

EXPORT FUNCTION(S)
<external functions defined for use outside package - one-line descriptions>
  doConnect()  - Connect to TimesTen or Oracle DB 
  doPrepares() - Prapare all statements
  doBinds()    - Bind the PLSQL IN and OUT parameters
  doExecute()  - Execute the PLSQL and OCI
  doShutdown() - Disconnect and free resources

INTERNAL FUNCTION(S)
<other external functions defined - one-line descriptions>

STATIC FUNCTION(S)
<static functions defined - one-line descriptions>

NOTES
<other useful comments, qualifications, etc.>

 */

/* 
  Use the TimesTen Quick Start for instructions to build and run this program.
*/

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


#define checkerr(x,y) \
do {\
  if (checkerrReally(__LINE__,x,y)) { doShutdown(); } \
} while (0);


extern int chg_echo(int echo_on);
static int checkerrReally(int line, OCIError *errhp, sword status);
void doConnect(int argc, char** argv);
void doPrepares( void );
void doBinds( void );
void doExecute( void );
void doShutdown( void );
void parse_args(int argc,char **argv);


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

int         srvlen = 0;
int         usernamelen = 0;
int         passwordlen = 0;

int         numEmps = 5;     /* The number of lowest paid employees to choose from */
int         empNo   = 0;     /* The emnployee ID of interest */
char        luckyEmp[10 + 1];    /* The employee who got a 10% payrise */

char        jobtype[ 9 + 1];
char        hired[  20 + 1];
double      salary;
int         dept;
int         worked_longer;
int         higher_sal;
int         total_in_dept;
sword       retCode = 0;

int            errCode = 0;
char           errText[256];

OCIEnv    *envhp = NULL;
OCIServer *srvhp = NULL;
OCISvcCtx *svchp = NULL;
OCIError  *errhp = NULL;
OCISession *authp = (OCISession *) NULL;

OCIStmt *stmtpl             = NULL;
OCIStmt *stmtplfn           = NULL;
OCIStmt *stmtOpenRefCursor  = NULL;
OCIStmt *stmtUseRefCursor   = NULL;
OCIStmt *stmtAnonBlock      = NULL;
OCIBind *bndpl1   = NULL, *bndpl2   = NULL, *bndpl3   = NULL, *bndpl4   = NULL;
OCIBind *bndplfn1 = NULL, *bndplfn2 = NULL, *bndplfn3 = NULL, *bndplfn4 = NULL;
OCIBind *bndplrc1 = NULL, *bndplrc2 = NULL, *bndplrc3 = NULL;
OCIBind *bndanonpl1  = NULL;
OCIBind *bndanonpl2  = NULL;
OCIBind *bndanonpl3  = NULL;
OCIBind *bndanonpl4  = NULL;
OCIBind *bndanonpl5  = NULL;
OCIBind *bndanonpl6  = NULL;
OCIBind *bndanonpl7  = NULL;
OCIBind *bndanonpl8  = NULL;
OCIBind *bndanonpl9  = NULL;
OCIBind *bndanonpl10 = NULL;


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
      (void) printf("Error - OCI_NODATA, line %d\n", line); 
      break;

    case OCI_ERROR:
      (void) OCIErrorGet(errhp, (ub4) 1, (text *) NULL, &errcode,
                         errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
      
      if (LOCK_TIMEOUT == errcode) {
                printf("Got a lock timeout for the update or delete:\n");
                printf("- Multiple connections were trying to modify the same row(s).\n");
                printf("- Increase the possible rows (-key) and/or decrease the concurrency (-proc).\n\n");
        break;

      } else if (DB_OUT_OF_SPACE == errcode) {
                (void) printf("  Increase the database size by making the PermSize attribute bigger\n");
                (void) printf("  The PermSize (in MB) must fit within physical RAM\n\n");
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

      printf("Process %ld, exiting after too many errors (%d)\n\n", (long) myPid, error_count);
      exit(1);
  }
  return 0;
}


/*
 *  Exit program with an exit code.
 */
void do_exit(sword exit_code)
{
 
  exit(exit_code);
}



/* SQL statements used in this program */

char * exec_plsql_stmnt =
  "begin \
    emp_pkg.givePayRaise(:numEmps, :luckyEmp, :errCode, :errText); \
   end;";

char * exec_plsql_fn =
  "begin \
     :luckyEmp := sample_pkg.getEmpName(:empNo, :errCode, :errText); \
   end;";

char * exec_OpenSalesPeopleCursor =
  "begin \
     emp_pkg.OpenSalesPeopleCursor(:cmdRefCursor, :errCode, :errText); \
   end;";


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
  /* Print the usage message */

  printf("\nUsage:\n"
          "  %s [-h] [-help] [-V] \n"
          "  %s [-user <user>] [-password <password>]\n"
          "             [-service <tnsServiceName>]\n\n"
          "  -h                     Prints this message and exits.\n"
          "  -help                  Same as -h.\n"
          "  -V                     Prints the version number and exits.\n"
          "  -user                  Specify DB username.\n"
          "                         Default username is %s\n"
          "  -password              Specify DB username.\n"
          "                         There is no default password.\n"
          "  -service               Specify the Oracle Net service name.\n"
          "                         The service name can point to Direct Linked or\n"
          "                         Client/Server TimesTen DSNs or to an Oracle database.\n"
          "                         Default service name is %s\n",
          prog, prog, UIDNAME, DEMODSN );

  printf("\nExample :\n");
  printf("  scott@myService\n\n");
  printf("  plsqlOCI -user scott -service myService\n\n");

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

  /* Initialize the service name */
  memset(service, sizeof(service), 0);

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
        memset(service, sizeof(service), 0);

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
        memset(username, sizeof(username), 0);

        /* Get the username */
        strcpy(username, argv[i+1]);

      }
      i += 2;
   } else if (strcmp("-password",argv[i]) == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        exit(1);
      }
      passwordlen = strlen (argv[i+1]);

      if (passwordlen >= MAX_PASSWORD_SIZE - 1) {
        printf("\nThe password [%d characters] was too long\n", passwordlen);
        usage(argv[0]);
        exit(1);

      } else {

        /* Initialize the password */
        memset(password, sizeof(password), 0);

        /* Get the password */
        strcpy(password, argv[i+1]);

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

  /* prompt for the password */
  if (0 == passwordlen) {

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


void doBinds( void )
{

  /* Bind the IN and OUT parameters for the stored procedure */
  checkerr(errhp, OCIBindByPos(stmtpl, 
                               &bndpl1, 
                               errhp, 
                               1, 
                               &numEmps, 
                               sizeof(numEmps), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtpl, 
                               &bndpl2, 
                               errhp, 
                               2, 
                               luckyEmp, 
                               sizeof(luckyEmp), 
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtpl, 
                               &bndpl3, 
                               errhp, 
                               3, 
                               &errCode, 
                               sizeof(errCode), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtpl, 
                               &bndpl4, 
                               errhp, 
                               4, 
                               &errText, 
                               sizeof(errText), 
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));


  /* Bind the PLSQL function */
  checkerr(errhp, OCIBindByPos(stmtplfn, 
                               &bndplfn1, 
                               errhp, 
                               1, 
                               luckyEmp, 
                               sizeof(luckyEmp), 
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtplfn, 
                               &bndplfn2, 
                               errhp, 
                               2, 
                               &empNo, 
                               sizeof(empNo), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtplfn, 
                               &bndplfn3, 
                               errhp, 
                               3, 
                               &errCode, 
                               sizeof(errCode), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtplfn, 
                               &bndplfn4, 
                               errhp, 
                               4, 
                               &errText, 
                               sizeof(errText), 
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));


  /* Bind the PLSQL anon block IN and OUT params */
  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl1, 
                               errhp, 
                               1, 
                               jobtype,
                               sizeof(jobtype),
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl2, 
                               errhp, 
                               2, 
                               hired,
                               sizeof(hired),
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl3, 
                               errhp, 
                               3, 
                               &salary,
                               sizeof(salary),
                               SQLT_FLT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl4, 
                               errhp, 
                               4, 
                               &dept, 
                               sizeof(dept), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl5, 
                               errhp, 
                               5, 
                               luckyEmp,
                               sizeof(luckyEmp),
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));


  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl6, 
                               errhp, 
                               6, 
                               &worked_longer, 
                               sizeof(worked_longer), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl7, 
                               errhp, 
                               7, 
                               &higher_sal, 
                               sizeof(higher_sal), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl8, 
                               errhp, 
                               8, 
                               &total_in_dept, 
                               sizeof(total_in_dept), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl9, 
                               errhp, 
                               9, 
                               &errCode, 
                               sizeof(errCode), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtAnonBlock, 
                               &bndanonpl10, 
                               errhp, 
                               10, 
                               &errText, 
                               sizeof(errText), 
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));


  /* Bind the PLSQL OpenSalesPeopleCursor OUT params */
  checkerr(errhp, OCIBindByPos(stmtOpenRefCursor, 
                               &bndplrc1, 
                               errhp, 
                               1, 
                               (dvoid*) &stmtUseRefCursor, /* the returned ref cusror */
                               0,
                               SQLT_RSET, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtOpenRefCursor, 
                               &bndplrc2, 
                               errhp, 
                               2, 
                               &errCode, 
                               sizeof(errCode), 
                               SQLT_INT, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  checkerr(errhp, OCIBindByPos(stmtOpenRefCursor, 
                               &bndplrc3, 
                               errhp, 
                               3, 
                               &errText, 
                               sizeof(errText), 
                               SQLT_STR, 
                               (dvoid *) 0,
                               (ub2 *) 0,
                               (ub2) 0,
                               (ub4) 0,
                               (ub4 *) 0,
                               OCI_DEFAULT));

  /* Commit the prepares and binds */
  checkerr(errhp, OCITransCommit(svchp, errhp, 0));

}  /* do Binds */


void doConnect(int argc, char** argv)
{

  /* Clear the password */
  memset(password, 0, sizeof(password));

  /* parse the command line arguments */
  parse_args(argc, argv);


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

}


void doPrepares( void )
{
  char anon_plsqlblock[2048];


  /* Initialize the input and output parameters for the plsql block */
  memset(anon_plsqlblock, 0, sizeof(anon_plsqlblock));
  
  /* Create the text for the PLSQL anonymous block */
  strcpy(anon_plsqlblock, "BEGIN");

  strcat(anon_plsqlblock, "  SELECT job, hiredate, sal, deptno");
  strcat(anon_plsqlblock, "    INTO :jobtype, :hired, :salary, :dept");
  strcat(anon_plsqlblock, "  FROM emp");
  strcat(anon_plsqlblock, "  WHERE ename = UPPER(:luckyEmp);");

  strcat(anon_plsqlblock, "  SELECT count(*)");
  strcat(anon_plsqlblock, "    INTO :worked_longer");
  strcat(anon_plsqlblock, "  FROM emp");
  strcat(anon_plsqlblock, "  WHERE hiredate < :hired;");

  strcat(anon_plsqlblock, "  SELECT count(*)");
  strcat(anon_plsqlblock, "    INTO :higher_sal");
  strcat(anon_plsqlblock, "  FROM emp");
  strcat(anon_plsqlblock, "  WHERE sal > :salary;");

  strcat(anon_plsqlblock, "  SELECT count(*)");
  strcat(anon_plsqlblock, "    INTO :total_in_dept");
  strcat(anon_plsqlblock, "  FROM emp");
  strcat(anon_plsqlblock, "  WHERE deptno = :dept;");

  strcat(anon_plsqlblock, " EXCEPTION");

  strcat(anon_plsqlblock, "    WHEN OTHERS THEN");
  strcat(anon_plsqlblock, "       :errCode  := SQLCODE;");
  strcat(anon_plsqlblock, "       :errText  := SUBSTR(SQLERRM, 1, 200);");

  strcat(anon_plsqlblock, " END;");


  /* Alocate a handle for executing the stored procedure */
  checkerr(errhp, OCIHandleAlloc((dvoid *)
                                  envhp,
                                  (dvoid **)
                                  &stmtpl, 
                                  OCI_HTYPE_STMT,
                                  0,
                                  (dvoid **) NULL));

  /* Alocate a handle for executing the stored function */
  checkerr(errhp, OCIHandleAlloc((dvoid *) envhp,
                                 (dvoid **) &stmtplfn, 
                                 OCI_HTYPE_STMT,
                                 0,
                                 (dvoid **) NULL));

  /* Alocate a handle for executing the PLSQL anonymous block */
  checkerr(errhp, OCIHandleAlloc((dvoid *) envhp,
                                 (dvoid **) &stmtAnonBlock, 
                                 OCI_HTYPE_STMT,
                                 0,
                                 (dvoid **) NULL));

  /* Alocate a handle for executing the stored procedure to open the ref cursor */
  checkerr(errhp, OCIHandleAlloc((dvoid *) envhp,
                                 (dvoid **) &stmtOpenRefCursor, 
                                 OCI_HTYPE_STMT,
                                 0,
                                 (dvoid **)
                                 NULL));

  /* Alocate a handle for using the ref cursor */
  checkerr(errhp, OCIHandleAlloc((dvoid *) envhp,
                                 (dvoid **) &stmtUseRefCursor, 
                                 OCI_HTYPE_STMT,
                                 0,
                                 (dvoid **)
                                 NULL));


  /* Prepare the PLSQL block which calls the store procedure */
  checkerr(errhp, OCIStmtPrepare(stmtpl, 
                                 errhp, 
                                 (text *) exec_plsql_stmnt,
                                 (ub4) strlen(exec_plsql_stmnt), 
                                 OCI_NTV_SYNTAX, 
                                 OCI_DEFAULT));

  /* Prepare the anonymous PLSQL block */
  checkerr(errhp, OCIStmtPrepare(stmtAnonBlock, 
                                 errhp, 
                                 (text *) anon_plsqlblock,
                                 (ub4) strlen(anon_plsqlblock), 
                                 OCI_NTV_SYNTAX, 
                                 OCI_DEFAULT));

  /* Prepare the PLSQL block which calls the stored function */
  checkerr(errhp, OCIStmtPrepare(stmtplfn, 
                                 errhp, 
                                 (text *) exec_plsql_fn,
                                 (ub4) strlen(exec_plsql_fn), 
                                 OCI_NTV_SYNTAX, 
                                 OCI_DEFAULT));

  /* Prepare the PLSQL block which opens the ref cursor */
  checkerr(errhp, OCIStmtPrepare(stmtOpenRefCursor, 
                                 errhp, 
                                 (text *) exec_OpenSalesPeopleCursor,
                                 (ub4) strlen(exec_OpenSalesPeopleCursor), 
                                 OCI_NTV_SYNTAX, 
                                 OCI_DEFAULT));

}


void doExecute( void )
{
  OCIDefine  *defnEmpNo  = (OCIDefine *) 0;
  OCIDefine  *defnEname  = (OCIDefine *) 0;
  OCIDefine  *defnSalary = (OCIDefine *) 0;

  sword       rc = 0;


  /* Initialize the input and output variables for the emp_pkg.givePayRaise procedure */
  memset(luckyEmp, 0, sizeof(luckyEmp));
  errCode =  0;
  numEmps = 10;
  strcpy(errText, "OK");

  /* execute the emp_pkg.givePayRaise procedure */
  checkerr(errhp, OCIStmtExecute(svchp, stmtpl, errhp, 1, 0, 0, 0, OCI_DEFAULT));

  if (errCode != 0) {

    printf("\nociplsql error (%d) : %s\n", errCode, errText);

  } else {

    printf("\nThe employee who got a 10%% pay raise was %s\n", luckyEmp);
  }

  /* Initialize the input and output variables for the anonymous PLSQL block */
  memset(jobtype, 0, sizeof(jobtype));
  memset(hired,   0, sizeof(hired));
  errCode       = 0;
  dept          = 0;
  worked_longer = 0;
  higher_sal    = 0;
  total_in_dept = 0;
  salary        = 0.0;
  strcpy(errText, "OK");

  checkerr(errhp, OCIStmtExecute(svchp, stmtAnonBlock, errhp, 1, 0, 0, 0, OCI_DEFAULT));

  if (0 == errCode)
   {

      /* Display the employee's information */
      printf("\n");
      printf("Name:        %s\n", luckyEmp);
      printf("Job:         %s\n", jobtype);
      printf("Hired:       %s\t(%2d people have served longer).\n", hired, worked_longer);
      printf("Salary:      $%.2f\t\t\t(%2d people have a higher salary).\n", salary, higher_sal);
      printf("Department:  %d\t\t\t\t(%2d people in the department).\n\n", dept, total_in_dept);
  
   }
   else
   {
      printf("A problem with plsqlblock:");
      printf("  Error: %d\n", errCode);
      printf("  Error Text: %s\n", errText);
   }

  /* Initialize the input and output variables for the sample_pkg.getEmpName function */
  memset(luckyEmp, 0, sizeof(luckyEmp));
  errCode = 0;
  empNo   = 7839;
  strcpy(errText, "OK");

  /* execute the sample_pkg.getEmpName function */
  checkerr(errhp, OCIStmtExecute(svchp, stmtplfn, errhp, 1, 0, 0, 0, OCI_DEFAULT));

  if (errCode != 0) {

    printf("\nociplsql error (%d) : %s\n", errCode, errText);
    return;

  } else {

    printf("The employee for empid %d is %s\n\n", empNo, luckyEmp);
  }


  /* Initialize the Output variables for the emp_pkg.OpenSalesPeopleCursor procedure */
  memset(luckyEmp, 0, sizeof(luckyEmp));
  memset(errText,  0, sizeof(errText));
  errCode = 0;
  empNo =   0;
  salary =  0.0;

  /* Call the emp_pkg.OpenSalesPeopleCursor procedure */
  checkerr(errhp, OCIStmtExecute(svchp, 
                  stmtOpenRefCursor, 
                  errhp, 
                  1, 
                  0, 
                  0, 
                  0, 
                  OCI_DEFAULT));

  if (errCode != 0) {

    printf("\nociplsql error (%d) : %s\n", errCode, errText);

  } else {

    /* define the output variables for the ref cursor */
    checkerr(errhp, OCIDefineByPos(stmtUseRefCursor, 
                                   &defnEmpNo, 
                                   errhp,
                                   (ub4) 1, 
                                   (dvoid *) &empNo,
                                   (sb4) sizeof(empNo), 
                                   SQLT_INT, 
                                   (dvoid *) 0,
                                   (ub2 *)0,
                                   (ub2 *)0, 
                                   (ub4) OCI_DEFAULT));

    checkerr(errhp, OCIDefineByPos(stmtUseRefCursor, 
                                   &defnEname, 
                                   errhp,
                                   (ub4) 2, 
                                   (dvoid *) luckyEmp,
                                   (sb4) sizeof(luckyEmp), 
                                   SQLT_STR, 
                                   (dvoid *) 0,
                                   (ub2 *)0,
                                   (ub2 *)0, 
                                   (ub4) OCI_DEFAULT));

    checkerr(errhp, OCIDefineByPos(stmtUseRefCursor, 
                                   &defnSalary, 
                                   errhp,
                                   (ub4) 3, 
                                   (dvoid *) &salary,
                                   (sb4) sizeof(salary), 
                                   SQLT_FLT, 
                                   (dvoid *) 0,
                                   (ub2 *)0,
                                   (ub2 *)0, 
                                   (ub4) OCI_DEFAULT));

    /* Iterate over the ref cursors result set */
    printf("The sales people are:\n\n");
    printf("  EMPNO ENAME      SALARY\n");
    printf("  ===== ========== ==========\n");

    while ((rc = OCIStmtFetch(stmtUseRefCursor, 
                              errhp, 
                              (ub4) 1, 
                              (ub4) OCI_FETCH_NEXT,
                              (ub4) OCI_DEFAULT)) == OCI_SUCCESS ||
            rc == OCI_SUCCESS_WITH_INFO)
    { 
      printf("  %5.0d %-10s %9.2f\n", empNo, luckyEmp, salary);
    }

    if ( rc != OCI_NO_DATA ) {
      checkerr(errhp, (sword) rc);
    }
  
    /* close the ref cursor */
    checkerr(errhp, OCIHandleFree((dvoid *) stmtUseRefCursor, 
                                  (ub4) OCI_HTYPE_STMT));

  }

}

void doShutdown( void )
{

  printf("\nDisconnecting\n");
  checkerr(errhp, OCITransCommit(svchp, errhp, 0));

  checkerr(errhp, OCIHandleFree((dvoid *) stmtpl,            OCI_HTYPE_STMT));
  checkerr(errhp, OCIHandleFree((dvoid *) stmtplfn,          OCI_HTYPE_STMT));
  checkerr(errhp, OCIHandleFree((dvoid *) stmtOpenRefCursor, OCI_HTYPE_STMT));
  checkerr(errhp, OCIHandleFree((dvoid *) stmtAnonBlock,     OCI_HTYPE_STMT));

  checkerr(errhp, OCISessionEnd(svchp,errhp,authp,OCI_DEFAULT));

  checkerr(errhp, OCIServerDetach(srvhp, errhp, OCI_DEFAULT));

  checkerr(errhp, OCIHandleFree((dvoid *) authp, OCI_HTYPE_SESSION));

  checkerr(errhp, OCIHandleFree((dvoid *) svchp, OCI_HTYPE_SVCCTX));

  checkerr(errhp, OCIHandleFree((dvoid *) srvhp, OCI_HTYPE_SERVER));

  OCIHandleFree((dvoid *) srvhp, OCI_HTYPE_ERROR);

  OCIHandleFree((dvoid *) srvhp, OCI_HTYPE_ENV);

  printf("Disconnected\n\n");

}

/*********************************************************************
 *
 *  FUNCTION:       main
 *
 *  DESCRIPTION:    This is the main function of the ociplsql program.
 *                  It just calls a package function and a package procedure.
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
  
  /* Connect to TimesTen or Oracle DB */
  doConnect(argc, argv);

  /* doPrepares */
  doPrepares();

  /* Bind the PLSQL procedure and function parameters  */
  doBinds();

  /* Execute the PLSQL procedures and functions */
  doExecute();

  /* Disconnect and free resources */
  doShutdown();

  return 0;
}  /* main */


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
