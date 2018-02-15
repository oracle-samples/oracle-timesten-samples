//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// pooltest.cpp
//
// pooltest.cpp demonstrates "best practices" for a multi-threaded
// TimesTen application, including a connection pool, signal-handling, and
// ODBC error handling (including datastore invalidation error-handling).
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

#include <ttclasses/TTInclude.h>
#include "portable_thread.h"
#include "testprog_utils.h"
#include "tt_version.h"

#include <stdio.h>
#include <signal.h>

#ifdef WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#endif

#ifdef GCC
#include <stdlib.h>
#endif /* GCC */


/* Function prototypes */
int chg_echo(int echo_on);
void getPassword(const char * prompt, char * pswd, size_t len);
void erasePassword(volatile char *buf, size_t len);

#define MAX_CONNECT_STRING 1024

/* GLOBALS */
char connStr[256];
char username[MAX_USERNAME_SIZE];
char password[MAX_PASSWORD_SIZE];

char passwordPrompt[64];

// -----------------------------------------------------------------
// First, need to a connection object to make a pool of
// -----------------------------------------------------------------

class PoolConnection : public TTConnection {

using TTConnection::Connect;
using TTConnection::Disconnect;

 private:
  TTCmd fetch ;
  TTCmd insert ;
  char conn_string[MAX_CONNECT_STRING];

 public:
  PoolConnection() {}

  ~PoolConnection()
  {
    try {
      Disconnect();
    }
    catch (TTError st) {
      cerr << "Error in PoolConnection::~PoolConnection could not disconnect: " << st << endl;
    }
  }

  virtual void Connect(const char * connStr, 
                       const char * username,
                       const char * password); // below

  virtual void Disconnect() {
    fetch.Drop();
    insert.Drop();
    TTConnection::Disconnect();
  }

  void insertRow (int id, char * value)
  {
    insert.setParam(1, id);
    insert.setParam(2, value);
    insert.Execute();
    Commit();
  } // insertRow

  void fetchRow (int id, char * valueP)
  {
    char * tmp ;
    fetch.setParam(1, id);
    try {
      fetch.Execute();
      // TTCmd::FetchNext returns 0 for success, 1 for SQL_NO_DATA_FOUND
      if (!fetch.FetchNext() ) {
        fetch.getColumn(1, &tmp) ;
        strcpy(valueP, tmp);
      }
      fetch.Close();
      //No need to commit reads, but commit at least once before disconnect
    } catch (TTStatus st) {
      cerr << "Error in PoolConnection::fetchRow(" << id << ")" << endl;

      throw st;
    }
  } // fetchRow

  void handleInvalidDatabase ()
  {    
    printf ("In handleInvalidDatabase \n");

    try {
      Rollback();
      Disconnect();

      getPassword(passwordPrompt, (char *) password, sizeof(password) );
      Connect(connStr, username, password);
    }
    catch (TTStatus st) {
      cerr << "Error in handleInvalidDatabase: " << st << endl ;
      throw st;
    }
  }

};

void
PoolConnection::Connect(const char * connStr, 
                        const char * username,
                        const char * password)
{
  try {
    try {
      TTConnection::Connect(connStr, username, password);
    }
    catch (TTWarning warn) { 
      // ignore connection warnings
    }
    insert.Prepare(this, "insert into tbl values(?, ?)");
    fetch.Prepare(this, "select val from tbl where id = ?");
    Commit();
  }
  catch (TTStatus st) {
    cerr << "Error in PoolConnection::Connect: (Username + Password) " << st << endl ;
    Rollback();
    throw st;
  }
}
                     
// -----------------------------------------------------------------
// end of PoolConnection class
// -----------------------------------------------------------------

//----------------------------------------------------------------------

void createTable (TTConnection *conn, 
                  const char * connStr, 
                  const char * username, 
                  const char * password)
{
  try {
    conn->Connect(connStr, username, password);
  }
  catch (TTWarning warn) {
    // ignore connection warnings
  }
  catch (TTError st) {
    cerr << "Unable to connect to TimesTen successfully with connStr=<"
         << connStr << "> : " << st << endl ;
    cerr << "Exiting program." << endl ;
    conn = (TTConnection*)NULL;
    exit(-2);
  }

  TTCmd createTbl;
  try {
    createTbl.ExecuteImmediate(conn, "create table tbl ( "
                               "id tt_integer not null primary key,"
                               "val char(16)) unique hash on (id) pages=100");
    conn->Commit();
  }
  catch (TTStatus st) {
    if ( st.native_error == tt_ErrTableAlreadyExists ) {
      cerr << endl << endl << "Table tbl already exists; no need to create it." << endl << endl;
      // But we *do* need to delete its rows, since we're about to insert and
      // want to avoid unique constraint errors
      TTCmd deleteRows;
      try {
        deleteRows.ExecuteImmediate(conn, "delete from tbl");
        conn->Commit();
      }
      catch (TTStatus st2) {
        cerr << "Unexpected error when deleting rows: " << st2 << endl ;
        throw st2;
      }
    }
    else {
      cerr << "Unexpected error when creating table: " << st << endl ;
      throw st;
    }
  }
}

