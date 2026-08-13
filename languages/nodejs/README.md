Copyright (c) 2019, 2026 Oracle and/or its affiliates. All rights reserved.

# TimesTen Node.js Samples

This folder contains Node.js samples that illustrate database connection and operations using the node-oracledb driver against  TimesTen databases. 

## Start here by use case

If you already know the kind of application pattern you want to see, start here and then work down into the individual sample sections below.

| Use case | Samples | Why start here |
| :------- | :------ | :------------- |
| AI and live application state | `aiResponseCache.js`, `chatSessionMemory.js`, `agentWorkflowState.js`, `featureStore.js` | Shows TimesTen for active response, session, agent-workflow, and personalization state. |
| JSON application data | `jsonSample.js` | Demonstrates JSON document storage, indexing, update, and query workflows. |
| Real-time financial authorization state | `paymentAuthorizationState.js` | Shows TimesTen as a low-latency store for payment authorization decisions, idempotent replay, and hot risk state. |
| Real-time telecom call routing state | `telecomCallRoutingState.js` | Shows TimesTen as a low-latency store for telecom routing decisions, idempotent replay, and hot session state. |
| Core SQL and transactional patterns | `sql.js`, `queriesAndPlsql.js`, `lobs.js`, `simple.js` | Good starting points for SQL, PL/SQL, and basic data-access examples. |

## Software & Platform Support
The following table describes the tested operating systems, node-oracledb driver and TimesTen software versions.

| OS | Node.js Version | node-oracledb Driver Version | TimesTen Client Driver | TimesTen Direct Driver |
| :-- | :-------------- | :--------------------------- | :--------------------- | :-------------------- |
| Linux 64-bit | 14.6.0+ | 6.4.0+ | 26.1.1.1.0+ | 26.1.1.1.0+ |
| Linux ARM 64-bit | 22.22.0+ | 6.10.0+ | 26.1.1.1.0+ | 26.1.1.1.0+ |
| macOS | 14.6.0+ | 6.4.0+ | 26.1.1.1.0+ | N/A |
| MS Windows 64-bit | 14.6.0+ | 6.4.0+ | 26.1.1.1.0+ | N/A |

