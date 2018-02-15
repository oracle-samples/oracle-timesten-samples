//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// changemon_multi.cpp: demonstrating usage of the TTClasses XLA functionality
// --> for multiple XLA tables
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

#include <ttclasses/TTInclude.h>
#include <ttclasses/TTXla.h>
#include "testprog_utils.h"
#include "tt_version.h"

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

TTXlaPersistConnection  conn;
const int FETCH_WAIT_SECS = 5 ;

void TerminateGracefully(TTXlaPersistConnection&);

//----------------------------------------------------------------------
// These functions create the ICF and ABC tables.
//----------------------------------------------------------------------

// "ICF" for int-char-float
void
createICFTable(char * tableName, TTConnection* cP)
{
  char scratch[512] ;

  TTCmd cmd;

  sprintf(scratch, "delete from %s", tableName) ;
  try {
    cmd.ExecuteImmediate(cP, scratch);
    cP->Commit() ;
  }
  catch (TTStatus st) {
    // cerr << "Info: " << st << endl ;
    st.resetErrors(true) ; // ignore all errors
  }

}

// "ABC" for "all types"
void
createABCTable(char * tableName, TTConnection* cP)
{
  char scratch[512] ;

  TTCmd cmd;

  try {
    sprintf(scratch, "delete from %s", tableName) ;
    cmd.ExecuteImmediate(cP, scratch);
    cP->Commit() ;
  }
  catch (TTStatus st) {
    // cerr << "Info: " << st << endl ;
    st.resetErrors(true) ; // ignore all errors
  }

}

//----------------------------------------------------------------------
// This class contains all the logic to be implemented whenever
// the MYTABLE table is changed.  This is the table that is
// created by "sample.cpp", one of the other TTClasses demos.
// That application should be executed before this one in order to
// create and populate the table.
//----------------------------------------------------------------------

class SampleHandler: public TTXlaTableHandler {
private:
  int Value;
  int Name;

protected:

public:
  SampleHandler(TTXlaPersistConnection& conn, const char* ownerP, const char* nameP);
  ~SampleHandler();

  virtual void HandleDelete(ttXlaUpdateDesc_t*);
  virtual void HandleInsert(ttXlaUpdateDesc_t*);
  virtual void HandleUpdate(ttXlaUpdateDesc_t*);
};

SampleHandler::SampleHandler(TTXlaPersistConnection& connection,
                             const char* ownerP, const char* nameP) :
  TTXlaTableHandler(connection, ownerP, nameP)
{
  Name = -1;
  Value = -1;

  // We are looking for two particular named columns.  We need to get
  // the ordinal position of the columns by name for later use.

  Value = tbl.getColNumber("I");
  if (Value < 0) {
    cerr << "target table has no 'I' column" << endl;
    TerminateGracefully(connection);
  }

  Name = tbl.getColNumber("NAME");
  if (Name < 0) {
    cerr << "target table has no 'NAME' column" << endl;
    TerminateGracefully(connection);
  }

}

SampleHandler::~SampleHandler()
{

}

void SampleHandler::HandleInsert(ttXlaUpdateDesc_t*)
{
  char* nameP;
  int   new_i;

  row.Get(Name, &nameP);
  cerr << "Name '" << nameP << "': Inserted ";
  if (row.isNull(Value) == true) {
    cerr << "<NULL>" << endl;
  }
  else {
    row.Get(Value, &new_i);
    cerr <<  new_i << endl;
  }
}

