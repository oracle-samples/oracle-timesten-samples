#
# Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
# Oracle TimesTen 22.1 Quick Start Sample Program Environment setup script.
#
shelltype=`basename "$0"`
if [ \( "${shelltype}" != "-bash" \) -a \
     \( "${shelltype}" != "bash" \) -a \
     \( "${shelltype}" != "-sh" \) -a \
     \( "${shelltype}" != "sh" \) ]
then
    echo "Please 'source' this script; do not run it as a regular script."
    exit 1
fi

#
# Edit and uncomment the line below to have this script automatically
# source the TimesTen environment script.
#
# source <tt_instance_home>/bin/ttenv.sh
#

canonPath()
{
    local cwd nwd nfn

    cwd=`pwd -P`
    nwd=`dirname "$1"`
    nfn=`basename "$1"`
    cd "${nwd}" >/dev/null 2>&1
    if [ $? -ne 0 ]
    then
        cd "${cwd}"
        return 1
    fi
    nwd=`pwd -P`
    if [ -d "${nfn}" ]
    then
        cd "${nfn}" >/dev/null 2>&1
        if [ $? -ne 0 ]
        then
            cd "${cwd}"
            return 1
        fi
        nwd=`pwd -P`
        echo "${nwd}"
    else
        echo "${nwd}/${nfn}"
    fi
    cd "${cwd}"
    return 0
}

if [ -z "${TIMESTEN_HOME}" ] 
then 
    echo "TIMESTEN_HOME has not been defined. Did you source ttenv.sh?"
    return 2
elif [ ! -e "${TIMESTEN_HOME}/bin/ttenv.sh" ];
then
    echo "TIMESTEN_HOME is not set correctly. Did you source ttenv.sh?"
    return 2
fi


QUICKSTART_HOME=`canonPath "${BASH_SOURCE[${#BASH_SOURCE[@]} - 1]}"`
QUICKSTART_HOME=`dirname "${QUICKSTART_HOME}"`


if [ "${QUICKSTART_HOME}" == "." ]
then
    QUICKSTART_HOME=`pwd`
fi
if [ -z "${QUICKSTART_HOME}" ]
then
    echo "Unable to determine QuickStart install location"
    return 3
fi
export QUICKSTART_HOME

echo 
echo "TIMESTEN_HOME=${TIMESTEN_HOME}"

echo
echo "QUICKSTART_HOME=${QUICKSTART_HOME}"

export PATH="${QUICKSTART_HOME}/sample_code/odbc:${QUICKSTART_HOME}/sample_code/proc:${QUICKSTART_HOME}/sample_code/oci:${QUICKSTART_HOME}/sample_code/odbc/xla:${QUICKSTART_HOME}/sample_code/jdbc:${QUICKSTART_HOME}/sample_code/odbc_drivermgr:${QUICKSTART_HOME}/sample_code/proc:${QUICKSTART_HOME}/sample_code/ttclasses:${QUICKSTART_HOME}/sample_code/ttclasses/xla:${PATH}"

export LD_LIBRARY_PATH="${QUICKSTART_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

export CLASSPATH="${QUICKSTART_HOME}/sample_code/jdbc${CLASSPATH:+:${CLASSPATH}}"

echo
echo "PATH=${PATH}"
echo
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"
echo
echo "CLASSPATH=${CLASSPATH}"
echo
if [ -z "${TNS_ADMIN}" ]
then
    echo "TNS_ADMIN is not set"
else
    echo "TNS_ADMIN=${TNS_ADMIN}" 
fi
echo

return 0
