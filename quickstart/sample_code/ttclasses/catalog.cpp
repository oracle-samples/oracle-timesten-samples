//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// catalog.cpp: demonstration of the TTCatalog class.
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
#include <string.h>
#include <stdio.h>
#include <ttclasses/TTInclude.h>
#include <ttclasses/TTCatalog.h>
#include "testprog_utils.h"
#include "tt_version.h"

//----------------------------------------------------------------------

const char * usage_string =
" catalog [<DSN> | -connstr <connection string>]\n"
"\noptions:\n";

//----------------------------------------------------------------------

int
main (int argc, char ** argv)
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  ParamParser parser (usage_string);
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
  ofstream output("catalog.txt");
  TTGlobal::setLogStream(output) ;
  TTGlobal::setLogLevel(TTLog::TTLOG_ERR) ; // TTLOG_WARN: default log level
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up signal handling
  // This code simply registers the signal-handling.
  //
  // Subsequent calls to StopRequested() and TerminateIfRequested()
  // stop the execution loops and rolls back/disconnects/exits gracefully.
  //
  // --> NB: since this program is so short, we don't actually need to
  // call StopRequested() or TerminateIfRequested() anywhere (there's no
  // loop).  See the other TTClasses sample programs for a more thorough
  // demonstration of signal-handling in a longer-running program (i.e.,
  // one with a loop with database calls).
  // ---------------------------------------------------------------
  int couldNotSetupSignalHandling = HandleSignals();
  if (couldNotSetupSignalHandling) {
    cerr << "Could not set up signal handling.  Aborting." << endl ;
    exit(-1);
  }
  // ---------------------------------------------------------------

  TTConnection conn ;

  try {
    conn.Connect(connStr, TTConnection::DRIVER_COMPLETE);
  }
  catch (TTStatus st) {
    cerr << "Error connecting to TimesTen " << st << endl ;
    if (st.rc != SQL_SUCCESS_WITH_INFO)
      exit(-1);
  }

  cerr << endl << "Connected -- now looking at the system catalog ..." << endl ;
  cerr << "(printing information about user tables and views only)" << endl ;

  TTCatalog cat (&conn) ;

  try {
    cat.fetchCatalogData() ;
  }
  catch (TTStatus st) {
    cerr << "Error getting the catalog data for TimesTen:\n" << st;
    // print out the catalog information before exiting
  }

  try {
    // Disconnect immediately -- no need to keep the connection open,
    // since all of the necessary database access was done in the
    // TTCatalog::fetchCatalogData() method.

    conn.Disconnect();
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting from TimesTen:\n" << st;
    // print out the catalog information before exiting
  }

  int i,j,k, num_columns ;
  int num_tables = cat.getNumTables() ;

  for ( i = 0 ; i < num_tables ; i++ )
  {
    const TTCatalogTable & table = cat.getTable(i) ;

    if (table.isSystemTable()) 
      continue;

    cerr << endl ;

    if (table.getNumColumns() == 0)
      cerr << "View #";
    else
      cerr << "Table #";
    cerr << i << " : "
         << table.getTableOwner() << "."
         << table.getTableName() << endl ;

    num_columns = table.getNumColumns() ;

    for ( j = 0 ; j < num_columns ; j++ )
    {
      const TTCatalogColumn & column = table.getColumn(j) ;
      cerr << "\tColumn #" << j << " : " << column.getColumnName() << endl ;
    }

// NB: in TimesTen, SQLSpecialColumns() just returns information about ROWID
// --> i.e., it's true for every table, so this info's not particularly
// helpful.

    num_columns = table.getNumSpecialColumns() ;

    for ( j = 0 ; j < num_columns ; j++ )
    {
      const TTCatalogSpecialColumn & column = table.getSpecialColumn(j) ;
      cerr << "\tSpecial Column #" << j << " : "
           << column.getColumnName() << endl ;
    }


    int num_indexes = table.getNumIndexes() ;

    for ( j = 0 ; j < num_indexes ; j++ )
    {
      const TTCatalogIndex & idx = table.getIndex(j) ;
      cerr << "Index # " << j << " : " << idx.getIndexOwner() << "."
           << idx.getIndexName() << endl ;
      cerr << "\t(is an index on table " << idx.getIndexOwner() << "."
           << idx.getTableName() << ")" << endl ;
      if ( idx.getType() == PRIMARY_KEY )
        cerr << "\tPrimary Key (Hash Index)" << endl ;
      else if ( idx.getType() == RANGE_INDEX )
        cerr << "\tRange Index" << endl ;
      else
        cerr << "\t'Other' Index type" << endl ;

      if ( idx.getType() == RANGE_INDEX )
      {
        if ( idx.isUnique() )
          cerr << "\t(UNIQUE index)" << endl ;
        else
          cerr << "\t(not a unique index)" << endl ;
      }

      cerr << "\tIndex columns are: " << endl ;

      num_columns = idx.getNumColumns() ;

      for ( k = 0 ; k < num_columns ; k++ )
      {
        cerr << "\t\t" << idx.getIndexOwner() << "."
             << idx.getTableName() << "."
             << idx.getColumnName (k) << endl ;
      }
    }

  }

  cerr << endl;
  cerr << "Summary:" << endl;
  cerr << "--------" << endl;
  cerr << "There are " << cat.getNumUserTables() << " user tables and views:" << endl;
  for (i = 0; i < cat.getNumUserTables() ; i++)
  {
    const TTCatalogTable & table = cat.getUserTable(i) ;

    cerr << "\t(" << i << ") "
         << table.getTableOwner() << "."
         << table.getTableName() << endl ;
  }

  cerr << endl;

  cerr << "Done looking at the system catalog." << endl ;

  return 0 ;
}


