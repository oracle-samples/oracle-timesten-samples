/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Project:    TimesTen Driver Manager
 * Version:    2.1
 * Date:       25th January 2018
 * 
 * Author:     chris.jenkins@oracle.com
 *
 * This is the header file that controls build options for the 
 * TimesTen Driver Manager (ttdrvmgr).
 */

#ifndef _TT_DMBLDOPT_H
#define _TT_DMBLDOPT_H

/*
 * #defines that can be used to control the build of TTDM.
 *
 * Most are simple on/off switches; if the macros is #defined then the
 * feature is enabled and if the macros is not defined the feature is disabled.
 * Some of the macros set things like default values.
 *
 * This first section describes all the options. Place the actual #defines 
 * that you wish to use at the end of the file.
 *
 * -------------------------------------------------------------------------
 *
 * ENABLE_THREAD_SAFETY
 *
 * Makes the library thread safe. It is strongly recommended that this
 * feature be enabled unless (a) your application code is not multi-threaded or
 * (b) you are very sure that your application code will never modify any
 * dependant ODBC objects concurrently from multiple threads. Note that
 * such modifications are not always obvious...
 *
 * For example, SQLAllocEnv() modifies global shared data and may modify other
 * already allocated Environments. SQLAllocStmt() modifies the parent HDBC.
 * SQLFreeConnect() not only modifies the HDBC but also the parent HENV. And
 * so on.
 *
 * Example:
 *
 * #define    ENABLE_THREAD_SAFETY   1
 *
 * -------------------------------------------------------------------------
 *
 * ENABLE_XLA_DIRECT
 *
 * Enables support for XLA for direct mode connections. TimesTen only 
 * supports XLA via the direct mode library; it cannot be used in client 
 * server mode. Hence any HDBC allocated with a view to being used for XLA 
 * must be allocated from a direct mode HENV. It is recommended that this 
 * feature be enabled,
 *
 * Example:
 *
 * #define  ENABLE_XLA_DIRECT       1
 *
 * -------------------------------------------------------------------------
 *
 * ENABLE_UTIL_LIB
 *
 * Enables support for the TimesTen utility library. This depends on the
 * availability of the direct mode library so this functionality will be 
 * enabled or disabled dynamically at runtime depending on whether all the
 * necessary libraries are available.
 *
 * Example:
 *
 * #define  ENABLE_UTIL_LIB
 *
 * -------------------------------------------------------------------------
 *
 * TT_LIB_VERSION
 *
 * Defines the version of TimesTen supported by this build.
 * This consists of an integer made up from the full version 
 * number of the relevant TimesTen version. In general this
 * should reflect the base release x.y.z.0.0 => xyz00 unless
 * specific features are relevant for a patch release.
 *
 * Example:
 *
 * #define  TT_LIB_VERSION      112200
 *
 * -------------------------------------------------------------------------
 *
 * TT_LIB_ID
 *
 * Defines the version information component of the TimesTen
 * library names. This is only relevant for Windows builds.
 * This consists of an integer made up from the major release
 * identification. For example, 11.2.1.x.y would be 1121.
 *
 * Example:
 *
 * #define  TT_LIB_ID          1122
 *
 * -------------------------------------------------------------------------
 *
 * FUNC_TRACING
 *
 * Enable function entry/exit tracing (for debugging TTDM)
 *
 * Example:
 *
 * #define  FUNC_TRACING            1
 *
 * -------------------------------------------------------------------------
 *
 * HVAL_TRACING
 *
 * Enable tracing of handle values on function entry (for debugging).
 * This only occurs if FUNC_TRACING is also defined
 *
 * Example:
 *
 * #define  HVAL_TRACING            1
 *
 * -------------------------------------------------------------------------
 *
 * ENABLE_DEBUG_FUNCTIONS
 *
 * Enable special public debugging functions that can be called
 * To help debug the driver manager.
 *
 * Example:
 *
 * #define  ENABLE_DEBUG_FUNCTIONS  1
 *
 * -------------------------------------------------------------------------
 *
 * SHLIB_SUFFIX
 *
 * Specifies the filename suffix for shared libraries. This is usually
 * .so but on some platforms may differ. For example:
 *
 *     Linux      .so
 *     macOS      .dylib
 *     Solaris    .so
 *     HP/UX      .sl
 *     Windows    .dll
 *
 * Example:
 *
 * #define  SHLIB_SUFFIX     ".so"
 *
 * -------------------------------------------------------------------------
 *
 * MUTEX_SPINCOUNT
 *
 * WINDOWS ONLY:
 * Defines the spincount value to be used whe trying to enter a critical
 * section. If not defined then the default value of 15000 will be used.
 *
 * Example:
 *
 * #define  MUTEX_SPINCOUNT   20000
 */

/****************** ACTUAL BUILD OPTION DEFINES ****************/

#define    ENABLE_THREAD_SAFETY   1
#define    ENABLE_XLA_DIRECT      1
#define    ENABLE_UTIL_LIB        1
#define    SHLIB_SUFFIX           ".so"
#if !defined(TT_LIB_VERSION)
#define    TT_LIB_VERSION         181110
#endif

#endif
