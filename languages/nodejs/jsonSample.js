/*
* Copyright (c) 2026, Oracle and/or its affiliates.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    Demonstrates TimesTen JSON features using the node-oracledb driver.
 */
'use strict';

const fs            = require('fs/promises');
const path          = require('path');
const oracledb      = require('oracledb');
const accessControl = require('./AccessControl');

oracledb.initOracleClient();
oracledb.autoCommit = true;
oracledb.fetchAsString = [oracledb.CLOB];

const TABLE_NAME      = 'j_purchaseorder';
const JSON_DOC1       = '../../quickstart/classic/sample_code/common/jsondoc1.json';
const JSON_DOC1_V2    = '../../quickstart/classic/sample_code/common/jsondoc1-v2.json';
const JSON_DOC2       = '../../quickstart/classic/sample_code/common/jsondoc2.json';
const ALLOWED_ARGS    = ['-u', '-p', '-c', '-keep', '-help', '-h'];
const JSON_FETCH_OPTIONS = { outFormat: oracledb.OUT_FORMAT_OBJECT };
const USAGE = `
Usage: node jsonSample.js -u <userName> -p <password> [-c <connectionString>] [-keep]

Demonstrates TimesTen JSON support by creating table ${TABLE_NAME}, loading
purchase-order documents, indexing JSON data, running queries, and optionally
dropping the table at completion. Provide credentials using -u/-p and an
optional connect string with -c. Use -keep to retain the table afterwards.
`;

main();

/**
 * Entrypoint for the JSON sample. Parses command-line arguments, establishes a
 * database connection, runs the sample workflow, and ensures the connection is
 * closed. Displays usage details when requested.
 *
 * @returns {Promise<void>} Resolves when the sample completes or exits.
 */
async function main() {
  const args = accessControl.parseArgs(ALLOWED_ARGS);

  if (!args || args['-help'] || args['-h']) {
    console.log(USAGE);
    return;
  }

  let credentials;
  try {
    credentials = accessControl.getCredentials('jsonSample.js');
  }
  catch (err) {
    console.error(err);
    process.exitCode = 1;
    return;
  }

  let connection;
  const keepData = Boolean(args['-keep']);

  try {
    console.log('Connecting');
    const connectionOptions = {
      user          : credentials['-u'],
      password      : credentials['-p'],
      connectString : credentials['-c']
    };

    connection = await oracledb.getConnection(connectionOptions);
    console.log('Connected');

    await runSample(connection, keepData);

    console.log('Completed JSON sample operations');
    if (!keepData) {
      await dropTable(connection, true);
    }
    else {
      console.log(`Leaving table ${TABLE_NAME} in place (-keep specified)`);
    }
  }
  catch (err) {
    console.error(err);
    process.exitCode = 1;
  }
  finally {
    if (connection) {
      try {
        await connection.close();
        console.log('Connection has been released');
      }
      catch (closeErr) {
        console.error(closeErr);
      }
    }
  }
}

/**
 * Executes the JSON sample workflow by recreating the table, loading data,
 * indexing JSON content, running query demonstrations, and cleaning up.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @param {boolean} keepData Indicates whether the table should remain after execution.
 * @returns {Promise<void>} Resolves when the workflow completes.
 */
async function runSample(connection, keepData) {
  await dropTable(connection, false);
  await createTable(connection);

  const jsonDoc1    = await readJsonDocument(JSON_DOC1);
  const jsonDoc1v2  = await readJsonDocument(JSON_DOC1_V2);
  const jsonDoc2    = await readJsonDocument(JSON_DOC2);

  await insertPurchaseOrder(connection, '1600', jsonDoc1);
  await insertPurchaseOrder(connection, '1721', jsonDoc2);

  await createJsonIndex(connection);

  await updatePurchaseOrder(connection, '1600', jsonDoc1v2, JSON_DOC1_V2);

  await selectPurchaseOrderById(connection, '1600');
  await selectPurchaseOrderById(connection, '1721');

  await selectPurchaseOrdersByUser(connection, 'ABULL');
  await selectPurchaseOrdersByUser(connection, 'CGIRAFFE');

  await listLineItemsForOrder(connection, '1600');
}