void SampleHandler::HandleUpdate(ttXlaUpdateDesc_t* )
{
  char* nameP;
  int   old_i;
  int   new_i;

  row.Get(Name, &nameP);
  cerr << "Name '" << nameP << "': Updated ";
  if (row.isNull(Value) == true) {
    cerr << "<NULL>";
  }
  else {
    row.Get(Value, &old_i);
    cerr << old_i;
  }
  cerr << " to ";
  if (row2.isNull(Value) == true) {
    cerr << "<NULL>";
  }
  else {
    row2.Get(Value, &new_i);
    cerr << new_i;
  }
  cerr << endl;

#ifdef TT_UPDATED_COLUMN_PRINTOUT
  cerr << row2.numUpdatedCols() << " columns updated: " << endl ;
  cerr << "columns " ;
  for ( int i = 1 ; i <= row2.numUpdatedCols() ; i++ )
  {
    cerr << row2.updatedCol(i) << "("
         << row2.getColumn(row2.updatedCol(i)-1)->getColName()
         << ")" ;
  }
  cerr << endl ;
#endif /* TT_UPDATED_COLUMN_PRINTOUT */
}

void SampleHandler::HandleDelete(ttXlaUpdateDesc_t* )
{
  char* nameP;
  int   old_i;

  row.Get(Name, &nameP);
  cerr << "Name '" << nameP << "': Deleted ";
  if (row.isNull(Value) == true) {
    cerr << "<NULL>" << endl;
  }
  else {
    row.Get(Value, &old_i);
    cerr << old_i << endl;
  }
}


//----------------------------------------------------------------------
// This class contains all the logic to be executed whenever the
// ICF table is updated.  The ICF table is created by this application,
// and is intended to:
// (1) Demonstrate how to use all supported data types
// (2) How to handle updates to two different tables in one application
//----------------------------------------------------------------------

class ICFHandler: public TTXlaTableHandler {
private:
  int I;                        // The ordinal position of the I column
  int C;                        // The ordinal position of the C column
  int F;                        // The ordinal position of the F column

protected:

public:
  ICFHandler(TTXlaPersistConnection& conn, const char* ownerP, const char* nameP);
  ~ICFHandler();

  virtual void HandleDelete(ttXlaUpdateDesc_t*);
  virtual void HandleInsert(ttXlaUpdateDesc_t*);
  virtual void HandleUpdate(ttXlaUpdateDesc_t*);
};

ICFHandler::ICFHandler(TTXlaPersistConnection& connection,
                       const char* ownerP, const char* nameP) :
  TTXlaTableHandler(connection, ownerP, nameP)
{
  I = C = F = -1 ;

  // We are looking for several particular named columns.  We need to get
  // the ordinal position of the columns by name for later use.

  I = tbl.getColNumber("I");
  if (I < 0) {
    cerr << "target table has no 'I' column" << endl;
    TerminateGracefully(connection);
  }

  C = tbl.getColNumber("C");
  if (C < 0) {
    cerr << "target table has no 'C' column" << endl;
    TerminateGracefully(connection);
  }

  F = tbl.getColNumber("F");
  if (F < 0) {
    cerr << "target table has no 'F' column" << endl;
    TerminateGracefully(connection);
  }

}

ICFHandler::~ICFHandler()
{ }

void ICFHandler::HandleInsert(ttXlaUpdateDesc_t* )
{
  int    i;
  char* cP;
  double f;

  row.Get(I, &i);
  row.Get(C, &cP);
  row.Get(F, &f);

  cerr << "Inserted: " << i << " / '" << cP << "' / " << f << endl;
}

void ICFHandler::HandleUpdate(ttXlaUpdateDesc_t* )
{
#ifdef TT_UPDATED_COLUMN_PRINTOUT
  cerr << row2.numUpdatedCols() << " columns updated: " << endl ;
  cerr << "columns " ;
  for ( int i = 1 ; i <= row2.numUpdatedCols() ; i++ )
  {
    cerr << row2.updatedCol(i) << "("
         << row2.getColumn(row2.updatedCol(i)-1)->getColName()
         << ")" ;
  }
  cerr << endl ;
#endif /* TT_UPDATED_COLUMN_PRINTOUT */
}

void ICFHandler::HandleDelete(ttXlaUpdateDesc_t* )
{
  return;                             // Ignore deletes for now
}



