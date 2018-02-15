//////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Universal Permissive License v 1.0 as shown
// at http://oss.oracle.com/licenses/upl
//
// xla_admin.cpp: ttXlaAdmin source file
// --> attempt to make XLA bookmark handling easier
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
#include <signal.h>
#include <stdio.h>

#include <ttclasses/TTInclude.h>
#include <ttclasses/TTXla.h>
#include "testprog_utils.h"
#include "tt_version.h"

#ifdef WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#endif

TTXlaPersistConnection  xlaconn;
TTConnection     conn;
char connStr[256];
char username[MAX_USERNAME_SIZE];
char password[MAX_PASSWORD_SIZE];
char passwordPrompt[64];

const char * ALL_SPACES_BOOKMARK = "\" \"" ;

// max XLA bookmark length
const int XLA_BOOKMARK_LEN = 31;

//----------------------------------------------------------------------

void TerminateGracefully(TTConnection&);
void TerminateGracefully(TTXlaPersistConnection&);
void TerminateIfRequested(TTConnection&);
void TerminateIfRequested(TTXlaPersistConnection&);

int chg_echo(int echo_on);
void getPassword(const char * prompt, char * pswd, size_t len);
void erasePassword(volatile char *buf, size_t len);

//----------------------------------------------------------------------

struct tt_bookmark_info {
  int writeLFN;
  int writeLFO;
  int forceLFN;
  int forceLFO;
  int holdLFN;
  int holdLFO;
} ;

//----------------------------------------------------------------------

struct xla_bookmark_info {
  char id [XLA_BOOKMARK_LEN+1];
  int  readlsnhigh ;
  int  readlsnlow ;
  int  purgelsnhigh ;
  int  purgelsnlow ;
  bool inuse ;
} ;

//----------------------------------------------------------------------

void DeleteSingleXLABookmark(const char * connStr, 
                             const char* username,
                             const char* password,
                             const char * bookmark_name,
                             xla_bookmark_info * bookmarks,
                             int num_bookmarks);

//----------------------------------------------------------------------

void
PrintBookmarks (int num_bookmarks,
                xla_bookmark_info * bookmarks,
                tt_bookmark_info & ttbookmark,
                bool delete_all_bookmarks)
{
  cerr << endl
       << "Most recent Log Sequence Numbers (LSNs):" << endl
       << "----------------------------------------" << endl
       << ttbookmark.forceLFN << "." << ttbookmark.forceLFO << endl << endl ;

  cerr << "XLA bookmarks:" << endl
       << "----------------------------------------" << endl ;

  int ttbookmark_lfn = ttbookmark.forceLFN;
  int ttbookmark_holdlfn = ttbookmark.holdLFN ;

  for (int i = 0 ; i < num_bookmarks ; i++ )
  {
    // Did the user say to delete *ALL* bookmarks?
    if (delete_all_bookmarks) {
      DeleteSingleXLABookmark(connStr,
                              username,
                              password,
                              bookmarks[i].id,
                              bookmarks,
                              num_bookmarks);
      continue;
    }

    int bookmark_lfn = bookmarks[i].purgelsnhigh;
    cerr << bookmarks[i].purgelsnhigh
         << "."
         << bookmarks[i].purgelsnlow
         << "\t"
         << bookmarks[i].id << "\t";
    if ( bookmark_lfn == ttbookmark_lfn )
      cerr << "(no log hold)";
    else if ( bookmark_lfn + 1 == ttbookmark_lfn )
      cerr << "(** hold on log" << bookmark_lfn << ")";
    else if ( bookmark_lfn + 2 == ttbookmark_lfn )
      cerr << "(**** hold on log" << bookmark_lfn << ", log" << bookmark_lfn+1 << ")";
    else
      cerr << "(****** hold on log" << bookmark_lfn << "-log" << ttbookmark_lfn-1 << ")";

    // Is this XLA bookmark in use?
    if ( bookmarks[i].inuse )
      cerr << "  [in use]" ;

    cerr << endl ;
  }

  if ( num_bookmarks == 0 )
    cerr << "No XLA bookmarks in datastore." << endl;

  cerr << endl ;

  cerr << "Replication bookmark:" << endl
       << "----------------------------------------" << endl ;
  if ( ttbookmark.holdLFN == -1 ) {
    cerr << "Replication not configured for datastore." << endl ;
  }
  else {
    cerr << ttbookmark.holdLFN << "." << ttbookmark.holdLFO ;
    if ( ttbookmark_lfn == ttbookmark_holdlfn ) {
      cerr << "   (no replication hold)" ;
    }
    else {
      cerr << "   (replication hold on " << (ttbookmark_lfn - ttbookmark_holdlfn)
           << " logfiles)" ;
    }
    cerr << endl << endl ;
  }
}

