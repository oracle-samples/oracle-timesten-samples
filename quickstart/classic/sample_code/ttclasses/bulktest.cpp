//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// bulktest.cpp: testing the bulk insert/update/delete functionality
// in TTClasses and TimesTen
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

//----------------------------------------------------------------------
// important global values

const int NROWS = 10000;
const int MAXLOOPS = 10;
const int MAX_BATCH_SIZE = 512;
int BATCH_SIZE = 256;
int BATCH_ITERS = NROWS / BATCH_SIZE;

//----------------------------------------------------------------------

#define CHAR_COLUMN_SIZE 10
#define CHAR_COLUMN_SIZE_DDL "10"
#define VARCHAR_COLUMN_SIZE 100
#define VARCHAR_COLUMN_SIZE_DDL "150"
#define BINARY_COLUMN_SIZE 12
#define BINARY_COLUMN_SIZE_DDL "16"
#define VARBINARY_COLUMN_SIZE 100
#define VARBINARY_COLUMN_SIZE_DDL "160"
#define NUMBER_COLUMN_SIZE 65

class BulkConnection : public TTConnection {

using TTConnection::Connect;
using TTConnection::Disconnect;

private:
  TTCmd        dropTable;
  TTCmd        createTable;

  TTCmd        insertData;
  TTCmd        insertDataBulk;

  TTCmd        updateData;
  TTCmd        updateDataBulk;

  TTCmd        deleteData;
  TTCmd        deleteDataBulk;

  TTCmd        deleteAll;

  // the insert bulk params' arrays

  SQLTINYINT       insArray1_ti[MAX_BATCH_SIZE];
  short            insArray2_si[MAX_BATCH_SIZE];
  int              insArray3_i[MAX_BATCH_SIZE];
  SQLBIGINT        insArray4_bi[MAX_BATCH_SIZE];
  float            insArray5_f[MAX_BATCH_SIZE];
  double           insArray6_d[MAX_BATCH_SIZE];
  char             insArray7_c[MAX_BATCH_SIZE][CHAR_COLUMN_SIZE+1];
  char             insArray8_vc[MAX_BATCH_SIZE][VARCHAR_COLUMN_SIZE+1];
  char             insArray9_b[MAX_BATCH_SIZE][BINARY_COLUMN_SIZE+1];
  SQLLEN           insArray9_blen[MAX_BATCH_SIZE]; // the length array
  char             insArrayA_vb[MAX_BATCH_SIZE][VARBINARY_COLUMN_SIZE+1];
  SQLLEN           insArrayA_vblen[MAX_BATCH_SIZE]; // the length array
  TIMESTAMP_STRUCT insArrayB_tss[MAX_BATCH_SIZE];
  DATE_STRUCT      insArrayC_ds[MAX_BATCH_SIZE];
  TIME_STRUCT      insArrayD_ts[MAX_BATCH_SIZE];
  char             insArrayE_num9[MAX_BATCH_SIZE][NUMBER_COLUMN_SIZE+1];
  char             insArrayF_num17_2[MAX_BATCH_SIZE][NUMBER_COLUMN_SIZE+1];

  // the update bulk params' arrays
  SQLTINYINT       updArray1_ti[MAX_BATCH_SIZE];

  short            updArray2_si[MAX_BATCH_SIZE];
  int              updArray3_i[MAX_BATCH_SIZE];
  SQLBIGINT        updArray4_bi[MAX_BATCH_SIZE];
  float            updArray5_f[MAX_BATCH_SIZE];
  double           updArray6_d[MAX_BATCH_SIZE];
  char             updArray7_c[MAX_BATCH_SIZE][CHAR_COLUMN_SIZE+1];
  char             updArray8_vc[MAX_BATCH_SIZE][VARCHAR_COLUMN_SIZE+1];
  char             updArray9_num9[MAX_BATCH_SIZE][NUMBER_COLUMN_SIZE+1];
  char             updArrayA_num17_2[MAX_BATCH_SIZE][NUMBER_COLUMN_SIZE+1];

