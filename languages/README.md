# TimesTen Open Source Languages Samples

This folder contains open source language samples for application developers using Oracle TimesTen In-Memory Database. Java/JDBC samples are maintained separately in the QuickStart tree at [quickstart/sample_code/jdbc](../quickstart/sample_code/jdbc). The Python and Node.js directories now also include AI response cache, chat/session memory, and feature store samples that show TimesTen as the primary store for active AI state. The Python and Node.js directories also include real-time payment authorization and telecom call routing samples for low-latency decisioning.

## Start here by use case

If you already know the kind of application pattern you want to see, start here and then pick the language folder that matches your stack.

| Use case | Primary samples | Why start here |
| :------- | :-------------- | :------------- |
| AI and live application state | [python/aiResponseCache.py](./python/aiResponseCache.py), [python/chatSessionMemory.py](./python/chatSessionMemory.py), [python/featureStore.py](./python/featureStore.py), [nodejs/aiResponseCache.js](./nodejs/aiResponseCache.js), [nodejs/chatSessionMemory.js](./nodejs/chatSessionMemory.js), [nodejs/featureStore.js](./nodejs/featureStore.js) | Shows TimesTen as a fast store for chat memory, feature state, and response cache patterns. |
| JSON application data | [python/jsonSample.py](./python/jsonSample.py), [nodejs/jsonSample.js](./nodejs/jsonSample.js) | Demonstrates JSON document storage and query workflows. |
| Real-time financial authorization state | [python/paymentAuthorizationState.py](./python/paymentAuthorizationState.py), [nodejs/paymentAuthorizationState.js](./nodejs/paymentAuthorizationState.js) | Shows TimesTen as a low-latency store for payment authorization decisions, idempotent replay, and hot risk state. |
| Real-time telecom call routing state | [python/telecomCallRoutingState.py](./python/telecomCallRoutingState.py), [nodejs/telecomCallRoutingState.js](./nodejs/telecomCallRoutingState.js) | Shows TimesTen as a low-latency store for telecom routing decisions, idempotent replay, and hot session state. |
| Core SQL and transactional patterns | [python/sql.py](./python/sql.py), [nodejs/sql.js](./nodejs/sql.js) | Good starting points for operational app-state examples. |

## Recommended samples

Start here for current application-development examples:

| Language | Start here | Description |
| :------- | :--------- | :---------- |
| Python | [python](./python) | Python samples using the `python-oracledb` driver. Includes connection, SQL, PL/SQL, LOB, access-control, JSON, AI state, financial authorization, and telecom routing examples. |
| Node.js | [nodejs](./nodejs) | Node.js samples using the `node-oracledb` driver. Includes connection, SQL, PL/SQL, LOB, access-control, JSON, AI state, financial authorization, and telecom routing examples. |

## Directory map

| Repository/Folder name            | Description                                     |
| :-------------------------- | :---------------------------------------------- |
| [nodejs](./nodejs)        | Node.js sample programs.                     |
| [python](./python)              | Python sample programs.                      |

## More information
You can find more information about the Oracle TimesTen In-Memory Database on our [Product Portal](https://www.oracle.com/database/technologies/related/timesten.html)

## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/en/database/other-databases/timesten/).
