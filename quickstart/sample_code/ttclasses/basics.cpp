//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// basics.cpp: sample usage of the TimesTen C++ Interface Classes
//
//////////////////////////////////////////////////////////////////////////
//
// This is a TTClasses example program.  This program utilizes TTClasses
// functionality and demonstrates TTClasses best practices.
//
// This program is distributed as part of TTClasses as as-is, unsupported
// example code.  This program is not intended for use in a production
// environment, and is not warranted or supported in any way, except for
// its demonstration of TTClasses functionality and best practices.
//
//////////////////////////////////////////////////////////////////////////


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

//----------------------------------------------------------------------

class SampleConnection : public TTConnection {

using TTConnection::Connect;
using TTConnection::Disconnect;

private:
  TTCmd        dropTable;
  TTCmd        createTable;
  TTCmd        insertData;
  TTCmd        queryData;

protected:

public:
  SampleConnection();
  ~SampleConnection();
  virtual void Connect(const char* connStr, TTConnection::DRIVER_COMPLETION_ENUM driverCompletion);
  virtual void Disconnect();
  void create();
  void insert(char* nameP);
  void query(const char* nameP, int* valueP);
  void invalid_query_param();
  void TerminateIfRequested();
};

//----------------------------------------------------------------------

SampleConnection::SampleConnection()
{
}

//----------------------------------------------------------------------

SampleConnection::~SampleConnection()
{
  try {
    Disconnect();
  }
  catch (TTError st) {
    cerr << "Could not disconnect : " << st << endl;
  }
}

//----------------------------------------------------------------------

void
SampleConnection::Disconnect()
{
  createTable.Drop();
  insertData.Drop();
  queryData.Drop();

  TTConnection::Disconnect() ;
}

//----------------------------------------------------------------------

void
SampleConnection::Connect(const char* connStr, TTConnection::DRIVER_COMPLETION_ENUM driverCompletion)
{
  try {
    try {
      TTConnection::Connect(connStr, driverCompletion);
    }
    catch (TTWarning st) {
      // ignore warning messages
    }

    // Show how to do DDL (drop table) in ttClasses and always start from a known point
    try {
      dropTable.ExecuteImmediate(this, "drop table basics");
    }
    catch (TTStatus st) {
      // Ignore errors .. table might legitimately not exist.
        cout << "Problem dropping table basics" << endl;
    }

    // Create a table in ttClasses
    createTable.Prepare(this,
                        "create table basics "
                        "(name char(10) not null primary key, "
                        " i tt_integer) "
                        "unique hash on (name) pages=100");

    // Create the table if it doesn't exist
    create();


    insertData.Prepare(this,
                       "insert into basics values(:name, :value)");

    queryData.Prepare(this,
                      "select i from basics where name = :name");

    Commit();
  }
  catch (TTStatus st) {
    cerr << "Error in SampleConnection::Connect: " << st << endl ;
    Rollback();
    throw st;
    return;
  }

  return;
}

//----------------------------------------------------------------------

void
SampleConnection::create()
{
  try {
    createTable.Execute();
    Commit();
  }
  catch (TTError st) {
    cerr << "Could not create table : " << st << endl ;
  }
}

//----------------------------------------------------------------------

void
SampleConnection::insert(char* nameP)
{
  static long i = 0;

  try {
    insertData.setParam(1, nameP);
    insertData.setParam(2, i++);
    insertData.Execute();
  }
  catch (TTError st) {
    cerr << "Could not insert data : " << st << endl ;
    throw st;
  }
}

//----------------------------------------------------------------------

