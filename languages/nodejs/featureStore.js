/*
 * Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    This sample demonstrates how an application can use TimesTen as a fast
 *    online feature store for real-time personalization support. The
 *    application keeps the latest feature values close to the service that
 *    needs them, so it can fetch the current state with very low latency,
 *    refresh stale data, and store a JSON audit trail for downstream analysis.
 *
 *    NOTE: The sample uses simulated feature updates and does not call an AI
 *    model, perform vector search, or run in-database model inference.
 *
 *    The sample performs the following steps:
 *      - Creates a 'user_features' table
 *      - Creates indexes for tenant/user and freshness lookups
 *      - Seeds one stale feature row so cleanup behavior is visible
 *      - Upserts fresh feature values for sample users
 *      - Fetches the current feature set for a user with low latency
 *      - Stores a JSON audit payload for the resulting personalization decision
 *      - Deletes stale feature rows
 *      - Drops the table
 */
'use strict';

const crypto = require('crypto');
const oracledb = require('oracledb');
const accessControl = require('./AccessControl');

oracledb.initOracleClient();
oracledb.autoCommit = true;

const TABLE_NAME = 'user_features';
const FEATURE_TTL_MINUTES = 60;

const CREATE_TABLE = `
  CREATE TABLE user_features(
    feature_key       VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id         VARCHAR2(30)   NOT NULL,
    user_id           VARCHAR2(30)   NOT NULL,
    feature_name      VARCHAR2(60)   NOT NULL,
    feature_value     JSON           NOT NULL,
    freshness_ts      TIMESTAMP      NOT NULL,
    model_version     VARCHAR2(40)   NOT NULL,
    audit_payload     JSON,
    created_at        TIMESTAMP      NOT NULL,
    updated_at        TIMESTAMP      NOT NULL,
    expires_at        TIMESTAMP      NOT NULL)
`;

const CREATE_TENANT_USER_INDEX = `
  CREATE INDEX idx_user_features_tenant_user
  ON user_features (tenant_id, user_id)
`;

const CREATE_FRESHNESS_INDEX = `
  CREATE INDEX idx_user_features_freshness
  ON user_features (expires_at)
`;

const INSERT_FEATURE = `
  INSERT INTO user_features(
    feature_key, tenant_id, user_id, feature_name, feature_value, freshness_ts,
    model_version, audit_payload, created_at, updated_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11)
`;

const SELECT_ACTIVE_FEATURES = `
  SELECT feature_name,
         JSON_SERIALIZE(feature_value RETURNING VARCHAR2(4000)),
         TO_CHAR(freshness_ts, 'YYYY-MM-DD HH24:MI:SS.FF3'),
         model_version,
         JSON_SERIALIZE(audit_payload RETURNING VARCHAR2(4000))
  FROM user_features
  WHERE tenant_id = :1
    AND user_id = :2
    AND expires_at > :3
  ORDER BY feature_name
`;

const SELECT_FEATURE_SUMMARY = `
  SELECT tenant_id,
         user_id,
         COUNT(*),
         SUM(CASE
               WHEN JSON_VALUE(feature_value, '$.valueType' RETURNING VARCHAR2(20)) = 'numeric'
               THEN JSON_VALUE(feature_value, '$.value' RETURNING TT_INT)
               ELSE 0
             END)
  FROM user_features
  WHERE expires_at > :1
  GROUP BY tenant_id, user_id
  ORDER BY tenant_id, user_id
`;

const DELETE_EXPIRED = 'DELETE FROM user_features WHERE expires_at <= :1';
const DROP_TABLE = 'DROP TABLE user_features';

const FEATURE_UPDATES = [
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    feature_name: 'cart_value',
    feature_value: {
      valueType: 'numeric',
      value: 128,
      source: 'checkout-events',
      freshness: 'seconds'
    },
    model_version: 'feature-agg-v1',
    decision: {
      variant: 'priority-shipping',
      reason: 'high_cart_value',
      confidence: 0.92
    }
  },
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    feature_name: 'preferred_channel',
    feature_value: {
      valueType: 'string',
      value: 'mobile',
      source: 'profile-service',
      freshness: 'minutes'
    },
    model_version: 'feature-agg-v1',
    decision: {
      variant: 'mobile-first',
      reason: 'recent_mobile_usage',
      confidence: 0.88
    }
  },
  {
    tenant_id: 'field_service',
    user_id: 'user_204',
    feature_name: 'device_risk',
    feature_value: {
      valueType: 'numeric',
      value: 73,
      source: 'device-telemetry',
      freshness: 'seconds'
    },
    model_version: 'feature-agg-v2',
    decision: {
      variant: 'proactive-support',
      reason: 'elevated_risk_score',
      confidence: 0.81
    }
  },
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    feature_name: 'delivery_eta_hours',
    feature_value: {
      valueType: 'numeric',
      value: 18,
      source: 'shipping-service',
      freshness: 'hours'
    },
    model_version: 'feature-agg-v1',
    decision: {
      variant: 'fast-tracked',
      reason: 'recent_support_contact',
      confidence: 0.84
    }
  }
];

main();