//----------------------------------------------------------------------

struct thread_param_struct {
  int id, timeout, inserts, inserters ;
  thread_param_struct (int id_, int to_, int in_, int ins_)
       : id(id_), timeout(to_), inserts(in_), inserters(ins_) {}
};

// global since the threads need direct access to it
TTConnectionPool pool;

//----------------------------------------------------------------------

extern "C" {

#ifndef _WIN32
/*
 * This sigset_t is set inside main() to set up the sigwaitThread thread
 * as a signal-blocking thread.
 */
sigset_t pooltest_sigset;

/* The Unix signal-handling thread */
void*
sigwaitThread(void*)
{
  int signum;
#ifndef GCC
  int rc =
#endif /* !GCC */
    sigwait(&pooltest_sigset, &signum);
  RequestStop(signum) ;
  fprintf(stderr, "*** Stop request noted (signal %d received); one moment please.\n", signum);
  return NULL;
}
#endif /* !_WIN32 */

}; /* extern "C" */

//----------------------------------------------------------------------

extern "C" {
void*
fetchTestThread(void* parmP)
{
  PoolConnection* cP = NULL;

  int thread_id    = ((thread_param_struct*)parmP)->id ;
  int pool_timeout = ((thread_param_struct*)parmP)->timeout ;
  int inserts      = ((thread_param_struct*)parmP)->inserts ;
  int inserters    = ((thread_param_struct*)parmP)->inserters ;
  int iters        = 0;
  int i            = 0;

  // Each thread (making inserts) inserted 'inserts' rows
  // There were 'inserters' threads (making inserts)
  // ==> thus, we can query ids in the range [0..(inserts*inserters)-1]
  int max_idval = inserts*inserters - 1;

  printf ("Fetch thread <%d> started.\n", thread_id);

  while (i < 1000000) { 
    if (StopRequested())
      break ; // out of while loop, terminating thread

    // check out a connection from the pool
    cP = (PoolConnection*) pool.getConnection(pool_timeout);

    if (!cP) {
#ifndef _WIN32
      // No condition variables on Windows
      cerr << "<Th " << thread_id << ">: "
           << "Could not check out a connection in " << pool_timeout << " ms." << endl
           << "Sleeping 25 ms and trying again." << endl ;
#endif /* _WIN32 */
      ttMillisleep(25);
      continue;
    }

    int idval = -1 ;
    char value [16+1] ;
    iters++ ;
    // Do a lookup
    try {
      // -------------------------------------------------------
      // randomize the row being fetched
      idval = rand() ;
#if defined(LINUX86)
      idval = idval % 32768;
#endif
      idval = (int) (((float)idval / RAND_MAX) * max_idval) ;
      // end of randomization
      // -------------------------------------------------------
      try {
        cP->fetchRow(idval, value);

      } 
      catch (TTStatus st) {
        if (st.rc == SQL_NO_DATA_FOUND) {
          cerr << "<Th " << thread_id << ">: "
               << "could not find row with id=" << idval << endl;
        }
      }
    }
    catch (TTStatus st) {
      if ( st.isConnectionInvalid() ) {
        cP->handleInvalidDatabase();
      }
      else {
        cerr << "<Th " << thread_id << ">: "
             << "Error in fetchTestThread: " << st << endl ;
      }
    }

    // Return the connection to the connection pool
    pool.freeConnection(cP);

    // Increment the loop
    i++;
  }

  
  printf ("<Th %d>: exiting; read <%d> rows.\n",thread_id,iters);
  return NULL;
}
}; /* extern "C" */

//----------------------------------------------------------------------
//----------------------------------------------------------------------

