/*
 * Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Project:    TimesTen Driver Manager
 * Version:    18.1.1
 * Date:       19th February 2021
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
 * that you wish to use at the end of the file. Alternatively you can
 * define these using compiler command line directives (-D).
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
 * #define  ENABLE_XLA_DIRECT
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
 * EXPORT_SQLCONNECTW
 *
 * The TimesTen drivers do not export SQLConnectW() because...
 * Instead they have a driver specific function, ttSQLConnectW()
 * with identical functionality.
 *
 * If you set this define then TTDM will export SQLConnectW and 
 * will map it to ttSQLConnectW.
 *
 * Example:
 *
 * #define  ENABLE_UTIL_LIB
 *
 * -------------------------------------------------------------------------
 *
 * FUNC_TRACING
 *
 * Enable function entry/exit tracing (for debugging TTDM)
 *
 * Example:
 *
 * #define  FUNC_TRACING
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
 * #define  HVAL_TRACING
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
 * #define  ENABLE_DEBUG_FUNCTIONS
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
 *
 * If not defined then defaults to ".so"
 *
 * Example:
 *
 * #define  SHLIB_SUFFIX     ".so"
 *
 * -------------------------------------------------------------------------
 */

/****************** ACTUAL BUILD OPTION DEFINES ****************/

#define    ENABLE_XLA_DIRECT
#define    ENABLE_UTIL_LIB
#define    EXPORT_SQLCONNECTW

#endif