  // the delete bulk params' arrays
  int              delArray3_i[MAX_BATCH_SIZE];

protected:

public:
  BulkConnection();
  ~BulkConnection();

  virtual void Connect(const char* connStr, TTConnection::DRIVER_COMPLETION_ENUM driverCompletion);
  virtual void Disconnect();

  void create();
  void deleteAllRows();

  void bindBulkParams();
  void initBulkParams(void);

  void insertRow(int i, char* nameP, TIMESTAMP_STRUCT&, DATE_STRUCT&, TIME_STRUCT&);
  void insertNonPrepRow(int i, char* nameP, TIMESTAMP_STRUCT&, DATE_STRUCT&, TIME_STRUCT&);
  void insertBatch(int i, char* nameP);

  void updateRow(int i, char* nameP);
  void updateNonPrepRow(int i, char* nameP);
  void updateBatch(int i, char* nameP);

  void deleteRow(int i);
  void deleteNonPrepRow(int i);
  void deleteBatch(int i);

  void TerminateIfRequested();
};

//----------------------------------------------------------------------

BulkConnection::BulkConnection()
{
}

//----------------------------------------------------------------------

BulkConnection::~BulkConnection()
{
  try {
    Disconnect();
  }
  catch (TTStatus st) {
    cerr << "Destructor - Error disconnecting :" << endl << st << endl ;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::Disconnect()
{
  try {

    createTable.Drop();

    insertData.Drop();
    insertDataBulk.Drop();

    updateData.Drop();
    updateDataBulk.Drop();

    deleteData.Drop();
    deleteDataBulk.Drop();

    deleteAll.Drop();

    TTConnection::Disconnect() ;
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting :" << endl << st << endl ;
  }
}

//----------------------------------------------------------------------

const char * insert_sql =
"insert into bulktest values(?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?)";

const char * update_sql =
"update bulktest set ti=?,si=?,bi=?,f=?,d=?,c=?,vc=?,num9=?,num17_2=? where i=?";

const char * delete_sql =
"delete from bulktest where i=?";

//----------------------------------------------------------------------

void
BulkConnection::Connect(const char* connStr, TTConnection::DRIVER_COMPLETION_ENUM driverCompletion)
{

  try {
    try {
      TTConnection::Connect(connStr, driverCompletion);
    }
    catch (TTStatus st) {
      if (st.rc != SQL_SUCCESS_WITH_INFO)
        throw st;
        return;
    }

    try {
      dropTable.ExecuteImmediate(this, "drop table bulktest");
    }
    catch (TTStatus st) {
      // Ignore errors .. table might legitimately not exist.
    }
    char crtbl[1024];
    const char *tblString =
      "create table bulktest ("
      " ti    tt_tinyint"
      " ,si   tt_smallint"
      " ,i    tt_integer not null primary key"
      " ,bi   tt_bigint"
      " ,f    binary_float "
      " ,d    binary_double "
      " ,c    char(" CHAR_COLUMN_SIZE_DDL ") "
      " ,vc   varchar(" VARCHAR_COLUMN_SIZE_DDL ") "
      " ,b    binary(" BINARY_COLUMN_SIZE_DDL ") "
      " ,vb   varbinary(" VARBINARY_COLUMN_SIZE_DDL ") "
      " ,tss  timestamp "
      " ,ds   tt_date "
      " ,ts   time"
      " ,num9     number(9)"
      " ,num17_2  number(17,2)"
      ") unique hash on (i) pages=%d";
    sprintf (crtbl, tblString, NROWS/256);
    createTable.Prepare(this, crtbl);
    create();  // Create the table if it doesn't exist

    insertData.Prepare(this, insert_sql);

    insertDataBulk.PrepareBatch(this, insert_sql,
                                TTCmd::TTCMD_USER_BIND_PARAMS,
                                BATCH_SIZE );

    updateData.Prepare(this, update_sql);

    updateDataBulk.PrepareBatch(this, update_sql,
                                TTCmd::TTCMD_USER_BIND_PARAMS,
                                BATCH_SIZE );

    deleteData.Prepare(this, delete_sql);

    deleteDataBulk.PrepareBatch(this, delete_sql,
                                TTCmd::TTCMD_USER_BIND_PARAMS,
                                BATCH_SIZE );

    deleteAll.Prepare(this, "truncate table bulktest");

    Commit();
  }
  catch (TTStatus st) {
    cerr << "Error in BulkConnection::Connect: " << st << endl ;
    Rollback();
    throw st;
    return;
  }

  return;
}

//----------------------------------------------------------------------

void
BulkConnection::create()
{
  try {
    createTable.Execute();
    Commit();
  }
  catch (TTStatus st) {
    cerr << "Error creating table : " << st << endl ;
    throw st;
    return;
  }

}

//----------------------------------------------------------------------

void
BulkConnection::deleteAllRows()
{
  try {
    deleteAll.Execute();
    Commit();
    ttMillisleep(500);
  }
  catch (TTStatus st) {
    cerr << "Error deleting all rows : " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::bindBulkParams()
{
  try {
    // bulk insert params

    insertDataBulk.BindParameter(1, BATCH_SIZE, insArray1_ti);
    insertDataBulk.BindParameter(2, BATCH_SIZE, insArray2_si);
    insertDataBulk.BindParameter(3, BATCH_SIZE, insArray3_i);
    insertDataBulk.BindParameter(4, BATCH_SIZE, insArray4_bi);
    insertDataBulk.BindParameter(5, BATCH_SIZE, insArray5_f);
    insertDataBulk.BindParameter(6, BATCH_SIZE, insArray6_d);
    insertDataBulk.BindParameter(7, BATCH_SIZE, (char**)insArray7_c,
                                 CHAR_COLUMN_SIZE);
    insertDataBulk.BindParameter(8, BATCH_SIZE, (char**)insArray8_vc,
                                 VARCHAR_COLUMN_SIZE);
    insertDataBulk.BindParameter(9, BATCH_SIZE, (const void**)insArray9_b,
                                 insArray9_blen, BINARY_COLUMN_SIZE);
    insertDataBulk.BindParameter(10, BATCH_SIZE, (const void**)insArrayA_vb,
                                 insArrayA_vblen, VARBINARY_COLUMN_SIZE);
    insertDataBulk.BindParameter(11, BATCH_SIZE, insArrayB_tss);
    insertDataBulk.BindParameter(12, BATCH_SIZE, insArrayC_ds);
    insertDataBulk.BindParameter(13, BATCH_SIZE, insArrayD_ts);
    insertDataBulk.BindParameter(14, BATCH_SIZE, (char**)insArrayE_num9, 
                                 NUMBER_COLUMN_SIZE);
    insertDataBulk.BindParameter(15, BATCH_SIZE, (char**)insArrayF_num17_2, 
                                 NUMBER_COLUMN_SIZE);

    // bulk update params

    updateDataBulk.BindParameter(10, BATCH_SIZE, updArray3_i); // keyval

    updateDataBulk.BindParameter(1, BATCH_SIZE, updArray1_ti);
    updateDataBulk.BindParameter(2, BATCH_SIZE, updArray2_si);
    updateDataBulk.BindParameter(3, BATCH_SIZE, updArray4_bi);
    updateDataBulk.BindParameter(4, BATCH_SIZE, updArray5_f);
    updateDataBulk.BindParameter(5, BATCH_SIZE, updArray6_d);
    updateDataBulk.BindParameter(6, BATCH_SIZE, (char**)updArray7_c,
                                 CHAR_COLUMN_SIZE);
    updateDataBulk.BindParameter(7, BATCH_SIZE, (char**)updArray8_vc,
                                 VARCHAR_COLUMN_SIZE);
    updateDataBulk.BindParameter(8, BATCH_SIZE, (char**)updArray9_num9,
                                 NUMBER_COLUMN_SIZE);
    updateDataBulk.BindParameter(9, BATCH_SIZE, (char**)updArrayA_num17_2,
                                 NUMBER_COLUMN_SIZE);

    // bulk delete params

    deleteDataBulk.BindParameter(1, BATCH_SIZE, delArray3_i);
  }
  catch (TTStatus st) {
    cerr << "Bind error in bindBulkParams: " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::initBulkParams(void)
{
  // first init the globals used by the non-bulk ops
  TIMESTAMP_STRUCT tss;
  tss.year = 2000; tss.month = 12; tss.day = 25;
  tss.hour = 12; tss.minute = 30; tss.second = 59; tss.fraction = 0;

  DATE_STRUCT ds;
  ds.year = 2000; ds.month = 12; ds.day = 25;

  TIME_STRUCT ts;
  ts.hour = 12; ts.minute = 30; ts.second = 59;

  // now set the TIMESTAMP, DATE, TIME param arrays to be something reasonable
  for (int i = 0; i < BATCH_SIZE; i++)
  {
    insArrayB_tss[i] = tss;
    insArrayC_ds[i]  = ds;
    insArrayD_ts[i]  = ts;

    // initialize the binary & varbinary len arrays
    insArray9_blen[i] = 0; // the length array
    insArrayA_vblen[i] = 0; // the length array
  }
}

//----------------------------------------------------------------------


void
BulkConnection::insertRow(int i, char* nameP, TIMESTAMP_STRUCT & tss,
                          DATE_STRUCT & ds, TIME_STRUCT & ts)
{
  char number1[32];
  char number2[32];
  sprintf(number1, "%d", i);
  sprintf(number2, "%f", i*1.11);

  tss.year=(i%25)+1981;
  ds.year=(i%25)+1981;
  ts.hour=(i%24);

  try {
    insertData.setParam(1, (i % 250) ); // ti
    insertData.setParam(2, i*2); // si
    insertData.setParam(3, i); // i
    insertData.setParam(4, i*4); // bi
    insertData.setParam(5, i*5.0); // f
    insertData.setParam(6, i*6.0); // d
    insertData.setParam(7, nameP); // c
    insertData.setParam(8, nameP); // vc
    insertData.setParam(9, nameP, strlen(nameP)); // b
    insertData.setParam(10, nameP, strlen(nameP)); // vb
    insertData.setParam(11, tss); // tss
    insertData.setParam(12, ds); // ds
    insertData.setParam(13, ts); // ts
    insertData.setParam(14, number1); // num9
    insertData.setParam(15, number2); // num17_2

    insertData.Execute();
  }
  catch (TTStatus st) {
    cerr << "Error in insertRow(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::insertNonPrepRow(int i, char* nameP, TIMESTAMP_STRUCT & tss,
                                 DATE_STRUCT & ds, TIME_STRUCT & ts)
{
  char number1[32];
  char number2[32];
  sprintf(number1, "%d", i);
  sprintf(number2, "%f", i*1.11);

  tss.year=(i%25)+1981;
  ds.year=(i%25)+1981;
  ts.hour=(i%24);

  TTCmd insertNonPrepData;

  try {
    insertNonPrepData.Prepare(this, insert_sql);

    insertNonPrepData.setParam(1, (i % 250) ); // ti
    insertNonPrepData.setParam(2, i*2); // si
    insertNonPrepData.setParam(3, i); // i
    insertNonPrepData.setParam(4, i*4); // bi
    insertNonPrepData.setParam(5, i*5.0); // f
    insertNonPrepData.setParam(6, i*6.0); // d
    insertNonPrepData.setParam(7, nameP); // c
    insertNonPrepData.setParam(8, nameP); // vc
    insertNonPrepData.setParam(9, nameP, strlen(nameP)); // b
    insertNonPrepData.setParam(10, nameP, strlen(nameP)); // vb
    insertNonPrepData.setParam(11, tss); // tss
    insertNonPrepData.setParam(12, ds); // ds
    insertNonPrepData.setParam(13, ts); // ts
    insertNonPrepData.setParam(14, number1); // num9
    insertNonPrepData.setParam(15, number2); // num17_2

    insertNonPrepData.Execute();
  }
  catch (TTStatus st) {
    cerr << "Error in insertRow(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::insertBatch(int i, char * nameP)
{
  // each BATCH operation starts at row (i*BATCH_SIZE)
  // each BATCH operation ends at row (i*BATCH_SIZE)+BATCH_SIZE-1
  int val = i*BATCH_SIZE;

  char insName[128];

  // update the parameter arrays for each iteration, then Execute
  for (int rowno = 0; rowno < BATCH_SIZE; rowno++, val++)
  {
    sprintf(insName, "%s%d", nameP, val);
    insArray1_ti[rowno] = (val % 250);
    insArray2_si[rowno] = val*2;
    insArray3_i[rowno]  = val;
    insArray4_bi[rowno] = val*4;
    insArray5_f[rowno]  = (float)val*(float)5.0;
    insArray6_d[rowno]  = val*6.0;
    strcpy(insArray7_c[rowno], insName);
    strcpy(insArray8_vc[rowno], insName);
    memcpy(insArray9_b[rowno], insName, strlen(insName));
    insArray9_blen[rowno] = strlen(insName); // the length array
    memcpy(insArrayA_vb[rowno], insName, strlen(insName));
    insArrayA_vblen[rowno] = strlen(insName); // the length array
    insArrayB_tss[rowno].year = (val%25)+1981;
    insArrayC_ds[rowno].year  = (val%25)+1981;
    insArrayD_ts[rowno].hour  = (val%24);
    sprintf(insArrayE_num9[rowno], "%d", i);
    sprintf(insArrayF_num17_2[rowno], "%f", i*1.11);

  }

  try {
    insertDataBulk.ExecuteBatch(BATCH_SIZE);
  }
  catch (TTStatus st) {
    cerr << "Error in insertBatch(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::updateRow(int i, char* nameP)
{
  char number1[32];
  char number2[32];
  sprintf(number1, "%d", i);
  sprintf(number2, "%f", i*1.11);

  try {
    updateData.setParam(10, i); // i=keyval

    updateData.setParam(1, ((i%4)+250) ); // ti
    updateData.setParam(2, i*3); // si
    //not// updateData.setParam(3, i); // i // last param
    updateData.setParam(3, i*44); // bi
    updateData.setParam(4, i*5.5); // f
    updateData.setParam(5, i*6.6); // d
    updateData.setParam(6, nameP); // c
    updateData.setParam(7, nameP); // vc
    updateData.setParam(8, number1); // i
    updateData.setParam(9, number2); // i

    updateData.Execute();
  }
  catch (TTStatus st) {
    cerr << "Error in insertBatch(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::updateNonPrepRow(int i, char* nameP)
{
  char number1[32];
  char number2[32];
  sprintf(number1, "%d", i);
  sprintf(number2, "%f", i*1.11);

  TTCmd updateNonPrepData;

  try {
    updateNonPrepData.Prepare(this, update_sql);

    updateNonPrepData.setParam(10, i); // i=keyval

    updateNonPrepData.setParam(1, ((i%4)+250) ); // ti
    updateNonPrepData.setParam(2, i*3); // si
    updateNonPrepData.setParam(3, i*44); // bi
    updateNonPrepData.setParam(4, i*5.5); // f
    updateNonPrepData.setParam(5, i*6.6); // d
    updateNonPrepData.setParam(6, nameP); // c
    updateNonPrepData.setParam(7, nameP); // vc
    updateNonPrepData.setParam(8, number1); // i
    updateNonPrepData.setParam(9, number2); // i

    updateNonPrepData.Execute();
  }
  catch (TTStatus st) {
    cerr << "Error in insertBatch(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::updateBatch(int i, char* nameP)
{
  // each BATCH operation starts at row (i*BATCH_SIZE)
  // each BATCH operation ends at row (i*BATCH_SIZE)+BATCH_SIZE-1
  int val = i*BATCH_SIZE;

  char updName[128];

  // update the parameter arrays for each iteration, then Execute
  //
  // NB: unlike in the updateRow() method above, the order that these
  // ops are performed is not important, since we already bound them
  // in the right order in the bindBulkParams() method.
  for (int rowno = 0; rowno < BATCH_SIZE; rowno++, val++)
  {
    sprintf(updName, "%s%d", nameP, val);
    updArray1_ti[rowno] = ((val%4)+250);
    updArray2_si[rowno] = val*3;
    updArray3_i[rowno]  = val; // keyval
    updArray4_bi[rowno] = val*44;
    updArray5_f[rowno]  = (float)val*(float)5.5;
    updArray6_d[rowno]  = val*6.6;
    strcpy(updArray7_c[rowno], updName);
    strcpy(updArray8_vc[rowno], updName);
    sprintf(updArray9_num9[rowno], "%d", val);
    sprintf(updArrayA_num17_2[rowno], "%f", val*1.11);
  }

  try {
    updateDataBulk.ExecuteBatch(BATCH_SIZE);
  }
  catch (TTStatus st) {
    cerr << "Error in updateBatch(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::deleteRow(int i)
{
  try {
    deleteData.setParam(1, i); // i
    deleteData.Execute();
  }
  catch (TTStatus st) {
    cerr << "Error in deleteRow(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::deleteNonPrepRow(int i)
{
  TTCmd deleteNonPrepData;

  try {
    deleteNonPrepData.Prepare(this, delete_sql);
    deleteNonPrepData.setParam(1, i); // i
    deleteNonPrepData.Execute();
  }
  catch (TTStatus st) {
    cerr << "Error in deleteNonPrepRow(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::deleteBatch(int i)
{
  // each BATCH operation starts at row (i*BATCH_SIZE)
  // each BATCH operation ends at row (i*BATCH_SIZE)+BATCH_SIZE-1
  int val = i*BATCH_SIZE;

  // update the parameter arrays for each iteration, then Execute
  for (int rowno = 0; rowno < BATCH_SIZE; rowno++, val++)
  {
    delArray3_i[rowno]  = val;
  }

  try {
    deleteDataBulk.ExecuteBatch(BATCH_SIZE);
  }
  catch (TTStatus st) {
    cerr << "Error in deleteBatch(): " << st << endl ;
    throw st;
    return;
  }
}

//----------------------------------------------------------------------

void
BulkConnection::TerminateIfRequested()
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
      cerr << "Error disconnecting : " << st << endl ;
      throw st;
      return;
    }
    cerr << "Exiting..." << endl ;
    exit(1);
  }
}

//----------------------------------------------------------------------

const char * usage_string =
" bulktest [-batchsize <n>>]\n"
"          [<DSN> | -connstr <connection string>]\n"
"\noptions:\n"
"  -batchsize <n>      Size of the batch (max is 512).\n";

//----------------------------------------------------------------------

int
main(int argc, char** argv)
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  ParamParser parser (usage_string);
  parser.setArg("-batchsize", false /* not required */, 10);
  parser.setTwinArg("-batchsize","-batch");
  parser.setTwinArg("-batchsize","-size");
  char connStr[256];

  if (1 == argc) {

    /* Default the DSN and UID */
    sprintf(connStr, "dsn=%s;%s", DEMODSN, UID);
  } else {

    /* old behaviour */
    connStr[0] = '\0';;
  }

  parser.processArgs(argc, argv, connStr);
  if (parser.argUsed("-batchsize")) {
    BATCH_SIZE = atoi(parser.getArgValue("-batchsize"));
    if (BATCH_SIZE > MAX_BATCH_SIZE) {
      cerr << "Sorry, maximum batchsize=" << MAX_BATCH_SIZE << endl;
      exit(1);
    }
    else if (BATCH_SIZE < 1) {
      cerr << "Sorry, minimum batchsize=1." << endl;
      exit(1);
    }
  }
  BATCH_ITERS = NROWS / BATCH_SIZE ;
  cerr << endl << "Connecting to TimesTen <" << connStr << ">" << endl ;
  cerr << "Batch size = " << BATCH_SIZE << endl;
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  ofstream output ("bulktest.txt");
  TTGlobal::setLogStream(output) ;
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

  try {

    int i;
    char name[100];
    BulkConnection conn;

    double          ikernel, iuser, ireal;
    ttWallClockTime wstart, wend;
    ttThreadTimes   tstart, tend;

    cerr << "Creating database / connecting to it ..." << endl;

    try {
      conn.Connect(connStr, TTConnection::DRIVER_COMPLETE);
    }
    catch (TTStatus st) {
      cerr << "Error connecting to TimesTen " << st << endl ;
      if (st.rc != SQL_SUCCESS_WITH_INFO)
        exit(-1);
    }

    cerr << "Connected" << endl << endl ;

    cerr << "-------------------------------------------" << endl ;
    cerr << "Preparing bulk operations' bindings ..." << endl ;
    cerr << "-------------------------------------------" << endl ;

    // first init the globals used by the non-bulk ops
    TIMESTAMP_STRUCT tss;
    tss.year = 2000; tss.month = 12; tss.day = 25;
    tss.hour = 12; tss.minute = 30; tss.second = 59; tss.fraction = 0;

    DATE_STRUCT ds;
    ds.year = 2000; ds.month = 12; ds.day = 25;

    TIME_STRUCT ts;
    ts.hour = 12; ts.minute = 30; ts.second = 59;

    // now bind & init the bulk ops arrays
    try {
      conn.bindBulkParams();
      conn.initBulkParams();
    }
    catch (TTStatus st) {
      cerr << "Error while setting up bulk params:" << endl << st;
      throw st;
      return -1;
    }

    cerr << "-------------------------------------------" << endl ;
    cerr << "Starting timings." << endl ;
    cerr << "First non-bulk operations, then bulk ops." << endl ;
    cerr << "-------------------------------------------" << endl ;

    for (int outerloop = 4 ; outerloop <= 9 && !StopRequested(); outerloop++)
    {
      // outerloop runs nine times:
      // loop 1: non-prepared non-bulk insert
      // loop 2: non-prepared non-bulk update
      // loop 3: non-prepared non-bulk delete
      // loop 4: non-bulk insert
      // loop 5: non-bulk update
      // loop 6: non-bulk delete
      // loop 7: bulk insert
      // loop 8: bulk update
      // loop 9: bulk delete

      char outerloop_type[64];

      switch (outerloop) {
      case 1: strcpy(outerloop_type,"(not-prepared-in-advance) insert") ; break ;
      case 2: strcpy(outerloop_type,"(not-prepared-in-advance) update") ; break ;
      case 3: strcpy(outerloop_type,"(not-prepared-in-advance) delete") ; break ;
      case 4: strcpy(outerloop_type,"(non-bulk) insert") ; break ;
      case 5: strcpy(outerloop_type,"(non-bulk) update") ; break ;
      case 6: strcpy(outerloop_type,"(non-bulk) delete") ; break ;
      case 7: strcpy(outerloop_type,"bulk insert") ; break ;
      case 8: strcpy(outerloop_type,"bulk update") ; break ;
      case 9: strcpy(outerloop_type,"bulk delete") ; break ;
      default: break;
      }

      cerr << "Starting timing of " << outerloop_type << " operations." << endl ;
      cerr << "-------------------------------------------------------" << endl ;

      double ireal_total = 0.0 ;

      try {

      for (int loop = 0; loop < MAXLOOPS && !StopRequested(); loop++)
      {
        // -----------------------------------------------------------------
        // pre-iter init
        // -----------------------------------------------------------------
        // if this is an insert iteration, delete pre-existing rows first
        if (outerloop == 1 || outerloop == 4 || outerloop == 7)
          conn.deleteAllRows() ;

        // if this is a delete operation, make sure to insert rows first
        if (outerloop == 3 || outerloop == 6 || outerloop == 9) {
          // just in case, delete first, then insert
          // --> since we don't know if an insert operation was performed
          // before now or not (insert timing might be commented out)
          conn.deleteAllRows();
          for (i = 0; i < BATCH_ITERS; i++)
          {
            sprintf(name, "Name");
            conn.insertBatch(i, name);
          }
          conn.Commit();
        }
        conn.DurableCommit(); // flush the log buffer
        ttMillisleep(1000); // let the disk catch up
        // -----------------------------------------------------------------
        // end of pre-iter init
        // -----------------------------------------------------------------

        ttGetWallClockTime(&wstart);
        ttGetThreadTimes(&tstart);

        if (outerloop <= 3) {
          // non-prepared operations

          try {
            for (i = 0; i < NROWS && !StopRequested(); i++) {
              sprintf(name, "Name%d", i);
              if (outerloop == 1)
                conn.insertNonPrepRow(i, name, tss, ds, ts);
              else if (outerloop == 2)
                conn.updateNonPrepRow(i, name);
              else // if (outerloop == 3)
                conn.deleteNonPrepRow(i);
              if (i % BATCH_SIZE == 0)
                conn.Commit();
            } // end for
          }
          catch (TTStatus st) {
            cerr << "Error in " << outerloop_type << " operation #<" << i << ">:"
                 << endl << st;
            conn.Rollback();
          }
          catch (...) {
            cerr << "Non-TTStatus exception caught ??" << endl ;
            exit(1) ;
          }

        }
        else if (outerloop <= 6) {

          // non-batch operations
          try {
            for (i = 0; i < NROWS && !StopRequested(); i++) {
              sprintf(name, "Name%d", i);
              if (outerloop == 4)
                conn.insertRow(i, name, tss, ds, ts);
              else if (outerloop == 5)
                conn.updateRow(i, name);
              else // if (outerloop == 6)
                conn.deleteRow(i);
              if (i % BATCH_SIZE == 0)
                conn.Commit();
            } // end for
          }
          catch (TTStatus st) {
            cerr << "Error in " << outerloop_type << " operation #<" << i << ">:"
                 << endl << st;
            conn.Rollback();
          }
          catch (...) {
            cerr << "Non-TTStatus exception caught, wtf?!!" << endl ;
            exit(1) ;
          }
        }
        else { // outerloop in {7,8,9}
          // bulk operations
          try {
            for (i = 0; i < BATCH_ITERS && !StopRequested(); i++) {
              sprintf(name, "Name");
              if (outerloop == 7)
                conn.insertBatch(i, name);
              else if (outerloop == 8)
                conn.updateBatch(i, name);
              else // if (outerloop == 9)
                conn.deleteBatch(i);
              conn.Commit();
            } // end for
          }
          catch (TTStatus st) {
            cerr << "Error in " << outerloop_type << " operation #<" << i << ">:"
                 << endl << st;
            conn.Rollback();
          }
        }

        conn.TerminateIfRequested();

        conn.Commit();

        ttGetThreadTimes(&tend);
        ttGetWallClockTime(&wend);
        ttCalcElapsedThreadTimes(&tstart, &tend, &ikernel, &iuser);
        ttCalcElapsedWallClockTime(&wstart, &wend, &ireal);
        fprintf(stderr,
                "Test time %.3f sec (%.3f user, %.3f sys) - "
                "%.2f SQL ops per sec\n",
                ireal / 1000.0, iuser, ikernel, NROWS / (ireal / 1000.0));

        ireal_total += ireal ;

      } // end for loop

      } catch (TTStatus st) {
        cerr << "Unexpected error: " << st << endl ;
      }

      conn.TerminateIfRequested();

      cerr << "-------------------------------------------" << endl ;
      cerr << "Average rate for " << outerloop_type << ": "
           << ((NROWS * MAXLOOPS) / (ireal_total / 1000.0)) << endl ;
      cerr << "-------------------------------------------" << endl ;


    } // outerloop

    cerr << "...end of timings" << endl ;

    // No need to clean up here when we catch a signal, since the following
    // code does the required cleanup.

    cerr << "Disconnecting ... ";

    try {
      conn.Disconnect();
    }
    catch (TTStatus st) {
      cerr << "Error disconnecting from TimesTen:" << endl << st;
      exit(1);
    }

    cerr << "disconnected" << endl << endl ;

  } // initial try block

  catch (TTStatus st) {
    cerr << "Error received:" << endl << st;
  }
  catch (...) {
    cerr << "UNEXPECTED EXCEPTION CAUGHT" << endl ;
  }

  return 0;
}
