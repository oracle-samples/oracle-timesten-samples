/* 
 * portable_thread.h : simple thread abstraction, to make writing multi-platform 
 *                     multi-threaded programs easier
 *
 * Copyright (C) 1999, 2007, Oracle. All rights reserved. 
 * All rights reserved. 
 */

#ifndef _TT_THREAD_H_
#define _TT_THREAD_H_

/* OS-specific header files */

#ifdef _WIN32
#include <windows.h>
#include <process.h>    /* _beginthread, _endthread */
#else 
#include <pthread.h>
#endif /* _WIN32 */

/* define the portable_thread type. */

#ifdef _WIN32
typedef HANDLE portable_thread;
#else /* UNIX */
typedef pthread_t portable_thread;
#endif /* _WIN32 */

/* The following two methods return 0 for success, -1 for error */

#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void*(*thread_signature)(void*);

int PortableThreadCreate(thread_signature th_sig, /* void*(*start_address)(void*), */
                         unsigned stack_size,
                         void* arg,
                         portable_thread* threadP /* OUT */
                         );

int PortableThreadJoin(portable_thread, void** statusPP, int* errorP);

#ifdef __cplusplus
}; /* extern "C" */
#endif /* __cplusplus */

#endif /* _TT_THREAD_H_ */



