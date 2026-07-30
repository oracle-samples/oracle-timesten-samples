/*
 * Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 *  DESCRIPTION
 *    This sample demonstrates how an application can use TimesTen as the
 *    primary store for active chat session memory. The
 *    application stores recent messages, tool-call metadata, safety labels,
 *    and citations in JSON so it can restore context quickly between turns.
 *
 *    NOTE: Responses are simulated; this sample does not call an AI model,
 *    perform vector search, or run in-database model inference.
 *
 *    The sample performs the following steps:
 *      - Creates a 'chat_sessions' table
 *      - Creates indexes for tenant/user and expiration lookups
 *      - Seeds one expired session so cleanup behavior is visible
 *      - Starts and resumes sample chat sessions
 *      - Stores chat memory in a JSON column
 *      - Queries JSON fields to summarize active sessions
 *      - Deletes expired sessions
 *      - Drops the table
 */
'use strict';

const crypto = require('crypto');
const oracledb = require('oracledb');
const accessControl = require('./AccessControl');

oracledb.initOracleClient();
oracledb.autoCommit = true;

const TABLE_NAME = 'chat_sessions';
const SESSION_TTL_MINUTES = 45;

const CREATE_TABLE = `
  CREATE TABLE chat_sessions(
    session_key        VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id          VARCHAR2(30)   NOT NULL,
    user_id            VARCHAR2(30)   NOT NULL,
    conversation_topic VARCHAR2(80)   NOT NULL,
    session_state      JSON,
    created_at         TIMESTAMP      NOT NULL,
    last_updated_at    TIMESTAMP      NOT NULL,
    expires_at         TIMESTAMP      NOT NULL)
`;

const CREATE_TENANT_USER_INDEX = `
  CREATE INDEX idx_chat_sessions_tenant_user
  ON chat_sessions (tenant_id, user_id)
`;

const CREATE_EXPIRES_INDEX = `
  CREATE INDEX idx_chat_sessions_expires
  ON chat_sessions (expires_at)
`;

const INSERT_SESSION = `
  INSERT INTO chat_sessions(
    session_key, tenant_id, user_id, conversation_topic, session_state,
    created_at, last_updated_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8)
`;

const SELECT_ACTIVE_SESSION = `
  SELECT JSON_SERIALIZE(session_state RETURNING VARCHAR2(4000)),
         last_updated_at,
         expires_at
  FROM chat_sessions
  WHERE session_key = :1 AND expires_at > :2
`;

const UPDATE_SESSION_STATE = `
  UPDATE chat_sessions
  SET session_state = :1,
      last_updated_at = :2,
      expires_at = :3
  WHERE session_key = :4
`;

const SELECT_ACTIVE_SUMMARY = `
  SELECT tenant_id,
         user_id,
         conversation_topic,
         JSON_VALUE(session_state, '$.turn_count' RETURNING TT_INT),
         JSON_VALUE(session_state, '$.model.name' RETURNING VARCHAR2(60)),
         JSON_VALUE(session_state, '$.safety.status' RETURNING VARCHAR2(20))
  FROM chat_sessions
  WHERE expires_at > :1
  ORDER BY tenant_id, user_id, conversation_topic
`;

const SELECT_ACTIVE_STATE = `
  SELECT session_key,
         tenant_id,
         user_id,
         conversation_topic,
         JSON_SERIALIZE(session_state RETURNING VARCHAR2(4000)),
         last_updated_at,
         expires_at
  FROM chat_sessions
  WHERE expires_at > :1
  ORDER BY last_updated_at DESC
`;

const DELETE_EXPIRED = 'DELETE FROM chat_sessions WHERE expires_at <= :1';
const DROP_TABLE = 'DROP TABLE chat_sessions';

const SAMPLE_TURNS = [
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    conversation_topic: 'order-status',
    model_name: 'support-summary-v1',
    user_message: 'Where is order 45012?',
    response_hint: 'The order is currently in transit and should arrive tomorrow.',
    tool_name: 'lookup_order_status',
    citation_ref: 'shipment-feed',
    preferences: {
      responseStyle: 'concise',
      locale: 'en-US'
    }
  },
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    conversation_topic: 'order-status',
    model_name: 'support-summary-v1',
    user_message: 'Can you repeat the earlier status in one sentence?',
    response_hint: 'The order is still in transit and is expected to arrive tomorrow.',
    tool_name: 'lookup_order_status',
    citation_ref: 'shipment-feed',
    preferences: {
      responseStyle: 'concise',
      locale: 'en-US'
    }
  },
  {
    tenant_id: 'field_service',
    user_id: 'user_204',
    conversation_topic: 'router-troubleshooting',
    model_name: 'technician-assist-v1',
    user_message: 'I need the previous troubleshooting steps for the packet-loss issue.',
    response_hint: 'The router is still showing intermittent packet loss, so continue with the cable and firmware checks.',
    tool_name: 'lookup_device_notes',
    citation_ref: 'field-notes',
    preferences: {
      responseStyle: 'detailed',
      locale: 'en-US'
    }
  },
  {
    tenant_id: 'retail_app',
    user_id: 'user_001',
    conversation_topic: 'order-status',
    model_name: 'support-summary-v1',
    user_message: 'Can you give me the latest update with a delivery window?',
    response_hint: 'The order is still in transit and is expected to arrive tomorrow.',
    tool_name: 'lookup_order_status',
    citation_ref: 'shipment-feed',
    preferences: {
      responseStyle: 'concise',
      locale: 'en-US'
    }
  }
];