extern "C" {
void*
insertTestThread(void* parmP)
{
  int i ;
  int thread_id    = ((thread_param_struct*)parmP)->id ;
  int pool_timeout = ((thread_param_struct*)parmP)->timeout ;
  int inserts      = ((thread_param_struct*)parmP)->inserts ;

  PoolConnection* cP = NULL;
  int loopCount = 10;

  char value[16+1];


  printf("Insert thread <%d> started.\n", thread_id);

  // based on thread_id, we insert a certain set of records
  int first_insert = thread_id * inserts ;
  int last_insert  = thread_id * inserts + inserts - 1;

  int insert_num = first_insert;

  // only insert as many as we're supposed to
  while (insert_num <= last_insert) {
    if (StopRequested())
      break ; // out of while loop, terminating thread

    // check out a connection from the pool
    // NB: non-zero timeout specified; if this fails, note that we could
    // not check out a connection; sleep a while, then try again
    cP = (PoolConnection*) pool.getConnection(pool_timeout);

    if (!cP) {
#ifndef _WIN32
      // No condition variables on Windows
      cerr << "<Th " << thread_id << ">: "
           << "Could not check out a connection in " << pool_timeout << " ms." << endl
           << "Sleeping 25 ms and trying again." << endl ;
#endif /* _WIN32 */
      ttMillisleep(25);
      continue;
    }

    // insert only a few rows at a time
    for (i = 0; i <= loopCount; i++) {
      if (insert_num > last_insert)
        break ;
      try {
        // once we've inserted everything we were supposed to, stop inserting
        sprintf(value, "val_%d", insert_num);
        cP->insertRow(insert_num++, value);
      }
      catch (TTStatus st) {
        if ( st.isConnectionInvalid() ) {
          cP->handleInvalidDatabase();
        }
        else {
          cerr << "<Th" << thread_id << ">: "
               << "Error in insertTestThread while inserting #< "
               << insert_num-1 << ">: " << st << endl ;
        }
      }
    } // end for

    // return the connection to the pool
    pool.freeConnection(cP);
  } // end while

  printf ("<Th %d>: exiting.\n", thread_id);
  return NULL;
}
}; /* extern "C" */

//----------------------------------------------------------------------

const char * usage_string =
" pooltest [-insert <nThreads>] [-fetch <nThreads>] [-conn <nConn>] [-iter <nIters>]\n"
"          [<DSN> | -connstr <connection string>]\n"
"\noptions:\n"
"  -insert  nThreads   # of insert threads (default 1)\n"
"  -fetch   nThreads   # of fetch threads (default 1)\n"
"  -conn    nConn      # of connections in pool (default 1)\n"
"  -iter    nIters     # of inserts per inserter thread (default 100)\n";

//----------------------------------------------------------------------

