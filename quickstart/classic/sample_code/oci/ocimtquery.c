/*
 * Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/**********************************************************
 *                                                        *
 * OCI based query timing utility for TimesTen and Oracle *
 *                                                        *
 **********************************************************/

#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#else
#include <termios.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <oci.h>
//#ifdef WIN32
//#include <sys/times.h>
//#else
#ifndef WIN32
#include <sys/time.h>
#endif

/* sleep function */
#ifndef WIN32
#define tt_yield() sched_yield()
#include <sched.h>
#ifdef __aix
typedef struct timestruc_t Timestruct;
#define nanosleep(a,b) nsleep(a,b)
#else  /* !AIX */
typedef struct timespec Timestruct;
#endif
#else  /* WIN32 */
#define tt_yield() Sleep(0)
#endif /* WIN32 */
#ifndef _WIN32
#define getTime(x) gettimeofday ((x), NULL)
#else 
#define getTime(x) _ftime((x));
#endif

#ifndef WIN32
#include <pthread.h>
typedef pthread_t  sbThread;
#else /* WIN32 */
typedef unsigned int  sbThread;
#endif /* WIN32 */

#define  ERR_OUT()					\
  if ( (rc != OCI_SUCCESS) && (rc !=  OCI_SUCCESS_WITH_INFO) ){		\
    fprintf(stdout, "error at file  %s, line %d\n", __FILE__, __LINE__); \
    handleError(rc, err, NULL);					\
    exit(1);								\
  }

/*
 * Timer macros
 */
#ifndef WIN32

#define  TIMER_DECL()				\
  struct timeval t1, t2, t3;
  
#define TIMER_START()				\
  gettimeofday(&t1, NULL);

#define TIMER_STOP()				\
  gettimeofday(&t2, NULL);

#define TIMER_STOP0()				\
  gettimeofday(&t3, NULL);


#define TIMER_ELAPSE()		    \
  ((long)(t2.tv_sec - t1.tv_sec)*1000000L +			\
   (long)(t2.tv_usec - t1.tv_usec));

#define TIMER_ELAPSE0()		    \
  ((long)(t3.tv_sec - t1.tv_sec)*1000000L +			\
   (long)(t3.tv_usec - t1.tv_usec));

#else /* WIN32 */

#define  TIMER_DECL()				\
  LARGE_INTEGER  t1, t2, t3;                    \
  LARGE_INTEGER  freq

#define TIMER_START()				\
  QueryPerformanceCounter(&t1);

#define TIMER_STOP()				\
  QueryPerformanceCounter(&t2);

#define TIMER_STOP0()				\
  QueryPerformanceCounter(&t3);

#define TIMER_ELAPSE()		       \
  (QueryPerformanceFrequency(&freq)) ?					\
  ((long)((1000000L * (t2.QuadPart-t1.QuadPart)) /freq.QuadPart)) : (0L)

#define TIMER_ELAPSE0()		       \
  (QueryPerformanceFrequency(&freq)) ?					\
  ((long)((1000000L * (t3.QuadPart-t1.QuadPart)) /freq.QuadPart)) : (0L)

#endif /* WIN32 */

#define MAXQRYLEN            1024*1024
#define MAXROWLEN            1024*1024
#define MAXQUERIES           1024
#define MAXITERATIONS        10000
#define DFLTITERATIONS       100
#define MAXSRVNAMELEN        128
#define MAXPWDLEN            128
#define SQUOTE               '\''
#define DQUOTE               '"'
#define MIN_VERBOSE          0
#define VB_BASIC             0
#define VB_DETAIL            1
#define VB_FULL              2
#define MAX_VERBOSE          2
#define ITER_PCILE_THRESHOLD 50

/*
 * Stats structure for a query.
 */
struct _qhist_ {
  long    execs;
  long    elapsed;
  long    rows;
  long    minrt;
  long    maxrt;
  long    sumrt;
  long    minexecrt;
  long    maxexecrt;
  long    sumexecrt;
  long   *latency;
};

typedef struct _qhist_ qhist_t;

/*
 * Thread state.
 */
typedef enum { STARTED, READY, RUN, RUNNING, FINISHED } tstate_t;

/*
 * Thread specific data.
 */
struct _tinfo_ {
  sbThread  tid;
  int       id;
  volatile  tstate_t state;
  int       runtime;
  int       iterations;
  int       attempts;
  int       misses;
  int       timeout;
  int       fetches;
  int       errors;
  double    elapsed;
  qhist_t * qhist;
};

typedef struct _tinfo_ tinfo_t;

/*
 * Global variables
 */
char     oracleSrv[MAXSRVNAMELEN+1];
char     pwdBuff[MAXPWDLEN+1];
char   * userName = NULL;
char   * passWord = NULL;
char   * qryFile = NULL;
int      secs = 0;
int      numThread = 0;
int      verbose = -1;
char   * qlist[MAXQUERIES];
int      nQueries = 0;
int      nIterations = 0;
int enablePercentiles = 1;
OCIEnv * env;

/*
 * Display program help information.
 */