async function main() {
  let connection;

  try {
    console.log('=== Feature store demo ===');
    connection = await connect();
    // Rebuild the table so every run starts from the same feature snapshot.
    await dropTable(connection, false);
    await createSchema(connection);
    // Seed a stale row so freshness cleanup is visible in the walkthrough.
    await seedStaleFeature(connection);

    // Each upsert models a fresh signal arriving from the application.
    for (const feature of FEATURE_UPDATES) {
      await upsertFeature(connection, feature);
    }

    // Read back the current feature state before removing stale rows.
    await printFeatureSummary(connection);
    await printFeatureSet(connection, 'retail_app', 'user_001');
    await printFeatureSet(connection, 'field_service', 'user_204');
    await deleteExpiredFeatures(connection);
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
  const credentials = accessControl.getCredentials('featureStore.js');
  const connection = await oracledb.getConnection({
    user: credentials['-u'],
    password: credentials['-p'],
    connectString: credentials['-c']
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
  await connection.execute(CREATE_TENANT_USER_INDEX);
  console.log('✓ Index IDX_USER_FEATURES_TENANT_USER created');
  await connection.execute(CREATE_FRESHNESS_INDEX);
  console.log('✓ Index IDX_USER_FEATURES_FRESHNESS created');
}

function buildFeatureKey(feature) {
  const keyText = [
    feature.tenant_id,
    feature.user_id,
    feature.feature_name
  ].join('|');

  return crypto.createHash('sha256').update(keyText, 'utf8').digest('hex');
}

function currentTimestamp() {
  return new Date();
}

function currentTimestampText() {
  const timestamp = currentTimestamp();
  const year = timestamp.getUTCFullYear();
  const month = String(timestamp.getUTCMonth() + 1).padStart(2, '0');
  const day = String(timestamp.getUTCDate()).padStart(2, '0');
  const hours = String(timestamp.getUTCHours()).padStart(2, '0');
  const minutes = String(timestamp.getUTCMinutes()).padStart(2, '0');
  const seconds = String(timestamp.getUTCSeconds()).padStart(2, '0');
  const milliseconds = String(timestamp.getUTCMilliseconds()).padStart(3, '0');

  return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}.${milliseconds}`;
}

function featureValueToJson(featureValue) {
  return JSON.stringify(featureValue);
}

function auditPayloadToJson(feature, decision, reason) {
  return JSON.stringify({
    tenantId: feature.tenant_id,
    userId: feature.user_id,
    featureName: feature.feature_name,
    modelVersion: feature.model_version,
    decision,
    reason,
    updatedAt: currentTimestampText(),
    simulatedModelCall: true
  });
}

async function insertFeature(connection, feature, freshnessTs, expiresAt, auditPayload) {
  const featureKey = buildFeatureKey(feature);

  await connection.execute(INSERT_FEATURE, [
    featureKey,
    feature.tenant_id,
    feature.user_id,
    feature.feature_name,
    featureValueToJson(feature.feature_value),
    freshnessTs,
    feature.model_version,
    auditPayload,
    freshnessTs,
    freshnessTs,
    expiresAt
  ]);

  return featureKey;
}

async function upsertFeature(connection, feature) {
  const startTime = process.hrtime.bigint();
  const freshnessTs = currentTimestamp();
  const expiresAt = new Date(freshnessTs.getTime() + (FEATURE_TTL_MINUTES * 60 * 1000));
  const auditPayload = auditPayloadToJson(feature, feature.decision, 'fresh_feature_upsert');

  // Each upsert keeps the latest feature value close to the decisioning path.
  await insertFeature(connection, feature, freshnessTs, expiresAt, auditPayload);
  console.log(
    '→ Feature upsert:',
    `tenant=${feature.tenant_id}`,
    `user=${feature.user_id}`,
    `feature=${feature.feature_name}`,
    `model=${feature.model_version}`,
    `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
  );
}

async function seedStaleFeature(connection) {
  const staleFeature = {
    tenant_id: 'retail_app',
    user_id: 'user_999',
    feature_name: 'cart_value',
    feature_value: {
      valueType: 'numeric',
      value: 12,
      source: 'old-events',
      freshness: 'minutes'
    },
    model_version: 'feature-agg-v0',
    decision: {
      variant: 'standard-shipping',
      reason: 'legacy_state',
      confidence: 0.5
    }
  };

  const freshnessTs = new Date(Date.now() - (3 * 60 * 60 * 1000));
  const expiresAt = new Date(Date.now() - (60 * 1000));
  const auditPayload = auditPayloadToJson(staleFeature, staleFeature.decision, 'stale_seed');

  await insertFeature(connection, staleFeature, freshnessTs, expiresAt, auditPayload);
  console.log('✓ Seeded 1 stale feature row');
}

async function printFeatureSummary(connection) {
  // Show the active feature footprint grouped by tenant and user.
  console.log('⋯ Active feature groups:');
  const result = await connection.execute(SELECT_FEATURE_SUMMARY, [currentTimestamp()]);
  for (const row of result.rows) {
    console.log(
      `  tenant=${String(row[0]).padEnd(14)} user=${String(row[1]).padEnd(10)} ` +
      `features=${Number(row[2])} numeric_sum=${Number(row[3])}`
    );
  }
}

async function printFeatureSet(connection, tenantId, userId) {
  const startTime = process.hrtime.bigint();
  const result = await connection.execute(SELECT_ACTIVE_FEATURES, [tenantId, userId, currentTimestamp()]);
  console.log(`Current features for tenant=${tenantId} user=${userId}:`);
  for (const row of result.rows) {
    console.log(`  feature=${row[0]} freshness=${row[2]} model=${row[3]}`);
    console.log(`    value=${row[1]}`);
    console.log(`    audit=${row[4]}`);
  }
  console.log(`  readback_elapsed_ms=${elapsedMs(startTime).toFixed(2)}`);
}

function elapsedMs(startTime) {
  return Number(process.hrtime.bigint() - startTime) / 1e6;
}

async function deleteExpiredFeatures(connection) {
  const result = await connection.execute(DELETE_EXPIRED, [currentTimestamp()]);
  const deleted = result.rowsAffected || 0;
  console.log(`✓ Deleted ${deleted} expired feature row`);
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
