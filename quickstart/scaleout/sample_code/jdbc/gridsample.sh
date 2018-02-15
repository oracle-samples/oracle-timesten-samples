#!/bin/bash
java -Djava.library.path=${TIMESTEN_HOME}/install/lib -cp ./GridSample.jar:${CLASSPATH} GridSample $*
exit $?
