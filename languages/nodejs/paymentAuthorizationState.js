/*
 * Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   This sample demonstrates how an application can use TimesTen as a fast
 *   store for real-time payment authorization state. The application keeps
 *   hot authorization decisions close to the service that needs them,
 *   applies deterministic authorization rules, stores the resulting decision
 *   and metadata in JSON, and cleans up expired state.
 *
 *   The sample uses simulated authorization rules. It does not call an
 *   external payment gateway, perform fraud-model inference, or depend on an
 *   external service.
 *
 *   The sample performs the following steps:
 *     - Creates a 'payment_authorizations' table
 *     - Creates indexes for tenant/account, payment id, and expiration lookups
 *     - Seeds one expired authorization record
 *     - Processes sample payment authorization requests
 *     - Shows idempotent replay for a repeated payment request
 *     - Stores request and decision metadata in JSON
 *     - Summarizes active authorizations by tenant/account/status
 *     - Deletes expired authorization records
 *     - Drops the table
 */
'use strict';

const crypto = require('crypto');
const oracledb = require('oracledb');
const accessControl = require('./AccessControl');

oracledb.initOracleClient();
oracledb.autoCommit = true;

const TABLE_NAME = 'payment_authorizations';

const CREATE_TABLE = `
  CREATE TABLE payment_authorizations(
    authorization_key  VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id          VARCHAR2(30)   NOT NULL,
    account_id         VARCHAR2(30)   NOT NULL,
    merchant_id        VARCHAR2(60)   NOT NULL,
    payment_id         VARCHAR2(40)   NOT NULL,
    amount_cents       TT_INT         NOT NULL,
    currency           VARCHAR2(3)    NOT NULL,
    payment_method     VARCHAR2(20)   NOT NULL,
    risk_score         NUMBER(5,2)    NOT NULL,
    status             VARCHAR2(20)   NOT NULL,
    decision_reason    VARCHAR2(120)  NOT NULL,
    request_payload    JSON,
    decision_payload   JSON,
    created_at         TIMESTAMP      NOT NULL,
    updated_at         TIMESTAMP      NOT NULL,
    expires_at         TIMESTAMP      NOT NULL)
`;

const CREATE_TENANT_ACCOUNT_INDEX = `
  CREATE INDEX idx_pay_auth_tenant_acct
  ON payment_authorizations (tenant_id, account_id)
`;

const CREATE_PAYMENT_ID_INDEX = `
  CREATE INDEX idx_payment_auth_payment_id
  ON payment_authorizations (payment_id)
`;

const CREATE_EXPIRES_INDEX = `
  CREATE INDEX idx_payment_auth_expires
  ON payment_authorizations (expires_at)
`;

const INSERT_AUTHORIZATION = `
  INSERT INTO payment_authorizations(
    authorization_key, tenant_id, account_id, merchant_id, payment_id,
    amount_cents, currency, payment_method, risk_score, status,
    decision_reason, request_payload, decision_payload, created_at,
    updated_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11, :12, :13, :14, :15, :16)
`;

const SELECT_EXISTING_AUTHORIZATION = `
  SELECT status,
         decision_reason,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS')
  FROM payment_authorizations
  WHERE authorization_key = :1
    AND expires_at > :2
`;

const SELECT_ACTIVE_SUMMARY = `
  SELECT tenant_id,
         account_id,
         status,
         COUNT(*),
         SUM(amount_cents)
  FROM payment_authorizations
  WHERE expires_at > :1
  GROUP BY tenant_id, account_id, status
  ORDER BY tenant_id, account_id, status
`;

const SELECT_ACTIVE_DETAILS = `
  SELECT payment_id,
         merchant_id,
         status,
         decision_reason,
         amount_cents,
         risk_score,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS')
  FROM payment_authorizations
  WHERE expires_at > :1
  ORDER BY tenant_id, account_id, payment_id
`;

const DELETE_EXPIRED = 'DELETE FROM payment_authorizations WHERE expires_at <= :1';
const DROP_TABLE = 'DROP TABLE payment_authorizations';

const PAYMENT_REQUESTS = [
  {
    tenant_id: 'retail_app',
    account_id: 'acct_1001',
    merchant_id: 'orchard-books',
    payment_id: 'pay_1001',
    amount_cents: 4995,
    currency: 'USD',
    payment_method: 'debit_card',
    risk_score: 0.12,
    spend_limit_cents: 25000,
    risk_threshold: 0.75,
    hold_minutes: 15
  },
  {
    tenant_id: 'retail_app',
    account_id: 'acct_1001',
    merchant_id: 'orchard-books',
    payment_id: 'pay_1001',
    amount_cents: 4995,
    currency: 'USD',
    payment_method: 'debit_card',
    risk_score: 0.12,
    spend_limit_cents: 25000,
    risk_threshold: 0.75,
    hold_minutes: 15
  },
  {
    tenant_id: 'retail_app',
    account_id: 'acct_1002',
    merchant_id: 'pro-office-supplies',
    payment_id: 'pay_2001',
    amount_cents: 39900,
    currency: 'USD',
    payment_method: 'credit_card',
    risk_score: 0.08,
    spend_limit_cents: 25000,
    risk_threshold: 0.75,
    hold_minutes: 10
  },
  {
    tenant_id: 'field_service',
    account_id: 'acct_2001',
    merchant_id: 'route-parts',
    payment_id: 'pay_3001',
    amount_cents: 14900,
    currency: 'USD',
    payment_method: 'mobile_wallet',
    risk_score: 0.87,
    spend_limit_cents: 20000,
    risk_threshold: 0.75,
    hold_minutes: 20
  },
  {
    tenant_id: 'field_service',
    account_id: 'acct_2001',
    merchant_id: 'depot-supplies',
    payment_id: 'pay_3002',
    amount_cents: 8600,
    currency: 'USD',
    payment_method: 'debit_card',
    risk_score: 0.18,
    spend_limit_cents: 20000,
    risk_threshold: 0.75,
    hold_minutes: 20
  }
];