void usage(){
  printf(
  "\nRun one or more SQL SELECT statements multiple times using one or more\n"
  "concurrent connections/threads and perform detailed throughput and response\n"
  "time measurements.\n\n"
  "Usage:\n\n"
  "  ocimtquery [ -h | -help ]\n"
  "  ocimtquery -oraclesrv srvstr -uid userid -file qfile [-pwd password]\n"
  "             [-threads nthread] [-qiter niter] [-secs nsecs] [-verbose vb]\n\n"
  "where\n\n"
  "  srvstr   - Specifies the database server to use. This can be a TNS name\n"
  "             or an EasyConnect string.\n\n"
  "  userid   - Specifies the database user name.\n\n"
  "  qfile    - A text file containing the SQL statements to run separated by\n"
  "             semi-colons (;).\n\n"
  "  password - Specifies the database password. If not specified you will be\n"
  "             prompted to enter the password.\n\n"
  "  nthread  - Use this many concurrent threads. All threads run the entire\n"
  "             set of queries (subject to any time limit). Default is 1.\n\n"
  "  niter    - Run this many iterations of each query (default is 100).\n\n"
  "  nsecs    - Limit the run time to this many seconds. This may result in\n"
  "             incomplete statistics as not all queries may be executed.\n"
  "             The time limit is only checked at the end of each query\n"
  "             execution in order to not impact the timing operations so\n"
  "             the limit is only applied approximately. Default is no\n"
  "             time limit.\n\n"
  "  vb       - Increase the detail level of the output.\n"
  "               0 - basic stats (the default).\n"
  "               1 - also show the query text and exec only stats.\n"
  "               2 - include extra thread level info.\n\n"
  "For the most accurate results, nthread * qiter should be a multiple of 100.\n\n"
  "If 'niter' is less that 10 then 95 and 99 percentile response times will\n"
  "not be reported.\n\n"
  );
  exit (-1);
}

/*
 * Pause for a specified number of milliseconds
 */
#ifndef WIN32
static void
sbSleep(int nMilliseconds)
{
  Timestruct sleepTime, timeRemaining;
  
  sleepTime.tv_sec = nMilliseconds/1000;
  sleepTime.tv_nsec = (nMilliseconds-1000*sleepTime.tv_sec)*1000000;
  
  while (1) {
    int r = nanosleep(&sleepTime, &timeRemaining);
    if (r == -1 && errno == EINTR)
      sleepTime = timeRemaining;
    else
      break;
  }
}
#else /* WIN32 */
#define sbSleep(x)  Sleep(x)
#endif /* WIN32 */

#define gassert(x) {				\
    if((x)==0){								\
      printf("assert("#x") in file %s, line %d\n", __FILE__, __LINE__);	\
      exit(-1);								\
    }									\
  }

/*
 * Prints a SQL statement in a 'nice' format.
 */
void sqlPrettyPrint( FILE * f, char * sql, char * prefix )
{
    char * p = sql;

    if (  (f == NULL) || (sql == NULL)  )
        return;

    if ( prefix != NULL )
        fputs( prefix, f );
    while (  *p  )
    {
        putc(*p, f);
        if ( (*p == '\n') && (prefix != NULL)  )
            fputs( prefix, f );
        p++;
    }
}

/*
 * Turn input echo on and off for console input.
 */
int chgEcho(int echo_on)
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
} /* chgEcho */

/*
 * Read a password from the terminal with echo disabled.
 */
void getPassword(const char * prompt, const char * uid, char * pswd, size_t len)
{
  int retCode = 0;

  /* Display the password prompt */
  printf("\n%s%s: ", prompt, uid);

  /* Do not echo the password to the console */
  retCode = chgEcho(0);

  /* get the password */
  fgets(pswd, len, stdin);

  /* Turn back on console output */
  retCode = chgEcho(1);

  /* Get rid of any CR or LF */
  pswd[strlen((char *) pswd) - 1] = '\0';

  printf("\n");
}

/*
 * Check if a string represents a positive integer (and
 * if so return its value).
 */
int isnumeric( char * str )
{
    int val = 0;

    if (  (str == NULL) || (*str == '\0')  )
        return -1;
    if (  strlen( str ) > 9  )
        return -1;
    while (  *str  ) {
        if (  (*str < '0') || (*str > '9')  )
            return -1;
        val = (val * 10) + (*str++ - '0');
    }

    return val;
}

/*
 * Create a thread and Join a thread.
 */
#ifndef WIN32
static  int sbThreadCreate(void(*start_address)(void*),
			   unsigned stack_size,
			   void* arg,
			   sbThread* threadP /* OUT */)
{
  pthread_attr_t   tAttr;
  int   rc;
  rc = pthread_attr_init(&tAttr);
  gassert(rc== 0);
  rc = pthread_attr_setscope(&tAttr,PTHREAD_SCOPE_SYSTEM);
  gassert(rc== 0);
  if (stack_size > 0){
    rc = pthread_attr_setstacksize(&tAttr, stack_size);
    gassert(rc== 0);
  }
  rc = pthread_create(threadP, &tAttr, (void *(*)(void *))start_address,
		      (void *)arg);
  gassert(rc ==0);
  rc = pthread_attr_destroy(&tAttr);
  gassert(rc == 0);
  return 0;
}

static int sbThreadJoin(sbThread t, void** statusPP)
{
  int rc;
  rc = pthread_join(t, statusPP);
  gassert(rc == 0);
  return 0;
} 
#else /* WIN32 */

