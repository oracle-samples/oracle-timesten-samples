//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// sizeall.cpp: ttSizeAll source file
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
// compilation problem with VC++ 6.0 if USE_OLD_CPP_STREAMS disabled
// ! (!OLD_STREAMS && _WIN32) ==> OLD_STREAMS || !_WIN32
#ifdef USE_OLD_CPP_STREAMS
#if !defined(_WIN32)
#include <iomanip.h>
#endif /* !_WIN32 */
#endif /* USE_OLD_CPP_STREAMS */
#include <ttclasses/TTInclude.h>
#include <ttclasses/TTCatalog.h>
#include "testprog_utils.h"
#include "tt_version.h"

// the max # of tables that can be specified
const int MAX_TABLES_SIZED = 100 ;

// max lens for table name & owner name
const int MAX_OWNER_LEN = 35 ;
const int MAX_TABLE_LEN = 35 ;

struct tableSpec {
  char ownerName[MAX_OWNER_LEN+1] ;
  char tableName[MAX_TABLE_LEN+1] ;
  int tableRows ;
  tableSpec(char * owner, char * table, int rows)
  {
    strncpy(ownerName, owner, MAX_OWNER_LEN) ;
    strncpy(tableName, table, MAX_TABLE_LEN) ;
    ownerName[MAX_OWNER_LEN] = '\0' ;
    tableName[MAX_TABLE_LEN] = '\0' ;
    tableRows = rows ;
  }
 private:
  tableSpec() ;
} ;


// the connection string
char connStr [1024] ;

// the owner name
char ownerName[MAX_OWNER_LEN+1] ;

// only print out what user tables & sizes are currently in database
bool listOnly = false ;

// print out info for system tables too?
bool listSys = false ;

// the tables we've parsed from the command line
tableSpec * tableSpecList[MAX_TABLES_SIZED] ;
int numTableSpecs=0 ;

// verbosity
bool verbose = false ;

// Total rows
long total_rows;

void TerminateGracefully(TTConnection &);

void
syntax(const char * errstr, bool err = true)
{
  if (err) {
    cerr << "Error in usage: " << errstr << endl << endl;
  }

  (err ? cerr : cout)
       << endl << "Usage:" << endl
       << " ttSizeAll [-h | -help | -?]" << endl
       << " ttSizeAll [-V | -version]" << endl
       << " ttSizeAll [-dsn DSN | -connStr <conn_string>] [-v]"  << endl
       << "           -list [-listsys]" << endl
       << endl
       << "For example:"                         << endl
       << endl
       << "    ttSizeAll -connStr \"dsn=mydsn;uid=xlauser\""     << endl
	   << endl
       << endl;

  if (err) {
    exit(1);
  }
}

// NB: The standard TTClasses argument-parsing routines (in file
// testprog_utils.cpp) are not sufficiently sophisticated for ttSizeAll.

