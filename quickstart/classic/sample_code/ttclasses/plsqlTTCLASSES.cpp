/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*

NAME
plsqlTTCLASSES.cpp - ttClasses demo that uses PLSQL

DESCRIPTION
The ttClasses program does the following:
1. Uses a PLSQL package procedure
2. Uses a PLSQL package function
3. Uses a PLSQL block with host variables
4. Opens a Ref cursor in a PLSQL procedure
and processes the resultset in ttClasses

EXPORT FUNCTION(S)
<external functions defined for use outside package - one-line descriptions>

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


#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#else 
#include <unistd.h>
#include <ctype.h>
#endif 
#include <ttclasses/TTInclude.h>
#include "testprog_utils.h"
#include "tt_version.h"


// Globals
const char *usage_string = " plsqlTTCLASSES [-logfile <logfile>]\n"
                           "        {<DSN> | -connstr <connection string>}\n"
                           "\noptions:\n""  -logfile <logfile>  Logs to file <logfile>\n";

TTConnection conn;
char empName[10+1]; // Used by givePayRaise() and plsqlblock()

//----------------------------------------------------------------------



/*********************************************************************************************/
/* Use a PLSQL package procedure to randomly give one of the lowest paid workers a payraise. */
/*********************************************************************************************/
void givePayRaise()
{
  TTCmd cmd;
  SQLINTEGER numEmps;
  SQLINTEGER errCode;
  char errText[255];

  /* Initialize the input and output parameters for procedure emp_pkg.givePayRaise */
  memset(empName, 0, sizeof(empName));
  memset(errText, 0, sizeof(errText));
  errCode = 0;
  numEmps = 0;

  try
  {
    /* Prepare the call to the package procedure */
    cmd.Prepare(&conn, "begin emp_pkg.givePayRaise(:numEmps, :empName, :errCode, :errText); end;");

  }
  catch (TTStatus st)
  {
    cerr << "Error preparing procedure emp_pkg.givePayRaise. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    /* Set the input param numEmps (ie randomly choose one of 10 lowest paid employees) */
    cmd.setParam(1, 10);

    /* Register the output params */
    cmd.registerParam(2, TTCmd::PARAM_OUT, SQL_VARCHAR, sizeof(empName));
    cmd.registerParam(3, TTCmd::PARAM_OUT, SQL_INTEGER);
    cmd.registerParam(4, TTCmd::PARAM_OUT, SQL_VARCHAR, sizeof(errText));
  }
  catch (TTStatus st)
  {
    cerr << "Error setting and registering the params. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    /* Call the package procedure */
    cmd.Execute();
  }
  catch (TTStatus st)
  {
    cerr << "Error executing procedure emp_pkg.givePayRaise. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    /* get the output param values */
    cmd.getParam(2, empName);
    cmd.getParam(3, &errCode);
    cmd.getParam(4, errText);
  }
  catch (TTStatus st)
  {
    cerr << "Error getting the params. ERROR: " << st << endl;
    exit( - 1);
  }

  if (0 == errCode)
  {
    cout << "The employee who got the 10% pay raise was " << empName << endl << endl;
  }
  else
  {
    cerr << endl << "A problem with emp_pkg.givePayRaise" << endl;
    cerr << "  Error: " << errCode << endl;
    cerr << "  Error Text: " << errText << endl;
  }


  try
  {
    cmd.Close();
  }
  catch (TTStatus st)
  {
    cerr << "Error closing cmd in givePayRaise. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    cmd.Drop();
  }
  catch (TTStatus st)
  {
    cerr << "Error dropping cmd in givePayRaise. ERROR: " << st << endl;
    exit( - 1);
  }
} // givePayRaise