static int sbThreadCreate(void(*start_address)(void*),
			  unsigned stack_size,
			  void * arg,
			  sbThread* threadP /* OUT */)
{
  *threadP = _beginthread(start_address, stack_size, arg);
  
  if ( *threadP != -1 )
    return 0;
  else
    return -1;
}

static int sbThreadJoin(sbThread t)
{
  
  WaitForSingleObject((HANDLE) t, INFINITE);
  return 0;
}
#endif /* WIN32 */

/*
 * Allocates an array of 'qhist_t' based on number of queries and number of iterations
 */
qhist_t * allocQhist(int nquery, int niterations)
{
    qhist_t * qh = NULL;
    int i;

    qh = (qhist_t *)calloc( (size_t)nquery, sizeof(qhist_t) );
    if (  qh != NULL  )
        for ( i = 0; i < nquery; i++ )
        {
            qh[i].minexecrt = qh[i].minrt = 999999999L;
            qh[i].latency = (long *)calloc( (size_t)niterations, sizeof(long) );
            if ( qh[i].latency == NULL  )
            {
                free( (void *)qh );
                qh = NULL;
                break;
            }
        }

    return qh;
}

/*
 * Check if all threads have a specific state.
 */
int allState( tinfo_t * tinfo, int nthreads, tstate_t state)
{
    int i;

    if (  tinfo == NULL  )
        return 0;
    for ( i = 0; i < nthreads; i++ )
        if (  tinfo[i].state != state  )
            return 0;
    return 1;
}

/*
 * General purpose error handling.
 */
int handleError(sword rc, OCIError *err, char *msg)
{
  ub4 recno = 0;
  sb4 oracle_err;
  ub4 ret = 0;
  text errbuf[1024];
  
  switch (rc) {
  case OCI_SUCCESS:
    break;
  case OCI_SUCCESS_WITH_INFO:
    if (verbose) {
      (void) printf("Warn  - OCI_SUCCESS_WITH_INFO\n");
    }
    break;
  case OCI_NEED_DATA:
    (void) printf("Error - OCI_NEED_DATA\n");
    break;
  case OCI_NO_DATA:
    if (verbose) {
      (void) printf("Error - OCI_NODATA\n");
    }
    break;
  case OCI_ERROR:
    (void) printf("Error - OCI_ERROR\n");
    break;
  case OCI_INVALID_HANDLE:
    (void) printf("Error - OCI_INVALID_HANDLE\n");
    break;
  case OCI_STILL_EXECUTING:
    (void) printf("Error - OCI_STILL_EXECUTING\n");
    break;
  case OCI_CONTINUE:
    (void) printf("Error - OCI_CONTINUE\n");
    break;
  default:
    break;
  }
  
  if ( (rc != OCI_SUCCESS) && (rc != OCI_NO_DATA) && (rc != OCI_SUCCESS_WITH_INFO) ) {
    for( recno = 0; 
	 OCIErrorGet(err, recno + 1, 0, &oracle_err,
		     errbuf, 1024, OCI_HTYPE_ERROR ) == OCI_SUCCESS;
	 ++recno) {
      fprintf(stderr,"error %d, msg = %s\n",recno + 1, errbuf);
      if (msg != NULL) {
	(void) fprintf(stderr,"%s\n", msg);
      }
      ret = 1;
    }
  }
  return ret;
}

/*
 * Sort an array of 'long' values using optimised bubble sort.
 */
void bubbleSort( long * larr, int asize )
{
    int i = 0, j, swap = 0;
    long tmp;

    do
    {
        swap = j = 0;
        do
        {
            if ( larr[j] > larr[j+1] )
            {
                tmp = larr[j];
                larr[j] = larr[j+1];
                larr[j+1] = tmp;
                swap = 1;
            }
            j++;
        }
        while ( j < (asize - i - 1)  );
        i++;
    }
    while ( swap && (i < (asize - 1)) );
}

/*
 * Calculate the 95%ile and 99%ile latency (response time) for a specific
 * query across all threads.
 */
int calculatePercentiles( tinfo_t * tinfo, int qryno, long * p95, long * p99 )
{
static long * larr = NULL;
    int i, l, t;
    int nsamples = nIterations * numThread;

    if (  (qryno >= nQueries) || (tinfo == NULL) ||
          (p95 == NULL) || (p99 == NULL)  )
        return 0;
    if (  larr == NULL  )
    {
        larr = (long *)malloc( sizeof(long) * nsamples );
        if (  larr == NULL  )
        {
            fprintf( stderr, "Unable to allocate memory for percentile calculations\n" );
            return 0;
        }
    }

    i = 0; 
    for ( t = 0; t < numThread; t++ )
        if (  tinfo[t].qhist[qryno].execs > 0  )
            for ( l = 0; l < tinfo[t].qhist[qryno].execs; l++ )
                larr[i++] = tinfo[t].qhist[qryno].latency[l];
    bubbleSort( larr, i );

    *p95 = larr[ ((95 * i) - 1) / 100 ];
    *p99 = larr[ ((99 * i) - 1) / 100 ];

    return 1;
}

/*
 * Parse command line arguments and set globals accordingly.
 */
