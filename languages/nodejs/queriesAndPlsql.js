/*
* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    This sample performs the follwing steps:
 *      - Creates the table 'items'
 *      - Populates the table (based on 'NUM_RECORDS').
 *      - Performs a SELECT statement fetching 100 rows
 *      - Calls a PL/SQL anonymous block to update a row
 *      - Calls a PL/SQL anonymous block to delete a row
 *      - Drops the table
 */
'use strict';
var oracledb      = require('oracledb');
var accessControl = require('./AccessControl');

const NUM_RECORDS = 100;  // Number of records to insert

// Declare all the statements
// Statements will use positional binding (:1,:2) -> [arg1, arg2]
const createStmnt =  `
  BEGIN 
    DECLARE 
      e_table_missing EXCEPTION; 
      PRAGMA EXCEPTION_INIT(e_table_missing, -00942); 
    BEGIN 
      EXECUTE IMMEDIATE ('DROP TABLE items'); 
    EXCEPTION 
      WHEN e_table_missing 
      THEN NULL; 
    END; 
    EXECUTE IMMEDIATE(' 

      CREATE TABLE items(
        id                TT_INT            NOT NULL,
        name              VARCHAR2(100)      NOT NULL,
        descr             VARCHAR2(100) NOT NULL,
        PRIMARY KEY (id))

    '); 
  END;
`;

const insertStmnt = "INSERT INTO items VALUES (:1,:2,:3)";

const selectStmnt = "SELECT id, name, descr FROM items WHERE id BETWEEN 0 AND 19";

// Block to update the description of a row
const updateBlock = `
  DECLARE 
    new_descr VARCHAR2(100) := 'updated description';
  BEGIN
    UPDATE items SET descr = new_descr WHERE id = :id_in ;
  END; 
`;

// Block to delete a row.
const deleteBlock = `
  BEGIN 
    DELETE FROM items WHERE id = :id_in;
  END; 
`;

const dropStmnt           = "DROP TABLE items";

// Execute script
main();

async function main(){
  let conn;
  try{
    conn = await connect();
    await createTable(conn);
    await insertExecuteMany(conn);
    await performSelect(conn);
    await updateWithAnonBlock(conn);
    await deleteWithAnonBlock(conn);
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
  let credentials = accessControl.getCredentials("queriesAndPlsql.js");
  // Set autocommit on
  oracledb.autocommit = true;
  let conn =  await oracledb.getConnection({
    user          : credentials['-u'],
    password      : credentials['-p'],
    connectString : credentials['-c']
  });

  return conn;
}

// Create table function
async function createTable(conn){
  // To execute a statement we call connection.execute()
  await conn.execute(createStmnt);
  let res = await conn.execute("SELECT COUNT(*) FROM items");
  // Check if table has been created properly
  if(res.rows[0] == 0)
    console.log("Table has been created");
}

// Populate with executeMany
async function insertExecuteMany(conn){
  console.log("Inserting with executeMany ...");
  // Setup all the rows to insert
  let rows = new Array(NUM_RECORDS);
  for(let i = 0; i < NUM_RECORDS; i++)
    rows[i] = [i, "name_"+i, "descr_"+i];

  // ExecteMany will insert as many rows as elements in the array
  await conn.executeMany(insertStmnt, rows);
  // Verify that the rowa have been inserted
  let res = await conn.execute("SELECT COUNT(*) FROM items");
  console.log("  " + res.rows[0], "Registries added")
}

// Perform Select and use iterators
async function performSelect(conn){
  console.log("Select some rows with one select ...");
  // 'rows': Array to save all the fetched rows
  let rows = new Array();
  
  try{
    let res = await conn.execute(selectStmnt);
    // We can iterate through 'result.rows'
    res.rows.forEach(function(row){
      rows.push(row);
    });
    console.log("  " + rows.length, "Rows have been fetched and iterated");
  }
  catch (err){
    console.error(err);
  }
}

// Update using a PLSQL anonymous block
async function updateWithAnonBlock(conn){
  console.log("Updating a row using an anonymous block ...");
  let bindVars = {
    id_in:  0  // Bind type is determined from the data type
  };

  try{
    // Check the previous value
    let res = await conn.execute("SELECT descr FROM items WHERE id = :id_in", bindVars);
    console.log("  Value before update: ", res.rows[0]);
    // Call the anonymous block
    await conn.execute(updateBlock, bindVars);
    // Verify that the value has changed
    res = await conn.execute("SELECT descr FROM items WHERE id = :id_in", bindVars);
    console.log("  Value after update: ", res.rows[0]);
  }
  catch (err){
    console.error(err);
  }
}

// Delete using a PLSQL anonymous block
async function deleteWithAnonBlock(conn){
  console.log("Delete a row using an anonymous block ...");
  let bindVars = {
    id_in:  0  // Bind type is determined from the data type
  };
  
  try{
    // Call the anonymous block
    await conn.execute(deleteBlock, bindVars);
    // Verify that the row has been deleted
    let res = await conn.execute("SELECT COUNT(*) FROM items");
    console.log("  Rows after delete = ", res.rows[0]);
  }
  catch (err){
    console.error(err);
  }
}

// Drop table 
async function dropTable(conn){
  await conn.execute(dropStmnt);
}

// Release connection
async function releaseConnection(conn) {
  if(conn){
    try{
      await conn.release();
      console.log("Connection has been released");
    }
    catch(err){
      console.error(err);
    }
  }
}
