/*
* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    Writing a CLOB synchronously from a file and
 *    reading its content asynchronously using a stream.
 */
'use strict';
var fs            = require('fs');
var oracledb      = require('oracledb');
var accessControl = require('./AccessControl');

// Execute the sample
run(lobSample);

// 1. Creates a table with a CLOB column
// 2. Inserts a CLOB from a string 
// 3. Reads the CLOB as a stream
//
async function lobSample(conn) {
  let result;

  console.log("> Creating table with CLOB column");
  try { await conn.execute("DROP TABLE clobs") } catch (err) { }
  await conn.execute("CREATE TABLE clobs (id NUMBER, text CLOB)"); 

  console.log("> Reading file");
  let str = fs.readFileSync(__dirname + '/text.txt', 'utf8');

  console.log("> Populating CLOB from file");
  result = await conn.execute(
    "INSERT INTO clobs VALUES (1, :colClob)",
    { colClob: str}
  ); 

  // Get the LOB for reading
  console.log("> Querying CLOB column");
  result = await conn.execute("SELECT text FROM clobs where id=1");
  let outClob = result.rows[0][0];

  console.log("> Reading CLOB");
  await new Promise( (resolve, reject) => {
    outClob.on("data", chunk => console.log(chunk.toString()));
    outClob.on("error", error => reject(error));
    outClob.on("end", () => {
      console.log("> Finished reading CLOB");
      resolve();
    });
  } );
}


// Connects to the database and executes `cb', passing the
// connection object as a parameter
async function run(cb) {
  let connection;
  try {
    console.log("> Connecting");
    let credentials = accessControl.getCredentials("lobs.js");
    connection  = await oracledb.getConnection({
      user: credentials['-u'], password: credentials['-p'],
      connectString: credentials['-c']
    });

    await cb(connection);
  }
  catch (err) {
    console.log(err); 
  }
  finally {
    if (connection) {
      try {
        connection.close();
        console.log("> Connection released");
      }
      catch (err) { console.log(err);}
    }
  }
}