void parseArguments(int argc, char **argv)
{
  int   i;
  char  *cp = NULL;
  
  if ( argc == 1 )
      usage();
  for (i = 1; i < argc; i++){
    if ( (strcmp(argv[i], "-h") == 0) || (strcmp(argv[i],"-help") == 0) )
      usage();
    else if (strcmp(argv[i], "-oraclesrv") == 0){
      i++;
      if ( (cp != NULL) || (i >= argc) )
        usage();
      if (  strlen(argv[i]) > MAXSRVNAMELEN  ) {
          fprintf( stderr, "Value for '-oraclesrv' is too long - max length is %d\n", MAXSRVNAMELEN );
          exit(-1);
      }
      strcpy(oracleSrv, argv[i]);
      cp = oracleSrv;
    }
    else if (strcmp(argv[i], "-uid") == 0){
      i++;
      if ( (userName != NULL) || (i >= argc) )
        usage();
      userName = argv[i];
    }
    else if (strcmp(argv[i], "-pwd") == 0){
      i++;
      if ( (passWord != NULL) || (i >= argc) )
        usage();
      if (  strlen(argv[i]) > MAXPWDLEN  ) {
          fprintf( stderr, "Value for '-pwd' is too long - max length is %d\n", MAXPWDLEN );
          exit(-1);
      }
      passWord = argv[i];
    }
    else if (strcmp(argv[i], "-file") == 0){
      i++;
      if ( (qryFile != NULL) || (i >= argc) )
        usage();
      qryFile = argv[i];
    }
    else if (strcmp(argv[i], "-secs") == 0){
      i++;
      if ( (secs > 0) || (i >= argc) )
        usage();
      secs = isnumeric(argv[i]);
      if (  secs <= 0  ) {
          fprintf( stderr, "Value for '-secs' must be > 0\n" );
          exit(-1);
      }
    }
    else if (strcmp(argv[i], "-verbose") == 0) {
      i++;
      if ( (verbose >= 0) || (i >= argc) )
        usage();
      verbose = isnumeric(argv[i]);
      if ( (verbose < MIN_VERBOSE) || (verbose > MAX_VERBOSE)  ) {
        fprintf(stderr, "Value for '-verbose' must be between %d and %d\n",
                        MIN_VERBOSE, MAX_VERBOSE );
        exit(-1);
      }
    }
    else if (strcmp(argv[i], "-threads") == 0) {
      i++;
      if ( (numThread > 0) || (i >= argc) )
        usage();
      numThread = isnumeric(argv[i]);
      if ( numThread < 1 ) {
        fprintf(stderr, "Value for '-threads' must be > 0\n");
        exit(-1);
      }
    }
    else if (strcmp(argv[i], "-qiter") == 0) {
      i++;
      if ( (nIterations > 0) || (i >= argc) )
        usage();
      nIterations = isnumeric(argv[i]);
      if ( nIterations < 1 ) {
        fprintf(stderr, "Value for '-qiter' must be > 0\n");
        exit(-1);
      }
      if ( nIterations < ITER_PCILE_THRESHOLD ) {
          enablePercentiles = 0;
          fprintf(stderr,"\nWarning: '-qiter' is less than %d so perentile reporting is disabled\n\n",ITER_PCILE_THRESHOLD);
      }
    }
    else
        usage();
  }
  
  if ( cp == NULL ) {
    fprintf(stderr, "No Oracle server specified (-oraclesrv)\n");
    usage();
  }
  if ( qryFile == NULL ) {
    fprintf(stderr, "No query file specified (-file)\n");
    usage();
  }
  if ( userName == NULL ) {
    fprintf(stderr, "No user name specified (-uid)\n");
    usage();
  }
  if ( passWord == NULL ) {
    getPassword("Password for ", (char *)userName, (char *)pwdBuff, MAXPWDLEN );
    passWord = pwdBuff;
  }
  else 
     strcpy( pwdBuff, passWord );
  if ( numThread == 0  )
      numThread = 1;
  if ( nIterations == 0  )
      nIterations = DFLTITERATIONS;
  if (  verbose < 0 )
      verbose = VB_BASIC;
}

/*
 * Read and parse the query file and build an array of SQL statements.
 */
int readQueries (char *filename)
{
  char qbuf[MAXQRYLEN];
  char *p = qbuf;
  char *q;
  FILE *fp;
  int  c;
  int len;
  int inquote = 0;
  int instmt = 0;

  fp = fopen (filename, "r");
  if (fp == NULL) {
    fprintf(stderr,"can't open %s for reading, errno=%d\n", filename, errno);
    return 1;
  }

  while ((c = fgetc(fp)) != EOF) {
    if ( instmt || ((c != ' ') && (c != '\t') && (c != '\n')) ) {
      instmt = 1;
      if (  (c == SQUOTE) || (c == DQUOTE)  ) {
          if (  c == inquote  )
              inquote = 0;
           else
              inquote = c;
      }
      if ( (c == ';') && ! inquote ) {
        *p = '\0';
        len = strlen(qbuf);
        if ( nQueries >= MAXQUERIES ) {
          fprintf(stderr,"too many queries in file - max is %d\n", MAXQUERIES);
          fclose(fp);
          return 1;
        }
        qlist[nQueries] = malloc(len+1);
        if (qlist[nQueries] == NULL) {
          fprintf(stderr,"can't allocate %d bytes for query %d\n", len, nQueries);
          fclose(fp);
          return 1;
        }
        strcpy(qlist[nQueries], qbuf);
        p = qbuf;
        nQueries++;
        instmt = 0;
      }
      else {
          *p++ = c;
      }
    } /* instmt */
  }
  fclose(fp);

  if (  nQueries == 0  ) {
      fprintf(stderr,"No queries found in file '%s'\n",filename);
      return 1;
  }

  return 0;
}