void
parse_args (int argc, char ** argv)
{
  // initialize the connection string

  bool dsn_specified = false ;
  bool owner_specified = false ;

  connStr[0]='\0' ;
  ownerName[0]='\0' ;
  char * tableName = NULL ;
  int tableSize = 0 ;

  // Parse command line

  for (int i = 1; i < argc; i++) {

    if (!tt_strcasecmp(argv[i], "-h") || !tt_strcasecmp(argv[i], "-help") ||
        !strcmp(argv[i], "?")) {
      syntax(0, false);
      exit(0);
    }

    else if (!strcmp(argv[i], "-V") || !tt_strcasecmp(argv[i], "-version")) {
      cout << TTVERSION_STRING << endl;
      exit(0);
    }

    else if (tt_strcasecmp(argv[i], "-connStr") == 0) {
      if (dsn_specified) syntax("'-connStr' usage redundant") ;
      i++;
      strcpy (connStr, argv[i]) ;
      dsn_specified = true ;
    }

    else if (tt_strcasecmp(argv[i], "-dsn") == 0) {
      if (dsn_specified) syntax("'-dsn' usage redundant") ;
      i++;
      sprintf (connStr, "dsn=%s", argv[i]) ;
      dsn_specified = true ;
    }

    else if (tt_strcasecmp(argv[i], "-list") == 0) {
      if (owner_specified) syntax("invalid '-list' usage when '-own' "
                                  "previously specified") ;
      listOnly = true ;
    }

    else if (tt_strcasecmp(argv[i], "-listsys") == 0) {
      if (owner_specified) syntax("invalid '-listsys' usage when '-own' "
                                  "previously specified") ;
      listSys = true ;
    }

    else if ( tt_strcasecmp(argv[i], "-own") == 0 ||
              tt_strcasecmp(argv[i], "-owner") == 0 ) {
      if (listOnly) syntax("invalid '-own' usage when '-list' previously "
                           "specified") ;
      i++;
      strncpy(ownerName, argv[i], MAX_OWNER_LEN) ;
      owner_specified = true ;
    }

    else if ( tt_strcasecmp(argv[i], "-tbl") == 0 ||
              tt_strcasecmp(argv[i], "-table") == 0 ) {
      if (listOnly) syntax("invalid '-tbl' usage when '-list' previously "
                           "specified") ;
      if (!owner_specified) syntax("invalid '-tbl' usage prior to '-own'") ;
      i++;
      tableName = argv[i] ;
      i++ ;
      tableSize = atoi(argv[i]);
      if (tableSize < 1) syntax("specified '-tbl' size param non-numeric or "
                                "less than 1");

      if (numTableSpecs == MAX_TABLES_SIZED-1) {
        cerr << "Too many tables specified; recompile file <sizeall.cpp> with "
             << " larger value for MAX_TABLES_SIZED."
             << endl ;
      }
      tableSpecList[numTableSpecs] = new tableSpec(ownerName, tableName,
                                                   tableSize) ;
      numTableSpecs++ ;
    }

    else if (tt_strcasecmp(argv[i], "-v") == 0) {
      verbose = true;
    }

    else {
      syntax(argv[i]);
    }

  } // end for

  if (!dsn_specified) {

	sprintf (connStr, "dsn=%s;%s", DEMODSN, UID) ;
  }

  // if nothing else specified, do -list output, by default
  if (!listOnly && (numTableSpecs==0))
    listOnly = true;

  // Command line is parsed.  Connect to data store

  return ;
}

//----------------------------------------------------------------------

int
calcSize(TTConnection & conn, const char * owner, const char * table, int & rows)
{
  TTCmd ttSize ;
  TTCmd nRows ;

  char ttsize_str [128] ;
  char nrows_str  [512] ;
  double size_bytes = 0 ;

  int  num_rows;

  // Get the number of rows from tables which the connected user can select from
  sprintf(nrows_str,
		  "select st.numtups "
          "from all_objects ao, sys.tables st "
          "where ao.object_type = 'TABLE' "
          "and ao.object_name = rtrim(st.tblname, ' ') "
          "and ao.owner = rtrim(st.tblowner, ' ') "
		  "and st.tblowner = '%s' "
		  "and st.tblname = '%s' ",
          owner, table) ;
  
  if (rows < 0) {
    sprintf(ttsize_str, "{call ttSize ('%s.%s', NULL, NULL)}", owner, table) ;
  }
  else {
    sprintf(ttsize_str, "{call ttSize ('%s.%s', %d, NULL)}", owner, table, rows) ;
  }

  try {
    // First make sure the object is contained in SYS.TABLES (and you can select from it)
    nRows.Prepare(&conn, nrows_str) ;
    nRows.Execute() ;
    // FetchNext returns 0 for success, 1 for SQL_NO_DATA_FOUND
    if ( !nRows.FetchNext() ) {
      nRows.getColumn(1, (SQLINTEGER*)&num_rows) ; 
    }
    else {
      return -1; // not present in SYS.TABLES --> it's a view
    }

    ttSize.Prepare(&conn, ttsize_str) ;
    ttSize.Execute() ;
    // FetchNext returns 0 for success, 1 for SQL_NO_DATA_FOUND
    if ( !ttSize.FetchNext() ) {
      ttSize.getColumn(1, &size_bytes) ;
      if (rows < 0) {
        rows = num_rows;
      }
    }
    else {
      // $$$ fatal error!!! ttSize should always return a row!!!
    }
    conn.Commit() ;
  }
  catch (TTStatus st) {
    cerr << "Error calling ttSize built-in procedure : " << st << endl ;
    TerminateGracefully(conn);
  }

  return (int) (size_bytes / 1000) ; // size in KB
}

//----------------------------------------------------------------------

void
TerminateGracefully (TTConnection & conn)
{
  try {

    cerr << "Rolling back active transaction...";
    conn.Rollback();

    cerr << "Disconnecting...";
    conn.Disconnect() ;

  }
  catch (TTStatus st) {
    cerr << "Error disconnecting : " << st << endl ;
  }

  cerr << "Exiting..." << endl ;
  exit(1);
}

//----------------------------------------------------------------------

