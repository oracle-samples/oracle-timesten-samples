/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*

   NAME
     addemp.c
     Improvements to cdemo81.c - Basic OCI V8 functionality

   DESCRIPTION

 *  An example program which adds new employee
 *  records to the personnel data base.  Checking
 *  is done to insure the integrity of the data base.
 *  The employee numbers are automatically selected using
 *  the current maximum employee number as the start.
 *
 *  The program queries the user for data as follows:
 *
 *  Enter employee name:
 *  Enter employee job:
 *  Enter employee salary:
 *  Enter employee dept:
 *
 *  The program terminates if return key (CR) is entered
 *  when the employee name is requested.
 *
 *  If the record is successfully inserted, the following
 *  is printed:
 *
 *  "ename" added to department "dname" as employee # "empno"

   Demonstrates creating a connection, a session and executing some SQL.
   Also shows the usage of allocating memory for application use which has the
   life time of the handle.

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "tt_version.h"

#include <oci.h>

#define STRING_LEN	255
#define SERVICE_SIZE      256

/* Get the connnection info from the user */
char username[ MAX_USERNAME_SIZE ];
char password[ MAX_PASSWORD_SIZE ];
char service[  SERVICE_SIZE ];


/* Define SQL statements to be used in program. */
static text *insert = (text *)"INSERT INTO\
 emp(empno, ename, job, hiredate, sal, deptno)\
 VALUES (:empno, :ename, :job, sysdate, :sal, :deptno)";
static text *seldept = (text *)"SELECT dname FROM dept WHERE deptno = :1";
static text *maxemp = (text *)"SELECT NVL(MAX(empno), 0) FROM emp";

static OCIEnv *envhp;
static OCIError *errhp;
static  OCISession *authp = (OCISession *) 0;
static  OCIServer *srvhp;
static  OCISvcCtx *svchp;
static sword status;

int         srvlen = 0;
int         usernamelen = 0;
int         passwordlen = 0;

static void checkerr(OCIError *errhp, sword status);
static void cleanup( void );
static void myfflush( void );
void usage(char *prog);
void parse_args(int argc,char **argv);

extern int chg_echo(int echo_on);


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
          "  %s [-user <user>] [-password <password>] [-service <tnsServiceName>]\n\n"
          "  -h                     Prints this message and exits.\n"
          "  -help                  Same as -h.\n"
          "  -V                     Prints the version number and exits.\n"
          "  -user                  Specify a database username.\n"
          "                         Default username is %s\n"
          "  -password              Specify password for user.\n"
          "  -service               Specify the Oracle Net service name.\n"
          "                         The service name can point to Direct Linked or\n"
          "                         Client/Server TimesTen DSNs or to an Oracle database.\n"
          "                         Default service name is %s\n",
          prog, prog, UIDNAME, DEMODSN
   );

  printf("\nExample :\n");
  printf("  Connect as appuser@sampledb\n\n");
  printf("  %s -user appuser -service sampledb\n\n", prog);

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
    }
    else if (strcmp("-password",argv[i]) == 0) {

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

        /* Initialize the username */
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
}


