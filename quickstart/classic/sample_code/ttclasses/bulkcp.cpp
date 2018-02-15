//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// bulkcp.cpp: dumping output of an arbitrary query into a file
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

#ifdef USE_OLD_CPP_STREAMS
#include <fstream.h>
#else
#include <fstream>
#endif /* USE_OLD_CPP_STREAMS */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ttclasses/TTInclude.h>
#include "testprog_utils.h"
#include "tt_version.h"

//----------------------------------------------------------------------

const int MAX_STR_LEN = 2048 ;

char input_queryStr [MAX_STR_LEN] ;
char output_file [MAX_STR_LEN] ;
char outputNULLStr [32] = {"NULL"} ;
char outputColumnSeparatorStr [32] = {"|"} ;
int verbose;

ostream * Output = &cout ;
ofstream OutputFile ;

#define OUTPUT (*Output)

//----------------------------------------------------------------------

const char * usage_string_proto =
" bulkcp [-sep <string>] [-null <string>] -query <string>\n"
"        [-output <output file>]\n"
"        [<DSN> | -connstr <connection string>]\n"
"\noptions:\n"
"  -sep <string>       Specifies the field separator used to separate output\n"
"                      columns [default: \"%s\"].\n"
"  -null <string>      The output string denoting SQL NULL\n"
"                      [default: \"%s\"].\n"
"  -query <string>     The query which selects rows from a table in\n"
"                      the specified DSN.\n"
"  -output <file>      The output file where all rows are sent; if not\n"
"                      specified, rows are sent to stdout.\n";


int
main (int argc, char ** argv)
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  char usage_string[2048];
  sprintf (usage_string, usage_string_proto, outputColumnSeparatorStr, outputNULLStr);
  ParamParser parser (usage_string);
  parser.setArg("-sep", false /* not required */, 2);
  parser.setArg("-null", false, 25);
  parser.setArg("-query", true, 2048);
  parser.setArg("-output", false, 50);
  // for convenience, we add some syntactic sugar
  parser.setTwinArg("-sep", "-s");
  parser.setTwinArg("-null", "-n");
  parser.setTwinArg("-query", "-q");
  parser.setTwinArg("-output", "-o");
  char connStr[256];


  if (1 == argc) {

    /* Default the DSN and UID */
    sprintf(connStr, "dsn=%s;%s", DEMODSN, UID);

  } else {

    /* old behaviour */
    connStr[0] = '\0';;
  }


  parser.processArgs(argc, argv, connStr);
  if (parser.argUsed("-sep"))
    strncpy(outputColumnSeparatorStr, parser.getArgValue("-sep"), 32-1);
  if (parser.argUsed("-null"))
    strncpy(outputNULLStr, parser.getArgValue("-null"), 32-1);
  strncpy(input_queryStr, parser.getArgValue("-query"), MAX_STR_LEN-1);
  if (parser.argUsed("-output"))
    strncpy(output_file, parser.getArgValue("-output"), MAX_STR_LEN-1);
  cerr << endl << "Connecting to TimesTen <" << connStr << ">" << endl ;
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  TTGlobal::setLogStream(STDCERR) ;
  TTGlobal::setLogLevel(TTLog::TTLOG_ERR) ;
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up signal handling
  // This code simply registers the signal-handling.
  //
  // Subsequent calls to StopRequested()
  // stop the execution loops and rolls back/disconnects/exits gracefully.
  // ---------------------------------------------------------------
  int couldNotSetupSignalHandling = HandleSignals();
  if (couldNotSetupSignalHandling) {
    cerr << "Could not set up signal handling.  Aborting." << endl ;
    exit(-1);
  }
  // ---------------------------------------------------------------

  // --------------------------------------------------------------
  // Set up output file (ostream * Output)
  // --------------------------------------------------------------
  if (output_file[0] == '\0' ) {
    // cerr << "Printing to stdout" << endl ;
  }
  else {
    OutputFile.open (output_file) ;
    if ( OutputFile.fail() ) {
      cerr << "Could not open file for output: " << output_file << endl ;
      cerr << "Aborting." << endl ;
      exit(0) ;
    }
    Output = &OutputFile ;
  }
  // --------------------------------------------------------------

  // --------------------------------------------------------------
  // Verify that it's a query
  // --------------------------------------------------------------
  if ( tt_strncasecmp (input_queryStr, "select", 6) != 0 ) {
    cerr << "Sorry, only 'select' queries supported by " << argv[0] << "." << endl ;
    exit(0) ;
  }

  // --------------------------------------------------------------

  cerr << endl ;
  cerr << "Accessing TimesTen via connection string: [ ``" << connStr << "'' ]" << endl ;
  cerr << endl << "Executing the following query: " << endl ;
  cerr << "\t``" << input_queryStr << "''" << endl ;
  cerr << endl<< "Output column separator is: [ ``" << outputColumnSeparatorStr << "'' ]" << endl ;
  cerr << "SQL NULLs will be printed as: [ ``" << outputNULLStr << "'' ]" << endl ;
  if ( output_file[0] != '\0' )
    cerr << "Printing output to file: [ " << output_file << " ]" << endl ;
  else
    cerr << "Printing output to STDOUT" << endl ;
  cerr << endl ;

  // --------------------------------------------------------------

  TTConnection conn ;

  try {
    conn.Connect(connStr, TTConnection::DRIVER_COMPLETE);
  }
  catch (TTWarning st) {
    cerr << "Warning connecting to TimesTen:" << endl << st << endl ;
  }
  catch (TTError st) {
    cerr << "Error connecting to TimesTen:" << endl << st << endl ;
    if (st.rc != SQL_SUCCESS_WITH_INFO)
      exit(-1);
  }

  cerr << endl;

  TTCmd query ;

  try {
    query.Prepare (&conn, input_queryStr) ;
  }
  catch (TTStatus st) {
    cerr << "bulkcp - query preparation failed:" << endl << st << endl ;
    conn.Rollback();
    conn.Disconnect();
    exit(-1);
  }

  try {
    query.Execute () ;
  }
  catch (TTStatus st) {
    cerr << "bulkcp - query execution failed:" << endl << st << endl ;
    conn.Rollback();
    conn.Disconnect();
    exit(-1);
  }

  int num_result_cols = query.getNColumns() ;

  try { // fetch loop

    while ( !StopRequested() ) // loop until no rows found, or a signal caught
    {
      // fetch a row; if no more rows, break out of loop

      // FetchNext returns 0 for success, 1 for SQL_NO_DATA_FOUND
      if ( query.FetchNext() )
        break ;

      // for each row fetched, print out its column values

      for ( int i = 1 ; i <= num_result_cols ; i++ )
      {
        query.printColumn (i, OUTPUT, outputNULLStr) ;
        if ( i < num_result_cols )
          OUTPUT << outputColumnSeparatorStr ;
      }

      OUTPUT << endl ;
    }
  } // end of fetch loop
  catch (TTStatus st) {
    cerr << "bulkcp -- error while fetching rows:" << endl << st << endl ;
    conn.Rollback();
    conn.Disconnect();
    exit(-1);
  }

  // No need to clean up here when we catch a signal, since the following
  // code does the required cleanup.

  // time to clean up
  try {

    cerr << endl << "Cleanup and disconnect ... " << endl;
    query.Close() ;
    query.Drop() ;
    conn.Commit() ;
    conn.Disconnect();
    cerr << "Disconnected. " << endl << endl;
  }
  catch (TTStatus st) {
    cerr << "Error cleaning up:" << endl << st << endl ;
    conn.Rollback();
    conn.Disconnect();
    exit(-1);
  }


  return 0 ;
}



