#
# Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
# Oracle TimesTen 22.1 Quick Start Sample Program Environment setup script.
#

set shelltype=`basename "$0"`
if ( ("${shelltype}" != "-tcsh" ) && \
     ( "${shelltype}" != "tcsh" ) && \
     ( "${shelltype}" != "-csh" ) && \
     ( "${shelltype}" != "csh" ) ) then
    echo "Please 'source' this script; do not run it as a regular script."
    exit 1
endif
#
## Edit and uncomment the line below to have this script automatically
# source the TimesTen environment script.
# #
# # source <tt_instance_home>/bin/ttenv.csh
# #

if ( ! ${?TIMESTEN_HOME} ) then
    echo "TIMESTEN_HOME has not been defined. Did you source ttenv.csh?"
    set _bad=1
else
    set _bad=0
endif

if (! ${_bad} ) then
    if ( ! -e "${TIMESTEN_HOME}/bin/ttenv.sh" ) then
        echo "TIMESTEN_HOME is not set correctly. Did you source ttenv.csh?"
    else
        set called=($_)
        set cwd=`pwd -P`
        set nwd=`dirname "${called[2]}"`
        set nfn=`basename "${called[2]}"`
        cd "${nwd}" >& /dev/null
        if ( $? != 0 ) then
            cd "${cwd}"
            set _bad=1
        else
            set nwd=`pwd -P`
            if ( -d "${nfn}" ) then
                cd "${nfn}" >&/dev/null
                if ( $? != 0 ) then
                    cd "${cwd}"
                    set _bad=1
                else
                    set nwd=`pwd -P`
                    setenv QUICKSTART_HOME "${nwd}"
                endif
            else
                setenv QUICKSTART_HOME "${nwd}/${nfn}"
            endif
        endif
        cd "${cwd}"

        if (! ${_bad} ) then
            setenv QUICKSTART_HOME `dirname "${QUICKSTART_HOME}"`
        endif
    
        if ( ! ${?QUICKSTART_HOME} ) then
            echo "Unable to determine QuickStart install location"
        else
    
            echo
            echo "TIMESTEN_HOME=${TIMESTEN_HOME}"
    
            echo
            echo "QUICKSTART_HOME=${QUICKSTART_HOME} "
    
            setenv PATH "$QUICKSTART_HOME/sample_code/odbc/bin:$QUICKSTART_HOME/sample_code/jdbc:$QUICKSTART_HOME/sample_code/database:${PATH}"
        
            if (${?CLASSPATH}) then
                setenv CLASSPATH  "${QUICKSTART_HOME}/sample_code/jdbc:${CLASSPATH}"
            else
                setenv CLASSPATH "${QUICKSTART_HOME}/sample_code/jdbc"
            endif
    
            echo
            echo "PATH=${PATH}"
            echo
            echo "CLASSPATH=${CLASSPATH}"
            echo
            if (${?TNS_ADMIN}) then
                echo "TNS_ADMIN=${TNS_ADMIN}"
            else
                echo "TNS_ADMIN is not set"
            endif
            echo
        endif
    endif
endif