//----------------------------------------------------------------------

void
GetTTBookmark (TTConnection & connection,
               tt_bookmark_info & ttbookmark)
{
  int rc ;
  TTCmd bookmark_cmd;
  try {
    bookmark_cmd.Prepare(&connection, "{call ttBookmark}");
    connection.Commit();
    bookmark_cmd.Execute();
    rc = bookmark_cmd.FetchNext();
    if (rc) {
      cerr << "Unable to successfully fetch from ttBookmark; aborting." << endl ;
      TerminateGracefully(connection);
    }
    bookmark_cmd.Close();
    bookmark_cmd.getColumn(1, &ttbookmark.writeLFN) ; // last written log file
    bookmark_cmd.getColumn(2, &ttbookmark.writeLFO) ; // offset of same
    bookmark_cmd.getColumn(3, &ttbookmark.forceLFN) ; // last log file forced to disk
    bookmark_cmd.getColumn(4, &ttbookmark.forceLFO) ; // offset of same
    bookmark_cmd.getColumn(5, &ttbookmark.holdLFN)  ; // replication bookmark log file
    bookmark_cmd.getColumn(6, &ttbookmark.holdLFO)  ; // offset of same
    connection.Commit();
  }
  catch (TTStatus st) {
    cerr << "Error while calling ttBookmark: " << st << endl ;
    cerr << "Aborting." << endl ;
    TerminateGracefully(connection);
  }
}

//----------------------------------------------------------------------

void
GetXLABookmarks (TTConnection & connection,
                 xla_bookmark_info ** bookmarks,
                 int & num_bookmarks)
{
  TTCmd bookmark_query;

  try {
    connection.SetIsoSerializable(); // this ought to prevent someone from
                                     // altering SYS.TRANSACTION_LOG_API entries

    bookmark_query.Prepare(&conn, "select id, readlsnhigh, readlsnlow, "
                           "purgelsnhigh, purgelsnlow, inuse from "
                           "sys.transaction_log_api");

    connection.Commit();

    // first just count the # of bookmarks
    num_bookmarks = 0;
    bookmark_query.Execute();

    while (!bookmark_query.FetchNext() && !StopRequested()) {
      num_bookmarks++;
    }
    bookmark_query.Close();

    if ( num_bookmarks == 0 ) {
      return ;
    }

    // set up the data structure
    *bookmarks = new xla_bookmark_info [num_bookmarks];
    char * inuse;
    char * bufP;
    int len;

    // now read all of the bookmark information
    bookmark_query.Execute();
    for (int i = 0 ; i < num_bookmarks && !StopRequested() ; i++)
    {
      if ( ! bookmark_query.FetchNext() ) {
        bookmark_query.getColumn(1, &bufP);
        strcpy((*bookmarks)[i].id, bufP);
        bookmark_query.getColumn(2, &(*bookmarks)[i].readlsnhigh);
        bookmark_query.getColumn(3, &(*bookmarks)[i].readlsnlow);
        bookmark_query.getColumn(4, &(*bookmarks)[i].purgelsnhigh);
        bookmark_query.getColumn(5, &(*bookmarks)[i].purgelsnlow);
        bookmark_query.getColumn(6, (void**) &inuse, &len);
        if (inuse[0] == 0x1)
          (*bookmarks)[i].inuse = true ;
        else
          (*bookmarks)[i].inuse = false ;

        tt_rtrim((*bookmarks)[i].id); // trim trailing spaces in bookmark name

        // handle weird "all spaces" bookmark name
        // --> turn this into "\" \"" + null terminator
        if (strlen((*bookmarks)[i].id) == 0) {
          sprintf((*bookmarks)[i].id, ALL_SPACES_BOOKMARK);
          (*bookmarks)[i].id[strlen(ALL_SPACES_BOOKMARK)] = '\0';
          (*bookmarks)[i].id[XLA_BOOKMARK_LEN] = '\0';
        }
      }
    } // for

    TerminateIfRequested(connection);

    connection.Commit();
  } // try
  catch (TTStatus st) {
    cerr << "Error while reading XLA bookmark information: "
         << st << endl ;
    TerminateGracefully(connection);
  }
}