//----------------------------------------------------------------------
// This class contains all the logic to be executed whenever the
// ABC table is updated.  The ABC table is created by this application,
// and is intended to:
// (1) Demonstrate how to use all supported data types
// (2) How to handle updates to three different tables in one application
//----------------------------------------------------------------------

class ABCHandler: public TTXlaTableHandler {
private:

  // these ints contain the ordinal position of the respective columns
  int DOUBLE_, REAL_, TINYINT_, SMALLINT_, TIME_, DATE_, TIMESTAMP_, BINARY_,
      NCHAR_, NVARCHAR2_, VARBINARY_, VARCHAR2_, NUMBER_, NUMBER_TWO_ ;

protected:

public:
  ABCHandler(TTXlaPersistConnection& conn, const char* ownerP, const char* nameP);
  ~ABCHandler();

  virtual void HandleDelete(ttXlaUpdateDesc_t*);
  virtual void HandleInsert(ttXlaUpdateDesc_t*);
  virtual void HandleUpdate(ttXlaUpdateDesc_t*);
};

ABCHandler::ABCHandler(TTXlaPersistConnection& connection,
                       const char* ownerP, const char* nameP) :
  TTXlaTableHandler(connection, ownerP, nameP)
{
  DOUBLE_ = REAL_     = TINYINT_ = SMALLINT_  = TIME_    = DATE_    = TIMESTAMP_
          = NVARCHAR2_ = BINARY_  = VARBINARY_ = VARCHAR2_ = NUMBER_ = NUMBER_TWO_   = -1 ;

  // We are looking for several particularly named columns.  We need to get
  // the ordinal position of the columns by name for later use.

  DOUBLE_ = tbl.getColNumber("DOUBLE_");
  if (DOUBLE_ < 0) {
    cerr << "target table has no 'DOUBLE_' column" << endl;
    TerminateGracefully(connection);
  }

  REAL_ = tbl.getColNumber("REAL_");
  if (REAL_ < 0) {
    cerr << "target table has no 'REAL_' column" << endl;
    TerminateGracefully(connection);
  }

  TINYINT_ = tbl.getColNumber("TINYINT_");
  if (TINYINT_ < 0) {
    cerr << "target table has no 'TINYINT_' column" << endl;
    TerminateGracefully(connection);
  }

  SMALLINT_ = tbl.getColNumber("SMALLINT_");
  if (SMALLINT_ < 0) {
    cerr << "target table has no 'SMALLINT_' column" << endl;
    TerminateGracefully(connection);
  }

  TIME_ = tbl.getColNumber("TIME_");
  if (TIME_ < 0) {
    cerr << "target table has no 'TIME_' column" << endl;
    TerminateGracefully(connection);
  }

  DATE_ = tbl.getColNumber("DATE_");
  if (DATE_ < 0) {
    cerr << "target table has no 'DATE_' column" << endl;
    TerminateGracefully(connection);
  }

  TIMESTAMP_ = tbl.getColNumber("TIMESTAMP_");
  if (TIMESTAMP_ < 0) {
    cerr << "target table has no 'TIMESTAMP_' column" << endl;
    TerminateGracefully(connection);
  }

  BINARY_ = tbl.getColNumber("BINARY_");
  if (BINARY_ < 0) {
    cerr << "target table has no 'BINARY_' column" << endl;
    TerminateGracefully(connection);
  }

  VARBINARY_ = tbl.getColNumber("VARBINARY_");
  if (VARBINARY_ < 0) {
    cerr << "target table has no 'VARBINARY_' column" << endl;
    TerminateGracefully(connection);
  }

  VARCHAR2_ = tbl.getColNumber("VARCHAR2_");
  if (VARCHAR2_ < 0) {
    cerr << "target table has no 'VARCHAR2_' column" << endl;
    TerminateGracefully(connection);
  }
  
  NUMBER_ = tbl.getColNumber("NUMBER_");
  if (NUMBER_ < 0) {
    cerr << "target table has no 'NUMBER_' column" << endl;
    TerminateGracefully(connection);
  }

  NUMBER_TWO_ = tbl.getColNumber("NUMBER_TWO_");
  if (NUMBER_TWO_ < 0) {
    cerr << "target table has no 'NUMBER_TWO_' column" << endl;
    TerminateGracefully(connection);
  }

  NCHAR_ = tbl.getColNumber("NCHAR_");
  if (NCHAR_ < 0) {
    cerr << "target table has no 'NCHAR_' column" << endl;
    TerminateGracefully(connection);
  }

  NVARCHAR2_ = tbl.getColNumber("NVARCHAR2_");
  if (NVARCHAR2_ < 0) {
    cerr << "target table has no 'NVARCHAR2_' column" << endl;
    TerminateGracefully(connection);
  }
}