/*
 * Prepare all queries on a connection.
 */
void prepQueries(OCISvcCtx *svc, OCIStmt **stmts, OCIError *err)
{
  sword rc;
  int   i;
  
  for (i = 0; i < nQueries; i++) {
    rc = OCIHandleAlloc(env, (dvoid **) &stmts[i], OCI_HTYPE_STMT, 0, 0);
    if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
      fprintf(stderr, "OCIHandleAlloc failed while allocating statement handle %d "
	      "Return code %d.  Check OCI manual for the error code \n", i, rc);
      exit(-1);
    }
  }
  return;
}

/*
 * Start a server session.
 */
void ociBeginSess(  
		  OCIError   **err,
		  OCIServer  **srv,
		  OCISvcCtx  **svc,
		  OCISession **sess
		  )
{
  sword  rc;
  
  rc = OCIHandleAlloc((dvoid *)env, 
		      (dvoid **)err,
		      OCI_HTYPE_ERROR, 0, 0);
  
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIHandleAlloc failed while allocating error handle, rc = %d\n", rc);
    exit(-1);
  }
  
  rc=OCIHandleAlloc((dvoid *) env, (dvoid **)srv, 
		    OCI_HTYPE_SERVER, (size_t) 0, (dvoid **) 0);
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIHandleAlloc failed while allocating server handle, rc = %d\n", rc);
    exit(-1);
  }
  
  rc=OCIHandleAlloc((dvoid *) env, (dvoid **)svc, 
		    OCI_HTYPE_SVCCTX, (size_t) 0, (dvoid **) 0);
  
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIHandleAlloc failed while allocating service handle, rc = %d\n", rc);
    exit(-1);
  }
  rc = OCIHandleAlloc((dvoid *) env, (dvoid **)sess, 
		      (ub4) OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) 0);
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIHandleAlloc failed while allocating session handle, rc = %d\n", rc);
    exit(-1);
  }
  rc=OCIServerAttach(*srv, *err, (text *)oracleSrv, strlen(oracleSrv), 0);
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIServerAttach failed, rc = %d\n", rc);
    handleError(rc, *err, NULL);
    exit(-1);
  }
  
  rc = OCIAttrSet((dvoid *) *svc, OCI_HTYPE_SVCCTX, (dvoid *) *srv, 
		  (ub4) 0, OCI_ATTR_SERVER, *err);
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIAttrSet (SERVER) failed, rc = %d\n", rc);
    handleError(rc, *err, NULL);
    exit(-1);
  }
  
  rc = OCIAttrSet((dvoid *) *sess, (ub4) OCI_HTYPE_SESSION,
		  (dvoid *) userName, (ub4) strlen(userName),
		  (ub4) OCI_ATTR_USERNAME, *err);
  
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIAttrSet (USERNAME) failed, rc = %d\n", rc);
    handleError(rc, *err, NULL);
    exit(-1);
  }
  rc = OCIAttrSet((dvoid *) *sess, (ub4) OCI_HTYPE_SESSION,
		  (dvoid *) passWord, (ub4) strlen(passWord),
		  (ub4) OCI_ATTR_PASSWORD, *err);
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIAttrSet (PASSWORD) failed, rc = %d\n", rc);
    handleError(rc, *err, NULL);
    exit(-1);
  }
  
  rc = OCISessionBegin(*svc, *err, *sess, OCI_CRED_RDBMS, OCI_DEFAULT);
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCISessionBegin failed, rc = %d\n", rc);
    handleError(rc, *err, NULL);
    exit(-1);
  }
  
  rc = OCIAttrSet((dvoid *) *svc,  OCI_HTYPE_SVCCTX,
		  (dvoid *)*sess,  0,
		  OCI_ATTR_SESSION, *err);
  if (!(rc == OCI_SUCCESS ||rc == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIAttrSet (SESSION) failed, rc = %d\n", rc);
    handleError(rc, *err, NULL);
    exit(-1);
  }
  
  return;
}

/*
 * End a server session.
 */
void ociEndSess(
		OCIError   *err,
		OCIServer  *srv,
		OCISvcCtx  *svc,
		OCISession *sess,
		OCIStmt    **stmts
		)
{
  int i;
  for (i = 0; i < nQueries; i++) {
    if (  stmts[i] != NULL  )
        OCIHandleFree((dvoid *) stmts[i], OCI_HTYPE_STMT);
  }
  OCISessionEnd(svc, err, sess, OCI_DEFAULT);
  OCIHandleFree((dvoid *) sess, OCI_HTYPE_SESSION);
  OCIServerDetach(srv, err, OCI_DEFAULT);
  OCIHandleFree((dvoid *) svc, OCI_HTYPE_SVCCTX);
  OCIHandleFree((dvoid *) srv, OCI_HTYPE_SERVER);
  OCIHandleFree((dvoid *) err, OCI_HTYPE_ERROR);
  return;
}