int
main(int argc, char** argv)
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  ParamParser parser (usage_string);
  int nInserters = 2 ;            // default is two insert threads
  int nFetchers  = 2 ;            // default is two fetch threads
  int nConn = 1;                  // default is one database connection
  int inserts = 100 ;             // default is 100 inserts per insert thread
  // ------------------------------------------------------
  parser.setArg("-insert", false, 10);
  parser.setArg("-fetch", false, 10);
  parser.setArg("-conn", false, 10);
  parser.setArg("-iter", false, 10);
  parser.setArg("-logfile", false /* not required */, 25);

  if (1 == argc) {

    /* Default the DSN and UID */
    sprintf(connStr, "dsn=%s;%s", DEMODSN, UID);
  } else {

    /* old behaviour */
    connStr[0] = '\0';
  }

  parser.processArgs(argc, argv, connStr);

  // Now verify the parameters' values
  bool quit = false ; // quit if invalid values for some parameters
  if (parser.argUsed("-insert")) {
    nInserters = atoi(parser.getArgValue("-insert"));
    if (nInserters < 1) {
      cerr << "Error using -insert: cannot have fewer than one inserter thread." << endl;
      quit=true;
    }
  }
  if (parser.argUsed("-fetch")) {
    nFetchers = atoi(parser.getArgValue("-fetch"));
    if (nFetchers < 1) {
      cerr << "Error using -fetch: cannot have fewer than one fetcher thread." << endl;
      quit=true;
    }
  }
  if (parser.argUsed("-conn")) {
    nConn = atoi(parser.getArgValue("-conn"));
    if (nConn < 1) {
      cerr << "Error using -conn: cannot have fewer than one database connection." << endl;
      quit=true;
    }
  }
  if (parser.argUsed("-iter")) {
    inserts = atoi(parser.getArgValue("-iter"));
    if (inserts < 1) {
      cerr << "Error using -iter: cannot have fewer than one insert per inserter." << endl;
      quit=true;
    }
  }

  if (quit) {
    exit(-1);
  }

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  ofstream output;
  if (parser.argUsed("-logfile")) {
    output.open(parser.getArgValue("-logfile"));
  }
  else {
    output.open("pooltest.log");
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
  // Use signal() on Windows to block these signals
  // ---------------------------------------------------------------
#ifdef _WIN32
  int couldNotSetupSignalHandling = HandleSignals();
  if (couldNotSetupSignalHandling) {
    cerr << "Could not set up signal handling.  Aborting." << endl;
    exit(-1);
  }
#else
  // ---------------------------------------------------------------
  // On Unix, use sigwait() to create a signal-blocking thread (a more
  // robust signal-handling solution for multi-threaded programs).
  // ---------------------------------------------------------------
  sigemptyset(&pooltest_sigset);
  sigaddset(&pooltest_sigset, SIGINT);
  pthread_sigmask(SIG_BLOCK, &pooltest_sigset, NULL);

  portable_thread sig_waiter;
  PortableThreadCreate((thread_signature)sigwaitThread, 0 /* use default stack size */,
                       (void*) NULL /* no arg to thread */, &sig_waiter);
#endif /* _WIN32 */
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // pseudo-randomize things by looking at the current seconds value in
  // the system clock
  // ---------------------------------------------------------------
  ttWallClockTime     seed_time ;
  ttGetWallClockTime (&seed_time) ;
#ifdef _WIN32
  int seed = seed_time.notSoLargeTime.time % 1003 ;
#else
  int seed = seed_time.tv_sec % 1003 ;
#endif /* _WIN32 */
  srand(seed) ;
  int i, rc;
  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // make sure that user-supplied values are reasonable
  // ---------------------------------------------------------------
  const int MAX_INSERTERS = 50 ;
  const int MAX_FETCHERS  = 50 ;
  const int MAX_CONNS = 16 ;
  const int MAX_INSERTS = 10000 ; // (inserts per insert thread)

  // user-specified value too high?
  nConn      = (nConn      > MAX_CONNS)     ? MAX_CONNS     : nConn ;
  nInserters = (nInserters > MAX_INSERTERS) ? MAX_INSERTERS : nInserters ;
  nFetchers  = (nFetchers  > MAX_FETCHERS)  ? MAX_FETCHERS  : nFetchers ;
  inserts    = (inserts    > MAX_INSERTS)   ? MAX_INSERTS   : inserts ;

#ifdef _WIN32
  // Since we don't have condition variables on windows, we see thread
  // starvation (that is, inability to check out a connection from the
  // connection pool) when there is more than a 2:1 ratio between fetch
  // threads and connections -- so bound 'nFetchers' accordingly.
  nFetchers  = (nFetchers  > nConn * 2)     ? nConn*2       : nFetchers;
#endif /* _WIN32 */
  
  printf("\n");
  printf("Number of connections = %d\n", nConn);
  printf("Number of insert threads = %d\n", nInserters);
  printf("Number of fetch threads = %d\n", nFetchers);
  printf("Number of inserts per inserter = %d\n", inserts);
  // ---------------------------------------------------------------

  portable_thread tid[MAX_INSERTERS+MAX_FETCHERS];
  PoolConnection connP[MAX_CONNS];
  TTConnection   conn;

  /* Default the username */
  strcpy((char *) username, UIDNAME);
  
  /* Assign password prompt */
  strcpy(passwordPrompt, "Enter password for ");

  /* get the password */
  getPassword(passwordPrompt, password, sizeof(password));
  printf("\n\n");
  
  printf("Connecting to TimesTen as %s ...", connStr);
  // Before we do anything else, don't forget to create the table!
  // NB: (This routine also serves to validate the connStr parameter.)
  createTable (&conn, connStr, username, password);

  // Make a number of connection objects and put them in the connection
  // pool. They are not connected to TimesTen until the method
  // TTConnectionPool::ConnectAll() is called.
  for (i = 0; i < nConn; i++) {
    rc = pool.AddConnectionToPool(&(connP[i]));
    if (rc) {
      cerr << "Error adding connection <" << i << "> to pool" << endl ;
      break;
    }
  }

  // Now connect all connections to the database.
  
  printf("\nCreating the connection pool ... ");
  pool.ConnectAll(connStr, username, password);
  printf("done connecting.\n");

  // Now get rid of the password as it is no longer needed
  erasePassword(password, sizeof(password));

  thread_param_struct * p;

  // First we run the insert threads, and let them do their work
  printf("\n");
  for ( i = 0; i < nInserters; i++)
  {
    printf("Spawning insert thread: <%d>\n", i);
    p = new thread_param_struct (i, 100 /* timeout */, inserts, nInserters) ;
    rc = PortableThreadCreate((thread_signature)insertTestThread, 0 /* use default stack size */,
                              (void*) p, &tid[i]);
    if ( rc ) {
      cerr << "Error -- could not create thread <" << i << ">: "
           << strerror(errno) << endl ;
      break;
    }
  }
  printf("\n");

  // Wait for all inserters to finish before we spawn the fetchers
  // (NB: StopRequested() may be true)
  for ( i = 0; i < nInserters; i++ )
  {
    if ( PortableThreadJoin(tid[i], NULL, NULL) != 0)
      cerr << "No thread <" << i << "> to join." << endl ;
    printf("Thread <%d> exited.\n", i);
  }

  printf("\nInsert threads all finished their work.\n");
  printf("\nEach fetch thread will now do 1 million queries or until stopped by <CTRL-C>.\n\n");

  // Now spawn the fetch threads, and let them run indefinitely until
  // we're interrupted (be sure to check StopRequested(), to make sure we
  // haven't already been interrupted)
  printf("\n");
  for ( i = nInserters; i < nInserters+nFetchers && !StopRequested(); i++ )
  {
    printf("Spawning fetch thread: <%d>\n", i);
    p = new thread_param_struct (i, 1000 /* timeout */, inserts, nInserters) ;
    rc = PortableThreadCreate((thread_signature)fetchTestThread, 0 /* use default stack size */,
                          (void*) p, &tid[i]);
    if ( rc ) {
      cerr << "Error -- could not create thread <" << i << ">: "
           << strerror(errno) << endl ;
      break;
    }
  }
  printf("\n");

  // The fetch threads will commit suicide shortly after StopRequested()
  // returns true (i.e., when Ctrl-C is pressed), and not before.
  for ( i = nInserters; i < nInserters+nFetchers; i++ ) {
    if ( PortableThreadJoin(tid[i], NULL, NULL) != 0)
      cerr << "No thread <" << i << "> to join." << endl ;
    printf("Thread <%d> exited.\n", i);
  }

  // Disconnect all the connections from the database; then deallocate them
  // (since they were dynamically allocated in the first place)
  printf("Disconnecting...\n");
  pool.DisconnectAll();
  try {
    conn.Commit();
    conn.Disconnect();
  }
  catch (TTStatus st) {
    cerr << "Error committing/disconnecting in main: " << st << endl ;
  }  

  // All done
  printf("Test Completed.\n");

  return 0;
}


