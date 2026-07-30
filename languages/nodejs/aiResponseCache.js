/*
 * Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    This sample demonstrates how an application can use TimesTen as the
 *    primary store for active AI response cache state. The application
 *    computes a deterministic cache key for each AI request, checks TimesTen
 *    for a fresh cached response, simulates a model call on cache miss, and
 *    stores the response and operational metadata with an expiration time.
 *
 *    NOTE: Responses are simulated; this sample does not call an AI model,
 *    perform vector search, run in-database model inference, or demonstrate
 *    TimesTen Cache for Oracle Database.
 *
 *    The sample performs the following steps:
 *      - Creates an 'ai_response_cache' table
 *      - Creates indexes for tenant/model and expiration lookups
 *      - Seeds one expired cache entry
 *      - Processes sample AI requests, showing cache misses and hits
 *      - Updates hit counts and last-accessed timestamps on cache hits
 *      - Queries cache summary and JSON metadata
 *      - Deletes expired cache entries
 *      - Drops the table
 */
'use strict';

const crypto = require('crypto');
const oracledb = require('oracledb');
const accessControl = require('./AccessControl');

oracledb.initOracleClient();
oracledb.autoCommit = true;

const TABLE_NAME = 'ai_response_cache';
const CACHE_TTL_MINUTES = 30;

const CREATE_TABLE = `
  CREATE TABLE ai_response_cache(
    cache_key        VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id        VARCHAR2(30)   NOT NULL,
    user_id          VARCHAR2(30)   NOT NULL,
    model_name       VARCHAR2(60)   NOT NULL,
    prompt_hash      VARCHAR2(64)   NOT NULL,
    prompt_summary   VARCHAR2(500)  NOT NULL,
    response_text    VARCHAR2(4000) NOT NULL,
    metadata         JSON,
    hit_count        TT_INT         NOT NULL,
    created_at       TIMESTAMP      NOT NULL,
    last_accessed_at TIMESTAMP      NOT NULL,
    expires_at       TIMESTAMP      NOT NULL)
`;

const CREATE_TENANT_MODEL_INDEX = `
  CREATE INDEX idx_ai_cache_tenant_model
  ON ai_response_cache (tenant_id, model_name)
`;

const CREATE_EXPIRES_INDEX = `
  CREATE INDEX idx_ai_cache_expires
  ON ai_response_cache (expires_at)
`;

const INSERT_CACHE_ENTRY = `
  INSERT INTO ai_response_cache (
    cache_key, tenant_id, user_id, model_name, prompt_hash, prompt_summary,
    response_text, metadata, hit_count, created_at, last_accessed_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11, :12)
`;

const SELECT_FRESH_ENTRY = `
  SELECT response_text,
         JSON_SERIALIZE(metadata RETURNING VARCHAR2(4000)),
         hit_count,
         expires_at
  FROM ai_response_cache
  WHERE cache_key = :1 AND expires_at > :2
`;

const UPDATE_CACHE_HIT = `
  UPDATE ai_response_cache
  SET hit_count = hit_count + 1,
      last_accessed_at = :1
  WHERE cache_key = :2
`;

const DELETE_EXPIRED = 'DELETE FROM ai_response_cache WHERE expires_at <= :1';
const DROP_TABLE = 'DROP TABLE ai_response_cache';

const SAMPLE_REQUESTS = [
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    model_name: 'support-summary-v1',
    prompt: 'Summarize order 45012 for a support agent.',
    temperature: 0.2
  },
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    model_name: 'support-summary-v1',
    prompt: 'Summarize order 45012 for a support agent.',
    temperature: 0.2
  },
  {
    tenant_id: 'field_service',
    user_id: 'user_204',
    model_name: 'technician-assist-v1',
    prompt: 'Draft troubleshooting steps for a router with intermittent packet loss.',
    temperature: 0.1
  },
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    model_name: 'support-summary-v2',
    prompt: 'Draft a brief shipping update for order 45012.',
    temperature: 0.2
  }
];

main();