ABCHandler::~ABCHandler()
{ }

void ABCHandler::HandleInsert(ttXlaUpdateDesc_t* )
{
  double  double_ ;
  float   real_ ;
  tinyint tinyint_ ;
  short   smallint_ ;
  TIME_STRUCT time_ ;
  DATE_STRUCT date_ ;
  TIMESTAMP_STRUCT timestamp_;
  void* binaryP ; // this column is binary, so we fetch via
  int binaryLen ; // a void* with a length
  void* varbinaryP ; // ditto
  int varbinaryLen ;
  char * varcharP ;  // ditto
  char * numericP ;
  char * decimalP ;

  // Fetch all of the values out

  row.Get(DOUBLE_, &double_);
  row.Get(REAL_, &real_);
  row.Get(TINYINT_, &tinyint_);
  row.Get(SMALLINT_, &smallint_);
  if (!row.isNull(TIME_))
    row.Get(TIME_, &time_);
  if (!row.isNull(DATE_))
    row.Get(DATE_, &date_);
  if (!row.isNull(TIMESTAMP_))
    row.Get(TIMESTAMP_, &timestamp_);
  row.Get(BINARY_, &binaryP, &binaryLen);
  if (!row.isNull(VARBINARY_))
    row.Get(VARBINARY_, &varbinaryP, &varbinaryLen);
  if (!row.isNull(VARCHAR2_))
    row.Get(VARCHAR2_, &varcharP);
  if (!row.isNull(NUMBER_))
    row.Get(NUMBER_, &numericP);
  if (!row.isNull(NUMBER_TWO_))
    row.Get(NUMBER_TWO_, &decimalP);

  // print the inserted values

  cerr << "Inserted: " ;

  cerr << double_ << " / " << real_ << " / " << (int) tinyint_ << " / " << smallint_
       << endl;
  cerr << "Time: " ;
  if ( row.isNull(TIME_) )
    cerr << "<NULL>" ;
  else
    cerr << time_.hour << ":" << time_.minute << ":" << time_.second;
  cerr << endl ;
  cerr << "Date: " ;
  if ( row.isNull(DATE_) )
    cerr << "<NULL>" ;
  else
    cerr << date_.year << "/" << date_.month << "/" << date_.day ;
  cerr << endl ;
  cerr << "Timestamp: " << timestamp_.year << "/" << timestamp_.month
       << "/" << timestamp_.day << " | " << timestamp_.hour << ":"
       << timestamp_.minute << ":" << timestamp_.second << "." << timestamp_.fraction
       << endl ;
  if ( row.isNull(BINARY_) )
    cerr << " Binary: <NULL>" ;
  else
    cerr << "Binary (len: " << binaryLen << ")" ;
  if ( row.isNull(VARBINARY_) )
    cerr << " / Varbinary: <NULL>" ;
  else
    cerr << " / Varbinary (len: "
         << varbinaryLen << ")" ;
  if ( row.isNull(VARCHAR2_) )
    cerr << " / <NULL VARCHAR>" ;
  else
    cerr << " / '" << varcharP << "'" ;
  cerr << endl ;
  cerr << "Number(11): " ;
  if ( row.isNull(NUMBER_) )
    cerr << "<NULL>" ;
  else
    cerr << numericP ;
  if ( row.isNull(NUMBER_TWO_) )
    cerr << " / Number(17,5): <NULL>" ;
  else
    cerr << " / Number(17,5): " << decimalP ;
  cerr << endl ;

}