/*
 * The main thread that executes all the queries and performs the timings.
 */
void OraThread(void *arg)
{
  OCIError      *err;
  OCIServer     *srv;
  OCISvcCtx     *svc;
  OCISession    *sess;
  OCIStmt       **stmts = NULL;
  OCIStmt       *stmt;
  OCIDefine     *defn = (OCIDefine*)0;
  OCIParam      *parmdp = (OCIParam *) 0;
  char          *resultBuf;
  void          *osig;
  ub4            char_semantics = 0, counter = 0, offset = 0;
  sb4            prefetch = 128;
  ub2            col_width;
  ub2            dtype;
  sb2            indp = OCI_IND_NULL;
  sword          rc;
  int            i;
  int            threadNo = 0;
  int            secsToRun = 0;
  int            qIterations = 0;
  qhist_t      * qhist = NULL;
  long           misses = 0, attempts = 0, qfetched = 0, fetched = 0, errors = 0;
  int            qno = -1;
  int            pass = 1;
  volatile int   ready = 0;
  time_t         t_start=0, t_current;
  long           elapsed, elapsed0;
  tinfo_t      * tinfo = NULL;
  TIMER_DECL();

  if (  arg == NULL  ) {
    fprintf(stderr, "OraThread called with invalid arguments\n");
    exit(-1);
  }
  tinfo = (tinfo_t *)arg;

  threadNo = tinfo->id;
  secsToRun = tinfo->runtime;
  qIterations = tinfo->iterations;
  qhist = tinfo->qhist;
  
  stmts = (OCIStmt **) calloc(nQueries, sizeof(OCIStmt *));
  resultBuf = (char *)calloc (1, MAXROWLEN);
  if (  (stmts == NULL) || (resultBuf == NULL) ) {
    fprintf(stderr, "OraThread (%d) failed while allocating memory for stmts\n", threadNo);
    exit(-1);
  }

  ociBeginSess(&err, &srv, &svc, &sess);
  
  prepQueries (svc, stmts, err);

  tinfo->state = READY;
  while (  tinfo->state != RUN  ) 
    tt_yield();
  tinfo->state = RUNNING;
  
  (void)time(&t_start);
  for (qno = 0; qno < nQueries; qno++)
  {
      (void)time(&t_current);
      if ( secsToRun && ((t_current-t_start) >= (secsToRun)) ) {
        tinfo->timeout = 1;
        break;
      }

      stmt = stmts[qno];
      if (verbose > VB_DETAIL) 
        fprintf(stderr,"OraThread %d: running query %d\n", threadNo, qno+1);
      
      rc = OCIStmtPrepare(stmt, err, (text *)qlist[qno], 
  			(ub4) strlen(qlist[qno]), 
  			OCI_NTV_SYNTAX, OCI_DEFAULT);
      ERR_OUT();
      
      rc = OCIAttrSet (stmt, OCI_HTYPE_STMT,
  		     &prefetch,                        /* prefetch up to 128 rows */
  		     0,
  		     OCI_ATTR_PREFETCH_ROWS,
  		     err);
      ERR_OUT();
      
      rc = OCIStmtExecute(svc, stmt, err, 0, 0, 0, 0, OCI_DEFAULT);
      if (handleError(rc, err, qlist[qno]) != 0) {
        continue;
      }
      if (rc != OCI_NO_DATA) {
        counter = 1;
        while (1) {
  	  parmdp = (OCIParam *)0;
  	  char_semantics = 0;
  	  col_width = 0;
  	  dtype = 0;
  	  rc = OCIParamGet((void *)stmt, (ub4) OCI_HTYPE_STMT, err, (void**)&parmdp, (ub4) counter);
  	  if (rc != OCI_SUCCESS) {
  	    break;
  	  }
  	  
  	  rc = OCIAttrGet((void*) parmdp, (ub4) OCI_DTYPE_PARAM,
  			  (void*) &dtype,(ub4 *) 0, (ub4) OCI_ATTR_DATA_TYPE,
  			  (OCIError *) err );
  	  ERR_OUT();
  	  
  	  rc = OCIAttrGet((void*) parmdp, (ub4) OCI_DTYPE_PARAM,
  			  (void*) &char_semantics,(ub4 *) 0, 
  			  (ub4) OCI_ATTR_CHAR_USED, (OCIError *) err);
  	  ERR_OUT();
  	  
  	  col_width = 0;
  	  rc = OCIAttrGet((void*) parmdp, (ub4) OCI_DTYPE_PARAM,
  			  (void*) &col_width, (ub4 *) 0, 
  			  (ub4) ((char_semantics) ? OCI_ATTR_CHAR_SIZE : OCI_ATTR_DATA_SIZE),
  			  (OCIError *) err);
  	  ERR_OUT();
    
  	  if (char_semantics)
  	      col_width*=4;
  	  
  	  defn = (OCIDefine *)NULL;
  	  indp = OCI_IND_NULL;
  	  rc = OCIDefineByPos(stmt, &defn, err, counter, 
  			      (dvoid *) &resultBuf[offset], col_width, dtype,
  			      (dvoid *) &indp, (ub2 *)0, (ub2 *) 0, OCI_DEFAULT);
  	  ERR_OUT();
  	  offset += col_width;
  	  if (offset > MAXROWLEN) {
  	    fprintf(stderr, "OraThread (%d) result set does not fit in buffer col=%d qno=%d query=\n%s\n", 
  		    threadNo, counter, qno, qlist[qno]);
            exit(-1);
  	  }
  	  counter++;
        }
      }
      
      (void)time(&t_current);
      if ( secsToRun && ((t_current-t_start) >= (secsToRun)) ) {
        tinfo->timeout = 1;
        break;
      }

      for (i = 0; i < qIterations; i++) {
        (void)time(&t_current);
        if ( secsToRun && ((t_current-t_start) >= (secsToRun)) ) {
          tinfo->timeout = 1;
          break;
        }
        attempts++;
        qfetched = 0;
        TIMER_START();
        rc = OCIStmtExecute(svc, stmt, err, 0, 0, 0, 0, OCI_DEFAULT);
        if (handleError(rc, err, qlist[qno]) != 0) {
  	errors++;
  	continue;
        }
        TIMER_STOP0();

  	rc = OCIStmtFetch2(stmt, err, 1, 0, 0, 0);
        while (rc == OCI_SUCCESS ) {
  	    fetched++;
  	    qfetched++;
  	    rc = OCIStmtFetch2(stmt, err, 1, 0, 0, 0);
        }
  	if ( (rc != OCI_NO_DATA) && (handleError(rc, err, qlist[qno]) != 0) ) {
  	    errors++;
  	    break;
  	}
        TIMER_STOP();
        elapsed = TIMER_ELAPSE();
        elapsed0 = TIMER_ELAPSE0();

        qhist[qno].execs++;
        qhist[qno].elapsed += elapsed;
        qhist[qno].rows += qfetched;
        qhist[qno].latency[i] = elapsed;
        qhist[qno].sumrt += elapsed;
        qhist[qno].sumexecrt += elapsed0;
        if (elapsed <  qhist[qno].minrt)
  	    qhist[qno].minrt = elapsed;
        if (elapsed > qhist[qno].maxrt)
  	    qhist[qno].maxrt = elapsed;
        if (elapsed0 <  qhist[qno].minexecrt)
  	    qhist[qno].minexecrt = elapsed0;
        if (elapsed0 > qhist[qno].maxexecrt)
  	    qhist[qno].maxexecrt = elapsed0;
      }

      if (verbose > VB_DETAIL) 
        fprintf(stderr,"OraThread %d: finished query %d, avgrt %.01f usec\n", 
  	      threadNo, qno+1, ((double)qhist[qno].sumrt/(double)qhist[qno].execs));

      if ( tinfo->timeout )
          break;
  } /* main for loop */
  
  tinfo->attempts= attempts;
  tinfo->misses  = misses;
  tinfo->fetches = fetched;
  tinfo->errors  = errors;
  
  ociEndSess(err, srv, svc, sess, stmts);
  tinfo->state = FINISHED;

  return;
}