async function main() {
  let connection;

  try {
    console.log('=== AI response cache demo ===');
    connection = await connect();
    // Rebuild the table so each run starts from the same cache state.
    await dropTable(connection, false);
    await createSchema(connection);
    // Seed one expired row so the TTL cleanup takes care of it.
    await seedExpiredEntry(connection);

    // Each request first probes the cache and only simulates a response on a miss.
    for (const request of SAMPLE_REQUESTS) {
      await processRequest(connection, request);
    }

    // Show the current cache footprint before stale rows are removed.
    await printCacheSummary(connection);
    await deleteExpiredEntries(connection);
    await dropTable(connection, true);
  }
  catch (err) {
    console.error(err);
  }
  finally {
    await releaseConnection(connection);
  }
}

async function connect() {
  const credentials = accessControl.getCredentials('aiResponseCache.js');
  const connection = await oracledb.getConnection({
    user          : credentials['-u'],
    password      : credentials['-p'],
    connectString : credentials['-c']
  });

  return connection;
}

async function dropTable(connection, reportMissing) {
  try {
    await connection.execute(DROP_TABLE);
    console.log(`✓ Table ${TABLE_NAME} dropped`);
  }
  catch (err) {
    if (reportMissing) {
      console.log(`⚠ Table ${TABLE_NAME} not dropped: ${err.message}`);
    }
  }
}

async function createSchema(connection) {
  await connection.execute(CREATE_TABLE);
  console.log(`✓ Table ${TABLE_NAME} created`);
  await connection.execute(CREATE_TENANT_MODEL_INDEX);
  console.log('✓ Index IDX_AI_CACHE_TENANT_MODEL created');
  await connection.execute(CREATE_EXPIRES_INDEX);
  console.log('✓ Index IDX_AI_CACHE_EXPIRES created');
}

function buildCacheKey(request) {
  const keyText = [
    request.tenant_id,
    request.model_name,
    request.prompt,
    String(request.temperature)
  ].join('|');

  return crypto.createHash('sha256').update(keyText, 'utf8').digest('hex');
}

function buildPromptHash(prompt) {
  return crypto.createHash('sha256').update(prompt, 'utf8').digest('hex');
}

function summarizePrompt(prompt) {
  if (prompt.length <= 80) {
    return prompt;
  }

  return prompt.slice(0, 77) + '...';
}

function simulateModelResponse(request) {
  const promptTokens = Math.max(8, request.prompt.split(/\s+/).length + 4);
  const responseText = `Simulated response for ${request.tenant_id} using ${request.model_name}: ${summarizePrompt(request.prompt)}`;
  const responseTokens = Math.max(12, responseText.split(/\s+/).length + 6);
  const latencyMs = 40 + (request.prompt.length % 35);
  const metadata = {
    temperature: request.temperature,
    promptTokens,
    responseTokens,
    latencyMs,
    safetyLabel: 'allowed',
    simulatedModelCall: true
  };

  return { responseText, metadata };
}

async function insertCacheEntry(connection, request, responseText, metadata, createdAt, expiresAt) {
  const cacheKey = buildCacheKey(request);

  await connection.execute(INSERT_CACHE_ENTRY, [
    cacheKey,
    request.tenant_id,
    request.user_id,
    request.model_name,
    buildPromptHash(request.prompt),
    summarizePrompt(request.prompt),
    responseText,
    JSON.stringify(metadata),
    0,
    createdAt,
    createdAt,
    expiresAt
  ]);

  return cacheKey;
}

async function seedExpiredEntry(connection) {
  const expiredRequest = {
    tenant_id: 'retail_app',
    user_id: 'user_099',
    model_name: 'support-summary-v1',
    prompt: 'Summarize an expired support request.',
    temperature: 0.2
  };
  const createdAt = new Date(Date.now() - (2 * 60 * 60 * 1000));
  const expiresAt = new Date(Date.now() - (60 * 1000));
  const { responseText, metadata } = simulateModelResponse(expiredRequest);

  await insertCacheEntry(connection, expiredRequest, responseText, metadata, createdAt, expiresAt);
  console.log('✓ Seeded 1 expired cache entry');
}

