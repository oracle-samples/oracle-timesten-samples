--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

-- This Oracle SQL*Plus script can be used to check if the instance 
--   - Is RAC enabled
--   - Has TAF failover configured
--   - Has FAN events are enabled
-- using a 10g server or greater.
--
-- For Cache Connect to take full advantage of Oracle RAC, the attributes 
-- should be:
--   RAC Enabled:   YES
--   Failover Type: SELECT
--   FAN Events:    YES
--
-- Note: 
--   In order to use this script you must have SELECT privileges on 
--   sys.v_$option, sys.v_$session, and sys.v_$services.
--

SET SERVEROUTPUT ON;

DECLARE 
  racEnabled      varchar(7);
  failoverType    v$session.failover_type%TYPE := 'NONE';
  fanNotification varchar(3)                   := 'NO';
BEGIN
  SELECT CASE value 
         WHEN 'TRUE' THEN 'YES' 
         WHEN 'FALSE' THEN 'NO'
         ELSE 'UNKNOWN' END
  INTO racEnabled
  FROM v$option 
  WHERE parameter='Real Application Clusters';

  IF (racEnabled = 'YES') THEN
    SELECT UPPER(failover_type)
    INTO failoverType 
    FROM  v$session 
    WHERE sys_context('USERENV','SID') = sid;

    SELECT DISTINCT UPPER(nvl(aq_ha_notification, 'NO'))
    INTO fanNotification
    FROM v$services 
    WHERE sys_context('USERENV', 'SERVICE_NAME') 
            = nvl2(SYS_CONTEXT('USERENV','DB_DOMAIN'), 
                   name || '.' || SYS_CONTEXT('USERENV','DB_DOMAIN'), 
                   name)
          OR 
          sys_context('USERENV', 'SERVICE_NAME') = name;
  END IF;

  DBMS_OUTPUT.PUT_LINE('RAC Enabled:   ' || racEnabled);
  DBMS_OUTPUT.PUT_LINE('Failover Type: ' || failoverType);
  DBMS_OUTPUT.PUT_LINE('FAN Events:    ' || fanNotification);
END;
.
/

SET SERVEROUTPUT OFF;