main();

async function main() {
  let connection;

  try {
    console.log('=== Chat session memory demo ===');
    connection = await connect();
    // Recreate the table so the conversation trail is deterministic.
    await dropTable(connection, false);
    await createSchema(connection);
    // Seed one expired row so the TTL cleanup takes care of it.
    await seedExpiredSession(connection);

    // Each turn shows either a new session or a resumed one.
    for (const turn of SAMPLE_TURNS) {
      await processTurn(connection, turn);
    }

    // Print the live memory snapshot before stale rows are removed.
    await printActiveSummary(connection);
    await deleteExpiredSessions(connection);
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
  const credentials = accessControl.getCredentials('chatSessionMemory.js');
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
  console.log('✓ Index IDX_CHAT_SESSIONS_TENANT_USER created');
  await connection.execute(CREATE_EXPIRES_INDEX);
  console.log('✓ Index IDX_CHAT_SESSIONS_EXPIRES created');
}

function buildSessionKey(turn) {
  const keyText = [
    turn.tenant_id,
    turn.user_id,
    turn.conversation_topic
  ].join('|');

  return crypto.createHash('sha256').update(keyText, 'utf8').digest('hex');
}

function shorten(text, limit = 80) {
  if (text.length <= limit) {
    return text;
  }

  return text.slice(0, limit - 3) + '...';
}

function currentTimestampText() {
  return new Date().toISOString();
}

function currentTimestamp() {
  return new Date();
}

function simulateAssistantReply(turn, existingState) {
  let memoryClause = '';

  if (existingState && existingState.messages) {
    const priorUserMessages = existingState.messages
      .filter((message) => message.role === 'user')
      .map((message) => message.content);
    if (priorUserMessages.length > 0) {
      memoryClause = ` I remember your earlier message: ${shorten(priorUserMessages[priorUserMessages.length - 1], 55)}.`;
    }
  }

  return (
    `Simulated reply for ${turn.tenant_id} using ${turn.model_name}:` +
    `${memoryClause} ${turn.response_hint}`
  ).trim();
}

function buildSessionState(turn, assistantText) {
  const timestampText = currentTimestampText();
  return {
    tenantId: turn.tenant_id,
    userId: turn.user_id,
    topic: turn.conversation_topic,
    model: {
      name: turn.model_name,
      provider: 'simulated'
    },
    preferences: turn.preferences || {},
    safety: {
      status: 'allowed'
    },
    turn_count: 1,
    messages: [
      {
        role: 'system',
        content: 'Keep recent chat context, citations, and safety metadata available for low-latency retrieval.',
        timestamp: timestampText
      },
      {
        role: 'user',
        content: turn.user_message,
        timestamp: timestampText
      },
      {
        role: 'assistant',
        content: assistantText,
        timestamp: timestampText,
        tool_calls: [
          {
            name: turn.tool_name,
            status: 'simulated'
          }
        ],
        citations: [
          {
            source: turn.citation_ref,
            status: 'simulated'
          }
        ]
      }
    ]
  };
}

function appendTurn(sessionState, turn, assistantText) {
  const timestampText = currentTimestampText();
  if (!sessionState.messages) {
    sessionState.messages = [];
  }

  sessionState.messages.push({
    role: 'user',
    content: turn.user_message,
    timestamp: timestampText
  });
  sessionState.messages.push({
    role: 'assistant',
    content: assistantText,
    timestamp: timestampText,
    tool_calls: [
      {
        name: turn.tool_name,
        status: 'simulated'
      }
    ],
    citations: [
      {
        source: turn.citation_ref,
        status: 'simulated'
      }
    ]
  });
  sessionState.tenantId = turn.tenant_id;
  sessionState.userId = turn.user_id;
  sessionState.topic = turn.conversation_topic;
  sessionState.model = {
    name: turn.model_name,
    provider: 'simulated'
  };
  sessionState.preferences = turn.preferences || {};
  sessionState.safety = {
    status: 'allowed'
  };
  sessionState.turn_count = (sessionState.turn_count || 0) + 1;
  sessionState.updatedAt = timestampText;
}

async function insertSession(connection, sessionKey, turn, sessionState, createdAt, expiresAt) {
  await connection.execute(INSERT_SESSION, [
    sessionKey,
    turn.tenant_id,
    turn.user_id,
    turn.conversation_topic,
    JSON.stringify(sessionState),
    createdAt,
    createdAt,
    expiresAt
  ]);
}

async function updateSession(connection, sessionKey, sessionState, updatedAt, expiresAt) {
  await connection.execute(UPDATE_SESSION_STATE, [
    JSON.stringify(sessionState),
    updatedAt,
    expiresAt,
    sessionKey
  ]);
}

async function loadActiveSession(connection, sessionKey) {
  const result = await connection.execute(SELECT_ACTIVE_SESSION, [sessionKey, currentTimestamp()]);
  if (result.rows && result.rows.length > 0) {
    const row = result.rows[0];
    return {
      sessionState: JSON.parse(row[0]),
      lastUpdatedAt: row[1],
      expiresAt: row[2]
    };
  }

  return null;
}

async function seedExpiredSession(connection) {
  const expiredTurn = {
    tenant_id: 'retail_app',
    user_id: 'user_099',
    conversation_topic: 'expired-order-status',
    model_name: 'support-summary-v1',
    user_message: 'What happened with the old order update?',
    response_hint: 'This is an expired session that will be cleaned up.',
    tool_name: 'lookup_order_status',
    citation_ref: 'shipment-feed',
    preferences: {
      responseStyle: 'concise',
      locale: 'en-US'
    }
  };

  const sessionKey = buildSessionKey(expiredTurn);
  const createdAt = new Date(Date.now() - (2 * 60 * 60 * 1000));
  const expiresAt = new Date(Date.now() - (60 * 1000));
  const assistantText = simulateAssistantReply(expiredTurn, null);
  const sessionState = buildSessionState(expiredTurn, assistantText);
  appendTurn(sessionState, expiredTurn, assistantText);

  await insertSession(connection, sessionKey, expiredTurn, sessionState, createdAt, expiresAt);
  console.log('✓ Seeded 1 expired chat session');
}

async function processTurn(connection, turn) {
  const sessionKey = buildSessionKey(turn);
  const startTime = process.hrtime.bigint();
  const activeSession = await loadActiveSession(connection, sessionKey);
  const now = currentTimestamp();
  const expiresAt = new Date(now.getTime() + (SESSION_TTL_MINUTES * 60 * 1000));

  // A resumed session appends to existing chat memory instead of starting over.
  if (activeSession) {
    const sessionState = activeSession.sessionState;
    const assistantText = simulateAssistantReply(turn, sessionState);
    appendTurn(sessionState, turn, assistantText);
    await updateSession(connection, sessionKey, sessionState, now, expiresAt);
    console.log(
      '→ Session resume:',
      `tenant=${turn.tenant_id}`,
      `user=${turn.user_id}`,
      `topic=${turn.conversation_topic}`,
      `turns=${sessionState.turn_count}`,
      `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
    );
    console.log(`  Assistant: ${assistantText}`);
  }
  else {
    const assistantText = simulateAssistantReply(turn, null);
    const sessionState = buildSessionState(turn, assistantText);
    await insertSession(connection, sessionKey, turn, sessionState, now, expiresAt);
    console.log(
      '→ Session start:',
      `tenant=${turn.tenant_id}`,
      `user=${turn.user_id}`,
      `topic=${turn.conversation_topic}`,
      'turns=1',
      `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
    );
    console.log(`  Assistant: ${assistantText}`);
  }
}

function elapsedMs(startTime) {
  return Number(process.hrtime.bigint() - startTime) / 1e6;
}

async function printActiveSummary(connection) {
  // Show the current chat memory state that is still worth keeping hot.
  console.log('⋯ Active sessions by tenant/user/topic:');
  let result = await connection.execute(SELECT_ACTIVE_SUMMARY, [currentTimestamp()]);
  for (const row of result.rows) {
    console.log(
      `  tenant=${String(row[0]).padEnd(14)} user=${String(row[1]).padEnd(10)} ` +
      `topic=${String(row[2]).padEnd(24)} turns=${Number(row[3])} ` +
      `model=${String(row[4]).padEnd(22)} safety=${row[5]}`
    );
  }

  console.log('Latest session memory snapshots:');
  result = await connection.execute(SELECT_ACTIVE_STATE, [currentTimestamp()]);
  for (const row of result.rows) {
    const sessionKey = row[0];
    const tenantId = row[1];
    const userId = row[2];
    const conversationTopic = row[3];
    const sessionState = JSON.parse(row[4]);
    const lastUpdatedAt = row[5];
    const expiresAt = row[6];
    const messages = sessionState.messages || [];
    const lastUser = [...messages].reverse().find((message) => message.role === 'user');
    const lastAssistant = [...messages].reverse().find((message) => message.role === 'assistant');

    console.log(
      `  session=${String(sessionKey).slice(0, 12)}... tenant=${tenantId} topic=${conversationTopic} ` +
      `turns=${sessionState.turn_count} updated=${lastUpdatedAt} expires=${expiresAt}`
    );
    console.log(`    last user: ${shorten(lastUser ? lastUser.content : '', 90)}`);
    console.log(`    last assistant: ${shorten(lastAssistant ? lastAssistant.content : '', 90)}`);
  }
}

async function deleteExpiredSessions(connection) {
  const result = await connection.execute(DELETE_EXPIRED, [currentTimestamp()]);
  const deleted = result.rowsAffected || 0;
  console.log(`✓ Deleted ${deleted} expired chat session`);
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