/*
 * The main thing!
 */
int main(int argc, char **argv)
{
  int          i, t;
  int          test2Info = 0;
  int          rc;
  long         attempts = 0, misses = 0, fetches = 0;
  long         overall = 0, errors = 0;
  sword        rc1;
  double       total = 0.0, waited = 0.0;
  long         p95 = 0L, p99 = 0L;
  long         texecs, trows, telapsed;
  long         tminrt, tmaxrt, tsumrt;
  long         tminexecrt, tmaxexecrt, tsumexecrt;
  tinfo_t    * threadInfo;

  /* Parse command line arguments */
  parseArguments(argc, argv);
  
  /* Parse and load queries */
  printf("Reading queries...");
  fflush(stdout);
  if (readQueries(qryFile))
    return -1;
  printf("done\n");
  fflush(stdout);
  
  /* Initialize OCI */
  rc1 = OCIEnvCreate(&env, OCI_THREADED | OCI_OBJECT, 0, 0, 0, 0, 0, 0);
  if (!(rc1 == OCI_SUCCESS ||rc1 == OCI_SUCCESS_WITH_INFO)){
    fprintf(stderr, "OCIEnvInit failed, rc = %d\n", rc );
    return(-1);
  }
  
  /* Create and initialize thread specific data */
  threadInfo = (tinfo_t *)calloc( (size_t)numThread, sizeof(tinfo_t));
  if (  threadInfo == NULL  ) {
    fprintf(stderr, "Unable to allocate memory for threadInfo array\n");
    return -1;
  }
  for (i = 0; i < numThread; i++){
    threadInfo[i].id = i;
    threadInfo[i].runtime = secs;
    threadInfo[i].iterations = nIterations;
    threadInfo[i].qhist = allocQhist( nQueries, nIterations );
    if (  threadInfo[i].qhist == NULL  ) {
      fprintf(stderr, "Unable to allocate memory for query history array\n");
      return -1;
    }
  }
  
  /* Create and start threads */
  printf ("Spawning threads...");
  fflush(stdout);
  for (i = 0; i < numThread; i++){
    rc = sbThreadCreate(OraThread,0,&threadInfo[i],&threadInfo[i].tid);
    gassert(rc == 0);
    threadInfo[i].state = STARTED;
  }
  printf ("done\n");
  fflush(stdout);
  
 /* Wait for all threads to be ready */
  while (  ! allState( threadInfo, numThread, READY )  )
    sbSleep(100);
  printf ("Running test...%s", (verbose > VB_DETAIL)?"\n":"");
  fflush(stdout);

  /* Start queries in all threads and wait for them all to finish */
  for (i = 0; i < numThread; i++)
    threadInfo[i].state = RUN;
  while (  ! allState( threadInfo, numThread, FINISHED )  )
    sbSleep(100);

  /* Clean up all threads and gather summary stats */
  for (t = 0; t < numThread; t++){
#ifndef WIN32
    rc = sbThreadJoin(threadInfo[t].tid, (void **)NULL);
#else /* WIN32 */
    rc = sbThreadJoin(threadInfo[t].tid);
#endif /* WIN32 */
    misses += threadInfo[t].misses;
    attempts += threadInfo[t].attempts;
    fetches += threadInfo[t].fetches;
    errors += threadInfo[t].errors;
    for (i = 0; i < nQueries; i++)
        overall += threadInfo[t].qhist[i].elapsed;
  }
  printf("%sdone\n", (verbose > VB_DETAIL)?"Test ":"");
  fflush(stdout);

  /* Clean up OCI */
  OCIHandleFree((dvoid *) env, OCI_HTYPE_ENV);

  /* Display summary statistics */
  printf(
  "\n------------------------------------------------------------\n\n"  
  "Run summary\n"
  "  Threads                           : %d\n"
  "  Total query time (milliseconds)   : %ld\n"
  "  Total queries executed            : %ld\n"
  "  Total rows fetched                : %ld\n",
  numThread, (overall/1000L), (attempts-errors), fetches
    );
  if ( (attempts-errors) > 0L) 
    printf(
  "  Average rows fetched per query    : %.1f\n",
  ((double)fetches / (double)(attempts-errors))
    );
  printf(
  "  Errors                            : %ld\n", errors
  );  
  if (overall > 0L) 
    printf(
    "  Average queries per second        : %.1f\n", ((double)attempts*1000000.0)/(double)overall
    );
  printf("\n");
  fflush(stdout);

  /* Display detailed stats depending on verbosity level */
  for (i = 0; i < nQueries; i++) {
    if (  enablePercentiles && ! calculatePercentiles( threadInfo, i, &p95, &p99 )  )
        exit(-1);
    texecs = trows = telapsed = 0L;
    tminrt = tminexecrt = 9999999999L;
    tmaxrt = tmaxexecrt = tsumrt = tsumexecrt = 0L;
    for (t = 0; t < numThread; t++) {
      if (threadInfo[t].qhist[i].execs > 0) {
          texecs += threadInfo[t].qhist[i].execs;
          trows += threadInfo[t].qhist[i].rows;
          telapsed += threadInfo[t].qhist[i].elapsed;
          tsumrt += threadInfo[t].qhist[i].sumrt;
          if (  threadInfo[t].qhist[i].minrt < tminrt  )
            tminrt = threadInfo[t].qhist[i].minrt;
          if (  threadInfo[t].qhist[i].maxrt > tmaxrt  )
            tmaxrt = threadInfo[t].qhist[i].maxrt;
          tsumexecrt += threadInfo[t].qhist[i].sumexecrt;
          if (  threadInfo[t].qhist[i].minexecrt < tminexecrt  )
            tminexecrt = threadInfo[t].qhist[i].minexecrt;
          if (  threadInfo[t].qhist[i].maxexecrt > tmaxexecrt  )
            tmaxexecrt = threadInfo[t].qhist[i].maxexecrt;
      }
    }

      printf(
  "------------------------------------------------------------\n\n"  
  "Query %d\n", i+1
      );
      if (verbose > VB_BASIC) {
        printf( "  SQL statement\n" );
        sqlPrettyPrint(stdout, qlist[i], "    ");
        printf( "\n" );
      }
      printf("  Execution statistics\n");
      printf( "    Executions                      : %ld\n", texecs );
      printf( "    Total query time (microseconds) : %ld\n", telapsed );
      if ( (texecs > 0) && (telapsed > 0) ) {
        printf(
  "    Rows/query                      : %ld\n"
  "    Queries/second                  : %.1f\n"
  "  Response times (microseconds)\n"
  "    Minimum                         : %ld\n"
  "    Average                         : %ld\n"
  "    Maximum                         : %ld\n",
      trows / texecs, ( (double)texecs * 1000000.0 ) / (double)telapsed,
      tminrt, tsumrt / texecs, tmaxrt
      );
      if ( enablePercentiles )
        printf(
  "    95th perentile                  : %ld\n"
  "    99th perentile                  : %ld\n",
      p95, p99
        );
        if (verbose > VB_BASIC) {
           printf(
  "  Exec only response times (microseconds)\n"
  "    Minimum                         : %ld\n"
  "    Average                         : %ld\n"
  "    Maximum                         : %ld\n",
        tminexecrt,
        tsumexecrt / texecs,
        tmaxexecrt
           );
        }
      }
      printf("\n");
    }
  printf("------------------------------------------------------------\n\n");
  
  return 0;
}