void
SampleConnection::query(const char* nameP, int* valueP /* OUT */ )
{

  try {
    queryData.setParam(1, nameP);
    queryData.Execute();

    if ( !queryData.FetchNext() ) {
      queryData.getColumn(1, (SQLINTEGER*)valueP);
    }
    queryData.Close();
  }
  catch (TTStatus st) {
    cerr << "Error in SampleConnection::query : " << st << endl;
    queryData.Close();
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
SampleConnection::invalid_query_param()
{

  try {
    queryData.setParam(2, "test");
    queryData.Execute();
  }
  catch (TTStatus st) {
    cerr << "Error in SampleConnection::query : " << st << endl;
    queryData.Close();
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
SampleConnection::TerminateIfRequested()
{
  if (StopRequested()) {
    STDOSTRSTREAM ostr;

    ostr << "Signal <" << SignalReceived() << "> received, "
         << "terminating execution." << ends << endl ;
    cerr << ostr.str() << ends << endl;
    cerr << "Rolling back active transaction...";

    try {
      Rollback();
      cerr << "Disconnecting...";
      Disconnect() ;
    }
    catch (TTStatus st) {
      cerr << "Problem disconnecting : " << st << endl;
    }

    cerr << "Exiting..." << endl ;
    exit(1);
  }
}

//----------------------------------------------------------------------

const char * usage_string =
" sample [-logfile <logfile>]\n"
"        [<DSN> | -connstr <connection string>]\n"
"\noptions:\n"
"  -logfile <logfile>  Logs to file <logfile>\n";

//----------------------------------------------------------------------

int
main(int argc, char** argv)
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  ParamParser parser (usage_string);
  parser.setArg("-logfile", false /* not required */, 128);
  char connStr[256];


  if (1 == argc) {

    /* Default the DSN and UID */
    sprintf(connStr, "dsn=%s;%s", DEMODSN, UID);
  } else {

    /* old behaviour */
    connStr[0] = '\0';;
  }

  parser.processArgs(argc, argv, connStr);
  cerr << endl << "Connecting to TimesTen <" << connStr << ">" << endl << endl;
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  ofstream output;
  if (parser.argUsed("-logfile")) {
    output.open(parser.getArgValue("-logfile"));
  }
  else {
    output.open("sample.txt");
  }
  TTGlobal::setLogStream(output) ;
  TTGlobal::setLogLevel(TTLog::TTLOG_WARN) ; // TTLOG_WARN: default log level
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up signal handling
  // This code simply registers the signal-handling.
  //
  // Subsequent calls to StopRequested() and TerminateIfRequested()
  // stop the execution loops and rolls back/disconnects/exits gracefully.
  // ---------------------------------------------------------------
  int couldNotSetupSignalHandling = HandleSignals();
  if (couldNotSetupSignalHandling) {
    cerr << "Could not set up signal handling.  Aborting." << endl ;
    exit(-1);
  }
  // ---------------------------------------------------------------

  int nRows = 50000;
  int i;
  int value;
  char name[100];
  SampleConnection conn;

  double          ikernel, iuser, ireal;
  ttWallClockTime wstart, wend;
  ttThreadTimes   tstart, tend;

  cerr << "Creating database / connecting to it ..." << endl;

  try {
    conn.Connect(connStr, TTConnection::DRIVER_COMPLETE);
  }
  catch (TTWarning st) {
    cerr << "Warning connecting to TimesTen: " << st << endl ;
  }
  catch (TTError st) {
    cerr << "Error connecting to TimesTen " << st << endl;
    exit(1);
  }
  catch (TTStatus st) {
    cerr << "Error connecting to TimesTen " << st << endl;
    exit(1);
  }

  cerr << endl << "Connected as " << connStr << endl ;

  char contextStr[18] ;

  try {
    conn.GetTTContext(contextStr) ;
    cerr << "Context is " << contextStr << endl ;
  }
  catch (TTStatus st) {
    cerr << "Error inserting row " << name << ":" << endl << st;
  }

  cerr << endl << "Inserting data ..." << endl;

  try {
    for (i = 0; i < nRows && !StopRequested(); i++) {
      sprintf(name, "Name%d", i);
      conn.insert(name);
    } // end for
  }
  catch (TTStatus st) {
    cerr << "Error inserting row " << name << ":" << endl << st;
    conn.Rollback();
  }

  conn.TerminateIfRequested();

  try {
  conn.Commit();
  }
  catch (TTStatus st) {
    cerr << "Error committing :" << endl << st;
  }


  // ---------------------------------------------------------------------------------

  cerr << endl << "-------------------------------------------" << endl ;
  cerr << "Now some benchmarks for fetching data ..." << endl << endl;

  conn.TerminateIfRequested();

  const int maxIsoIter = 2;

  for (int isoIter = 1; isoIter <= maxIsoIter && !StopRequested(); isoIter++) {

    switch (isoIter) {
    case 1:
      cerr << "Setting serializable isolation" << endl ;
      conn.SetIsoSerializable() ;
      break ;

    case 2:
      cerr << "Setting read committed isolation" << endl ;
      conn.SetIsoReadCommitted() ;
      break ;

    default:
      break;
    }

    cerr << "-------------------------------------------" << endl ;

    double ireal_total = 0.0 ;

    for (int loop = 0; loop < 10 && !StopRequested(); loop++) {

      ttGetWallClockTime(&wstart);
      ttGetThreadTimes(&tstart);

      try {
        for (i = nRows-1; i >= 0 && !StopRequested(); i--) {
          sprintf(name, "Name%d", i);
          conn.query(name, &value);
          // cerr << name << ": " << value << endl ;
        } // end for
      }
      catch (TTStatus st) {
        cerr << "Error fetching " << name << ":" << endl << st;
        conn.Rollback();
      }

      try {
        conn.Commit();
      }
      catch (TTStatus st) {
        cerr << "Error committing " << name << ":" << endl << st;
      }

      ttGetThreadTimes(&tend);
      ttGetWallClockTime(&wend);
      ttCalcElapsedThreadTimes(&tstart, &tend, &ikernel, &iuser);
      ttCalcElapsedWallClockTime(&wstart, &wend, &ireal);
      fprintf(stderr,
              "Test time %.3f sec (%.3f user, %.3f sys) - "
              "%.2f per second\n",
              ireal / 1000.0, iuser, ikernel, nRows / (ireal / 1000.0));

      ireal_total += ireal ;

    } // end for loop

    conn.TerminateIfRequested();

    cerr << "-------------------------------------------" << endl ;
    cerr << "Average rate: " << ((nRows * 10) / (ireal_total / 1000.0)) << endl ;
    cerr << "-------------------------------------------" << endl ;

  } // end for isoIter

  cerr << "... end of fetch test" << endl ;

  cerr << endl << endl << endl;
  cerr << endl << "***********************************************************************"
       << endl ;
  cerr << endl << "About to create some ERRORs on purpose - its OK!." << endl ;

  cerr << endl << "***********************************************************************"
       << endl ;
  cerr << "Now generate a unique constraint exception, and catch it ..." << endl ;

  try {
    for (i = 0; i < nRows; i++) {
      sprintf(name, "Name%d", i);
      conn.insert(name);
    } // end for
  }
  catch (TTStatus st) {
    STDOSTRSTREAM ostr;
    ostr << "Error inserting row " << name << ":" << endl << st << ends << endl ;
    cerr << ostr.str() << ends << endl;
    conn.Rollback();
  }

  conn.TerminateIfRequested();

  cerr << "***********************************************************************"
       << endl
       << "(The above error message can safely be ignored; that's what "
       << endl
       << "a unique constraint violation error looks like.)"
       << endl
       << "***********************************************************************"
       << endl ;

  cerr << endl << "***********************************************************************"
       << endl ;
  cerr << "Now throw an exception when an invalid parameter number is used in TTCmd::setParam() ..." << endl ;

  try {
    conn.invalid_query_param();
  }
  catch (TTStatus st) {
    STDOSTRSTREAM ostr;
    ostr << "Error when calling TTCmd::setParam() with an invalid parameter number :" << endl << st << ends;
    cerr << ostr.str() << ends << endl;
    conn.Rollback();
  }

  conn.TerminateIfRequested();

  cerr << "***********************************************************************"
       << endl
       << "(The above error message can safely be ignored; that's what "
       << endl
       << "misuse of TTCmd::setParam() looks like.)"
       << endl
       << "***********************************************************************"
       << endl ;

  try {
    conn.Commit() ;
  }
  catch (TTStatus st) {
    cerr << "Error committing : " << st << endl;
    conn.Rollback();
  }

  cerr << endl << endl;

  // Done checking the stack overflow prevention logic.
  cerr << "Disconnecting ...";

  try {
    conn.Disconnect();
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting from TimesTen: " << endl << st;
    exit(1);
  }

  cerr << "disconnected" << endl << endl;

  return 0;
}