**NOTE**: Access to TimesTen Databases on any supported TimesTen server platforms can be achieved using the TimesTen client driver from any of the platforms listed above. For more information on supported TimesTen platforms, see [TimesTen Release Notes](https://docs.oracle.com/en/database/other-databases/timesten/26.1/release-notes/toc.htm).


## PRE-REQUISITES
 
1. Node.js language is installed. 
2. The node-oracledb driver is installed. 
3. A TimesTen database is created and data source is setup to access that database. 
4. Environment to access Node.js, node-oracledb driver and TimesTen data source are set up (i.e. the TimesTen environment script ttenv.sh/ttenv.csh/ttquickstartenv.cmd has been executed)

For more information on setup, see [TimesTen In-Memory Database Open Source Languages Support Guide](https://docs.oracle.com/en/database/other-databases/timesten/26.1/open-source-languages/index.html).

## Known Problems and Limitations

* NVARCHAR/NCHAR data types in a Node.js application are encoded as UTF-16, the [same difference between Oracle and TimesTen](https://docs.oracle.com/en/database/other-databases/timesten/26.1/cache/compatibility-timesten-and-oracle-databases.html#GUID-13FF0E9B-9250-49DB-810A-89EE48605E5E) as noted in the TimesTen Documentation.
* Fetching a clob inside a stored procedure through a Node.js application can result in error ORA-00600.
* DML statements with RETURN INTO are currently not supported.
* The value returned for the sub-second field of a PL/SQL output parameter of type Timestamp may be incorrect. 



## Node.js sample programs
### Download Samples

Node.js sample programs to access TimesTen databases can be downloaded from [Oracle/oracle-timesten-samples/languages/nodejs on github](https://github.com/oracle/oracle-timesten-samples/tree/master/languages/nodejs). Command-line command such as "git clone" can be used for github download. For example,

```
% git clone https://github.com/oracle/oracle-timesten-samples
```

Once the samples are downloaded locally, you can change to languages/nodejs subdirectory and run the samples directly from the local machine.  Descriptions of the sample programs and examples of how to run them are below.

### Passwords for the modern samples

The six modern samples accept `-p` for a quick local run. To avoid placing a
password in the command line or shell history, set `TT_PASSWORD` and omit
`-p` instead:

```
% export TT_PASSWORD='password'
% node aiResponseCache.js -u username [-c <connectionString>]
```

When both are provided, `-p` takes precedence over `TT_PASSWORD`.

### simple.js

This simple sample program connects to a TimesTen database and performs the following operations:

* Creates a table named Employees
* Inserts 3 rows to the table
* Select the rows from the table
* Drops the table
* Disconnects from the database.

Example:

```
% node simple.js -u username -p password
Table has been created
Inserted 3 employees into the table
Selected employee: ROBERT ROBERTSON
Selected employee: ANDY ANDREWS
Selected employee: MICHAEL MICHAELSON
Table has been dropped
Connection has been released
```


### sql.js
The sql sample program connects to a TimesTen database and performs SQL operations against an `api_sessions` table that represents active sessions for application services. It performs the following operations:


* Creates a table called `api_sessions`
* Populates the table with sample API session records
* Performs a number of selects, updates, and deletes against the table
* Drops the table
* Disconnects from the database

Example:

```
% node sql.js -u username -p password
Table has been created
Populating table
  Inserted  10 rows
  Inserted  20 rows
  Inserted  30 rows
  Inserted  40 rows
  Inserted  50 rows
  Inserted  60 rows
  Inserted  70 rows
  Inserted  80 rows
  Inserted  90 rows
  Inserted  100 rows
Performing selects
  select(ed) 10 rows
  select(ed) 20 rows
  select(ed) 30 rows
  select(ed) 40 rows
  select(ed) 50 rows
  select(ed) 60 rows
  select(ed) 70 rows
  select(ed) 80 rows
Performing updates
  update(ed) 10 rows
  update(ed) 20 rows
Performing deletes
  delete(ed) 10 rows
  delete(ed) 20 rows
Connection has been released
```

### aiResponseCache.js

This sample demonstrates how an application can use TimesTen as the primary store for active AI response cache state. The application computes deterministic cache keys for AI requests, checks TimesTen for fresh cached responses, simulates model calls on cache misses, stores responses with operational metadata and expiration timestamps, and removes expired entries.

> **Note:** Responses are simulated; this sample does not call an AI model, perform vector search, run in-database model inference, or demonstrate TimesTen Cache for Oracle Database.

The sample performs the following steps:

* Creates an `ai_response_cache` table
* Creates indexes for tenant/model and expiration lookups
* Seeds one expired cache entry
* Processes sample AI requests, showing cache misses and cache hits
* Updates hit counts and last-accessed timestamps on cache hits
* Stores model metadata in a JSON column and queries it with SQL/JSON
* Deletes expired cache entries

Example output (abbreviated; elapsed times and timestamps vary by environment):

```
% node aiResponseCache.js -u username [-p password] [-c <connectionString>]
=== AI response cache demo ===

Connecting to TimesTen
✓ Connected
✓ Table ai_response_cache created
✓ Index IDX_AI_CACHE_TENANT_MODEL created
✓ Index IDX_AI_CACHE_EXPIRES created
✓ Seeded 1 expired cache entry
→ Cache miss: tenant=retail_app model=support-summary-v1 stored_for_minutes=30
  Response: Simulated response for retail_app using support-summary-v1: Summarize order 45012 for a support agent.
→ Cache hit: tenant=retail_app model=support-summary-v1 hits=1 expires=...
  Response: Simulated response for retail_app using support-summary-v1: Summarize order 45012 for a support agent.
  Safety label from metadata: allowed
→ Cache miss: tenant=field_service model=technician-assist-v1 stored_for_minutes=30
  Response: Simulated response for field_service using technician-assist-v1: Draft troubleshooting steps for a router with intermittent packet loss.
⋯ Cache summary by tenant and model:
  tenant=field_service  model=technician-assist-v1   entries=1 hits=0
  tenant=retail_app     model=support-summary-v1     entries=2 hits=1
⋯ Metadata safety labels:
  cache_key=... safetyLabel=allowed
✓ Deleted 1 expired cache entry
✓ Table ai_response_cache dropped
✓ Completed AI response cache sample operations
```

### chatSessionMemory.js

This sample demonstrates how an application can use TimesTen as the primary store for active chat session memory. It stores recent messages, tool-call metadata, safety labels, and citations in a JSON column so the application can restore context quickly between turns.

> **Note:** Responses are simulated; this sample does not call an AI model, perform vector search, or run in-database model inference.

The sample performs the following steps:

* Creates a `chat_sessions` table
* Creates indexes for tenant/user and expiration lookups
* Seeds one expired chat session
* Starts and resumes sample chat sessions
* Stores recent messages and metadata in JSON
* Queries JSON fields to summarize active sessions
* Deletes expired chat sessions

Example output (abbreviated; elapsed times and timestamps vary by environment):

```
% node chatSessionMemory.js -u username [-p password] [-c <connectionString>]
=== Chat session memory demo ===

Connecting to TimesTen
✓ Connected
✓ Table chat_sessions created
✓ Index IDX_CHAT_SESSIONS_TENANT_USER created
✓ Index IDX_CHAT_SESSIONS_EXPIRES created
✓ Seeded 1 expired chat session
→ Session start: tenant=retail_app user=user_001 topic=order-status turns=1
  Assistant: Simulated reply for retail_app using support-summary-v1: The order is currently in transit and should arrive tomorrow.
→ Session resume: tenant=retail_app user=user_001 topic=order-status turns=2
  Assistant: Simulated reply for retail_app using support-summary-v1: I remember your earlier message: Where is order 45012?. The order is still in transit and is expected to arrive tomorrow.
→ Session start: tenant=field_service user=user_204 topic=router-troubleshooting turns=1
  Assistant: Simulated reply for field_service using technician-assist-v1: The router is still showing intermittent packet loss, so continue with the cable and firmware checks.
⋯ Active sessions by tenant/user/topic:
  tenant=field_service user=user_204  topic=router-troubleshooting   turns=1 model=technician-assist-v1   safety=allowed
  tenant=retail_app    user=user_001  topic=order-status            turns=2 model=support-summary-v1    safety=allowed
⋯ Latest session memory snapshots:
  session=... tenant=retail_app topic=order-status updated=... expires=...
✓ Deleted 1 expired chat session
✓ Table chat_sessions dropped
✓ Completed chat session memory sample operations
```

The sample also prints a session summary that is easier to scan as a table:

| tenant | user | topic | turns | model | safety |
| :----- | :--- | :---- | ----: | :---- | :----- |
| field_service | user_204 | router-troubleshooting | 1 | technician-assist-v1 | allowed |
| retail_app | user_001 | order-status | 2 | support-summary-v1 | allowed |

### agentWorkflowState.js

This sample shows how an application can keep current agent-run and tool-call
state in TimesTen. It starts deterministic agent runs, stores their plan and
current state in JSON, completes simulated tool calls, reuses a completed tool
result when the same call is retried, and removes expired state.

> **Note:** Agent steps, tool calls, and responses are simulated; this sample
> does not call an AI model, agent framework, external tool service, perform
> vector search, or run in-database model inference.

The sample performs the following steps:

* Creates `agent_runs` and `agent_tool_calls` tables
* Creates indexes for tenant/agent summaries, tool calls by run, and expiration cleanup
* Seeds one expired agent run and tool call
* Starts sample agent runs and records their current state
* Completes simulated tool calls and replays a repeated tool call
* Rereads the stored tool result if a concurrent request inserts the same key
* Stores run plans, tool inputs, and results in JSON
* Summarizes active runs and their tool calls
* Deletes expired runs and tool calls

Example output (abbreviated; elapsed times and timestamps vary by environment):

```
% node agentWorkflowState.js -u username [-p password] [-c <connectionString>]
=== Agent workflow state demo ===

Connecting to TimesTen
✓ Connected
✓ Table agent_runs created
✓ Table agent_tool_calls created
✓ Index IDX_AGENT_RUNS_TENANT_AGENT created
✓ Index IDX_AGENT_RUNS_EXPIRES created
✓ Index IDX_AGENT_TOOLS_RUN created
✓ Index IDX_AGENT_TOOLS_EXPIRES created
✓ Seeded 1 expired agent run and tool call
→ Agent run started: tenant=retail_app user=user_001 agent=delivery-assist status=RUNNING step=plan_ready elapsed_ms=...
→ Tool call completed: agent=delivery-assist tool=lookup_order status=COMPLETED elapsed_ms=...
→ Tool call completed: agent=delivery-assist tool=check_delivery_status status=COMPLETED elapsed_ms=...
→ Tool call replay: agent=delivery-assist tool=lookup_order status=COMPLETED expires_at=... elapsed_ms=...
→ Agent run completed: tenant=retail_app agent=delivery-assist status=COMPLETED elapsed_ms=...
⋯ Active agent runs by tenant/agent/status:
  tenant=field_service  agent=technician-assist     status=COMPLETED  runs=1
  tenant=retail_app     agent=delivery-assist       status=COMPLETED  runs=1
⋯ Tool calls for active runs:
  run=... tool=lookup_order             status=COMPLETED  completed_at=...
    input={"orderId":"45012"}
    result={"orderId":"45012","status":"in_transit","carrier":"NorthStar"}
✓ Deleted 1 expired agent run and 1 expired tool call
✓ Table agent_tool_calls dropped
✓ Table agent_runs dropped
✓ Completed agent workflow state sample operations
```

### featureStore.js

This sample demonstrates how an application can use TimesTen as a fast online feature store for real-time personalization support. It keeps the latest feature values close to the service that needs them, so it can fetch the current state with very low latency, refresh stale data, and store a JSON audit trail for downstream analysis.

> **Note:** The sample uses simulated feature updates and does not call an AI model, perform vector search, or run in-database model inference.

The sample performs the following steps:

* Creates a `user_features` table
* Creates indexes for tenant/user and freshness lookups
* Seeds one stale feature row so cleanup behavior is visible
* Upserts fresh feature values for sample users
* Fetches the current feature set for a user with low latency
* Stores a JSON audit payload for the resulting personalization decision
* Deletes stale feature rows

Example output (abbreviated; elapsed times and timestamps vary by environment):

```
% node featureStore.js -u username [-p password] [-c <connectionString>]
=== Feature store demo ===

Connecting to TimesTen
✓ Connected
✓ Table user_features created
✓ Index IDX_USER_FEATURES_TENANT_USER created
✓ Index IDX_USER_FEATURES_FRESHNESS created
✓ Seeded 1 stale feature row
→ Feature upsert: tenant=retail_app user=user_001 feature=cart_value model=feature-agg-v1
→ Feature upsert: tenant=retail_app user=user_001 feature=preferred_channel model=feature-agg-v1
→ Feature upsert: tenant=field_service user=user_204 feature=device_risk model=feature-agg-v2
⋯ Active feature groups:
  tenant=field_service user=user_204  features=1 numeric_sum=73
  tenant=retail_app    user=user_001  features=2 numeric_sum=128
Current features for tenant=retail_app user=user_001:
  feature=cart_value freshness=... model=feature-agg-v1
    value={"valueType":"numeric","value":128,"source":"checkout-events","freshness":"seconds"}
    audit={"tenantId":"retail_app","userId":"user_001","featureName":"cart_value",...}
  feature=preferred_channel freshness=... model=feature-agg-v1
    value={"valueType":"string","value":"mobile","source":"profile-service","freshness":"minutes"}
    audit={"tenantId":"retail_app","userId":"user_001","featureName":"preferred_channel",...}
✓ Deleted 1 expired feature row
✓ Table user_features dropped
✓ Completed feature store sample operations
```

### paymentAuthorizationState.js

This sample demonstrates how an application can use TimesTen as a fast store for real-time payment authorization state. It keeps hot authorization decisions close to the service that needs them, applies deterministic approval and decline rules, stores request and decision metadata in JSON, and removes expired authorization records.

> **Note:** The sample uses simulated authorization rules. It does not call an external payment gateway, perform fraud-model inference, or depend on an external service.

The sample performs the following steps:

* Creates a `payment_authorizations` table
* Creates indexes for tenant/account, payment id, and expiration lookups
* Seeds one expired authorization record
* Processes sample payment authorization requests
* Shows idempotent replay for a repeated payment request
* Rereads the stored decision if a concurrent request inserts the same key
* Stores request and decision metadata in JSON
* Summarizes active authorizations by tenant/account/status
* Deletes expired authorization records

Example output (abbreviated; elapsed times and timestamps vary by environment):

```
% node paymentAuthorizationState.js -u username [-p password] [-c <connectionString>]
=== Payment authorization demo ===

Connecting to TimesTen
✓ Connected
✓ Table payment_authorizations created
✓ Index IDX_PAY_AUTH_TENANT_ACCT created
✓ Index IDX_PAYMENT_AUTH_PAYMENT_ID created
✓ Index IDX_PAYMENT_AUTH_EXPIRES created
✓ Seeded 1 expired authorization record
→ Authorization decision: tenant=retail_app account=acct_1001 merchant=orchard-books payment_id=pay_1001 status=APPROVED amount=$49.95 risk=0.12 reason=within_limit_and_low_risk hold_expires=...
→ Authorization replay: tenant=retail_app account=acct_1001 merchant=orchard-books payment_id=pay_1001 status=APPROVED reason=within_limit_and_low_risk expires_at=...
→ Authorization decision: tenant=retail_app account=acct_1002 merchant=pro-office-supplies payment_id=pay_2001 status=DECLINED amount=$399.00 risk=0.08 reason=amount_exceeds_limit hold_expires=...
→ Authorization decision: tenant=field_service account=acct_2001 merchant=route-parts payment_id=pay_3001 status=REVIEW amount=$149.00 risk=0.87 reason=risk_score_requires_review hold_expires=...
⋯ Active authorizations by tenant/account/status:
  tenant=field_service account=acct_2001 status=REVIEW   rows=1  total=$149.00
  tenant=retail_app    account=acct_1001 status=APPROVED rows=1  total=$49.95
  tenant=retail_app    account=acct_1002 status=DECLINED rows=1  total=$399.00
⋯ Active authorization details:
  payment_id=pay_1001   merchant=orchard-books        status=APPROVED amount=$49.95 risk=0.12 reason=within_limit_and_low_risk   expires_at=...
✓ Deleted 1 expired authorization record
✓ Table payment_authorizations dropped
✓ Completed payment authorization sample operations
```

### telecomCallRoutingState.js

This sample demonstrates how an application can use TimesTen as a fast store for real-time telecom call routing state. It keeps hot routing decisions close to the service that needs them, applies deterministic routing rules, stores request and decision metadata in JSON, and removes expired routing records.

> **Note:** The sample uses simulated routing rules. It does not call a telecom switch, perform network signaling, or depend on an external service.

The sample performs the following steps:

* Creates a `call_routing_state` table
* Creates indexes for tenant/subscriber, call id, and expiration lookups
* Seeds one expired routing record
* Processes sample call routing requests
* Shows idempotent replay for a repeated call request
* Rereads the stored decision if a concurrent request inserts the same key
* Stores request and decision metadata in JSON
* Summarizes active routing decisions by tenant/subscriber/state
* Deletes expired routing records

Example output (abbreviated; elapsed times and timestamps vary by environment):

```
% node telecomCallRoutingState.js -u username [-p password] [-c <connectionString>]
=== Telecom call routing demo ===

Connecting to TimesTen
✓ Connected
✓ Table call_routing_state created
✓ Index IDX_CALL_ROUTE_TENANT_SUB created
✓ Index IDX_CALL_ROUTE_CALL_ID created
✓ Index IDX_CALL_ROUTE_EXPIRES created
✓ Seeded 1 expired routing record
→ Routing decision: tenant=north_mobile subscriber=sub_1001 call_id=call_1001 state=ROUTED source=us-east target=us-east slice=gold reason=standard_route hold_expires=...
→ Routing replay: tenant=north_mobile subscriber=sub_1001 call_id=call_1001 state=ROUTED reason=standard_route expires_at=...
→ Routing decision: tenant=north_mobile subscriber=sub_2002 call_id=call_2001 state=BLOCKED source=us-east target=eu-west slice=silver reason=roaming_not_allowed hold_expires=...
→ Routing decision: tenant=field_support subscriber=sub_3003 call_id=call_3001 state=PRIORITIZED source=us-west target=us-west slice=platinum reason=emergency_route_override hold_expires=...
⋯ Active routing decisions by tenant/subscriber/state:
  tenant=field_support subscriber=sub_3003 state=PRIORITIZED rows=1
  tenant=north_mobile subscriber=sub_1001 state=ROUTED      rows=1
  tenant=north_mobile subscriber=sub_2002 state=BLOCKED     rows=1
⋯ Active routing details:
  call_id=call_1001   source=us-east  target=us-east  slice=gold     priority=standard   state=ROUTED      reason=standard_route           expires_at=...
✓ Deleted 1 expired routing record
✓ Table call_routing_state dropped
✓ Completed telecom call routing sample operations
```

### queriesAndPlsql.js

The queriesAndPlsql sample program connects to a TimesTen database and performs a number of database operations with PL/SQL: 


* Creates a table named "items"
* Populates the table
* Performs Selects fetching 100 rows
* Calls a PL/SQL procedure to update a row
* Calls a PL/SQL procedure to delete a row
* Drops the table
* Disconnects from the database

Example:

```
% node queriesAndPlsql.js -u username -p password
Table has been created
Inserting with executeMany ...
  100 Registries added
Select some rows with one select ...
  20 Rows have been fetched and iterated
Updating a row using an anonymous block ...
  Value before update:  [ 'descr_0' ]
  Value after update:  [ 'updated description' ]
Delete a row using an anonymous block ...
  Rows after delete =  [ 99 ]
Connection has been released

```
### lobs.js

The lobs sample program connects to a TimesTen database and performs a number of database operations against a CLOB data type table:


* Creates a table named "CLOB"
* Populates the table
* Performs Select/fetching row
* Disconnects from the database

Example:

```
% node lobs.js -u username -p password
> Connecting
> Creating table with CLOB column
> Reading file
> Populating CLOB from file
> Querying CLOB column
> Reading CLOB
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce facilisis lacinia mauris et sodales. Ut non ligula eget lorem elementum maximus. Fusce pretium, felis a ultrices sodales, ligula augue ullamcorper eros, nec accumsan risus diam id justo. Mauris sed dictum nunc, vitae vehicula felis. Praesent et sodales odio. Nunc pulvinar ipsum ac erat iaculis efficitur. Nunc in aliquet est. Donec porta est est, nec iaculis leo lacinia at.

Nunc quis sodales sem. Nam felis dolor, cursus volutpat cursus vel, tempus nec mauris. Morbi bibendum urna nec leo commodo, id pellentesque nisi dictum. Vivamus venenatis velit nec orci imperdiet sagittis. Praesent fermentum, tortor sed tempus condimentum, lectus ipsum condimentum diam, at facilisis ligula purus vitae neque. Quisque erat quam, tristique at nisl sed, mattis feugiat diam. Curabitur ipsum nibh, mollis at venenatis nec, rutrum eu quam. Fusce augue tellus, porta ut dapibus at, ornare ut nisi. In imperdiet elit non dolor pharetra, quis bibendum odio aliquam. Quisque porttitor tempus augue eu consequat. Cras ac metus malesuada, pellentesque tortor in, porta leo.

Aliquam erat volutpat. Duis vitae quam id est maximus commodo. Morbi dolor lacus, bibendum ac nisl nec, fringilla eleifend nibh. Proin a tellus rhoncus, fermentum magna sed, imperdiet nibh. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae; Nullam sed consequat nisi. Morbi tortor ipsum, consequat a suscipit sed, ullamcorper ut massa.

Aliquam erat volutpat. Maecenas porttitor vel sapien non viverra. Sed dignissim luctus lectus at cursus. Sed condimentum at massa in egestas. In sed dui eget augue posuere finibus. Etiam pulvinar libero sit amet magna efficitur scelerisque. Sed pretium, turpis in condimentum blandit, risus tortor venenatis nisl, facilisis luctus mauris metus ut sem. Proin dapibus sit amet nunc a ultricies. Phasellus interdum lobortis leo sed fermentum. Phasellus ac aliquam erat. Duis at ultricies urna. Nulla at pharetra dolor, id sodales sapien. Ut auctor mi cras amet.

> Finished reading CLOB
> Connection released


```

### jsonSample.js

This sample showcases TimesTen JSON capabilities from Node.js using the `node-oracledb` driver. It recreates a JSON-enabled purchase order table, loads and updates JSON documents, creates a functional JSON index, runs JSON_VALUE queries by user, and renders line items through `JSON_TABLE`.

The sample performs the following steps:

* Creates a `j_purchaseorder` table to store JSON purchase orders
* Loads two purchase order documents from JSON files
* Builds a JSON value index on the `User` field for targeted lookups
* Updates an existing purchase order with revised JSON content
* Retrieves purchase orders by identifier and by user name
* Formats purchase order line items using `JSON_TABLE`

Example:

```
% node jsonSample.js -u <username> -p <password> [-c <connectionString>]
Connecting
Connected
Table j_purchaseorder created
Reading jsondoc1.json
Reading jsondoc1-v2.json
Reading jsondoc2.json
Inserted purchase order with id 1600
Inserted purchase order with id 1721
JSON index IDX_JSON_USER created
Updated purchase order with id 1600 using jsondoc1-v2.json
Purchase order 1600:
{
  "PONumber" : 1600,
  "Reference" : "ABULL-20140421",
  "Requestor" : "Alexis Bull",
  "User" : "ABULL",
  "CostCenter" : "A50",
  "ShippingInstructions" :
  {
    "name" : "Alexis Bull",
    "Address" :
    {
      "street" : "200 Sporting Green",
      "city" : "South San Francisco",
      "state" : "CA",
      "zipCode" : 99236,
      "country" : "United States of America"
    },
    "Phone" :
    [
      {
        "type" : "Office",
        "number" : "909-555-7307"
      },
      {
        "type" : "Mobile",
        "number" : "415-555-1234"
      }
    ]
  },
  "Special Instructions" : null,
  "AllowPartialShipment" : false,
  "LineItems" :
  [
    {
      "ItemNumber" : 1,
      "Part" :
      {
        "Description" : "One Magic Christmas",
        "UnitPrice" : 19.95,
        "UPCCode" : 13131092899
      },
      "Quantity" : 9
    }
  ]
}
Purchase order 1721:
{
  "PONumber" : 1721,
  "Reference" : "CGIRAFFE-20140421",
  "Requestor" : "Carlos Giraffe",
  "User" : "CGIRAFFE",
  "CostCenter" : "A50",
  "ShippingInstructions" :
  {
    "name" : "Carlos Giraffe",
    "Address" :
    {
      "street" : "200 Main Street",
      "city" : "Napa",
      "state" : "CA",
      "zipCode" : 99150,
      "country" : "United States of America"
    },
    "Phone" :
    [
      {
        "type" : "Office",
        "number" : "908-555-1207"
      },
      {
        "type" : "Mobile",
        "number" : "415-555-4321"
      }
    ]
  },
  "Special Instructions" : null,
  "AllowPartialShipment" : false,
  "LineItems" :
  [
    {
      "ItemNumber" : 1,
      "Part" :
      {
        "Description" : "Lethal Weapon",
        "UnitPrice" : 19.95,
        "UPCCode" : 85391628927
      },
      "Quantity" : 2
    },
    {
      "ItemNumber" : 2,
      "Part" :
      {
        "Description" : "Some Random Movie",
        "UnitPrice" : 17.95,
        "UPCCode" : 18368923299
      },
      "Quantity" : 1
    }
  ]
}
Purchase orders for user ABULL:
{
  "PONumber" : 1600,
  "Reference" : "ABULL-20140421",
  "Requestor" : "Alexis Bull",
  "User" : "ABULL",
  "CostCenter" : "A50",
  "ShippingInstructions" :
  {
    "name" : "Alexis Bull",
    "Address" :
    {
      "street" : "200 Sporting Green",
      "city" : "South San Francisco",
      "state" : "CA",
      "zipCode" : 99236,
      "country" : "United States of America"
    },
    "Phone" :
    [
      {
        "type" : "Office",
        "number" : "909-555-7307"
      },
      {
        "type" : "Mobile",
        "number" : "415-555-1234"
      }
    ]
  },
  "Special Instructions" : null,
  "AllowPartialShipment" : false,
  "LineItems" :
  [
    {
      "ItemNumber" : 1,
      "Part" :
      {
        "Description" : "One Magic Christmas",
        "UnitPrice" : 19.95,
        "UPCCode" : 13131092899
      },
      "Quantity" : 9
    }
  ]
}
Purchase orders for user CGIRAFFE:
{
  "PONumber" : 1721,
  "Reference" : "CGIRAFFE-20140421",
  "Requestor" : "Carlos Giraffe",
  "User" : "CGIRAFFE",
  "CostCenter" : "A50",
  "ShippingInstructions" :
  {
    "name" : "Carlos Giraffe",
    "Address" :
    {
      "street" : "200 Main Street",
      "city" : "Napa",
      "state" : "CA",
      "zipCode" : 99150,
      "country" : "United States of America"
    },
    "Phone" :
    [
      {
        "type" : "Office",
        "number" : "908-555-1207"
      },
      {
        "type" : "Mobile",
        "number" : "415-555-4321"
      }
    ]
  },
  "Special Instructions" : null,
  "AllowPartialShipment" : false,
  "LineItems" :
  [
    {
      "ItemNumber" : 1,
      "Part" :
      {
        "Description" : "Lethal Weapon",
        "UnitPrice" : 19.95,
        "UPCCode" : 85391628927
      },
      "Quantity" : 2
    },
    {
      "ItemNumber" : 2,
      "Part" :
      {
        "Description" : "Some Random Movie",
        "UnitPrice" : 17.95,
        "UPCCode" : 18368923299
      },
      "Quantity" : 1
    }
  ]
}
Line items for purchase order 1600:
Line  SKU           Description                     Qty  Unit Price  Extended
   1  13131092899  One Magic Christmas               9       19.95    179.55
Completed JSON sample operations
Table j_purchaseorder dropped
Connection has been released
```



## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/en/database/other-databases/timesten/).  Online documentation for the node-oracledb driver can be found [here](https://oracle.github.io/node-oracledb/doc/api.html).