int
main (int argc, char ** argv)
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  // parse args
  parse_args (argc, argv) ;
  cerr << endl << "Connecting to TimesTen <" << connStr << ">" << endl ;
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  ofstream output("sizeall.txt");
  TTGlobal::setLogStream(output) ;
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

  TTConnection conn ;

  try {
    conn.Connect(connStr, TTConnection::DRIVER_COMPLETE);
  }
  catch (TTWarning st) {
    cerr << "Warning connecting to TimesTen: " << st << endl;
  }
  catch (TTError st) {
    cerr << "Error connecting to TimesTen: " << st << endl;
    exit(-1);
  }

  if ( verbose )
    cerr << "Connected -- now looking at the system catalog ..." << endl ;

  TTCatalog cat (&conn) ;

  try {
    cat.fetchCatalogData() ;
  }
  catch (TTError st) {
    cerr << "Error using cat.fetchCatalogData() : " << st << endl;
    exit(-1);
  }

  int tbl_size = 0, total_size = 0, num_rows, num_tables ;
  int size_width = 10, rows_width = 10 ;

  // ---------------------------------------------------------
  // print header
  // ---------------------------------------------------------

  cout << endl ;
  cout.width(size_width) ;
  cout << "size (KB)" ;
  cout.width(rows_width) ;
  cout << "# rows" ;
  cout << "  table name" << endl ;
  cout.width(size_width) ;
  cout << "---------" ;
  cout.width(rows_width) ;
  cout << "---------" ;
  cout << "  --------------------------------------" << endl ;

  // ---------------------------------------------------------
  // case 1: list only
  // ---------------------------------------------------------

  if ( listOnly ) {

    int i ;
    int num_cat_tables = cat.getNumTables() ;

    num_tables = num_cat_tables ;

    for ( i = 0 ; i < num_cat_tables && !StopRequested() ; i++ )
    {
      const TTCatalogTable & table = cat.getTable(i) ;

      // Ignore system tables unless user has indicated interest
      // and only show the stats for tables you have the priv to see
      if (!listSys)
        if (    ( tt_strcasecmp(table.getTableOwner(), "SYS")       == 0)
             || ( tt_strcasecmp(table.getTableOwner(), "TTREP")     == 0)
		    ) {
          num_tables-- ;
          continue;
        }

      num_rows = -1 ;
      tbl_size = calcSize (conn, table.getTableOwner(),
                           table.getTableName(), num_rows) ;
      if (tbl_size < 0) { // it's a view
        num_tables--;
        continue;
      }

      total_size += tbl_size ;
	  total_rows += num_rows;
      cout.width(size_width) ;
      cout << tbl_size ;
      cout.width(rows_width) ;
      cout << num_rows
           << "  "
           << table.getTableOwner() << "."
           << table.getTableName() << endl ;
    }
  } // listOnly

  // ---------------------------------------------------------
  // case 2 : querying for specific tables' sizes
  // ---------------------------------------------------------

  else {

    num_tables = numTableSpecs ;

    for ( int i = 0 ; i < numTableSpecs && !StopRequested(); i++ )
    {
      tableSpec * table = tableSpecList[i] ;

      // first, verify this table exists in database
      int cat_table_idx = cat.getTableIndex(table->ownerName, table->tableName) ;

      if ( cat_table_idx < 0 ) { // table is not in the database!
        num_rows = 0 ;
        tbl_size = 0 ;
      }
      else {
        num_rows = table->tableRows ;
        tbl_size = calcSize (conn, table->ownerName,
                             table->tableName, num_rows) ;
        if (tbl_size < 0) { // it's a view
          num_tables--;
          continue;
        }
      }
      total_size += tbl_size ;

      cout.width(size_width) ;
      cout << tbl_size ;
      cout.width(rows_width) ;
      cout << num_rows
           << "  "
           << table->ownerName << "."
           << table->tableName ;
      if ( cat_table_idx < 0 )
        cout << " *** not in database ***" ;
      cout << endl ;
    }

  } // !listOnly

  // ---------------------------------------------------------
  // print footer
  // ---------------------------------------------------------

  cout.width(size_width) ;
  cout << "---------" ;
  cout.width(rows_width) ;
  cout << "---------" ;
  cout << "  --------------------------------------" << endl ;
  cout.width(size_width) ;
  cout << total_size ;
  cout.width(rows_width) ;
  cout << total_rows ;
  cout << " " ;
  cout << "  " << num_tables << " tables listed" << endl << endl;

  // No need to clean up here when we catch a signal, since the following
  // code does the required cleanup.

  try {
    conn.Disconnect();
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting from TimesTen:" << endl << st;
    exit(1);
  }

  return 0 ;
}