async function findCachedResponse(connection, cacheKey) {
  const result = await connection.execute(SELECT_FRESH_ENTRY, [cacheKey, new Date()]);
  if (result.rows && result.rows.length > 0) {
    return result.rows[0];
  }

  return null;
}

async function processRequest(connection, request) {
  const startTime = process.hrtime.bigint();
  const cacheKey = buildCacheKey(request);
  const cachedRow = await findCachedResponse(connection, cacheKey);

  // A cache hit reuses the stored response and just refreshes the access metadata.
  if (cachedRow) {
    const responseText = cachedRow[0];
    const metadataJson = cachedRow[1];
    const hitCount = Number(cachedRow[2]);
    const expiresAt = cachedRow[3];
    const metadata = JSON.parse(metadataJson);

    await connection.execute(UPDATE_CACHE_HIT, [new Date(), cacheKey]);
    console.log(
      '→ Cache hit: ' +
      `tenant=${request.tenant_id} model=${request.model_name} ` +
      `hits=${hitCount + 1} expires=${formatTimestamp(expiresAt)} ` +
      `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
    );
    console.log(`  Response: ${responseText}`);
    console.log(`  Safety label from metadata: ${metadata.safetyLabel}`);
    return;
  }

  const { responseText, metadata } = simulateModelResponse(request);
  const createdAt = new Date();
  const expiresAt = new Date(createdAt.getTime() + (CACHE_TTL_MINUTES * 60 * 1000));

  // Cache misses become the new working set for the next request.
  await insertCacheEntry(connection, request, responseText, metadata, createdAt, expiresAt);
  console.log(
    '→ Cache miss: ' +
    `tenant=${request.tenant_id} model=${request.model_name} ` +
    `stored_for_minutes=${CACHE_TTL_MINUTES} ` +
    `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
  );
  console.log(`  Response: ${responseText}`);
}

function elapsedMs(startTime) {
  return Number(process.hrtime.bigint() - startTime) / 1e6;
}

function formatTimestamp(value) {
  const date = new Date(value);
  const pad2 = (n) => String(n).padStart(2, '0');
  const pad3 = (n) => String(n).padStart(3, '0');
  return `${date.getUTCFullYear()}-${pad2(date.getUTCMonth() + 1)}-${pad2(date.getUTCDate())} ` +
         `${pad2(date.getUTCHours())}:${pad2(date.getUTCMinutes())}:${pad2(date.getUTCSeconds())}.` +
         `${pad3(date.getUTCMilliseconds())}`;
}

async function printCacheSummary(connection) {
  // Show the active cache footprint grouped by tenant and model.
  console.log('⋯ Cache summary by tenant and model:');
  const summaryResult = await connection.execute(`
    SELECT tenant_id, model_name, COUNT(*), SUM(hit_count)
    FROM ai_response_cache
    GROUP BY tenant_id, model_name
    ORDER BY tenant_id, model_name
  `);

  for (const row of summaryResult.rows) {
    const tenantId = row[0];
    const modelName = row[1];
    const entryCount = Number(row[2]);
    const hitCount = Number(row[3] || 0);
    console.log(
      `  tenant=${tenantId.padEnd(14)} model=${modelName.padEnd(22)} ` +
      `entries=${entryCount} hits=${hitCount}`
    );
  }

  console.log('⋯ Metadata safety labels:');
  const metadataResult = await connection.execute(`
    SELECT cache_key,
           JSON_VALUE(metadata, '$.safetyLabel' RETURNING VARCHAR2(30))
    FROM ai_response_cache
    ORDER BY created_at
  `);

  for (const row of metadataResult.rows) {
    const cacheKey = row[0];
    const safetyLabel = row[1];
    console.log(`  cache_key=${cacheKey.slice(0, 12)}... safetyLabel=${safetyLabel}`);
  }
}

async function deleteExpiredEntries(connection) {
  const result = await connection.execute(DELETE_EXPIRED, [new Date()]);
  const deleted = result.rowsAffected || 0;
  console.log(`✓ Deleted ${deleted} expired cache entry`);
}

async function releaseConnection(connection) {
  if (connection) {
    try {
      await connection.release();
      console.log('Connection has been released');
    }
    catch (err) {
      console.error(err);
    }
  }
}
