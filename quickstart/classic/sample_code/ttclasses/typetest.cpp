//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// typetest.cpp: Verify that the datatypes all work
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
#endif
#include <ttclasses/TTInclude.h>
#include "testprog_utils.h"
#include "tt_version.h"

void loadUnicodeColumn (int i, SQLWCHAR * unicode, int * size) ;
void loadBinaryColumn (void * binary, int size) ;
void printBinaryColumn (void * binary, int size) ;

//----------------------------------------------------------------------

class TypetestConnection : public TTConnection {

using TTConnection::Connect;
using TTConnection::Disconnect;

private:
  TTCmd        dropTable;
  TTCmd        createTable;
  TTCmd        insertData;
  TTCmd        queryData;

protected:

public:
  TypetestConnection();
  virtual ~TypetestConnection();
  virtual void Connect(const char* connStr, TTConnection::DRIVER_COMPLETION_ENUM driverCompletion);
  virtual void Disconnect() ;
  void create();
  void insert(char* nameP);
  void query(const char* nameP);

};

//----------------------------------------------------------------------

TypetestConnection::TypetestConnection()
{
}

//----------------------------------------------------------------------

TypetestConnection::~TypetestConnection()
{
  try {
    Disconnect();
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting from TimesTen:\n" << st << endl;
  }
}

//----------------------------------------------------------------------

void
TypetestConnection::Disconnect()
{
  try {
    createTable.Drop();
    insertData.Drop();
    queryData.Drop();

    TTConnection::Disconnect();
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting: " << st << endl;
  }
}

//----------------------------------------------------------------------

void
TypetestConnection::Connect(const char* connStr, TTConnection::DRIVER_COMPLETION_ENUM driverCompletion)
{

  try {

    try {
      TTConnection::Connect(connStr, driverCompletion);
    }
    catch (TTStatus st) {
      if (st.rc != SQL_SUCCESS_WITH_INFO) {
        cerr << "Error connecting: " << st << endl;
        throw st;
        return;
      }
    }

    cerr << endl << "About to try to drop table typetest - it may not exist." << endl;

    try {
      dropTable.ExecuteImmediate(this, "drop table typetest");
    }
    catch (TTStatus st) {
      // Ignore errors, table might legitimately not exist.
    }

    cerr << endl << "Preparing the create table statement " << endl;

    try {
      createTable.Prepare(this,
                        "create table typetest "
                        "("
                        " name   char(10) not null primary key, "   /* 1 */
                        " i      tt_integer, "                          
                        " ti     tt_tinyint, "                      /* 3 */
                        " si     tt_smallint, "                         
                        " bi     tt_bigint, "                           
                        " oda    ora_date, "                        /* 6 */
                        " c      char(10), "                         
                        " vcil   varchar2(50), "
                        " vcool  varchar2(200), "                   /* 9 */
                        " nc     nchar(10), "                        
                        " nvcil  nvarchar2(50), "                     
                        " nvcool nvarchar2(1000), "                 /* 12 */
                        " b      binary(8), "
                        " vbil   varbinary(8), "
                        " vbool  varbinary(200), "                  /* 15 */
                        " ts     timestamp, "
                        " f      binary_float, "
                        " d      binary_double, "                   /* 18 */
                        " dt     tt_date, "
                        " tm     time, "                             
                        " num    number, "                          /* 21 */
                        " num2   number(2), "
                        " num4   number(4), "
                        " num9   number(9), "                       /* 24 */
                        " num18  number(18), "
                        " num11_2 number(11,2)"                     /* 26 */
                        ") "                  
                        "unique hash on (name) pages=100" );

    }
    catch (TTStatus st) {
      cerr << "Error preparing create table:" << st << endl;
      throw st;
      return;
    }

    try {
      create();                      // Create the table if it doesn't exist
    }
    catch (TTError st) {
      cerr << "Error creating table:" << st << endl;
      throw st;
      return;
    }

    cerr << endl << endl << "Preparing insert ..." << endl;

    try {
      insertData.Prepare(this,
                         "insert into typetest values(?,?,?,?,?,   ?,?,?,?,?,  "
                         "?,?,?,?,?,  ?,?,?,?,?,  ?,?,?,?,?,?"
                         ")");
    }
    catch (TTError st) {
      cerr << "Error preparing insert :" << st << endl;
      throw st;
      return;
    }

    cerr << endl << "Preparing query ..." << endl << endl;

    try {
      queryData.Prepare(this,
                        "select name, i, ti, si, bi, oda, c, vcil, vcool, "
                        "nc, nvcil, nvcool, b, vbil, vbool, "
                        "ts, f, d, dt, tm, num, num2, num4, num9, num18, "
                        "num11_2 "
                        "from typetest "
                        "where name = ?");
    }
    catch (TTError st) {
      cerr << "Error preparing select:" << st << endl;
      throw st;
      return;
    }

    try {
      Commit();
    }
    catch (TTError st) {
      cerr << "Error Committing :" << st << endl;
      throw st;
      return;
    }

  }
  catch (TTStatus st) {
    cerr << "Error in TypetestConnection::Connect: " << st << endl ;
    Rollback();
    throw st;
    return;
  }

  return;
}

