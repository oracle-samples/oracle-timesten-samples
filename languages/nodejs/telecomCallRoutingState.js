/*
 * Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   This sample demonstrates how an application can use TimesTen as a fast
 *   store for real-time telecom call routing state. The application keeps
 *   hot routing decisions close to the service that needs them, applies
 *   deterministic routing rules, stores request and decision metadata in
 *   JSON, and removes expired routing state.
 *
 *   The sample uses simulated routing rules. It does not call a telecom
 *   switch, perform network signaling, or depend on an external service.
 *
 *   The sample performs the following steps:
 *     - Creates a 'call_routing_state' table
 *     - Creates indexes for tenant/subscriber, call id, and expiration
 *       lookups
 *     - Seeds one expired routing record
 *     - Processes sample call routing requests
 *     - Shows idempotent replay for a repeated call request
 *     - Stores request and decision metadata in JSON
 *     - Summarizes active routing decisions by tenant/subscriber/state
 *     - Deletes expired routing records
 *     - Drops the table
 */
'use strict';

const crypto = require('crypto');
const oracledb = require('oracledb');
const accessControl = require('./AccessControl');

oracledb.initOracleClient();
oracledb.autoCommit = true;

const TABLE_NAME = 'call_routing_state';

const CREATE_TABLE = `
  CREATE TABLE call_routing_state(
    routing_key        VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id          VARCHAR2(30)   NOT NULL,
    subscriber_id      VARCHAR2(30)   NOT NULL,
    call_id            VARCHAR2(40)   NOT NULL,
    source_region      VARCHAR2(30)   NOT NULL,
    target_region      VARCHAR2(30)   NOT NULL,
    network_slice      VARCHAR2(30)   NOT NULL,
    priority_class     VARCHAR2(20)   NOT NULL,
    route_state        VARCHAR2(20)   NOT NULL,
    route_reason       VARCHAR2(120)  NOT NULL,
    request_payload    JSON,
    decision_payload   JSON,
    created_at         TIMESTAMP      NOT NULL,
    updated_at         TIMESTAMP      NOT NULL,
    expires_at         TIMESTAMP      NOT NULL)
`;

const CREATE_TENANT_SUBSCRIBER_INDEX = `
  CREATE INDEX idx_call_route_tenant_sub
  ON call_routing_state (tenant_id, subscriber_id)
`;

const CREATE_CALL_ID_INDEX = `
  CREATE INDEX idx_call_route_call_id
  ON call_routing_state (call_id)
`;

const CREATE_EXPIRES_INDEX = `
  CREATE INDEX idx_call_route_expires
  ON call_routing_state (expires_at)
`;

const INSERT_ROUTING = `
  INSERT INTO call_routing_state(
    routing_key, tenant_id, subscriber_id, call_id, source_region,
    target_region, network_slice, priority_class, route_state, route_reason,
    request_payload, decision_payload, created_at, updated_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11, :12, :13, :14, :15)
`;

const SELECT_EXISTING_ROUTING = `
  SELECT route_state,
         route_reason,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM call_routing_state
  WHERE routing_key = :1
    AND expires_at > :2
`;

const SELECT_ACTIVE_SUMMARY = `
  SELECT tenant_id,
         subscriber_id,
         route_state,
         COUNT(*)
  FROM call_routing_state
  WHERE expires_at > :1
  GROUP BY tenant_id, subscriber_id, route_state
  ORDER BY tenant_id, subscriber_id, route_state
`;

const SELECT_ACTIVE_DETAILS = `
  SELECT call_id,
         source_region,
         target_region,
         network_slice,
         priority_class,
         route_state,
         route_reason,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM call_routing_state
  WHERE expires_at > :1
  ORDER BY tenant_id, subscriber_id, call_id
`;

const DELETE_EXPIRED = 'DELETE FROM call_routing_state WHERE expires_at <= :1';
const DROP_TABLE = 'DROP TABLE call_routing_state';

