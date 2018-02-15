//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// xla_demo.cpp: another demonstration of the TTClasses XLA functionality
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

#include <stdlib.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/types.h>
#if !defined(AIX) && !defined(TRU64)
#include <sys/unistd.h>
#endif /* !AIX && !TRU64 */
#endif /* !_WIN32 */
#include <signal.h>
#include <stdio.h>

#ifdef _WIN32
#include <process.h> /* for getpid() */
#endif /* _WIN32 */

#if defined(AIX) || defined(GCC_SOLARIS)
#include <unistd.h>  /* for getpid() */
#endif /* AIX || GCC_SOLARIS */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#if defined(AIX) || defined(GCC_SOLARIS)
#include <unistd.h> /* for getpid() */
#endif /* AIX || GCC */

#include <ttclasses/TTInclude.h>
#include <ttclasses/TTXla.h>
#include "testprog_utils.h"
#include "tt_version.h"

TTXlaPersistConnection  conn;
const int FETCH_WAIT_SECS = 5 ;

void TerminateGracefully(TTXlaPersistConnection&);

//----------------------------------------------------------------------
// This class contains all the logic to be implemented whenever
// the command-line specified table is changed.
//----------------------------------------------------------------------

class SampleHandler: public TTXlaTableHandler {
private:
  //  int col[TT_XLA_MAX_COLUMNS] ;
  int numColumns ;

protected:

public:
  SampleHandler(TTXlaPersistConnection& conn, const char* ownerP, const char* nameP);
  ~SampleHandler();

  virtual void HandleChange(ttXlaUpdateDesc_t*, void *pData);
  virtual void HandleChange(ttXlaUpdateDesc_t*) {}
  virtual void HandleDelete(ttXlaUpdateDesc_t*);
  virtual void HandleInsert(ttXlaUpdateDesc_t*);
  virtual void HandleUpdate(ttXlaUpdateDesc_t*);
};

SampleHandler::SampleHandler(TTXlaPersistConnection& connection,
                             const char* ownerP, const char* nameP) :
  TTXlaTableHandler(connection, ownerP, nameP)
{

  // We aren't treating columns differently.  All we're going to do
  // is print out which columns have changed.
  //
  // Later on, we'll print out the column values.

  numColumns = tbl.getNCols() ;
}

SampleHandler::~SampleHandler()
{
}

void
SampleHandler::HandleChange(ttXlaUpdateDesc_t *p, void *pData)
{
  bool commit_flag;

  try {
    switch (p->type) {
    case INSERTTUP:
      row.setTuple(p, INSERTED_TUP);    // Report the inserted row
      this->HandleInsert(p);
      break;

    case UPDATETUP:
      resetUpdatedColumnFlags() ;
      row.setTuple(p, UPDATE_OLD_TUP);  // Report the old row value
      row2.setTuple(p, UPDATE_NEW_TUP); // Report the new row value
      setUpdatedColumnFlags() ;
      this->HandleUpdate(p);
      break;

    case DELETETUP:
      row.setTuple(p, DELETED_TUP);     // Report the deleted row
      this->HandleDelete(p);
      break;

    default:
      // Ignore other notifications
      return;
    }
  }
  catch (TTStatus st) {
    cerr << "Error caught while processing XLA record:" << endl;
    cerr << st << endl;
  }

  // Is this record at the end of a transaction?  If so, then return this
  // information to the XLA reader so that we can move the bookmark
  // forward.
  commit_flag = isXLACommitRecord(p);
  if (commit_flag) {
    *(int *)pData = TRUE;
  }
}

void
SampleHandler::HandleInsert(ttXlaUpdateDesc_t*)
{
  cerr << "1 row inserted." << endl ;
}

void
SampleHandler::HandleUpdate(ttXlaUpdateDesc_t* )
{
  cerr << row2.numUpdatedCols() << " column(s) updated: " ;
  for ( int i = 1 ; i <= row2.numUpdatedCols() ; i++ )
  {
    cerr << row2.updatedCol(i) << "("
         << row2.getColumn(row2.updatedCol(i)-1)->getColName()
         << ") " ;
  }
  cerr << endl ;
}

void
SampleHandler::HandleDelete(ttXlaUpdateDesc_t* p)
{
  const int BUFFSIZE=256;
  char buffer [BUFFSIZE+1];
  SQLINTEGER actualLen;

  try {
    generateSQL(p, buffer, BUFFSIZE, &actualLen);
  }
  catch (TTStatus st) {
    if (st.native_error == tt_ErrNoUniqueIndex) {
      cerr << "** Cannot display SQL for XLA (delete) record; table does not have a unique, "
           << endl
           << "** non-nullable index." << endl;
    }
    else {
      cerr << "** Unexpected error while generating SQL for XLA (delete) record: " << st << endl;
    }
    return;
  }

  cerr << "Row deleted: ['" << buffer << "'] (" << actualLen << "/" << BUFFSIZE << ")" << endl;
}

//----------------------------------------------------------------------

void
TerminateGracefully (TTXlaPersistConnection & connection)
{
  // cerr << "Rolling back active transaction...";
  cerr << "Disconnecting...";

  try {
    connection.Disconnect() ;
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting :" << endl;
    cerr << st << endl;
  }
  cerr << "Exiting..." << endl ;
  exit(1);
}

//----------------------------------------------------------------------

void
TerminateIfRequested (TTXlaPersistConnection & connection)
{
  if (StopRequested()) {
    STDOSTRSTREAM ostr;
    ostr << "Signal <" << SignalReceived() << "> received, "
         << "terminating execution." << ends << endl ;
    cerr << ostr.str() << ends << endl;
    TerminateGracefully(connection);
  }
}