void ABCHandler::HandleUpdate(ttXlaUpdateDesc_t* )
{
  double  double_ ;
  float   real_ ;
  tinyint tinyint_ ;
  short   smallint_ ;
  TIME_STRUCT time_ ;
  DATE_STRUCT date_ ;
  TIMESTAMP_STRUCT timestamp_;
  void* binaryP ; // this column is binary, so we fetch via
  int binaryLen ; // a void* with a length
  void* varbinaryP ; // ditto
  int varbinaryLen ;
  char * varcharP ;  // ditto
  int ncharLen ;
  SQLWCHAR * ncharP; // ditto
  int nvarcharLen ;
  SQLWCHAR * nvarcharP; // ditto
  char * numericP ;
  char * decimalP ;

  // Fetch all of the old values out

  row.Get(DOUBLE_, &double_);
  row.Get(REAL_, &real_);
  row.Get(TINYINT_, &tinyint_);
  row.Get(SMALLINT_, &smallint_);
  if (!row.isNull(TIME_))       row.Get(TIME_,      &time_);
  if (!row.isNull(DATE_))       row.Get(DATE_,      &date_);
  if (!row.isNull(TIMESTAMP_))  row.Get(TIMESTAMP_, &timestamp_);
  row.Get(BINARY_, &binaryP, &binaryLen);
  if (!row.isNull(VARBINARY_))  row.Get(VARBINARY_, &varbinaryP, &varbinaryLen);
  if (!row.isNull(VARCHAR2_))   row.Get(VARCHAR2_,   &varcharP);
  if (!row.isNull(NUMBER_))     row.Get(NUMBER_,   &numericP);
  if (!row.isNull(NUMBER_TWO_)) row.Get(NUMBER_TWO_,   &decimalP);
  if (!row.isNull(NVARCHAR2_))  row.Get(NVARCHAR2_,  &nvarcharP, &nvarcharLen);
  if (!row.isNull(NCHAR_))      row.Get(NCHAR_,     &ncharP,    &ncharLen);

  cerr << "Old values: "
       << endl ;
  cerr << "-----------------------------------------------------------------------"
       << endl ;
  cerr << double_ << " / " << real_ << " / " << (int) tinyint_ << " / " << smallint_
       << endl;
  cerr << "Time: " ;
  if ( row.isNull(TIME_) )
    cerr << "<NULL>" ;
  else
    cerr << time_.hour << ":" << time_.minute << ":" << time_.second;
  cerr << endl ;
  cerr << "Date: " ;
  if ( row.isNull(DATE_) )
    cerr << "<NULL>" ;
  else
    cerr << date_.year << "/" << date_.month << "/" << date_.day ;
  cerr << endl ;
  cerr << "Timestamp: " << timestamp_.year << "/" << timestamp_.month
       << "/" << timestamp_.day << " | " << timestamp_.hour << ":"
       << timestamp_.minute << ":" << timestamp_.second << "." << timestamp_.fraction
       << endl ;
  if ( row.isNull(BINARY_) )
    cerr << " Binary: <NULL>" ;
  else
    cerr << "Binary (len: " << binaryLen << ")" ;
  if ( row.isNull(VARBINARY_) )
    cerr << " / Varbinary: <NULL>" ;
  else
    cerr << " / Varbinary (len: "
         << varbinaryLen << ")" ;
  if ( row.isNull(VARCHAR2_) )
    cerr << " / <NULL VARCHAR2>" ;
  else
    cerr << " / '" << varcharP << "'" ;
  cerr << endl ;
  cerr << "Number(11): " ;
  if ( row.isNull(NUMBER_) )
    cerr << "<NULL>" ;
  else
    cerr << numericP ;
  if ( row.isNull(NUMBER_TWO_) )
    cerr << " / Number(17,5): <NULL>" ;
  else
    cerr << " / Number(17,5): " << decimalP ;
  cerr << endl;
  if ( row.isNull(NCHAR_) )
    cerr << "NCHAR: <NULL>" ;
  else {
    cerr << "NCHAR: ";
    sqlwcharToStream(ncharP, ncharLen, cerr) ;
  }
  if ( row.isNull(NVARCHAR2_) )
    cerr << " / NVARCHAR2: <NULL>" ;
  else {
    cerr << " / NVARCHAR2: ";
    sqlwcharToStream(nvarcharP, nvarcharLen, cerr) ;
  }
  cerr << endl ;

  // Now fetch the new values

  row2.Get(DOUBLE_, &double_);
  row2.Get(REAL_, &real_);
  row2.Get(TINYINT_, &tinyint_);
  row2.Get(SMALLINT_, &smallint_);
  if (!row2.isNull(TIME_))       row2.Get(TIME_,      &time_);
  if (!row2.isNull(DATE_))       row2.Get(DATE_,      &date_);
  if (!row2.isNull(TIMESTAMP_))  row2.Get(TIMESTAMP_, &timestamp_);
  row2.Get(BINARY_, &binaryP, &binaryLen);
  if (!row2.isNull(VARBINARY_))  row2.Get(VARBINARY_, &varbinaryP, &varbinaryLen);
  if (!row2.isNull(VARCHAR2_))   row2.Get(VARCHAR2_,   &varcharP);
  if (!row2.isNull(NUMBER_))     row2.Get(NUMBER_,    &numericP);
  if (!row2.isNull(NUMBER_TWO_)) row2.Get(NUMBER_TWO_,&decimalP);
  if (!row2.isNull(NCHAR_))      row2.Get(NCHAR_,     &ncharP,    &ncharLen);
  if (!row2.isNull(NVARCHAR2_))  row2.Get(NVARCHAR2_,  &nvarcharP, &nvarcharLen);

  cerr << "-----------------------------------------------------------------------"
       << endl ;
  cerr << double_ << " / " << real_ << " / " << (int) tinyint_ << " / " << smallint_
       << endl;
  cerr << "Time: " ;
  if ( row2.isNull(TIME_) )
    cerr << "<NULL>" ;
  else
    cerr << time_.hour << ":" << time_.minute << ":" << time_.second;
  cerr << endl ;
  cerr << "Date: " ;
  if ( row2.isNull(DATE_) )
    cerr << "<NULL>" ;
  else
    cerr << date_.year << "/" << date_.month << "/" << date_.day ;
  cerr << endl ;
  cerr << "Timestamp: " << timestamp_.year << "/" << timestamp_.month
       << "/" << timestamp_.day << " | " << timestamp_.hour << ":"
       << timestamp_.minute << ":" << timestamp_.second << "." << timestamp_.fraction
       << endl ;
  if ( row2.isNull(BINARY_) )
    cerr << " Binary: <NULL>" ;
  else
    cerr << "Binary (len: " << binaryLen << ")" ;
  if ( row2.isNull(VARBINARY_) )
    cerr << " / Varbinary: <NULL>" ;
  else
    cerr << " / Varbinary (len: "
         << varbinaryLen << ")" ;
  if ( row2.isNull(VARCHAR2_) )
    cerr << " / <NULL VARCHAR2>" ;
  else
    cerr << " / '" << varcharP << "'" ;
  cerr << endl ;
  cerr << "Number(11): " ;
  if ( row2.isNull(NUMBER_) )
    cerr << "<NULL>" ;
  else
    cerr << numericP ;
  if ( row2.isNull(NUMBER_TWO_) )
    cerr << " / Number(17,5): <NULL>" ;
  else
    cerr << " / Number(17,5): " << decimalP ;
  cerr << endl;
  if ( row2.isNull(NCHAR_) )
    cerr << "NCHAR: <NULL>" ;
  else {
    cerr << "NCHAR: ";
    sqlwcharToStream(ncharP, ncharLen, cerr) ;
  }
  if ( row2.isNull(NVARCHAR2_) )
    cerr << " / NVARCHAR2: <NULL>" ;
  else {
    cerr << " / NVARCHAR2: ";
    sqlwcharToStream(nvarcharP, nvarcharLen, cerr) ;
  }
  cerr << endl ;
  cerr << "****************************************************************"
       << endl ;

#ifdef TT_UPDATED_COLUMN_PRINTOUT
  cerr << row2.numUpdatedCols() << " columns updated: " << endl ;
  cerr << "columns " ;
  for ( int i = 1 ; i <= row2.numUpdatedCols() ; i++ )
  {
    cerr << row2.updatedCol(i) << "("
         << row2.getColumn(row2.updatedCol(i)-1)->getColName()
         << ")" ;
  }
  cerr << endl ;
#endif /* TT_UPDATED_COLUMN_PRINTOUT */

}