const CALL_REQUESTS = [
  {
    tenant_id: 'north_mobile',
    subscriber_id: 'sub_1001',
    call_id: 'call_1001',
    source_region: 'us-east',
    target_region: 'us-east',
    network_slice: 'gold',
    priority_class: 'standard',
    roaming_allowed: true,
    preferred_route: 'edge-east-1',
    hold_minutes: 20
  },
  {
    tenant_id: 'north_mobile',
    subscriber_id: 'sub_1001',
    call_id: 'call_1001',
    source_region: 'us-east',
    target_region: 'us-east',
    network_slice: 'gold',
    priority_class: 'standard',
    roaming_allowed: true,
    preferred_route: 'edge-east-1',
    hold_minutes: 20
  },
  {
    tenant_id: 'north_mobile',
    subscriber_id: 'sub_2002',
    call_id: 'call_2001',
    source_region: 'us-east',
    target_region: 'eu-west',
    network_slice: 'silver',
    priority_class: 'standard',
    roaming_allowed: false,
    preferred_route: 'edge-eu-2',
    hold_minutes: 10
  },
  {
    tenant_id: 'field_support',
    subscriber_id: 'sub_3003',
    call_id: 'call_3001',
    source_region: 'us-west',
    target_region: 'us-west',
    network_slice: 'platinum',
    priority_class: 'emergency',
    roaming_allowed: true,
    preferred_route: 'edge-west-9',
    hold_minutes: 15
  },
  {
    tenant_id: 'field_support',
    subscriber_id: 'sub_4004',
    call_id: 'call_4001',
    source_region: 'us-west',
    target_region: 'us-west',
    network_slice: 'gold',
    priority_class: 'standard',
    roaming_allowed: true,
    preferred_route: 'edge-west-5',
    hold_minutes: 12
  }
];

const EXPIRED_ROUTING = {
  tenant_id: 'north_mobile',
  subscriber_id: 'sub_9999',
  call_id: 'call_expired_0001',
  source_region: 'us-west',
  target_region: 'us-west',
  network_slice: 'bronze',
  priority_class: 'standard',
  roaming_allowed: true,
  preferred_route: 'edge-west-1',
  hold_minutes: 5
};

main();

async function main() {
  let connection;
  let completed = false;

  try {
    console.log('=== Telecom call routing demo ===');
    connection = await connect();
    // Recreate the table so each run starts from a clean routing state.
    await dropTable(connection, false);
    await createSchema(connection);
    // Seed one expired row so the TTL cleanup takes care of it.
    await seedExpiredRouting(connection);

    // Each request exercises either replayed or freshly computed routing.
    for (const callRequest of CALL_REQUESTS) {
      await routeCall(connection, callRequest);
    }

    // Show the live routing state before stale rows are removed.
    await summarizeActiveRouting(connection);
    await cleanupExpiredRouting(connection);
    await dropTable(connection, true);
    completed = true;
  }
  catch (err) {
    console.error(err);
  }
  finally {
    await releaseConnection(connection);
    if (completed) {
      console.log('✓ Completed telecom call routing sample operations');
    }
  }
}

async function connect() {
  const credentials = accessControl.getCredentials('telecomCallRoutingState.js');
  console.log('Connecting to TimesTen');
  const connection = await oracledb.getConnection({
    user: credentials['-u'],
    password: credentials['-p'],
    connectString: credentials['-c']
  });

  console.log('✓ Connected');
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
  await connection.execute(CREATE_TENANT_SUBSCRIBER_INDEX);
  console.log('✓ Index IDX_CALL_ROUTE_TENANT_SUB created');
  await connection.execute(CREATE_CALL_ID_INDEX);
  console.log('✓ Index IDX_CALL_ROUTE_CALL_ID created');
  await connection.execute(CREATE_EXPIRES_INDEX);
  console.log('✓ Index IDX_CALL_ROUTE_EXPIRES created');
}

function buildRoutingKey(callRequest) {
  const keyText = [
    callRequest.tenant_id,
    callRequest.subscriber_id,
    callRequest.call_id
  ].join('|');

  return crypto.createHash('sha256').update(keyText, 'utf8').digest('hex');
}

