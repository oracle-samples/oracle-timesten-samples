@echo off

if x%TIMESTEN_HOME% == x goto err

set QUICKSTART_HOME=%~dp0

set PATH=%QUICKSTART_HOME%sample_code\odbc\bin;%QUICKSTART_HOME%sample_code\jdbc;%QUICKSTART_HOME%sample_code\database;%PATH%

set CLASSPATH=%QUICKSTART_HOME%sample_code\jdbc;%CLASSPATH%

goto fini

:err
echo TIMESTEN_HOME is not set!

:fini