void ABCHandler::HandleDelete(ttXlaUpdateDesc_t* )
{
  return;                         // This example program does not handle deletes (arbitrary decision)
}

//----------------------------------------------------------------------

void
TerminateGracefully (TTXlaPersistConnection & connection)
{
  cerr << "Disconnecting...";

  try {
    connection.Disconnect() ;
  }
  catch (TTError st) {
    cerr << "Error Disconnecting: " << st << endl ;
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
" xlasubscriber2 [<DSN> | -connstr <connection string>]\n"
"\noptions:\n";

//----------------------------------------------------------------------

int
main(int argc, char* argv[])
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  ParamParser parser (usage_string);
  char connStr[256];
  char owner_connStr[256];

  if (1 == argc) {

    // Default the DSN and UID 
    sprintf(connStr, "dsn=%s;%s", DEMODSN, XLAUID);
  } else {

    // old behaviour 
    connStr[0] = '\0';;
  }

  // Default the table owners DSN + USERNAME
  sprintf(owner_connStr, "dsn=%s;%s", DEMODSN, UID);


  parser.processArgs(argc, argv, connStr);
  cerr << endl << "Connecting to TimesTen <" << connStr << ">" << endl << endl;
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  ofstream output ("xlasubscriber2.txt");
  TTGlobal::setLogStream(output);
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

  char ICFtableName [30] ; // int, char, float
  char ABCtableName [30] ; // all other types

  bool twoReps = false ; // should we apply every XLA record twice (by default, no)

  // unique-ifying the bookmark name
  int pid = getpid();
  char bookmarkName[32] ;


  sprintf(ICFtableName, "ICF") ;
  sprintf(ABCtableName, "ABC") ;
  sprintf(bookmarkName, "xlasubscriber2_%d", pid);

  TTXlaTableList list(&conn);  // List of tables to monitor

  // Handlers, one for each table we want to monitor

  SampleHandler* sampP = NULL;
  ICFHandler*    icfP  = NULL;
  ABCHandler*    abcP  = NULL;

  // Misc stuff
  ttXlaUpdateDesc_t ** arry;

  int j, records;


  // Now that the tables are created, we go ahead and start up the
  // change monitoring application

  try {
    conn.Connect(connStr, TTConnection::DRIVER_COMPLETE, bookmarkName);
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

  cout << endl << "Connected to " << connStr << endl << endl;

  cout << 
"This application monitors tables (MYTABLE, " << ICFtableName << " and " << ABCtableName << ")\n"
"for changes and reports such changes as they occur.  To run it, \n"
"in one window run this application.  In another window run \n"
"\n"
"      ttIsql -connStr " << owner_connStr << "\n"
"\n"
"to make changes to these tables.  The changes will be visible as \n"
"output from this application : "
       << endl << endl;



  // Make a handler to process changes to the MYTABLE table and
  // add the handler to the list of all handlers

  try {
    sampP = new SampleHandler(conn, UIDNAME, XLA_TABLE);
    if (!sampP) {
      cerr << "Could not create SampleHandler" << endl;
      TerminateGracefully(conn);
    }
    list.add(sampP);
  }
  catch (TTStatus st) {
    cerr << "Error creating SampleHandler -- most likely, you didn't run the "
         << endl
         << "'sample' program before running 'changemon'." << endl ;
    TerminateGracefully(conn);
  }

  // Enable transaction logging for the table we're interested in

  sampP->EnableTracking();

  // Repeat for the ICF table

  icfP = new ICFHandler(conn, UIDNAME, ICFtableName) ;
  if (!icfP) {
    cerr << "Could not create ICFHandler" << endl;
  }
  list.add(icfP);
  icfP->EnableTracking();

  // Repeat for the ABC table

  abcP = new ABCHandler(conn, UIDNAME, ABCtableName) ;
  if (!abcP) {
    cerr << "Could not create ABCHandler" << endl;
  }
  list.add(abcP);
  abcP->EnableTracking();

  // Get updates.  Dispatch them to the appropriate handler.
  // This loop will handle updates to all the tables.

  try {
    while (!StopRequested()) {

      if ( twoReps )
        // Trying out the TT 4.3 XLA functionality to get/set bookmark indexes
        conn.getBookmarkIndex () ;
      
      try {
        conn.fetchUpdatesWait(&arry, 100, &records, FETCH_WAIT_SECS);
      }
      catch (TTStatus st) {
        if (st.rc) {
          cerr << "Error fetching updates" << st << endl;
          TerminateGracefully(conn);
        }
      }

      // Interpret the updates
      for(j=0;j < records;j++){
        ttXlaUpdateDesc_t *p;
        p = arry[j];
        list.HandleChange(p);
      } // end for each record fetched

      if ( twoReps )
        {
          // restore to saved bookmark, so we can fetch those same updates again
          conn.setBookmarkIndex() ;
          try {
	    conn.fetchUpdatesWait(&arry, 100, &records, FETCH_WAIT_SECS);
          } 
          catch (TTStatus st) {
            if (st.rc) {
              cerr << "Error fetching updates" << st << endl;
              TerminateGracefully(conn);
            }
          }

          // Interpret the updates
          for(j=0;j < records;j++){
            ttXlaUpdateDesc_t *p;
            p = arry[j];
            list.HandleChange(p);
          } // end for each record fetched
        }

      if (records) {
        cerr << "Processed " << records << " records" << endl ;
      }

      // Need to "acknowledge updates", even if no XLA records were
      // received, in order to move the XLA bookmark forward.
      conn.ackUpdates() ;

    } // end while !StopRequested()
  } // try
  catch (TTStatus st) {
    cerr << "Exception caught : " << st << endl ;
    cerr << "Aborting ..." << endl ;
  }

  // No need to clean up here when we catch a signal, since the following
  // code does the required cleanup.

  // When we get to here, the program is exiting.

  try {
    list.del(sampP);              // Take the table out of the list
    sampP->DisableTracking();
    delete sampP;
  } // try
  catch (TTStatus st) {
    cerr << "Could not disable tracking for samp : " << st << endl ;
  }

  try {
    list.del(icfP);
    icfP->DisableTracking() ;
    delete icfP;
  } // try
  catch (TTStatus st) {
    cerr << "Could not disable tracking for icf : " << st << endl ;
  }

  try {
    list.del(abcP);
    abcP->DisableTracking();
    delete abcP;
  } // try
  catch (TTStatus st) {
    cerr << "Could not disable tracking for abc : " << st << endl ;
  }


  try {
    conn.deleteBookmarkAndDisconnect () ;  // clean up this bookmark
  } // try
  catch (TTStatus st) {
    cerr << "Could not delete bookmark: " << st << endl ;
  }

  return 0;
}