/**************************************************************************************/
/* Use a PLSQL anonymous block to get some details on the worker who got the payraise */
/**************************************************************************************/
void plsqlblock()
{
  TTCmd cmd;
  SQLINTEGER empNo;
  SQLINTEGER errCode;
  char plsqlblock[2048];
  char errText[200+1];
  char jobtype[9+1];
  char hired[20+1];
  double salary;
  int dept;
  int worked_longer;
  int higher_sal;
  int total_in_dept;
  char outbuf[256];


  /* Initialize the input and output parameters for the plsql block */
  memset(plsqlblock, 0, sizeof(plsqlblock));
  memset(errText, 0, sizeof(errText));
  memset(jobtype, 0, sizeof(jobtype));
  memset(hired, 0, sizeof(hired));
  salary = 0.0;
  dept = 0;

  /* Create the text for the PLSQL anonymous block */
  strcpy(plsqlblock, "BEGIN");

  strcat(plsqlblock, "  SELECT job, hiredate, sal, deptno");
  strcat(plsqlblock, "    INTO :jobtype, :hired, :salary, :dept");
  strcat(plsqlblock, "  FROM emp");
  strcat(plsqlblock, "  WHERE ename = UPPER(:empname);");

  strcat(plsqlblock, "  SELECT count(*)");
  strcat(plsqlblock, "    INTO :worked_longer");
  strcat(plsqlblock, "  FROM emp");
  strcat(plsqlblock, "  WHERE hiredate < :hired;");

  strcat(plsqlblock, "  SELECT count(*)");
  strcat(plsqlblock, "    INTO :higher_sal");
  strcat(plsqlblock, "  FROM emp");
  strcat(plsqlblock, "  WHERE sal > :salary;");

  strcat(plsqlblock, "  SELECT count(*)");
  strcat(plsqlblock, "    INTO :total_in_dept");
  strcat(plsqlblock, "  FROM emp");
  strcat(plsqlblock, "  WHERE deptno = :dept;");

  strcat(plsqlblock, " EXCEPTION");

  strcat(plsqlblock, "    WHEN OTHERS THEN");
  strcat(plsqlblock, "       :errCode  := SQLCODE;");
  strcat(plsqlblock, "       :errText  := SUBSTR(SQLERRM, 1, 200);");

  strcat(plsqlblock, " END;");


  try
  {

    /* Prepare the call to PLSQL anonymous block */
    cmd.Prepare(&conn, plsqlblock);
  }
  catch (TTStatus st)
  {
    cerr << "Error preparing plsqlblock. ERROR: " << st << endl;
    exit( - 1);
  }


  try
  {

    /* Register the input and output params */
    cmd.registerParam(1, TTCmd::PARAM_OUT, SQL_VARCHAR, sizeof(jobtype));
    cmd.registerParam(2, TTCmd::PARAM_OUT, SQL_VARCHAR, sizeof(hired));
    cmd.registerParam(3, TTCmd::PARAM_OUT, SQL_DECIMAL, 7, 2); // salary
    cmd.registerParam(4, TTCmd::PARAM_OUT, SQL_INTEGER); // dept
    cmd.registerParam(5, TTCmd::PARAM_IN, SQL_VARCHAR, sizeof(empName));
    cmd.registerParam(6, TTCmd::PARAM_OUT, SQL_INTEGER); // worked_longer
    cmd.registerParam(7, TTCmd::PARAM_OUT, SQL_INTEGER); // higher_sal
    cmd.registerParam(8, TTCmd::PARAM_OUT, SQL_INTEGER); // total_in_dept
    cmd.registerParam(9, TTCmd::PARAM_OUT, SQL_INTEGER); // errCode
    cmd.registerParam(10, TTCmd::PARAM_OUT, SQL_VARCHAR, sizeof(errText));

    /* Set the input params */
    cmd.setParam(5, empName);
    cmd.setParam(5, empName);
  }
  catch (TTStatus st)
  {
    cerr << "Error registering or setting the parameters. ERROR: " << st << endl;
    exit( - 1);
  }


  try
  {

    /* Call the PLSQL anonymous block */
    cmd.Execute();
  }
  catch (TTStatus st)
  {
    cerr << "Error executing the PLSQL anonymous block. ERROR: " << st << endl;
    exit( - 1);
  }


  try
  {
    /* get the output param values */
    cmd.getParam(1, jobtype);
    cmd.getParam(2, hired);
    cmd.getParam(3, &salary);
    cmd.getParam(4, &dept);
    cmd.getParam(6, &worked_longer);
    cmd.getParam(7, &higher_sal);
    cmd.getParam(8, &total_in_dept);
    cmd.getParam(9, &errCode);
    cmd.getParam(10, errText);

    if (0 == errCode)
    {

      /* Display all the information */
      sprintf(outbuf, "Name:        %s", empName);
      cout << outbuf <<endl;
      sprintf(outbuf, "Job:         %s", jobtype);
      cout << outbuf <<endl;
      sprintf(outbuf, "Hired:       %s\t(%d people have served longer).", hired, worked_longer);
      cout << outbuf <<endl;
      sprintf(outbuf, "Salary:      $%.2f\t\t\t(%d people have a higher salary).", salary, higher_sal);
      cout << outbuf <<endl;
      sprintf(outbuf, "Department:  %d\t\t\t\t(%d people in the department).", dept, total_in_dept);
      cout << outbuf <<endl<<endl;

    }
    else
    {
      cerr << endl << "A problem with plsqlblock:" << endl;
      cerr << "  Error: " << errCode << endl;
      cerr << "  Error Text: " << errText << endl << endl;
    }

  }
  catch (TTStatus st)
  {
    cerr << "Error in plsqlblock. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    cmd.Close();
  }
  catch (TTStatus st)
  {
    cerr << "Error closing cmd in plsqlblock. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    cmd.Drop();
  }
  catch (TTStatus st)
  {
    cerr << "Error dropping cmd in plsqlblock. ERROR: " << st << endl;
    exit( - 1);
  }
} // plsqlblock