//----------------------------------------------------------------------

void
DeleteSingleXLABookmark (const char * conn_str,
                         const char* username,
                         const char* password,
                         const char * bookmark_name,
                         xla_bookmark_info * bookmarks,
                         int num_bookmarks)
{
  int bookmarkIdx = -1 ;
  u_int bookmarkLen = strlen(bookmark_name);
  for (int i = 0 ; i < num_bookmarks ; i++ )
  {
    if (strlen(bookmarks[i].id) == bookmarkLen &&
        !strcmp(bookmarks[i].id, bookmark_name)) {
      bookmarkIdx = i ;
      break ;
    }

    if (!strcmp(bookmarks[i].id, ALL_SPACES_BOOKMARK)) {
      bool all_spaces = true;
      for (u_int c = 0; c < bookmarkLen; c++)
      {
        if ((bookmark_name)[c] != ' ') {
          all_spaces = false;
          break;
        }
      }
      if (all_spaces) {
        bookmarkIdx = i;
        break;
      }
    }
  } // outer for loop

  if (bookmarkIdx == -1) {
    cerr << endl
         << "Specified XLA bookmark <" << bookmark_name << "> does not exist "
         << "in datastore." << endl ;
    return ;
  }
  else {
    if (bookmarks[bookmarkIdx].inuse) {
      cerr << "Cannot delete XLA bookmark <" << bookmark_name << "> since it is "
           << "currently in use." << endl ;
      return ;
    }
    cerr << "Deleting XLA bookmark <" << bookmark_name << "> as requested."
         << endl;
  }

  // The bookmark exists, and it's not in use -- so delete it

  try {
    try {
      xlaconn.Connect(conn_str,
                      username,
                      password,
                      bookmark_name,
                      false);
    }
    catch (TTWarning st) {
      // ignore connection warnings
    }
    catch (TTError st) {
      cerr << st << endl;
      TerminateGracefully(xlaconn);
    }
    xlaconn.deleteBookmarkAndDisconnect(); // implicitly calls TTXlaPersistConnection::Disconnect()
  }
  catch (TTStatus st) {
    cerr << "Error deleting XLA bookmark: " << st << endl ;
    TerminateGracefully(xlaconn);
  }

}

//----------------------------------------------------------------------

void
TerminateGracefully (TTConnection & connection)
{
  cerr << "Rolling back active transaction...";

  try {
    cerr << "Disconnecting...";
    connection.Rollback();
    connection.Disconnect() ;
  }
  catch (TTError st) {
    cerr << "Error disconnecting : " << st << endl ;
  }

  cerr << "Exiting..." << endl ;
  exit(1);
}

//----------------------------------------------------------------------

void
TerminateGracefully (TTXlaPersistConnection & connection)
{
  cerr << "Disconnecting...";

  try {
    connection.Disconnect() ;
    cerr << "Disconnected." << endl ;
  }
  catch (TTStatus st) {
    cerr << "Error disconnecting : " << st << endl ;
  }

  exit(1);
}

//----------------------------------------------------------------------

void
TerminateIfRequested (TTConnection & connection)
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

void
TerminateIfRequested (TTXlaPersistConnection & connection)
{
  if (StopRequested()) {
    STDOSTRSTREAM ostr;
    ostr << "Signal <" << SignalReceived() << "> received, "
         << "terminating execution." << ends << endl ;
    cerr << ostr.str() << ends << endl;
  }
}

//----------------------------------------------------------------------

