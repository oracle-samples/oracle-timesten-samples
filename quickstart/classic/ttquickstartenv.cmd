@echo off

if x%TIMESTEN_HOME% == x goto err

set QUICKSTART_HOME=%~dp0

set PATH=%QUICKSTART_HOME%sample_code\odbc;%QUICKSTART_HOME%sample_code\proc;%QUICKSTART_HOME%sample_code\oci;%QUICKSTART_HOME%sample_code\jdbc;%QUICKSTART_HOME%sample_code\odbc_drivermgr;%QUICKSTART_HOME%sample_code\ttclasses;%PATH%

goto fini

:err
echo TIMESTEN_HOME is not set!

:fini