/**
 * Creates the sample purchase order table used by the JSON demonstration.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @returns {Promise<void>} Resolves when the table has been created.
 */
async function createTable(connection) {
  const sql = `CREATE TABLE ${TABLE_NAME} (
    id          VARCHAR2(32) NOT NULL PRIMARY KEY,
    date_loaded TIMESTAMP,
    po_document JSON
  )`;

  await connection.execute(sql);
  console.log(`Table ${TABLE_NAME} created`);
}

/**
 * Drops the sample table if it exists. Optionally reports when the table is
 * missing instead of treating it as an error.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @param {boolean} reportMissing When true, logs an informational message if the table does not exist.
 * @returns {Promise<void>} Resolves regardless of table existence.
 */
async function dropTable(connection, reportMissing) {
  const sql = `DROP TABLE ${TABLE_NAME}`;

  try {
    await connection.execute(sql);
    console.log(`Table ${TABLE_NAME} dropped`);
  }
  catch (err) {
    if (reportMissing) {
      console.log(`Table ${TABLE_NAME} not dropped: ${err.message}`);
    }
  }
}

/**
 * Inserts a JSON purchase order document into the sample table.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @param {string} id Purchase order identifier.
 * @param {string} jsonDocument JSON document content.
 * @returns {Promise<void>} Resolves when the row has been inserted.
 */
async function insertPurchaseOrder(connection, id, jsonDocument) {
  const sql = `INSERT INTO ${TABLE_NAME} (id, date_loaded, po_document)
               VALUES (:id, :date_loaded, :po_document)`;

  await connection.execute(sql, {
    id,
    date_loaded : new Date(),
    po_document : jsonDocument
  });
  console.log(`Inserted purchase order with id ${id}`);
}

/**
 * Updates an existing purchase order document, recording the update timestamp.
 * Reports whether the row was updated and which document file was used.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @param {string} id Purchase order identifier.
 * @param {string} jsonDocument Updated JSON document content.
 * @param {string} sourcePath Path of the JSON file used for the update.
 * @returns {Promise<void>} Resolves when the update completes.
 */
async function updatePurchaseOrder(connection, id, jsonDocument, sourcePath) {
  const sql = `UPDATE ${TABLE_NAME}
               SET date_loaded = :date_loaded,
                   po_document = :po_document
               WHERE id = :id`;

  const result = await connection.execute(sql, {
    date_loaded : new Date(),
    po_document : jsonDocument,
    id
  });

  if (result.rowsAffected > 0) {
    console.log(`Updated purchase order with id ${id} using ${path.basename(sourcePath)}`);
  }
  else {
    console.log(`No rows updated for id ${id}`);
  }
}

/**
 * Creates a JSON value index on the purchase order user attribute to
 * accelerate user-based queries.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @returns {Promise<void>} Resolves when the index has been created.
 */
async function createJsonIndex(connection) {
  const sql = `CREATE INDEX idx_json_user ON ${TABLE_NAME} (
    JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128) ERROR ON ERROR)
  )`;

  await connection.execute(sql);
  console.log('JSON index IDX_JSON_USER created');
}

/**
 * Retrieves and prints a formatted JSON purchase order document by its
 * identifier.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @param {string} id Purchase order identifier.
 * @returns {Promise<void>} Resolves when output has been written to the console.
 */
async function selectPurchaseOrderById(connection, id) {
  const sql = `SELECT JSON_SERIALIZE(po_document PRETTY) AS po
               FROM ${TABLE_NAME}
               WHERE id = :id`;

  const result = await connection.execute(sql, { id }, JSON_FETCH_OPTIONS);

  if (result.rows.length === 0) {
    console.log(`No purchase order found for id ${id}`);
    return;
  }

  console.log(`Purchase order ${id}:`);
  result.rows.forEach(row => console.log(row.PO));
}

/**
 * Retrieves and prints all purchase orders associated with a specific user.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @param {string} user Purchase order user value.
 * @returns {Promise<void>} Resolves when output has been written to the console.
 */