const char * usage_string =
" ttXlaAdmin [ -list | -remove <bookmark> | -removeAll ]\n"
"            [<DSN> | -connstr <connection string>]\n"
"\noptions:\n"
"  -list               List all current XLA bookmarks.\n"
"  -remove <bookmark>  Remove specified XLA bookmark.\n"
"  -removeAll          Remove all XLA bookmarks in datastore.\n";

//----------------------------------------------------------------------

int
main (int argc, char* argv[])
{
  // ---------------------------------------------------------------
  // Parse command-line arguments
  // ---------------------------------------------------------------
  ParamParser parser (usage_string);
  char bookmarkName[64];
  bool listonly = true;
  bool removeall = false;
  // ------------------------------------------------------
  parser.setArg("-list", false, 0);
  parser.setArg("-remove", false, 64);
  parser.setTwinArg("-list", "-l");
  parser.setTwinArg("-remove", "-rm");
  parser.setTwinArg("-remove", "-del");
  parser.setTwinArg("-remove", "-delete");
  parser.setArg("-removeAll", false, 0);

  // Default the DB and UID
  sprintf(connStr, "dsn=%s;%s", DEMODSN, "UID=adm");

  parser.processArgs(argc, argv, connStr);
  if (parser.argUsed("-remove")) {
    listonly = false;
    strncpy(bookmarkName, parser.getArgValue("-remove"), 64-1);
    if (parser.argUsed("-list")) {
      cerr << "Error using '-list': already specified '-remove' option."
           << endl;
      exit(-1);
    }
  }
  else if (parser.argUsed("-list")) {
    listonly = true ; // it's actually true by default
  }
  else if (parser.argUsed("-removeAll")) {
    removeall = true;
    listonly  = true ; // it's actually true by default
  }


  // ---------------------------------------------------------------

  // ---------------------------------------------------------------
  // Set up TTClasses logging
  // ---------------------------------------------------------------
  TTGlobal::setLogStream(STDCERR);
  TTGlobal::setLogLevel(TTLog::TTLOG_ERR) ;
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

  // First: connect and see which bookmarks exist

  xla_bookmark_info * bookmarks;
  tt_bookmark_info ttbookmark;

  cerr << endl << "Connecting to TimesTen <" << connStr << ">" << endl ;

  /* Set the password prompt */
  strcpy(passwordPrompt, "Enter password for ");

  // Get the password for the TTConnection and TTXlaPersistConnection
  getPassword(passwordPrompt, (char *) password, sizeof(password) );

  sprintf(username, "%s", "adm");

  try {
    conn.Connect(connStr, username, password);
  }
  catch (TTWarning st) {
    cerr << "Warning connecting to TimesTen: " << st << endl ;
  }
  catch (TTError st) {
    cerr << "Error connecting to TimesTen: " << st << endl ;
    exit(-1);
  }

  cerr << endl << "Connected."  << endl;
  int num_bookmarks=0;
  
  GetXLABookmarks(conn, &bookmarks /*OUT*/, num_bookmarks /*OUT*/);
  
  GetTTBookmark(conn, ttbookmark /*OUT*/);

  try {
    conn.Disconnect();
  }
  catch (TTError st) {
    cerr << "Error disconnecting from TimesTen: " << st << endl ;
  }

  // We've read all of the bookmark information
  //
  // Now do what the user requested (print out bookmark list, remove one,
  // or remove them all).

  if ( listonly ) {
	  
    PrintBookmarks(num_bookmarks, bookmarks, ttbookmark, removeall);

	if (!removeall) {
	  cerr << endl << "To remove unwanted bookmarks, use the \"-remove\" or \"-removeall\" command options" << endl;
	}
    exit(0);
  }

  cerr << endl << "About to DeleteSingleXLABookmark " << endl;
  // Since we've gotten to here, the user wants to remove a single
  // XLA bookmark.
  DeleteSingleXLABookmark (connStr,
                           username,
                           password,
                           bookmarkName,
                           bookmarks,
                           num_bookmarks);

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
  printf("\n%s%s : ", prompt, ADMINNAME);
  fflush(stdout);

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