/* Turn on and off echoing text to the console */
int chg_echo(int echo_on)
{
#ifdef WIN32

   HANDLE h;
   DWORD mode;

   h = GetStdHandle(STD_INPUT_HANDLE);
   if (! GetConsoleMode(h, &mode)) {
     return 0;
   }
   if (echo_on) {
     mode |= ENABLE_ECHO_INPUT;
   } else {
     mode &= ~ENABLE_ECHO_INPUT;
   }
   if (! SetConsoleMode(h, mode)) {
     return 0;
   }
   return 1;

#else
   
   struct termios tios;

   int fd = 0;
   if (tcgetattr(fd, &tios) == -1) {
     return 0;
   }
   if (echo_on) {
     tios.c_lflag |= ECHO;
     tios.c_lflag &= ~ECHONL;
   } else {
     tios.c_lflag &= ~ECHO;
     tios.c_lflag |= ECHONL;
   }
   if (tcsetattr(fd, TCSADRAIN, &tios) == -1) {
     perror("");
     return 0;
   }
   return 0;
#endif
} /* chg_echo */


void getPassword(const char * prompt, char * pswd, size_t len)
{
  int retCode = 0;

  /* Display the password prompt */
  printf("\n%s%s : ", prompt, UIDNAME);

  /* Do not echo the password to the console */
  retCode = chg_echo(0);

  /* get the password */
  fgets(pswd, len, stdin);
  
  /* Turn back on console output */
  retCode = chg_echo(1);

  /* Get rid of any CR or LF */
  pswd[strlen((char *) pswd) - 1] = '\0';
}

/*********************************************************************
 *
 *  FUNCTION:       tt_MemErase
 *
 *  DESCRIPTION:    Securely erase memory, for example to clear any trace
 *                  of a plaintext password.  Do it in a way that shouldn't
 *                  be optimized away by the compiler.
 *
 *  PARAMETERS:     void * buf       The plaintext password to erase
 *                  size_t len       The length of the plaintext password
 *
 *  RETURNS:        Nothing
 *
 *  NOTES:          memset of the buffer is not enough as optimizing compilers
 *                  a may think they know better and not erase the buffer
 *
 *********************************************************************/

void
erasePassword(volatile char *buf, size_t len)
{
   if (buf != NULL) {

     volatile char* p = buf;
     size_t i;
     for (p = buf, i = 0; i < len; ++p, ++i) {
       *p = 0;
     }
   }
}  /* erasePassword */