//----------------------------------------------------------------------

void
TypetestConnection::create()
{
  try {
    createTable.Execute();
    Commit();
  }
  catch (TTStatus st) {
    cerr << "Error creating table: " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
TypetestConnection::insert(char* nameP)
{
  static SQLBIGINT i = 100000;
  static char bufc[11];
  static char bufb[8];

  SQLWCHAR bufwc[100];
  int bufwc_len;

  TIMESTAMP_STRUCT ts;
  ts.year = 2000;
  ts.month = 12;
  ts.day = 1;
  ts.hour = 1;
  ts.minute = 1;
  ts.second = 3;
  ts.fraction = 123456789;

  DATE_STRUCT dt;
  dt.year = 2000;
  dt.month = 9;
  dt.day = 6;

  TIME_STRUCT tim;
  tim.hour = 12;
  tim.minute = 5;
  tim.second = 59;


  try {

  insertData.setParam(1, nameP);

  if (i % 2 == 0) {
    insertData.setParam(2, (int)i);
    insertData.setParam(2, (long)i); // to make sure setParam(long) works
    insertData.setParam(3, (SQLTINYINT)i%255);
    insertData.setParam(4, (short)i%32767);
    insertData.setParam(5, i*i*i);
    insertData.setParam(6, ts);

    sprintf((char*) &bufc, "DD%d", (int)i);
    insertData.setParam(7, (char*) &bufc);
    insertData.setParam(8, (char*) &bufc);
    insertData.setParam(9, (char*) &bufc);

    loadUnicodeColumn((int)i, bufwc, &bufwc_len);
    insertData.setParam(10, (SQLWCHAR*) &bufwc, bufwc_len);
    insertData.setParam(11, (SQLWCHAR*) &bufwc, bufwc_len);
    insertData.setParam(12, (SQLWCHAR*) &bufwc, bufwc_len);

    loadBinaryColumn((void*)bufb, 8) ;
    insertData.setParam(13, (void*) &bufb, 8);
    insertData.setParam(14, (void*) &bufb, 8);
    insertData.setParam(15, (void*) &bufb, 8);

    insertData.setParam(16, ts);
    insertData.setParam(17, (float)((float)(i*i*i)/-15.35));
    insertData.setParam(18, (double)((double)(i*i*i)/-15.35));
    insertData.setParam(19, dt);
    insertData.setParam(20, tim);
    insertData.setParam(21, 123456.789);
    insertData.setParam(22, 12);
    insertData.setParam(23, 1234);
    insertData.setParam(24, 123456789);
    insertData.setParam(25, TTC_INT8_CONST(1234567890123456));
    insertData.setParam(26, 1234565.89);
  }
  else {
    insertData.setParamNull(2);
    insertData.setParamNull(3);
    insertData.setParamNull(4);
    insertData.setParamNull(5);
    insertData.setParamNull(6);
    insertData.setParamNull(7);
    insertData.setParamNull(8);
    insertData.setParamNull(9);
    insertData.setParamNull(10);
    insertData.setParamNull(11);
    insertData.setParamNull(12);
    insertData.setParamNull(13);
    insertData.setParamNull(14);
    insertData.setParamNull(15);
    insertData.setParamNull(16);
    insertData.setParamNull(17);
    insertData.setParamNull(18);
    insertData.setParamNull(19);
    insertData.setParamNull(20);
    insertData.setParamNull(21);
    insertData.setParamNull(22);
    insertData.setParamNull(23);
    insertData.setParamNull(24);
    insertData.setParamNull(25);
    insertData.setParamNull(26);
  }

  insertData.Execute();

  }
  catch (TTStatus st) {
    cerr << "Error inserting :\n" << st;
    throw st;
    return;
  }

  i++;
}


void loadUnicodeColumn (int i, SQLWCHAR * wchar_buf, int * size)
{
  char buf[32];
  int len;

  sprintf((char*) &buf, "UU%d", (int)i);
  len = (int)strlen(buf);

  for (int j = 0 ; j < len ; j++ )
  {
    wchar_buf[j]   = buf[j];
  }
  wchar_buf[len]   = 0;

  *size = len*2;
}

void loadBinaryColumn (void * binary, int size)
{
  for (int j = 0 ; j < size ; j++ )
    {
      u_char booga = (u_char)(1 << j);
      memset(&((char*)binary)[j], booga, sizeof(u_char)) ;
    }
}

void printBinaryColumn (void * binary, int size)
{

  cout << "< 0x" << hex ;

  for (int j = 0 ; j < size ; j++ ) {
      cout.width(2);
      cout.fill('0');
      int t = ((u_char*)binary)[j] ;
      cout << t;
  }
  cout << ">" << dec;

}


//----------------------------------------------------------------------

void
TypetestConnection::query(const char* nameP)
{

  // for binary data retrieval
  u_char binary_array[10] ;
  long len ;

  queryData.setParam(1, nameP);

  try {
    cout << nameP << ":";

    try {
      queryData.Execute();
    }
    catch (TTStatus st) {
      cerr << "Error executing query :\n" << st << endl;
      return;
    }

    try {
      // FetchNext returns 0 for success, 1 for SQL_NO_DATA_FOUND
      if ( queryData.FetchNext() ) {
        cout << "Not found!" << endl;
        queryData.Close();
        return;
      }
    }
    catch (TTStatus st) {
      cerr << "Error fetching data:\n" << st << endl;
    }

    // -------------------------------------------------------------------
    // These lines are simply to verify that we can use all (reasonable)
    // native C++ types for TTCmd::getColumn(*).
    SQLTINYINT ti;
    SQLBIGINT bi;
    short si;
    int ii;
    long li;

    try {
      if (!queryData.isColumnNull(1))
        queryData.getColumn(2, &ii);
      if (!queryData.isColumnNull(1))
        queryData.getColumn(2, &li);
      if (!queryData.isColumnNull(2))
        queryData.getColumn(3, &ti);
      if (!queryData.isColumnNull(3))
        queryData.getColumn(4, &si);
      if (!queryData.isColumnNull(4))
        queryData.getColumn(5, &bi);

    // -------------------------------------------------------------------

    queryData.printColumn(1, cout, "<null char>") ;
    cout << ", ";
    queryData.printColumn(2, cout, "<null int>") ;
    cout << ", ";
    queryData.printColumn(3, cout, "<null tinyint>") ;
    cout << ", ";
    queryData.printColumn(4, cout, "<null smallint>") ;
    cout << ", ";
    queryData.printColumn(5, cout, "<null bigint>") ;
    cout << ", ";
    queryData.printColumn(6, cout, "<null date>") ;
    cout << ", ";
    queryData.printColumn(7, cout, "<null char>") ;
    cout << ", ";
    queryData.printColumn(8, cout, "<null varchar2 il>") ;
    cout << ", ";
    queryData.printColumn(9, cout, "<null varchar2 ool>") ;
    cout << ", ";
    queryData.printColumn(10, cout, "<null nchar>") ;
    cout << ", ";
    queryData.printColumn(11, cout, "<null nvarchar il>") ;
    cout << ", ";
    queryData.printColumn(12, cout, "<null nvarchar ool>") ;
    cout << ", ";

    if (queryData.getColumnNullable(13, binary_array, &len))
      cout << "<null binary>";
    else
      printBinaryColumn ((void*)binary_array, (int)len) ;
    cout << ", ";

    if (queryData.getColumnNullable(14, binary_array, &len))
      cout << "<null varbinary il>";
    else
      printBinaryColumn ((void*)binary_array, (int)len) ;
    cout << ", ";

    if (queryData.getColumnNullable(15, binary_array, &len))
      cout << "<null varbinary ool>";
    else
      printBinaryColumn ((void*)binary_array, (int)len) ;
    cout << ", ";

    queryData.printColumn(16, cout, "<null timestamp>") ;
    cout << ", ";
    queryData.printColumn(17, cout, "<null binary_float>") ;
    cout << ", ";
    queryData.printColumn(18, cout, "<null binary_double>") ;
    cout << ", ";
    queryData.printColumn(19, cout, "<null tt_date>") ;
    cout << ", ";
    queryData.printColumn(20, cout, "<null time>") ;
    cout << ", ";
    queryData.printColumn(21, cout, "<null NUMBER>") ;
    cout << ", ";
    queryData.printColumn(22, cout, "<null NUMBER(2)>") ;
    cout << ", ";
    queryData.printColumn(23, cout, "<null NUMBER(4)>") ;
    cout << ", ";
    queryData.printColumn(24, cout, "<null NUMBER(9)>") ;
    cout << ", ";
    queryData.printColumn(25, cout, "<null NUMBER(18)>") ;
    cout << ", ";
    queryData.printColumn(26, cout, "<null NUMBER(11,2)>") ;

    cout << endl;

    queryData.Close();

    }
    catch (TTStatus st) {
      cerr << "Error getting the data from the resultset : " << st << endl ;
      queryData.Close();
      throw st;
      return;
    }
  }
  catch (TTStatus st) {
    cerr << "Error in TypetestConnection::query : " << st << endl ;
    queryData.Close();
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

const char * usage_string =
" typetest {<DSN> | -connstr <connection string>}\n"
"\nusage:\n";

//----------------------------------------------------------------------

int
main(int argc, char** argv)
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
  cerr << endl << "Connecting to TimesTen <" << connStr << ">" << endl ;
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  TTGlobal::setLogStream(STDCERR) ;
  TTGlobal::setLogLevel(TTLog::TTLOG_WARN) ; // TTLOG_WARN: default log level
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
  // one with a loop).
  // ---------------------------------------------------------------
  int couldNotSetupSignalHandling = HandleSignals();
  if (couldNotSetupSignalHandling) {
    cerr << "Could not set up signal handling.  Aborting." << endl ;
    exit(-1);
  }
  // ---------------------------------------------------------------

  int nRows = 50; 
  int i;
  char name[100];
  TypetestConnection conn;

  cerr << "Creating database / connecting to it ... " << endl;

  try {
    conn.Connect(connStr, TTConnection::DRIVER_COMPLETE);
  }
  catch (TTStatus st) {
    cerr << "Error connecting to TimesTen " << st << endl ;
    if (st.rc != SQL_SUCCESS_WITH_INFO)
      exit(-1);
  }

  cerr << "Connected" << endl ;

  cerr << endl << "Inserting data ... " << endl;

  try {
    for (i = 0; i < nRows; i++) {
      sprintf(name, "Name%d", i);
      conn.insert(name);
    } // end for
  }
  catch (TTStatus st) {
    cerr << "Error inserting row " << name << ":" << endl << st << endl ;
    conn.Rollback();
  }

  try {
    conn.Commit();
  }
  catch (TTStatus st) {
    cerr << "Error committing : " << endl << st << endl ;
  }

  // what do we expect to see in the binary columns?
  char bufb[11] ;
  cout << endl << "Expected binary(8) contents: " ;
  loadBinaryColumn ((void*)bufb, 8) ;
  printBinaryColumn ((void*)bufb, 8) ;
  cout << endl << endl ;

  cerr << endl << "Fetching data ..." << endl << endl;

  try {
    sprintf(name, "Name%d", nRows-1);
    conn.query(name);
    sprintf(name, "Name%d", nRows-2);
    conn.query(name);
    sprintf(name, "Name%d", nRows-3);
    conn.query(name);
    sprintf(name, "Name%d", nRows-4);
    conn.query(name);
  }
  catch (TTStatus st) {
    cerr << "Error fetching " << name << ":" << endl << st << endl ;
    conn.Rollback();
  }

  try {
    conn.Commit();
  }
  catch (TTStatus st) {
    cerr << "Error committing : " << endl << st << endl ;
  }

  cerr << endl << "... end of fetch test" << endl ;

  cerr << endl << "Disconnecting ... ";


  try {
    conn.Disconnect();
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting from TimesTen:" << endl << st << endl ;
    exit(1);
  }

  cerr << "Disconnected" << endl << endl;

  return 0;
}