/************************************************************************************/
/* Use a PLSQL package function to get the name of an employee based on their empid */
/************************************************************************************/
void getEmpName()
{
  TTCmd cmd;
  SQLINTEGER empNo;
  SQLINTEGER errCode;
  char empName[64];
  char errText[255];

  /* Initialize the input and output parameters for function sample_pkg.getEmpName */
  memset(empName, 0, sizeof(empName));
  memset(errText, 0, sizeof(errText));
  errCode = 0;
  empNo = 7839;

  try
  {

    /* Prepare the call to the package function */
    cmd.Prepare(&conn, "begin :empName := sample_pkg.getEmpName(:empNo, :errCode, :errText); end;");

    /* Register the input and output params */
    cmd.registerParam(1, TTCmd::PARAM_OUT, SQL_VARCHAR, sizeof(empName));
    cmd.registerParam(2, TTCmd::PARAM_IN, SQL_INTEGER);
    cmd.registerParam(3, TTCmd::PARAM_OUT, SQL_INTEGER);
    cmd.registerParam(4, TTCmd::PARAM_OUT, SQL_VARCHAR, sizeof(errText));

    /* Set the input param empNo */
    cmd.setParam(2, empNo);

    /* Call the package procedure */
    cmd.Execute();

    /* get the output param values */
    cmd.getParam(1, empName);
    cmd.getParam(3, &errCode);
    cmd.getParam(4, errText);

    if (0 == errCode)
    {
      cout << "The employee with empid " << empNo << " is " << empName << endl;
    }
    else
    {
      cerr << endl << "A problem with function sample_pkg.getEmpName" << endl;
      cerr << "  Error: " << errCode << endl;
      cerr << "  Error Text: " << errText << endl;
    }
  }
  catch (TTStatus st)
  {
    cerr << "Error in sample_pkg.getEmpName. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    cmd.Close();
  }
  catch (TTStatus st)
  {
    cerr << "Error closing cmd in sample_pkg.getEmpName. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    cmd.Drop();
  }
  catch (TTStatus st)
  {
    cerr << "Error dropping cmd in sample_pkg.getEmpName. ERROR: " << st << endl;
    exit( - 1);
  }
} // getEmpName


