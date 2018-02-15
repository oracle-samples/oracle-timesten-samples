/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*

  NAME
    parseargs.c - <one-line expansion of the name>

  DESCRIPTION
    Parse the command line arguments and display usage() if incorrect

  EXPORT FUNCTION(S)
    parse_args()
    usage()

  INTERNAL FUNCTION(S)
    <other external functions defined - one-line descriptions>

  STATIC FUNCTION(S)
    <static functions defined - one-line descriptions>

  NOTES
    <other useful comments, qualifications, etc.>

  MODIFIED   (MM/DD/YY)
  cdjenkin    04/06/17 - Acadia QuickStart file
  ogeest      11/11/13 - Changed to sampledb
  ogeest      11/29/10 - Changed to sampledb_1122
  dhood       01/06/10 - Remove dead code
  ogeest      12/29/09 - Removed usage of stderr
  dhood       12/23/09 - add usage() and parse_args()
  dhood       12/23/09 - Creation

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tt_version.h"

/*---------------------------------------------------------------------------
                    PRIVATE TYPES AND CONSTANTS
 ---------------------------------------------------------------------------*/
               
#define SERVICE_SIZE   256

/*---------------------------------------------------------------------------
                    STATIC FUNCTION DECLARATIONS
 ---------------------------------------------------------------------------*/

/* Prototypes */
#if defined(__STDC__)
  void usage(char *prog);
  void parse_args(int argc, char **argv);
extern  int chg_echo(int echo_on);
#else
  void usage(char *);
  void parse_args(int, char **);
extern  int chg_echo(int echo_on);
#endif


char  username[MAX_USERNAME_SIZE];
char  password[MAX_PASSWORD_SIZE];
char  service[SERVICE_SIZE];

int   srvlen = 0;
int   usernamelen = 0;
int   passwordlen = 0;


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
parse_args(int argc, char **argv)
{

  int           i              = 1;


  /* Use the default userame, can override from command-line */
  strcpy(username, UIDNAME);
  usernamelen = strlen(username);

  /* Initialize the password */
  memset(password, sizeof(password), 0);

  /* Initialize the service name */
  memset(service, sizeof(service), 0);

  /* Use the default Service Name, can override from command-line */
  strcpy(service, DEMODSN);
  srvlen = strlen(service);

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

  /* prompt for the password if needed */
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


/* end of file parseargs.c */