function currentTimestamp() {
  return new Date();
}

function formatTimestamp(date) {
  const pad2 = (value) => String(value).padStart(2, '0');
  const pad3 = (value) => String(value).padStart(3, '0');
  return `${date.getUTCFullYear()}-${pad2(date.getUTCMonth() + 1)}-${pad2(date.getUTCDate())} ` +
         `${pad2(date.getUTCHours())}:${pad2(date.getUTCMinutes())}:${pad2(date.getUTCSeconds())}.` +
         `${pad3(date.getUTCMilliseconds())}`;
}

function requestPayloadToJson(callRequest) {
  return JSON.stringify({
    tenantId: callRequest.tenant_id,
    subscriberId: callRequest.subscriber_id,
    callId: callRequest.call_id,
    sourceRegion: callRequest.source_region,
    targetRegion: callRequest.target_region,
    networkSlice: callRequest.network_slice,
    priorityClass: callRequest.priority_class,
    roamingAllowed: callRequest.roaming_allowed,
    preferredRoute: callRequest.preferred_route,
    holdMinutes: callRequest.hold_minutes
  });
}

function decisionPayloadToJson(callRequest, state, reason, expiresAtText) {
  return JSON.stringify({
    tenantId: callRequest.tenant_id,
    subscriberId: callRequest.subscriber_id,
    callId: callRequest.call_id,
    routeState: state,
    reason,
    sourceRegion: callRequest.source_region,
    targetRegion: callRequest.target_region,
    preferredRoute: callRequest.preferred_route,
    holdMinutes: callRequest.hold_minutes,
    expiresAt: expiresAtText,
    ruleVersion: 'telecom-route-rules-v1',
    simulatedSwitch: true
  });
}

function evaluateRouting(callRequest) {
  if (callRequest.priority_class === 'emergency') {
    return {
      state: 'PRIORITIZED',
      reason: 'emergency_route_override',
      ttlMinutes: 15
    };
  }

  if (callRequest.target_region !== callRequest.source_region && !callRequest.roaming_allowed) {
    return {
      state: 'BLOCKED',
      reason: 'roaming_not_allowed',
      ttlMinutes: 10
    };
  }

  if (callRequest.priority_class === 'vip') {
    return {
      state: 'FAST_TRACK',
      reason: 'vip_priority_route',
      ttlMinutes: 20
    };
  }

  return {
    state: 'ROUTED',
    reason: 'standard_route',
    ttlMinutes: callRequest.hold_minutes
  };
}

async function seedExpiredRouting(connection) {
  const routingKey = buildRoutingKey(EXPIRED_ROUTING);
  const now = currentTimestamp();
  const expiredAt = new Date(now.getTime() - 60 * 1000);
  const requestPayload = requestPayloadToJson(EXPIRED_ROUTING);
  const decisionPayload = decisionPayloadToJson(
    EXPIRED_ROUTING,
    'EXPIRED',
    'seeded_expired_state',
    formatTimestamp(expiredAt)
  );

  await connection.execute(INSERT_ROUTING, [
    routingKey,
    EXPIRED_ROUTING.tenant_id,
    EXPIRED_ROUTING.subscriber_id,
    EXPIRED_ROUTING.call_id,
    EXPIRED_ROUTING.source_region,
    EXPIRED_ROUTING.target_region,
    EXPIRED_ROUTING.network_slice,
    EXPIRED_ROUTING.priority_class,
    'EXPIRED',
    'seeded_expired_state',
    requestPayload,
    decisionPayload,
    now,
    now,
    expiredAt
  ]);

  console.log('✓ Seeded 1 expired routing record');
}

async function lookupExistingRouting(connection, routingKey) {
  const result = await connection.execute(SELECT_EXISTING_ROUTING, [
    routingKey,
    currentTimestamp()
  ]);

  return result.rows.length > 0 ? result.rows[0] : null;
}