int main(int argc, char* argv[])
{

  sword    empno, sal, deptno;
  sb4      enamelen = 10;
  sb4      joblen = 9;
  sb4      deptlen = 14;
  sb2      sal_ind, job_ind;
  text     *cp, *ename, *job, *dept;

  sword    retCode = 0;

  OCIStmt   *inserthp,
            *stmthp,
            *stmthp1;
  OCIDefine *defnp = (OCIDefine *) 0;

  OCIBind  *bnd1p = (OCIBind *) 0;             /* the first bind handle */
  OCIBind  *bnd2p = (OCIBind *) 0;             /* the second bind handle */
  OCIBind  *bnd3p = (OCIBind *) 0;             /* the third bind handle */
  OCIBind  *bnd4p = (OCIBind *) 0;             /* the fourth bind handle */
  OCIBind  *bnd5p = (OCIBind *) 0;             /* the fifth bind handle */
  OCIBind  *bnd6p = (OCIBind *) 0;             /* the sixth bind handle */

  sword errcode = 0;


 /* Clear the password */
  memset(password, 0, sizeof(password));

  /* parse the command line arguments */
  parse_args(argc,argv);

  /* Don't prompt for password if given on cmdline */
  if (0 == password[0]) {
    printf("\nEnter password for %s : ", username);

    /* Turn off echo */
    chg_echo(0);

    /* Get the password */
    fgets(password, sizeof(password), stdin);

    /* Turn on echo */
    chg_echo(1);
    password[strlen((char *) password)-1] = '\0';
  }


  printf("\n\nTrying to connect using:\n   %s@%s\n", 
         username,
         service);



  errcode = OCIEnvCreate((OCIEnv **) &envhp, (ub4) OCI_DEFAULT,
                  (dvoid *) 0, (dvoid * (*)(dvoid *,size_t)) 0,
                  (dvoid * (*)(dvoid *, dvoid *, size_t)) 0,
                  (void (*)(dvoid *, dvoid *)) 0, (size_t) 0, (dvoid **) 0);

  if (errcode != 0) {
    (void) printf("OCIEnvCreate failed with errcode = %d.\n", errcode);
    exit(1);
  }

  (void) OCIHandleAlloc( (dvoid *) envhp, (dvoid **) &errhp, OCI_HTYPE_ERROR,
                   (size_t) 0, (dvoid **) 0);

  /* server contexts */
  (void) OCIHandleAlloc( (dvoid *) envhp, (dvoid **) &srvhp, OCI_HTYPE_SERVER,
                   (size_t) 0, (dvoid **) 0);

  (void) OCIHandleAlloc( (dvoid *) envhp, (dvoid **) &svchp, OCI_HTYPE_SVCCTX,
                   (size_t) 0, (dvoid **) 0);

  /* Connect to the target */
  retCode =  OCIServerAttach( srvhp, errhp, (text *) service, 
                              strlen((const char *) service), 0);

  if (-1 == retCode) {
    printf("\nThe following TNS Service Name or Easy Connect Name is invalid:\n   %s\n\n",
            service);
    exit( -1 );
  }

  /* set attribute server context in the service context */
  (void) OCIAttrSet( (dvoid *) svchp, OCI_HTYPE_SVCCTX, (dvoid *)srvhp,
                     (ub4) 0, OCI_ATTR_SERVER, (OCIError *) errhp);

  (void) OCIHandleAlloc((dvoid *) envhp, (dvoid **)&authp,
                        (ub4) OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) 0);

  /* Set the username */
  (void) OCIAttrSet((dvoid *) authp, (ub4) OCI_HTYPE_SESSION,
                 (dvoid *) username, (ub4) strlen((char *)username),
                 (ub4) OCI_ATTR_USERNAME, errhp);

  /* Set the password */
  (void) OCIAttrSet((dvoid *) authp, (ub4) OCI_HTYPE_SESSION,
                 (dvoid *) password, (ub4) strlen((char *)password),
                 (ub4) OCI_ATTR_PASSWORD, errhp);

  /* Try to authenicate on the target connection */
  checkerr(errhp, OCISessionBegin ( svchp,  errhp, authp, OCI_CRED_RDBMS,
                          (ub4) OCI_DEFAULT));

  (void) OCIAttrSet((dvoid *) svchp, (ub4) OCI_HTYPE_SVCCTX,
                   (dvoid *) authp, (ub4) 0,
                   (ub4) OCI_ATTR_SESSION, errhp);

  checkerr(errhp, OCIHandleAlloc( (dvoid *) envhp, (dvoid **) &stmthp,
           OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0));

  checkerr(errhp, OCIHandleAlloc( (dvoid *) envhp, (dvoid **) &stmthp1,
           OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0));

  /* Retrieve the current maximum employee number. */
  checkerr(errhp, OCIStmtPrepare(stmthp, errhp, maxemp,
                                (ub4) strlen((char *) maxemp),
                                (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT));

  /* bind the input variable */
  checkerr(errhp, OCIDefineByPos(stmthp, &defnp, errhp, 1, (dvoid *) &empno,
                   (sword) sizeof(sword), SQLT_INT, (dvoid *) 0, (ub2 *)0,
                   (ub2 *)0, OCI_DEFAULT));

  /* execute and fetch */
  if ((status = OCIStmtExecute(svchp, stmthp, errhp, (ub4) 1, (ub4) 0,
               (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT)))
  {
    if (status == OCI_NO_DATA)
      empno = 10;
    else
    {
      checkerr(errhp, status);
      cleanup();
      return OCI_ERROR;
    }
  }


  /*
   * When we bind the insert statement we also need to allocate the storage
   * of the employee name and the job description.
   * Since the lifetime of these buffers are the same as the statement, we
   * will allocate it at the time when the statement handle is allocated; this
   * will get freed when the statement disappears and there is less
   * fragmentation.
   *
   * sizes required are enamelen+2 and joblen+2 to allow for \n and \0
   *
   */


  checkerr(errhp, OCIHandleAlloc( (dvoid *) envhp, (dvoid **) &inserthp,
           OCI_HTYPE_STMT, (size_t) enamelen + 2 + joblen + 2,
           (dvoid **) &ename));
  job   = (text *) (ename+enamelen+2);


  checkerr(errhp, OCIStmtPrepare(stmthp, errhp, insert,
                                (ub4) strlen((char *) insert),
                                (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT));

  checkerr(errhp, OCIStmtPrepare(stmthp1, errhp, seldept,
                                (ub4) strlen((char *) seldept),
                                (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT));


  /*  Bind the placeholders in the INSERT statement. */
  if ((status = OCIBindByName(stmthp, &bnd1p, errhp, (text *) ":ENAME",
             -1, (dvoid *) ename,
             enamelen+1, SQLT_STR, (dvoid *) 0,
             (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT)) ||
      (status = OCIBindByName(stmthp, &bnd2p, errhp, (text *) ":JOB",
             -1, (dvoid *) job,
             joblen+1, SQLT_STR, (dvoid *) &job_ind,
             (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT)) ||
      (status = OCIBindByName(stmthp, &bnd3p, errhp, (text *) ":SAL",
             -1, (dvoid *) &sal,
             (sword) sizeof(sal), SQLT_INT, (dvoid *) &sal_ind,
             (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT)) ||
      (status = OCIBindByName(stmthp, &bnd4p, errhp, (text *) ":DEPTNO",
             -1, (dvoid *) &deptno,
             (sword) sizeof(deptno), SQLT_INT, (dvoid *) 0,
             (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT)) ||
      (status = OCIBindByName(stmthp, &bnd5p, errhp, (text *) ":EMPNO",
             -1, (dvoid *) &empno,
             (sword) sizeof(empno), SQLT_INT, (dvoid *) 0,
             (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT)))
  {
    checkerr(errhp, status);
    cleanup();
    return OCI_ERROR;
  }

  /*  Bind the placeholder in the "seldept" statement. */
  if ((status = OCIBindByPos(stmthp1, &bnd6p, errhp, 1,
           (dvoid *) &deptno, (sword) sizeof(deptno),SQLT_INT,
           (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT)))
  {
    checkerr(errhp, status);
    cleanup();
    return OCI_ERROR;
  }

  /*  Allocate the dept buffer now that you have length. */
  /* the deptlen should eventually get from dschndl3.    */
  deptlen = 14;
  dept = (text *) malloc((size_t) deptlen + 1);

  /*  Define the output variable for the select-list. */
  if ((status = OCIDefineByPos(stmthp1, &defnp, errhp, 1,
                             (dvoid *) dept, deptlen+1, SQLT_STR,
                             (dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)))
  {
    checkerr(errhp, status);
    cleanup();
    return OCI_ERROR;
  }

  for (;;)
  {
    /* Prompt for employee name.  Break on no name. */
    printf("\nEnter employee name (or CR to EXIT): ");
    fgets((char *) ename, (int) enamelen+1, stdin);
    cp = (text *) strchr((char *) ename, '\n');
    if (cp == ename)
    {
      printf("Exiting... \n\n");
      cleanup();
      return OCI_SUCCESS;
    }
    if (cp)
      *cp = '\0';
    else
    {
      printf("Employee name may be truncated.\n");
      myfflush();
    }
    /*  Prompt for the employee's job and salary. */
    printf("Enter employee job: ");
    job_ind = 0;
    fgets((char *) job, (int) joblen + 1, stdin);
    cp = (text *) strchr((char *) job, '\n');
    if (cp == job)
    {
      job_ind = -1;            /* make it NULL in table */
      printf("Job is NULL.\n");/* using indicator variable */
    }
    else if (cp == 0)
    {
      printf("Job description may be truncated.\n");
      myfflush();
    }
    else
      *cp = '\0';

    printf("Enter employee salary: ");
    scanf("%d", &sal);
    myfflush();
    sal_ind = (signed short) ((sal <= 0) ? -2 : 0);  /* set indicator variable */

    /*
     *  Prompt for the employee's department number, and verify
     *  that the entered department number is valid
     *  by executing and fetching.
     */
    do
    {
      printf("Enter employee dept: ");
      scanf("%d", &deptno);
      myfflush();
      if ((status = OCIStmtExecute(svchp, stmthp1, errhp, (ub4) 1, (ub4) 0,
               (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT))
          && (status != OCI_NO_DATA))
      {
        checkerr(errhp, status);
        cleanup();
        return OCI_ERROR;
      }
      if (status == OCI_NO_DATA)
        printf("The dept you entered doesn't exist.\n");
      } while (status == OCI_NO_DATA);

      /*
       *  Increment empno by 10, and execute the INSERT
       *  statement. If the return code is 1 (duplicate
       *  value in index), then generate the next
       *  employee number.
       */
      empno += 10;
      if ((status = OCIStmtExecute(svchp, stmthp, errhp, (ub4) 1, (ub4) 0,
               (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT))
               && status != 1)
      {
        checkerr(errhp, status);
        cleanup();
        return OCI_ERROR;
      }
      while (status == 1)
      {
        empno += 10;
        if ((status = OCIStmtExecute(svchp, stmthp, errhp, (ub4) 1, (ub4) 0,
               (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT))
                && status != 1)
        {
          checkerr(errhp, status);
          cleanup();
          return OCI_ERROR;
        }
      }  /* end for (;;) */

      /* Commit the change. */
      if ((status = OCITransCommit(svchp, errhp, 0)))
      {
        checkerr(errhp, status);
        cleanup();
        return OCI_ERROR;
      }
      printf("\n\n%s added to the %s department as employee number %d\n",
                 (char *) ename, (char *) dept, empno);
  }
}


static void checkerr(OCIError *errhp, sword status)
{
  text errbuf[512];
  sb4 errcode = 0;

  switch (status)
  {
  case OCI_SUCCESS:
    break;
  case OCI_SUCCESS_WITH_INFO:
    (void) printf("Error - OCI_SUCCESS_WITH_INFO\n");
    break;
  case OCI_NEED_DATA:
    (void) printf("Error - OCI_NEED_DATA\n");
    break;
  case OCI_NO_DATA:
    (void) printf("Error - OCI_NODATA\n");
    break;
  case OCI_ERROR:
    (void) OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                        errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
    (void) printf("Error - %.*s\n", 512, (char *) errbuf);
    break;
  case OCI_INVALID_HANDLE:
    (void) printf("Error - OCI_INVALID_HANDLE\n");
    break;
  case OCI_STILL_EXECUTING:
    (void) printf("Error - OCI_STILL_EXECUTE\n");
    break;
  case OCI_CONTINUE:
    (void) printf("Error - OCI_CONTINUE\n");
    break;
  default:
    break;
  }
}


/*
 *  Exit program with an exit code.
 */
static void cleanup( void )
{
  sword retCode = 0;

  retCode = OCISessionEnd(svchp, errhp, authp, OCI_DEFAULT);
  if (retCode)
   printf("OCISessionEnd retCode = %d\n", retCode); 

  retCode = OCIServerDetach(srvhp, errhp, OCI_DEFAULT);
  if (retCode)
    printf("OCIServerDetach retCode = %d\n", retCode); 

  if (envhp)
    (void) OCIHandleFree((dvoid *) envhp, OCI_HTYPE_ENV);
  return;
}


static void myfflush( void )
{
  eb1 buf[50];

  fgets((char *) buf, 50, stdin);
}


/* end of file cdemo81.c */