//----------------------------------------------------------------------

const char * usage_string =
" xlasubscriber1 [<DSN> | -connstr <connection string>]\n";

//----------------------------------------------------------------------

int
main(int argc, char* argv[])
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  ParamParser parser (usage_string);
  char connStr[256];
  char ownerName[64], tableName[64] ;

  // Default the DSN and UID if the DSN or Connect String is not specified
  if (1 == argc) {

    /* Default the DSN and UID */
    sprintf(connStr, "dsn=%s;%s", DEMODSN, XLAUID);
  } else {

    /* old behaviour */
    connStr[0] = '\0';;
  }

  // ------------------------------------------------------
  parser.setArg("-logfile", false /* not required */, 128);  
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
    output.open("xlasubscriber1.txt");
  }
  TTGlobal::setLogStream(output);
  TTGlobal::setLogLevel(TTLog::TTLOG_WARN) ;
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

  //
  // Don't try to subscribe to system tables
  //
  if (tt_strcasecmp(ownerName, "TTREP")==0 ||
      tt_strcasecmp(ownerName, "SYS")==0) {
    cerr << "*** Error: cannot monitor TimesTen system tables (SYS.*, TTREP.*) via XLA." << endl;
    exit(-1);
  }

  // unique-ifying the bookmark name
  int pid = getpid();
  char bookmarkName[32] ;
  sprintf(bookmarkName, "xlasubscriber1_%d", pid);

  TTXlaTableList list(&conn);    // List of tables to monitor

  // Handlers, one for each table we want to monitor

  SampleHandler* sampP = NULL;

  // Misc stuff
  ttXlaUpdateDesc_t ** arry;

  int j, records;
  
  

  try {
    conn.Connect(connStr, SQL_DRIVER_COMPLETE, bookmarkName);
  }
  catch (TTStatus st) {
    if (st.rc == SQL_SUCCESS_WITH_INFO) {
      cerr << "Warning received when trying to connect to TimesTen with XLA bookmark <"
         << bookmarkName << ">: " << st << endl ;
    }
    else {
        cerr << "Error connecting to TimesTen via XLA: " << st << endl ;
        exit(-2) ;
    }
  }

  cerr << endl << "Which table do you want to monitor : ";
  fgets(tableName, sizeof(tableName), stdin);
  tableName[ strlen(tableName) -1 ] = '\0';

  cerr << "Which user owns table " <<  tableName << " : ";
  fgets(ownerName, sizeof(ownerName), stdin);
  ownerName[ strlen(ownerName) -1 ] = '\0';

  cerr << endl << "Monitoring table " << ownerName << "." << tableName << " for changes." << endl ;

  cerr << endl << "To run it, in one window run this application.  In another window run \n"
"\n"
"      ttIsql -connStr \"dsn=sampledb;uid=appuser\"\n"
"\n"
"to make changes to these tables.  The changes will be visible as \n"
"output from this application : " << endl << endl;



  // Make a handler to process changes to the MYDATA table and
  // add the handler to the list of all handlers

  try {
    sampP = new SampleHandler(conn, ownerName, tableName) ;
    if (!sampP) {
      cerr << "Could not create SampleHandler" << endl;
      TerminateGracefully(conn);
    }
    list.add(sampP);
  }
  catch (TTStatus st) {
    cerr << "Error creating SampleHandler -- most likely, the specified table"
         << endl
         << "  " << ownerName << "." << tableName << " does not exist."
         << endl ;
    TerminateGracefully(conn);
  }

  // Enable transaction logging for the table we're interested in

  try {
    sampP->EnableTracking();
  }
  catch (TTStatus st) {
    cerr << "Error enabling tracking for table " << ownerName << "." 
         << tableName << ":" << endl << st << endl;
    TerminateGracefully(conn);
  }


  // Get updates.  Dispatch them to the appropriate handler.
  // This loop will handle updates to all the tables.

  // This variable keeps track of whether or not the XLA records terminate on
  // a transaction boundary.  If so, then we should "ack updates".
  int doAcknowledge;

  try {
    while (!StopRequested()) {

      try {
      conn.fetchUpdatesWait(&arry, 1000, &records, FETCH_WAIT_SECS);
      }
      catch (TTStatus st) {
        if (st.rc) {
          cerr << "Error fetching updates" << st << endl;
          TerminateGracefully(conn);
        }
      }

      doAcknowledge = FALSE;

      // Interpret the updates
      for(j=0;j < records;j++){
        ttXlaUpdateDesc_t *p;
        p = arry[j];
        list.HandleChange(p, &doAcknowledge);
      } // end for each record fetched

      if (records) {
        cerr << "Processed " << records << " records" << endl ;
      }

      // If our batch of XLA records terminates on a transaction boundary,
      // then we acknowledge the updates.
      if (doAcknowledge == TRUE) {
        conn.ackUpdates() ;
        cerr << "Ack'd updates; moved XLA bookmark forward at transaction boundary." << endl;
      }

      // NB: In general, it's a good idea to ack updates at regular (but
      // not overly frequent) intervals, to move the XLA bookmark forward.

    } // end while !StopRequested()
  } // try
  catch (TTStatus st) {
    cerr << "Exception caught : " << st << endl ;
    cerr << "Aborting ..." << endl ;
    TerminateGracefully(conn);
  }

  // No need to clean up here when we catch a signal, since the following
  // code does the required cleanup.

  // When we get to here, the program is exiting.

  list.del(sampP);                  // Take the table out of the list
  delete sampP;

  conn.deleteBookmarkAndDisconnect() ;  // clean up this bookmark

  return 0;
}



