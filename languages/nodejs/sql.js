/*
 * Copyright (c) 2019, 2026 Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    This sample simulates a small application service that keeps active API
 *    session state in TimesTen. Applications often need fast access to this
 *    type of short-lived operational data to route requests, update activity
 *    counters, and remove sessions that are no longer active.
 *
 *    The sample uses an 'api_sessions' table with session identifiers, user
 *    names, service names, regions, request counters, session status, and a
 *    last-seen timestamp. It demonstrates basic SQL operations that are common
 *    in low-latency application state management:
 *      - Creates the 'api_sessions' table
 *      - Populates the table (based on 'NUM_RECORDS')
 *      - Performs a number of SELECTS (based on 'READ_PERCENTAGE')
 *      - Updates request counters and last-seen timestamps for active sessions
 *        (based on 'UPDATE_PERCENTAGE')
 *      - Deletes a number of session records (based on 'UPDATE_PERCENTAGE')
 *      - Drops the table
 */
'use strict';
var oracledb      = require('oracledb');
var accessControl = require('./AccessControl');
oracledb.initOracleClient();

const NUM_RECORDS       = 100;  // Number of records to insert. (Must have an exact square root)
const READ_PERCENTAGE   = 80;   // Percentage of records to perform SELECTS.
const UPDATE_PERCENTAGE = 20;   // Percentage of records to perform UPDATES & DELETES.

// Declare all the statements.
// Statements will use positional binding (:1,:2) -> [arg1, arg2]
const createStmnt = `
      CREATE TABLE api_sessions(
        session_id     TT_INT        NOT NULL PRIMARY KEY,
        user_name      VARCHAR2(30)  NOT NULL,
        service_name   VARCHAR2(40)  NOT NULL,
        region         VARCHAR2(20)  NOT NULL,
        request_count  TT_INT        NOT NULL,
        session_status VARCHAR2(12)  NOT NULL,
        last_seen      TIMESTAMP     NOT NULL)
 `;

const insertStmnt = 'INSERT INTO api_sessions VALUES (:1,:2,:3,:4,:5,:6,:7)';

const selectStmnt = `
   SELECT user_name, service_name, region, request_count, session_status
   FROM api_sessions
   WHERE session_id = :1
 `;

const updateStmnt = `
   UPDATE api_sessions
   SET request_count = request_count + 1,
       session_status = :1,
       last_seen = :2
   WHERE session_id = :3
 `;

const deleteStmnt = 'DELETE FROM api_sessions WHERE session_id = :1';

const dropStmnt = 'DROP TABLE api_sessions';

// Execute script
main();

async function main() {
  let conn;
  try {
    conn = await connect();
    await createTable(conn);
    await populateTable(conn);
    await performDML(conn, 'select');
    await performDML(conn, 'update');
    await performDML(conn, 'delete');
    await dropTable(conn);
  }
  catch (err) {
    console.error(err);
  }
  finally {
    await releaseConnection(conn);
  }
}

// Get connection
async function connect() {
  let credentials = accessControl.getCredentials('sql.js');
  // Set autocommit on
  oracledb.autoCommit = true;
  // Get connection
  let conn = await oracledb.getConnection({
    user          : credentials['-u'],
    password      : credentials['-p'],
    connectString : credentials['-c']
  });

  return conn;
}

// Create table
async function createTable(conn) {
  await conn.execute(createStmnt);
  let res = await conn.execute('SELECT COUNT(*) FROM api_sessions');
  // Check if table has been created properly
  if (res.rows[0][0] === 0)
    console.log('Table has been created');
}

// Populate table
async function populateTable(conn) {
  console.log('Populating table');
  const keyCnt = parseInt(Math.sqrt(NUM_RECORDS));
  const services = ['orders', 'payments', 'search', 'support'];
  const regions = ['us-east', 'us-west', 'eu-central', 'ap-south'];

  for (var i = 0; i < keyCnt; i++) {
    for (var j = 0; j < keyCnt; j++) {
      let sessionId = (i * keyCnt) + j;
      let userName = 'user_' + i + j;
      let serviceName = services[sessionId % services.length];
      let region = regions[sessionId % regions.length];
      let requestCount = sessionId % 25;
      let sessionStatus = 'ACTIVE';
      let lastSeen = new Date();

      await conn.execute(insertStmnt,
        [sessionId, userName, serviceName, region, requestCount, sessionStatus, lastSeen]);
    }
    console.log('  Inserted ', (i + 1) * keyCnt + ' rows');
  }

  // Verify that the rows have been inserted properly
  let res = await conn.execute('SELECT COUNT(*) FROM api_sessions');
  if (res.rows[0][0] !== (keyCnt * keyCnt))
    console.error('Error populating table');
}

// Perform DML operations
async function performDML(conn, operation) {
  console.log('Performing', operation + 's');
  // Calculate 'numOperations' depending on the proper percentage
  let numOperations;
  if (operation === 'select')
    numOperations = parseInt(NUM_RECORDS * (parseFloat(READ_PERCENTAGE) / 100));
  else if (operation === 'update' || operation === 'delete')
    numOperations = parseInt(NUM_RECORDS * (parseFloat(UPDATE_PERCENTAGE) / 100));
  else
    throw 'Unknown operation: performDML()';

  let operationsPerformed = 0;

  const keyCnt = parseInt(Math.sqrt(NUM_RECORDS));
  for (var i = 0; i < keyCnt; i++) {
    for (var j = 0; j < keyCnt; j++) {
      let sessionId = (i * keyCnt) + j;
      let stmnt = undefined;
      let args = undefined;

      // Check which operation was requested and execute it
      switch (operation) {
        case 'select':
          stmnt = selectStmnt;
          args = [sessionId];
          break;
        case 'update':
          stmnt = updateStmnt;
          args = ['ACTIVE', new Date(), sessionId];
          break;
        case 'delete':
          stmnt = deleteStmnt;
          args = [sessionId];
          break;
        default:
          throw 'Unknown operation: performDML()';
      }

      try {
        let res = await conn.execute(stmnt, args);
        if (operation === 'select')
          operationsPerformed += res.rows.length;
        else
          operationsPerformed += res.rowsAffected;

        if (operationsPerformed === numOperations) {
          console.log('  ' + operation + '(ed)', operationsPerformed, 'rows');
          return;
        }
      }
      catch (err) {
        console.error(err);
      }
    }
    console.log('  ' + operation + '(ed)', (i + 1) * keyCnt, 'rows');
  }
}

// Drop table
async function dropTable(conn) {
  await conn.execute(dropStmnt);
}

// Release connection
async function releaseConnection(conn) {
  if (conn) {
    try {
      await conn.release();
      console.log('Connection has been released');
    }
    catch (err) {
      console.error(err);
    }
  }
}
