REM INSERTING into ACCOUNT_TYPE

Insert into ACCOUNT_TYPE (TYPE,DESCRIPTION) values ('I','Individual');
Insert into ACCOUNT_TYPE (TYPE,DESCRIPTION) values ('F','Family');
Insert into ACCOUNT_TYPE (TYPE,DESCRIPTION) values ('H','Home');
Insert into ACCOUNT_TYPE (TYPE,DESCRIPTION) values ('B','Business');
Insert into ACCOUNT_TYPE (TYPE,DESCRIPTION) values ('P','Prepaid Individual');
Insert into ACCOUNT_TYPE (TYPE,DESCRIPTION) values ('Q','Prepaid Family');

REM INSERTING into ACCOUNT_STATUS

Insert into ACCOUNT_STATUS (STATUS,DESCRIPTION) values (10,'Active - Account is in good standing');
Insert into ACCOUNT_STATUS (STATUS,DESCRIPTION) values (20,'Pending - Payment is being processed');
Insert into ACCOUNT_STATUS (STATUS,DESCRIPTION) values (30,'Grace - Automatic payment did not process successfully');
Insert into ACCOUNT_STATUS (STATUS,DESCRIPTION) values (40,'Suspend - Account is in process of being disconnected');
Insert into ACCOUNT_STATUS (STATUS,DESCRIPTION) values (50,'Disconnected - You can no longer make calls or receive calls');
