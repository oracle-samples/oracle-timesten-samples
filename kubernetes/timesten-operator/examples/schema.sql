create user appuser identified by appuser;
grant create session, create table to appuser;
create table appuser.testtab
(
    pkey      number(8,0) not null primary key,
    col1      varchar2(40)
);
insert into appuser.testtab values (1, 'Row 1');