async function selectPurchaseOrdersByUser(connection, user) {
  const sql = `SELECT JSON_SERIALIZE(po_document PRETTY) AS po
               FROM ${TABLE_NAME}
               WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = :user`;

  const result = await connection.execute(sql, { user }, JSON_FETCH_OPTIONS);

  if (result.rows.length === 0) {
    console.log(`No purchase orders found for user ${user}`);
    return;
  }

  console.log(`Purchase orders for user ${user}:`);
  result.rows.forEach(row => console.log(row.PO));
}

/**
 * Lists and formats each line item within a specified purchase order,
 * calculating the extended price for display.
 *
 * @param {oracledb.Connection} connection Active database connection.
 * @param {string} id Purchase order identifier.
 * @returns {Promise<void>} Resolves when the line items have been printed.
 */
async function listLineItemsForOrder(connection, id) {
  const sql = `
    SELECT jt.line_number AS line_number,
           jt.sku         AS sku,
           jt.description AS description,
           jt.quantity    AS quantity,
           jt.unit_price  AS unit_price
      FROM ${TABLE_NAME} po,
           JSON_TABLE(po.po_document, '$.LineItems[*]'
             COLUMNS (
               line_number FOR ORDINALITY,
               sku         VARCHAR2(40)  PATH '$.Part.UPCCode',
               description VARCHAR2(256) PATH '$.Part.Description',
               quantity    NUMBER        PATH '$.Quantity',
               unit_price  NUMBER        PATH '$.Part.UnitPrice'
             )
           ) jt
     WHERE po.id = :id`;

  const result = await connection.execute(sql, { id }, JSON_FETCH_OPTIONS);

  if (result.rows.length === 0) {
    console.log(`No line items found for purchase order ${id}`);
    return;
  }

  console.log(`Line items for purchase order ${id}:`);
  console.log('Line  SKU           Description                     Qty  Unit Price  Extended');
  result.rows.forEach(row => {
    const extended = row.UNIT_PRICE * row.QUANTITY;
    const line = row.LINE_NUMBER.toString().padStart(4, ' ');
    const sku = (row.SKU || '').toString().padEnd(12, ' ');
    const desc = (row.DESCRIPTION || '').toString().padEnd(30, ' ');
    const qty = Number(row.QUANTITY).toFixed(0).padStart(4, ' ');
    const unit = Number(row.UNIT_PRICE).toFixed(2).padStart(10, ' ');
    const ext = extended.toFixed(2).padStart(8, ' ');
    console.log(`${line}  ${sku} ${desc} ${qty}  ${unit}  ${ext}`);
  });
}

/**
 * Reads a JSON document from a relative path either under the sample's
 * directory or the current working directory.
 *
 * @param {string} relativePath Relative path to the JSON document.
 * @returns {Promise<string>} Resolves with the file contents.
 */
async function readJsonDocument(relativePath) {
  const resolved = await locateJsonFile(relativePath);
  console.log(`Reading ${path.basename(resolved)}`);
  return fs.readFile(resolved, 'utf8');
}

/**
 * Determines the absolute path to a JSON document by checking the sample
 * directory first and then the current working directory.
 *
 * @param {string} relativePath Relative path to search for.
 * @returns {Promise<string>} Resolves with the absolute path when found.
 * @throws {Error} When the document cannot be located.
 */
async function locateJsonFile(relativePath) {
  const candidate = path.resolve(__dirname, relativePath);
  if (await fileExists(candidate)) {
    return candidate;
  }

  const local = path.resolve(process.cwd(), path.basename(relativePath));
  if (await fileExists(local)) {
    return local;
  }

  throw new Error(`JSON document not found: ${relativePath}`);
}

/**
 * Checks for file existence using read access.
 *
 * @param {string} filePath Absolute path to a file.
 * @returns {Promise<boolean>} Resolves true when the file exists.
 */
async function fileExists(filePath) {
  try {
    await fs.access(filePath);
    return true;
  }
  catch (err) {
    return false;
  }
}