const EXPIRED_AUTHORIZATION = {
  tenant_id: 'retail_app',
  account_id: 'acct_9999',
  merchant_id: 'legacy-outlet',
  payment_id: 'pay_expired_0001',
  amount_cents: 2599,
  currency: 'USD',
  payment_method: 'debit_card',
  risk_score: 0.18,
  spend_limit_cents: 15000,
  risk_threshold: 0.70,
  hold_minutes: 5
};

main();

async function main() {
  let connection;

  try {
    console.log('=== Payment authorization demo ===');
    connection = await connect();
    // Recreate the table so the authorization trail starts cleanly.
    await dropTable(connection, false);
    await createSchema(connection);
    // Seed one expired row so the TTL cleanup path is visible.
    await seedExpiredAuthorization(connection);

    // Each request exercises either a replay or a fresh authorization decision.
    for (const payment of PAYMENT_REQUESTS) {
      await authorizePayment(connection, payment);
    }

    // Show the active decisions before stale rows are removed.
    await summarizeActiveAuthorizations(connection);
    await cleanupExpiredAuthorizations(connection);
    await dropTable(connection, true);
    console.log('✓ Completed payment authorization sample operations');
  }
  catch (err) {
    console.error(err);
  }
  finally {
    await releaseConnection(connection);
  }
}

async function connect() {
  const credentials = accessControl.getCredentials('paymentAuthorizationState.js');
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
  await connection.execute(CREATE_TENANT_ACCOUNT_INDEX);
  console.log('✓ Index IDX_PAY_AUTH_TENANT_ACCT created');
  await connection.execute(CREATE_PAYMENT_ID_INDEX);
  console.log('✓ Index IDX_PAYMENT_AUTH_PAYMENT_ID created');
  await connection.execute(CREATE_EXPIRES_INDEX);
  console.log('✓ Index IDX_PAYMENT_AUTH_EXPIRES created');
}

function buildAuthorizationKey(payment) {
  const keyText = [
    payment.tenant_id,
    payment.account_id,
    payment.merchant_id,
    payment.payment_id
  ].join('|');

  return crypto.createHash('sha256').update(keyText, 'utf8').digest('hex');
}

function currentTimestamp() {
  return new Date();
}

function currentTimestampText() {
  return new Date().toISOString();
}

function formatMoney(amountCents) {
  return `$${(amountCents / 100).toFixed(2)}`;
}

function requestPayloadToJson(payment) {
  return JSON.stringify({
    tenantId: payment.tenant_id,
    accountId: payment.account_id,
    merchantId: payment.merchant_id,
    paymentId: payment.payment_id,
    amountCents: payment.amount_cents,
    currency: payment.currency,
    paymentMethod: payment.payment_method,
    riskScore: payment.risk_score,
    spendLimitCents: payment.spend_limit_cents,
    riskThreshold: payment.risk_threshold
  });
}

function decisionPayloadToJson(payment, status, reason, expiresAtText) {
  return JSON.stringify({
    tenantId: payment.tenant_id,
    accountId: payment.account_id,
    merchantId: payment.merchant_id,
    paymentId: payment.payment_id,
    decision: status,
    reason,
    holdMinutes: payment.hold_minutes,
    expiresAt: expiresAtText,
    ruleVersion: 'payment-auth-rules-v1',
    simulatedRiskService: true
  });
}

function evaluatePayment(payment) {
  if (payment.amount_cents > payment.spend_limit_cents) {
    return {
      status: 'DECLINED',
      reason: 'amount_exceeds_limit',
      ttlMinutes: 10
    };
  }

  if (payment.risk_score >= payment.risk_threshold) {
    return {
      status: 'REVIEW',
      reason: 'risk_score_requires_review',
      ttlMinutes: 20
    };
  }

  return {
    status: 'APPROVED',
    reason: 'within_limit_and_low_risk',
    ttlMinutes: payment.hold_minutes
  };
}

