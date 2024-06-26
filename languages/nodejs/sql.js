/*
* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    This sample performs the follwing steps:
 *      - Creates a table 'vpn_users'
 *      - Populates the table (based on 'NUM_RECORDS').
 *      - Performs a number of SELECTS (based on 'READ_PERCENTAGE')
 *      - Updates a number of records (based on 'UPDATE_PERCENTAGE')
 *      - Deletes a number of records (based on 'UPDATE_PERCENTAGE')
 *      - Drops the table
 */
'use strict';
var oracledb      = require('oracledb');
var accessControl = require('./AccessControl');
oracledb.initOracleClient();

const NUM_RECORDS       = 100;  // Number of records to insert. (Must have an exact square root)
const READ_PERCENTAGE   = 80;     // Percentage of records to perform SELECTS.
const UPDATE_PERCENTAGE = 20;     // Percentage of records to perform UPDATES & DELETES.

// Declare all the statements.
// Statements will use positional binding (:1,:2) -> [arg1, arg2]
const createStmnt =  `
      CREATE TABLE vpn_users( 
        vpn_id             TT_INT   NOT NULL,  
        vpn_nb             TT_INT   NOT NULL,  
        directory_nb       CHAR(100)  NOT NULL, 
        last_calling_party CHAR(100)  NOT NULL,  
        descr              CHAR(100) NOT NULL, 
        PRIMARY KEY (vpn_id, vpn_nb)); 
 `;

const insertStmnt = "INSERT INTO vpn_users VALUES (:1,:2,:3,:4,:5)";

const selectStmnt = `
   SELECT directory_nb, last_calling_party, descr FROM vpn_users 
   WHERE vpn_id = :1 AND vpn_nb= :2 
 `;

const updateStmnt = `
   UPDATE vpn_users SET last_calling_party = :1 
   WHERE vpn_id = :2 AND vpn_nb = :3 
 `;

const deleteStmnt = "DELETE FROM vpn_users WHERE vpn_id = :1 AND vpn_nb = :2";

const dropStmnt   = "DROP TABLE vpn_users";

// Execute script
main();

async function main(){
 let conn; 
  try{
    conn = await connect();
    await createTable(conn);
    await populateTable(conn);
    await performDML(conn, "select");
    await performDML(conn, "update");
    await performDML(conn, "delete");
    await dropTable(conn);
  }
  catch (err){
    console.error(err);
  }
  finally {
    await releaseConnection(conn);
  }
}

// Get connection
async function connect() {
  let credentials = accessControl.getCredentials("sql.js");
  // Set autocommit on
  oracledb.autocommit = true;
  // Get connection
  let conn = await oracledb.getConnection({
    user          : credentials['-u'],
    password      : credentials['-p'],
    connectString : credentials['-c']
  });

  return conn;
}

// Create table
async function createTable(conn){
  await conn.execute(createStmnt);
  let res = await conn.execute("SELECT COUNT(*) FROM vpn_users");
  // Check if table has been created properly
  if(res.rows[0] == 0)
    console.log("Table has been created");
}

// Populate table
async function populateTable(conn){
  console.log("Populating table");
  // Keys are a combination of vpn_id and vpn_nb
  const keyCnt = parseInt(Math.sqrt(NUM_RECORDS));
  
  for(var i = 0; i < keyCnt; i++){
    for(var j = 0; j < keyCnt; j++){
      let id = i;
      let nb = j;
      let directoryNb       = "dir_"  + i + j;
      let lastCallingParty  = "call_" + i + j;
      let desc              = "desc"  + i + j;

      await conn.execute(insertStmnt, 
        [id, nb, directoryNb, lastCallingParty, desc]);
    }
    console.log("  Inserted ", (i+1)*keyCnt + " rows");
  }

  // Verify that the rows have been inserted properly
  let res = await conn.execute("SELECT COUNT(*) FROM vpn_users");
  if(res.rows[0] != (keyCnt * keyCnt))
    console.error("Error populating table");
}

// Perform DML operations
async function performDML(conn, operation){
  console.log("Performing", operation+"s");
  // Calculate 'numOperations' depending on the proper percentage
  let numOperations;
  if (operation   === "select")
    numOperations = parseInt(100 * (parseFloat(READ_PERCENTAGE) / parseFloat(NUM_RECORDS)));
  else if (operation == "update" || "delete")
    numOperations = parseInt(100 * (parseFloat(UPDATE_PERCENTAGE) / parseFloat(NUM_RECORDS)));
    
  let operationsPerformed = 0;

  // Keys are a combination of vpn_id and vpn_nb
  const keyCnt = parseInt(Math.sqrt(NUM_RECORDS));
  for(var i = 0; i < keyCnt; i++){
    for(var j = 0; j < keyCnt; j++){
      let id = i;
      let nb = j;
      let stmnt = undefined;
      let args  = undefined;

      // Check which operation was requested and execute it
      switch(operation){
        case "select":
          stmnt = selectStmnt;
          args  = [id, nb];
          break;
        case "update":
          stmnt = updateStmnt;
          args  = [("callU_" + id + nb), id, nb];
          break;
        case "delete":
          stmnt = deleteStmnt;
          args  = [id, nb];
          break;
        default:
          throw "Unknown operation: performDML()";
      }

      try{
        let res = await conn.execute(stmnt, args);
        if(operation == "select")
          operationsPerformed += res.rows.length;
        else
          operationsPerformed += res.rowsAffected;

        if(operationsPerformed == numOperations){
          console.log("  " + operation+"(ed)", operationsPerformed, "rows");
          return;
        }
      }
      catch (err){
        console.error(err);
      }
    }
    console.log("  " + operation+"(ed)", (i+1)*keyCnt, "rows");
  }
}

// Drop table
async function dropTable(conn, cb){
  await conn.execute(dropStmnt);
}

// Release connection
async function releaseConnection(conn) {
  if(conn){
   try {
      await conn.release();
      console.log("Connection has been released");
    }
    catch (err){
      console.error(err);
    }
  }  
}