async function storeRouting(connection, callRequest, state, reason, expiresAt) {
  const now = currentTimestamp();
  const routingKey = buildRoutingKey(callRequest);
  const requestPayload = requestPayloadToJson(callRequest);
  const decisionPayload = decisionPayloadToJson(
    callRequest,
    state,
    reason,
    formatTimestamp(expiresAt)
  );

  await connection.execute(INSERT_ROUTING, [
    routingKey,
    callRequest.tenant_id,
    callRequest.subscriber_id,
    callRequest.call_id,
    callRequest.source_region,
    callRequest.target_region,
    callRequest.network_slice,
    callRequest.priority_class,
    state,
    reason,
    requestPayload,
    decisionPayload,
    now,
    now,
    expiresAt
  ]);
}

async function routeCall(connection, callRequest) {
  const startTime = process.hrtime.bigint();
  const routingKey = buildRoutingKey(callRequest);
  const existing = await lookupExistingRouting(connection, routingKey);

  // Replaying an existing route keeps the routing path deterministic.
  if (existing !== null) {
    const [state, reason, expiresAtText] = existing;
    console.log(
      `→ Routing replay: tenant=${callRequest.tenant_id} subscriber=${callRequest.subscriber_id} ` +
      `call_id=${callRequest.call_id} state=${state} reason=${reason} expires_at=${expiresAtText} ` +
      `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
    );
    return state;
  }

  const decision = evaluateRouting(callRequest);
  const expiresAt = new Date(currentTimestamp().getTime() + decision.ttlMinutes * 60000);
  // New routing decisions are stored so later replays can return the same result.
  await storeRouting(connection, callRequest, decision.state, decision.reason, expiresAt);
  console.log(
    `→ Routing decision: tenant=${callRequest.tenant_id} subscriber=${callRequest.subscriber_id} ` +
    `call_id=${callRequest.call_id} state=${decision.state} source=${callRequest.source_region} ` +
    `target=${callRequest.target_region} slice=${callRequest.network_slice} ` +
    `reason=${decision.reason} hold_expires=${formatTimestamp(expiresAt)} ` +
    `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
  );

  return decision.state;
}

function elapsedMs(startTime) {
  return Number(process.hrtime.bigint() - startTime) / 1e6;
}

async function summarizeActiveRouting(connection) {
  // Show the active routing footprint grouped by tenant, subscriber, and state.
  console.log('⋯ Active routing decisions by tenant/subscriber/state:');

  const summary = await connection.execute(SELECT_ACTIVE_SUMMARY, [currentTimestamp()]);
  for (const row of summary.rows) {
    const [tenantId, subscriberId, state, rowCount] = row;
    console.log(
      `  tenant=${padRight(tenantId, 12)} subscriber=${padRight(subscriberId, 10)} ` +
      `state=${padRight(state, 11)} rows=${rowCount}`
    );
  }

  console.log('⋯ Active routing details:');
  const details = await connection.execute(SELECT_ACTIVE_DETAILS, [currentTimestamp()]);
  for (const row of details.rows) {
    const [callId, sourceRegion, targetRegion, networkSlice, priorityClass, state, reason, expiresAtText] = row;
    console.log(
      `  call_id=${padRight(callId, 12)} source=${padRight(sourceRegion, 8)} ` +
      `target=${padRight(targetRegion, 8)} slice=${padRight(networkSlice, 8)} ` +
      `priority=${padRight(priorityClass, 10)} state=${padRight(state, 11)} ` +
      `reason=${padRight(reason, 24)} expires_at=${expiresAtText}`
    );
  }
}

async function cleanupExpiredRouting(connection) {
  const result = await connection.execute(DELETE_EXPIRED, [currentTimestamp()]);
  console.log(
    `✓ Deleted ${result.rowsAffected} expired routing record` +
    (result.rowsAffected === 1 ? '' : 's')
  );
}

async function releaseConnection(connection) {
  if (connection !== undefined) {
    try {
      await connection.close();
      console.log('Connection has been released');
    }
    catch (err) {
      console.error(err);
    }
  }
}

function padRight(text, width) {
  const value = text === undefined || text === null ? '' : String(text);
  if (value.length >= width) {
    return value;
  }

  return value + ' '.repeat(width - value.length);
}