/**************************************************************************/
/* Use a PLSQL Packge procedure to open a ref cursor for all sales people */
/* Use ttClasses to iterate over of the resultset of the ref cursor       */
/**************************************************************************/
void getSalesPeople()
{
  TTCmd cmd;
  TTCmd *cmdRefCursor = NULL;

  SQLINTEGER empNo;
  double salary;
  char empName[64];

  SQLINTEGER errCode;
  char errText[255];

  char outbuf[256];

  /* Initialize the input and output parameters for the routine */
  memset(errText, 0, sizeof(errText));
  errCode = 0;
  empNo = 0;

  try
  {

    /* prepare the PLSQL block */
    cmd.Prepare(&conn, "begin emp_pkg.OpenSalesPeopleCursor(:cmdRefCursor, :errCode, :errText); end;");
  }
  catch (TTStatus st)
  {
    cerr << "Error in preparing the PLQL block. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    /* register the output params */
    cmd.registerParam(1, TTCmd::PARAM_OUT, SQL_REFCURSOR);
    cmd.registerParam(2, TTCmd::PARAM_OUT, SQL_INTEGER);
    cmd.registerParam(3, TTCmd::PARAM_OUT, SQL_VARCHAR, sizeof(errText));
  }
  catch (TTStatus st)
  {
    cerr << "Error in registering the params. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    /* execute the PLSQL block to open the ref cursor */
    cmd.Execute();
  }
  catch (TTStatus st)
  {
    cerr << "Error in executing the open of the ref cursor. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    /* get the reference to the cursor (Ref Cursor) */
    cmd.getParam(1, &cmdRefCursor);
    cmd.getParam(2, &errCode);
    cmd.getParam(3, errText);
  }
  catch (TTStatus st)
  {
    cerr << "Error in getting the PLSQL out params. ERROR: " << st << endl;
    exit( - 1);
  }

  /* iterate over the (ref) cursor */
  cout << endl << "The sales people are: " << endl << endl;

  cout << "  EMPNO ENAME      SALARY " << endl;
  cout << "  ===== ========== ========== " << endl;

  try
  {
    while (!cmdRefCursor->FetchNext())
    {
      cmdRefCursor->getColumn(1, &empNo);
      cmdRefCursor->getColumn(2, empName);
      cmdRefCursor->getColumn(3, &salary);

      sprintf(outbuf, "  %5.0d %-10s %10.2f", empNo, empName, salary); 
      cout << outbuf  << endl;

    }

  }
  catch (TTStatus st)
  {
    cerr << "Error in iterating over Ref Cursor resultset. ERROR: " << st << endl;
    /* continue to try to free resources */
  }

  try
  {
    cmd.Close();
    cmdRefCursor->Close();
  }
  catch (TTStatus st)
  {
    cerr << "Error closing cmds for Ref Cursor and PLSQL block. ERROR: " << st << endl;
    exit( - 1);
  }

  try
  {
    cmd.Drop();
    cmdRefCursor->Drop();
  }
  catch (TTStatus st)
  {
    cerr << "Error dropping cmds for Ref Cursor and PLSQL block. ERROR: " << st << endl;
    exit( - 1);
  }

  /* free the cmdRefCursor object */
  if (NULL == cmdRefCursor)
  {
    delete cmdRefCursor;
  }

} // getSalesPeople


int main(int argc, char **argv)
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  ParamParser parser(usage_string);
  parser.setArg("-logfile", false /* not required */, 128);
  char connStr[256];

  if (1 == argc)
  {

    /* Default the DSN and UID */
    sprintf(connStr, "dsn=%s;%s", DEMODSN, UID);
  }
  else
  {

    /* old behaviour */
    connStr[0] = '\0';
    ;
  }
  parser.processArgs(argc, argv, connStr);
  cout << endl << "Connecting to TimesTen <" << connStr << ">" << endl;

  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  ofstream output;
  if (parser.argUsed("-logfile"))
  {
    output.open(parser.getArgValue("-logfile"));
  }
  else
  {
    output.open("plsqlTTCLASSES.txt");
  }
  TTGlobal::setLogStream(output);
  TTGlobal::setLogLevel(TTLog::TTLOG_ERR); // TTLOG_WARN: default log level
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up signal handling
  // This code simply registers the signal-handling.
  //
  // Subsequent calls to StopRequested() and TerminateIfRequested()
  // stop the execution loops and rolls back/disconnects/exits gracefully.
  // ---------------------------------------------------------------
  int couldNotSetupSignalHandling = HandleSignals();
  if (couldNotSetupSignalHandling)
  {
    cerr << "Could not set up signal handling.  Aborting." << endl;
    exit( - 1);
  }
  // ---------------------------------------------------------------  

  try
  {
    conn.Connect(connStr, TTConnection::DRIVER_COMPLETE);
  }
  catch (TTWarning st)
  {
    cerr << "Warning connecting to TimesTen: " << st << endl;
  }
  catch (TTError st)
  {
    cerr << "Error connecting to TimesTen: " << st << endl;
    exit(1);
  }
  cout << "connected" << endl << endl;



  /* Use a PLSQL package procedure to randomly give one of the lowest paid workers a payraise. */
  givePayRaise();

  /* Use a PLSQL block to get some details on the worker who got the payraise */
  plsqlblock();

  /* Use a PLSQL package function to get the name of an employee based on their empid */
  getEmpName();

  /* Use a PLSQL Packge procedure to open a ref cursor for all sales people */
  /* Use ttClasses to iterate over of the resultset of the ref cursor */
  getSalesPeople();


  cout << endl << "disconnecting..." << endl;
  try
  {
    conn.Disconnect();
  }
  catch (TTStatus st)
  {
    cerr << "Error disconnecting from TimesTen:" << endl << st;
    exit(1);
  }
  cout << "disconnected" << endl;

  return 0;
}
