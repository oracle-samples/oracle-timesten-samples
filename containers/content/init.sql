create user APPUSER identified by "appuser" account lock password expire;
grant create session, create table to APPUSER;

create table APPUSER.SIMPLETAB
(
    PK    number(8,0) not null primary key,
    C1    varchar2(30) not null,
    C2    timestamp
);

insert into APPUSER.SIMPLETAB values (1, 'This is row 1', sysdate);