async function seedExpiredAuthorization(connection) {
  const authKey = buildAuthorizationKey(EXPIRED_AUTHORIZATION);
  const now = currentTimestamp();
  const expiredAt = new Date(now.getTime() - 60 * 1000);
  const requestPayload = requestPayloadToJson(EXPIRED_AUTHORIZATION);
  const decisionPayload = decisionPayloadToJson(
    EXPIRED_AUTHORIZATION,
    'EXPIRED',
    'seeded_expired_state',
    expiredAt.toISOString()
  );

  await connection.execute(INSERT_AUTHORIZATION, [
    authKey,
    EXPIRED_AUTHORIZATION.tenant_id,
    EXPIRED_AUTHORIZATION.account_id,
    EXPIRED_AUTHORIZATION.merchant_id,
    EXPIRED_AUTHORIZATION.payment_id,
    EXPIRED_AUTHORIZATION.amount_cents,
    EXPIRED_AUTHORIZATION.currency,
    EXPIRED_AUTHORIZATION.payment_method,
    EXPIRED_AUTHORIZATION.risk_score,
    'EXPIRED',
    'seeded_expired_state',
    requestPayload,
    decisionPayload,
    now,
    now,
    expiredAt
  ]);

  console.log('✓ Seeded 1 expired authorization record');
}

async function lookupExistingAuthorization(connection, authKey) {
  const result = await connection.execute(SELECT_EXISTING_AUTHORIZATION, [
    authKey,
    currentTimestamp()
  ]);

  return result.rows.length > 0 ? result.rows[0] : null;
}

async function storeAuthorization(connection, payment, status, reason, expiresAt) {
  const now = currentTimestamp();
  const authKey = buildAuthorizationKey(payment);
  const requestPayload = requestPayloadToJson(payment);
  const decisionPayload = decisionPayloadToJson(
    payment,
    status,
    reason,
    expiresAt.toISOString()
  );

  await connection.execute(INSERT_AUTHORIZATION, [
    authKey,
    payment.tenant_id,
    payment.account_id,
    payment.merchant_id,
    payment.payment_id,
    payment.amount_cents,
    payment.currency,
    payment.payment_method,
    payment.risk_score,
    status,
    reason,
    requestPayload,
    decisionPayload,
    now,
    now,
    expiresAt
  ]);

  return authKey;
}

async function authorizePayment(connection, payment) {
  const authKey = buildAuthorizationKey(payment);
  const startTime = process.hrtime.bigint();
  const existing = await lookupExistingAuthorization(connection, authKey);

  // Replaying an existing decision keeps the authorization path idempotent.
  if (existing !== null) {
    const [status, reason, expiresAtText] = existing;
    console.log(
      `→ Authorization replay: tenant=${payment.tenant_id} account=${payment.account_id} ` +
      `merchant=${payment.merchant_id} payment_id=${payment.payment_id} ` +
      `status=${status} reason=${reason} expires_at=${expiresAtText} ` +
      `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
    );
    return status;
  }

  const decision = evaluatePayment(payment);
  const expiresAt = new Date(currentTimestamp().getTime() + decision.ttlMinutes * 60000);
  // New decisions are stored so the same payment request can be answered consistently.
  await storeAuthorization(connection, payment, decision.status, decision.reason, expiresAt);
  console.log(
    `→ Authorization decision: tenant=${payment.tenant_id} account=${payment.account_id} ` +
    `merchant=${payment.merchant_id} payment_id=${payment.payment_id} ` +
    `status=${decision.status} amount=${formatMoney(payment.amount_cents)} ` +
    `risk=${payment.risk_score.toFixed(2)} reason=${decision.reason} ` +
    `hold_expires=${expiresAt.toISOString()} ` +
    `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
  );

  return decision.status;
}

function elapsedMs(startTime) {
  return Number(process.hrtime.bigint() - startTime) / 1e6;
}

async function summarizeActiveAuthorizations(connection) {
  // Show both the grouped view and the detailed authorization state.
  console.log('⋯ Active authorizations by tenant/account/status:');

  const summary = await connection.execute(SELECT_ACTIVE_SUMMARY, [currentTimestamp()]);
  for (const row of summary.rows) {
    const [tenantId, accountId, status, rowCount, amountCents] = row;
    console.log(
      `  tenant=${tenantId.padEnd(12)} account=${accountId.padEnd(10)} ` +
      `status=${status.padEnd(8)} rows=${String(rowCount).padEnd(2)} ` +
      `total=${formatMoney(amountCents || 0)}`
    );
  }

  console.log('Active authorization details:');
  const details = await connection.execute(SELECT_ACTIVE_DETAILS, [currentTimestamp()]);
  for (const row of details.rows) {
    const [paymentId, merchantId, status, reason, amountCents, riskScore, expiresAtText] = row;
    console.log(
      `  payment_id=${paymentId.padEnd(10)} merchant=${merchantId.padEnd(20)} ` +
      `status=${status.padEnd(8)} amount=${formatMoney(amountCents).padEnd(8)} ` +
      `risk=${Number(riskScore).toFixed(2)} reason=${reason.padEnd(28)} ` +
      `expires_at=${expiresAtText}`
    );
  }
}

async function cleanupExpiredAuthorizations(connection) {
  const result = await connection.execute(DELETE_EXPIRED, [currentTimestamp()]);
  console.log(
    `✓ Deleted ${result.rowsAffected} expired authorization record` +
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
