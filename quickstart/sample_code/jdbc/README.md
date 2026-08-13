Copyright (c) 2010, 2026, Oracle and/or its affiliates. All rights reserved.

# Compile and Run JDBC Sample Programs

This directory is the home for TimesTen Java samples. The samples remain in the QuickStart tree so existing documentation, scripts, and workflows continue to work. New Java developers should start with the recommended samples below, then use the additional samples when they need broader API coverage, compatibility checks, or benchmarking.

## Start here by use case

If you already know the kind of application pattern you want to see, start here and then work down into the recommended and specialized sample lists below.

| Use case | Primary samples | Why start here |
| :------- | :-------------- | :------------- |
| AI and live application state | `ChatSessionMemory`, `AgentWorkflowState`, `FeatureStore`, `AiResponseCache` | Shows TimesTen for active session, agent-workflow, personalization, and response state. |
| JSON application data | `JsonSample` | Demonstrates JSON document storage, indexing, update, filtering, and relational projection with `JSON_TABLE`. |
| Real-time financial authorization state | `PaymentAuthorizationState` | Shows TimesTen as a low-latency store for payment authorization decisions, idempotent replay, and hot risk state. |
| Real-time telecom call routing state | `TelecomCallRoutingState` | Shows TimesTen as a low-latency store for telecom routing decisions, idempotent replay, and hot session state. |
| Core JDBC and transactional patterns | `TTJdbcExamples`, `plsqlJDBC`, `level3`, `level4` | Good starting points for JDBC fundamentals and transactional workloads. |

## Recommended Java samples

| Sample | File | Why start here |
| :----- | :--- | :------------- |
| API session state | [ApiSessions.java](./ApiSessions.java) | Shows a modern application-session pattern: active session storage, request counters, last-seen timestamps, and row cleanup in TimesTen. |
| AI chat session memory | [ChatSessionMemory.java](./ChatSessionMemory.java) | Shows a short-lived AI context pattern: recent messages, JSON metadata, TTL cleanup, and fast session restoration in TimesTen. |
| Agent workflow state | [AgentWorkflowState.java](./AgentWorkflowState.java) | Shows current agent runs, simulated tool-call results, retry-safe reuse, JSON state, and TTL cleanup in TimesTen. |
| Online feature store | [FeatureStore.java](./FeatureStore.java) | Shows a real-time personalization pattern: feature upserts, JSON audit metadata, freshness checks, and fast retrieval in TimesTen. |
| JSON with JDBC | [JsonSample.java](./JsonSample.java) | Demonstrates a current application pattern: JSON document storage, indexing, update, filtering, and relational projection with `JSON_TABLE`. |
| AI response cache | [AiResponseCache.java](./AiResponseCache.java) | Shows how TimesTen can keep a fast cache for simulated AI responses, including hit tracking, expiration, and JSON metadata. |
| Payment authorization state | [PaymentAuthorizationState.java](./PaymentAuthorizationState.java) | Shows how TimesTen can keep hot payment authorization decisions, idempotent replay, and JSON metadata close to the transaction flow. |
| Telecom call routing state | [TelecomCallRoutingState.java](./TelecomCallRoutingState.java) | Shows how TimesTen can keep hot telecom routing decisions, idempotent replay, and JSON metadata close to the session flow. |
| JDBC basics | [TTJdbcExamples.java](./TTJdbcExamples.java) | Shows core JDBC operations including connection handling, DDL, DML, prepared statements, indexes, query plans, and batch updates. |
| PL/SQL from Java | [plsqlJDBC.java](./plsqlJDBC.java) | Shows how Java applications call TimesTen PL/SQL procedures, functions, anonymous blocks, and ref cursors. |
| Transactional order processing | [level3.java](./level3.java) and [level4.java](./level4.java) | Single-threaded and multi-threaded transaction examples for order-processing workloads. |

## Additional and specialized Java samples

The remaining samples are still available for users who need specific TimesTen features or compatibility references.

| Category | Samples | Description |
| :------- | :------ | :---------- |
| Introductory JDBC | `level1`, `level2` | Basic DriverManager/DataSource usage and simple table operations. |
| Transaction processing | `level3`, `level4` | Order-processing workload, including rollback behavior and multi-threaded throughput. |
| PL/SQL integration | `plsqlJDBC` | Calls stored procedures, functions, anonymous blocks, and ref cursors. |
| JSON | `JsonSample` | Current JSON document example using the TimesTen JSON data type and SQL/JSON functions. |
| AI chat/session memory | `ChatSessionMemory` | Simulated chat memory cache with TTL, message history, and JSON metadata stored in TimesTen as the primary store for active session state. |
| Agent workflow state | `AgentWorkflowState` | Simulated agent-run and tool-call state with retry-safe result reuse, JSON metadata, and TTL cleanup. |
| Online feature store | `FeatureStore` | Simulated personalization feature store with TTL, JSON feature values, audit metadata, and low-latency reads in TimesTen. |
| AI response cache | `AiResponseCache` | Simulated AI response cache with TTL, hit counting, and JSON metadata stored in TimesTen as the primary store for active cache state. |
| Payment authorization state | `PaymentAuthorizationState` | Simulated payment authorization flow with TTL, idempotent replay, JSON request/decision metadata, and low-latency decisioning in TimesTen. |
| Telecom call routing state | `TelecomCallRoutingState` | Simulated telecom routing flow with TTL, idempotent replay, JSON request/decision metadata, and low-latency decisioning in TimesTen. |
| Benchmarking | `Tptbm` | Multi-user throughput benchmark with configurable transaction mix. |
| JMS/XLA | `asyncJMS`, `asyncJMS2`, `syncJMS`, `syncJMS2` | Change-notification examples using TimesTen JMS/XLA. These are specialized samples with additional prerequisites. |
| Support utilities | `AccessControl`, `IOLibrary`, `InitializeDatabase`, `PasswordField`, `EraserThread`, `tt_version` | Shared helper classes used by the runnable samples. |

## IMPORTANT PRE-REQUISITES

1. Manually Configure the Sample DSN for the Sample Programs. Refer to _quickstart/html/developer/sample\_dsn\_setup.html_.

2. Set up the Instance Environment Variables e.g. If your TimesTen instance location is under /home/timesten/instance/<instance_name> directory, execute the following command:

    `source /home/timesten/instance/<instance_name>/bin/ttenv.sh`

3. Set up quickstart environment variables:
    
    Unix/Linux:

    `. quickstart/ttquickstartenv.sh`

    or

    `source quickstart/ttquickstartenv.csh`

    Windows:

    `call quickstart/ttquickstartenv.cmd`
 
4. Run the _quickstart/sample\_scripts/createdb/build\_sampledb_ script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

    Unix/Linux:
    
    `cd  quickstart/sample_scripts/createdb`
    
    `./build_sampledb.sh`


## How to compile the sample JDBC programs

To compile the sample programs in the sample\_code/jdbc directory, use a JDK supported by your TimesTen release and platform. For current platform and JDK support details, refer to the TimesTen release notes and installation documentation in the [Oracle TimesTen In-Memory Database Documentation Library](https://docs.oracle.com/en/database/other-databases/timesten/).

For JDK 11, 17, 21, and 25, the TimesTen JDBC JAR is timesten_home/install/lib/ttjdbc<jdk_version>.jar, where <jdk_version> indicates the JDK version, 11, 17, 21, or 25, for example, ttjdbc25.jar for JDK 25. The JDBC JARs are also packaged as a Java module with the Java module name **timesten.jdbc** and JMS/XLA JAR (timestenjmsxla.jar) too with module name **timesten.jmsxla** so you can use both for module compilation (import them in module-info.java), if your JDK supports modules. 

**NOTE:** The JMS/XLA samples are specialized examples with additional prerequisites. XLA does not support applications linked with a driver manager library or the client/server library, so the JMS/XLA samples cannot be compiled or run in client-only installations. The `asyncJMS` and `syncJMS` samples use Javax JMS and require `jms.jar` in `CLASSPATH`. The `asyncJMS2` and `syncJMS2` samples use Jakarta JMS and require `jakarta.jms.jar` in `CLASSPATH`. Support for Jakarta JMS was added in TimesTen 22.1.1.20.0, and Jakarta is the default if both Jakarta JMS and Javax JMS JARs are found in `CLASSPATH`.

### Compile with a supported JDK

To compile specific program:

`javac -d out <progname>.java`

To compile the sample programs all at once:

`javac -d out *.java`

The "-d" is necessary to build the module application in "out" directory. If you check the "out" directory you will find package directories.

### Compile using the TimesTen JDBC module

#### Compile as a non-modular or mixed application:

To compile specific program:

`javac -cp $CLASSPATH --module-path <module_path> -d out <progname>.java`

To compile the sample programs all at once:

`javac -cp $CLASSPATH --module-path <module_path> -d out *.java`

Where <module_path> is timesten_home/install/lib/ttjdbc<jdk_version>.jar:<existing_path_to_modules>. The "-d" is necessary to build the module application in "out" directory. If you check the "out" directory you will find package directories. 

#### Compile as a modular application:

To compile specific program:

`javac --module-path <module_path> -d out module-info.java <progname>.java`

To compile the sample programs all at once:

`javac --module-path <module_path> -d out *.java`

Where <module_path> is timesten_home/install/lib/ttjdbc<jdk_version>.jar:<existing_path_to_modules>. The "-d" option is necessary to build the module application in the "out" directory. The "out" directory contains module-info.class and package directories. The module name, dependencies, and exports are defined in module-info.java. All Java files compiled into the sample module need package names matching the sample source files, such as "jdbc.demo", "jms.demo", and "jakarta.jms.demo". For these samples, when you compile as a module application, you can use $CLASSPATH (set in **IMPORTANT PRE-REQUISITES**) as your <module_path> so dependencies and sample output directories are available as modules. When you build your own module application, define your own module-info.java and package names accordingly.

**NOTE:** JMS/XLA samples have additional requirements. They cannot be compiled or run in client-only installations, and the Jakarta JMS variants (`asyncJMS2` and `syncJMS2`) require `jakarta.jms.jar` in `CLASSPATH`.

## How to run the sample JDBC programs

**NOTE:** On platforms where both the 32-bit and 64-bit JDKs are installed in
the same directory (e.g. Solaris), java must be invoked with the -d64
option in order to run the 64-bit JVM.

**NOTE:** On some platforms, such as macOS, you may need to explicitly pass a setting for java.library path to the JVM in order to run the samples. For example:

    java -Djava.library.path=${TIMESTEN_HOME}/install/lib <packagename><progname>

**NOTE:** When you run with the TimesTen JDBC module, include the module path and enable native access. For example:
    
Run as a non-modular or mixed application

    java -cp $CLASSPATH --module-path <module_path> --enable-native-access="timesten.jdbc" jdbc.demo.<progname> …

    java -cp $CLASSPATH --module-path <module_path> --enable-native-access="timesten.jdbc,timesten.jmsxla" jms.demo.<progname> …

    java -cp $CLASSPATH --module-path <module_path> --enable-native-access="timesten.jdbc,timesten.jmsxla" jakarta.jms.demo.<progname> …

Run as a modular application

    java --module-path <module_path> --enable-native-access="timesten.jdbc" --module my.jdbc.app.module/jdbc.demo.<progname> …

    java --module-path <module_path> --enable-native-access="timesten.jdbc,timesten.jmsxla" --module my.jms.app.module/jms.demo.<progname> …

    java --module-path <module_path> --enable-native-access="timesten.jdbc,timesten.jmsxla" --module my.jakarta.jms.app.module/jakarta.jms.demo.<progname> …

For JMS programs, also include **timesten.jmsxla** in **--enable-native-access**, for example **--enable-native-access=timesten.jdbc,timesten.jmsxla**. In the commands above, the sample module names are **my.jdbc.app.module**, **my.jms.app.module**, and **my.jakarta.jms.app.module**. The package names are **jdbc.demo**, **jms.demo**, and **jakarta.jms.demo**. The module names and dependencies are defined in module-info.java. When you build and run your own module application, define your own module-info.java and package names accordingly.

### Sample programs instructions and examples

For JDK 25, compile the six new demos as a modular application:

    javac --module-path $TIMESTEN_HOME/install/lib/ttjdbc25.jar -d out *.java

The modular commands in their individual sections assume this `out` directory.

### Passwords for the modern samples

The five modern demos accept `-p` for a quick local run. To avoid placing a
password in the command line or shell history, set `TT_PASSWORD` and omit
`-p` instead:

    export TT_PASSWORD='password'

When both are provided, `-p` takes precedence over `TT_PASSWORD`. If neither
is provided, the sample prompts for the password.

**asyncJMS** or **asyncJMS2**

These two programs use the TimesTen JMS/XLA implementation to process messages. `asyncJMS` uses Javax JMS, while `asyncJMS2` uses Jakarta JMS. The functions of these two programs are:

a) Connect to the database as an XLA user

b) Listen to committed changes to the CUSTOMER table

c) Use a JMS Subscriber

d) ASYNCHRONOUSLY receive messages via onMessage() and a JMS MessageListener

e) Display the change records to the console

f) Disconnect from the database when the user enters CTRL-C

Either the level1 - level4 JDBC programs or ttIsql can be used to apply committed changes to the CUSTOMER table.

Examples:

  Connect using the default DSN sampledb, uid=xlauser, prompted password, listen to APPUSER.CUSTOMER
  
  `java jms.demo.asyncJMS`

  or

  `java jakarta.jms.demo.asyncJMS2`

  Connect using the default DSN sampledb, uid=xlauser, listen to APPUSER.CUSTOMER
 
  `java jms.demo.asyncJMS -xlauser <xlausername>`

  or

  `java jakarta.jms.demo.asyncJMS2 -xlauser <xlausername>`

  Connect using the default DSN sampledb, uid=xlauser and listen to the MYUSER.CUSTOMER table
  
  `java jms.demo.asyncJMS -xlauser <xlausername> -schema myUser`

  or

  `java jakarta.jms.demo.asyncJMS2 -xlauser <xlausername> -schema myUser`

  For the full syntax of the program, use "java jms.demo.asyncJMS -h" or "java jakarta.jms.demo.asyncJMS2 -h".

**level1**

This program uses the TimesTen JDBC DriverManager interface to show basic operations on a TimesTen database: 

a) Connect using the com.timesten.jdbc.timesTenDriver interface

b) Insert into the CUSTOMER table all the data read from input file input1.dat file

c) Call ttOptUpdateStats to update statistics

d) SELECT from the CUSTOMER table, fetch, and print the result set to stdout

e) Disconnect from the database

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java jdbc.demo.level1`

  Connect using default dsn and uid, and client/server mode
  
  `java jdbc.demo.level1 -c`

  Connect using the dsn **my_dsn**
  
  `java jdbc.demo.level1 my_dsn`

  For the full syntax of the program, use "java jdbc.demo.level1 -h".

**level2**

This program uses the TimesTen DataSource interface to show basic operations on a TimesTen database:

a) Connect using the TimesTenDataSource interface

b) Insert all the data read from the input2.dat file located in the datfiles subdirectory

c) Call ttOptUpdateStats to update statistics

d) DELETE duplicate product data from the PRODUCT table

e) Update the PRODUCT table by increasing the product price by 10%

f) SELECT the data from the PRODUCT table, fetch and print the result set to stdout

g) Disconnect from the database.

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java jdbc.demo.level2`

  Connect using default dsn and uid, and client/server mode
  
  `java jdbc.demo.level2 -c`

  Connect using the dsn **my_dsn**
  
  `java jdbc.demo.level2 my_dsn`

  For the full syntax of the program, use "java jdbc.demo.level2 -h".

**level3**

This program uses the TimesTen JDBC driver to perform order processing operations:

a) Connect to the database using the TimesTenDataSource interface

b) Process all orders in the input3.dat file by inserting into the ORDERS and ORDER_ITEM tables, select from the Inventory table to check available quantity, and update the INVENTORY table to debit the quantity from the order. If there is not enough quantity in the inventory, the program rolls back the transaction and issues the message.

c) Disconnect from the database.

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java jdbc.demo.level3`

  Connect using default dsn and uid, and client/server mode
  
  `java jdbc.demo.level3 -c`

  Connect using the dsn **my_dsn**
  
  `java jdbc.demo.level3 my_dsn`

  For the full syntax of the program, use "java jdbc.demo.level3 -h".

**level4**

This program is the multi-threaded version of the level3.java program. This program uses multiple threads to increase the application throughput.

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java jdbc.demo.level4`

  Connect using default dsn and uid, and client/server mode
  
  `java jdbc.demo.level4 -c`

  Connect using the dsn **my_dsn**
  
  `java jdbc.demo.level4 my_dsn`

  For the full syntax of the program, use "java jdbc.demo.level4 -h".

**plsqlJDBC**

This sample program uses JDBC to access common PLSQL packages (emp\_pkg and sample\_pkg) in four different ways:

- Calls a stored procedure with IN and OUT parameters

- Calls a stored function with IN and OUT parameters

- Calls an anonymous block and passes host variables into and out of the block

- Calls a store procedure to open a ref cursor and uses JDBC to process 
the result-set of the ref cursor

The EMP table and the emp\_pkg and sample\_pkg PLSQL packages need to exist for this program to work.

Examples:

  Run the program using the default DSN sampledb, it will prompt for the username and password
  
  `java jdbc.demo.plsqlJDBC`

For the full syntax of the program, use "java jdbc.demo.plsqlJDBC -help".

**syncJMS** or **syncJMS2**

These two programs use the TimesTen JMS/XLA implementation to process messages. `syncJMS` uses Javax JMS, while `syncJMS2` uses Jakarta JMS. The functions of these two programs are:

a) Connects to the database as an XLA user

b) Listen to committed changes for ANY table (defaults to the appuser.customer table)

c) Use a JMS Subscriber

d) SYNCHRONOUSLY receive messages via the receive method in the get routine

e) Display the change records to the console

f) Disconnect from the database when the user enters CTRL-C

Either the level1 - level4 JDBC programs or ttIsql can be used to apply committed changes to the CUSTOMER, PRODUCT, ORDERS, ORDER_ITEM and INVENTORY tables

Examples:

  Connect using the default DSN sampledb, uid=xlauser, prompted password, listen to APPUSER.CUSTOMER
  
  `java jms.demo.syncJMS`

  or

  `java jakarta.jms.demo.syncJMS2`

  Connect using the default DSN sampledb, uid=xlauser, listen to APPUSER.CUSTOMER
  
  `java jms.demo.syncJMS -xlauser <xlausername>`

  or

  `java jakarta.jms.demo.syncJMS2 -xlauser <xlausername>`

  Connect using the default DSN sampledb, uid=xlauser and listen to the MYUSER.PRODUCT table
  
  `java jms.demo.syncJMS -xlauser <xlausername> -schema myUser -table product `

  or

  `java jakarta.jms.demo.syncJMS2 -xlauser <xlausername> -schema myUser -table product `

For the full syntax of the program, use "java jms.demo.syncJMS -h" or "java jakarta.jms.demo.syncJMS2 -h".

**Tptbm**

This program implements a multi-user throughput benchmark. By default, the transaction mix consists of 80% SELECT (read) transactions and 20% UPDATE (write) transactions. In addition to SELECTs and UPDATEs, INSERTs can also be included in the transaction mix. The ratio of SELECTs, UPDATEs and INSERTs is specified at the command line. Each transaction consists of one or more SQL operations.

The benchmark initially populates the data store, and then executes the transaction mix on it. The number of tuples inserted as part of the transaction mix cannot exceed the number of tuples with which the database is populated initially.

The measurement error for the benchmark is at most 2 seconds. This will
be negligible at loads with a duration in excess of 200 seconds. A suggested load for the benchmark is one that lasts at least 600 seconds.

The schema for this test is described in the program source file tptbm.java.

Examples:

  
Run the program using default workload mix of 80% reads, 20% updates, dsn=sampledb, uid=appuser
  
  `java jdbc.demo.Tptbm`

80% reads, 20% updates, 2 threads, populate the table with 400,000 rows, and run for 60 seconds with a 10 second ramp-up and ramp-down time.
  
  `java jdbc.demo.Tptbm -threads 2 -key 200 -sec 60`

85% reads, 10% inserts, 5% updates, 4 threads.
  
  `java jdbc.demo.Tptbm -threads 4 -read 85 -insert 10 `

For the full syntax of the program, use "java jdbc.demo.Tptbm -h".


**TTJdbcExamples**

This program demonstrates the following operations using the TimesTen JDBC DriverManager interface to:

a) Connect and disconnect

b) Perform some DDL and DML operations: create table, create index, prepare statements, insert into and select from a table

c) Make changes to influence the query plan

d) Execute batch updates

Examples:

  Run all examples, connect using the default DSN (sampledb) as user appuser
  
  `java jdbc.demo.TTJdbcExamples`

  To run only example 2
  
  `java jdbc.demo.TTJdbcExamples -run 2`

  To run the 64-bit JDK for example 2
  
  `java -d64 jdbc.demo.TTJdbcExamples -run 2`

  For the full syntax of the program, use "java jdbc.demo.TTJdbcExamples -h".


**JsonSample**

This sample demonstrates how to work with JSON data in TimesTen using JDBC. It creates a JSON-enabled purchase order table, loads JSON documents from disk, creates a functional index for JSON_VALUE lookups, updates an existing document, runs queries filtering by purchase order and user, and lists line items using `JSON_TABLE`. By default the table is dropped at the end unless the `-keep` flag is supplied.

Example:

  Run the program using the default DSN (sampledb) and clean up on completion
  
  `java jdbc.demo.JsonSample`

  Run the program and retain the JSON table after completion
  
  `java jdbc.demo.JsonSample -keep`


**ApiSessions**

This sample demonstrates a modern application-session pattern in TimesTen. It creates an `api_sessions` table, seeds active session rows, performs reads to simulate session lookups, updates request counters and last-seen timestamps for active sessions, and removes some inactive rows before cleanup. The data model is intentionally short-lived and operational, which makes it a good fit for an in-memory database.

Example:

  Run the program using the default DSN (sampledb)

  `java jdbc.demo.ApiSessions`

**AiResponseCache**

This sample demonstrates a fast AI response cache pattern in TimesTen. It creates a cache table with response text, metadata, hit counts, and expiration timestamps; simulates cache hits and misses; and then removes expired rows before dropping the table. The demo keeps TimesTen as the primary store for active cache state and uses simulated responses so the focus stays on the storage and retrieval pattern.

Responses are simulated; this sample does not call an AI model, perform vector search, run in-database model inference, or demonstrate TimesTen Cache for Oracle Database.

The sample performs the following steps:

  - Creates an `ai_response_cache` table
  - Creates indexes for tenant/model and expiration lookups
  - Seeds one expired cache entry
  - Processes sample AI requests, showing cache misses and cache hits
  - Updates hit counts and last-accessed timestamps on cache hits
  - Stores model metadata in a JSON column and queries it with SQL/JSON
  - Deletes expired cache entries

Example:

  Run the program using the default DSN (sampledb)

    java --module-path out:$TIMESTEN_HOME/install/lib/ttjdbc25.jar \
      --enable-native-access=timesten.jdbc \
      --module my.jdbc.app.module/jdbc.demo.AiResponseCache -u username [-p password]

**ChatSessionMemory**

This sample demonstrates a short-lived AI chat/session memory pattern in TimesTen. It creates a chat session table with tenant, user, topic, session-state, and expiration fields; stores recent messages, tool-call metadata, safety labels, and citations in JSON; simulates a resumed conversation; and removes expired rows before dropping the table. The demo uses simulated replies so the focus stays on fast session restoration and state management.

The sample performs the following steps:

  - Creates a `chat_sessions` table
  - Creates indexes for tenant/user and expiration lookups
  - Seeds one expired chat session
  - Starts and resumes sample chat sessions
  - Stores recent messages and metadata in JSON
  - Queries JSON fields to summarize active sessions
  - Deletes expired chat sessions

Example:

  Run the program using the default DSN (sampledb)

    java --module-path out:$TIMESTEN_HOME/install/lib/ttjdbc25.jar \
      --enable-native-access=timesten.jdbc \
      --module my.jdbc.app.module/jdbc.demo.ChatSessionMemory -u username [-p password]

**AgentWorkflowState**

This sample demonstrates an agent workflow state pattern in TimesTen. It uses
one table for current agent runs and another for tool calls, then stores plans,
tool inputs, tool results, and current state in JSON. It reuses a completed
tool result for a repeated call and rereads that result if concurrent requests
insert the same tool-call key. The demo uses simulated agent steps and tool
results so the focus stays on orchestration state and retry handling.

Agent steps, tool calls, and responses are simulated; this sample does not call
an AI model, agent framework, external tool service, perform vector search, or
run in-database model inference.

The sample performs the following steps:

  - Creates `agent_runs` and `agent_tool_calls` tables
  - Creates indexes for tenant/agent summaries, tool calls by run, and expiration cleanup
  - Seeds one expired agent run and tool call
  - Starts sample agent runs and records their current state
  - Completes simulated tool calls and replays a repeated tool call
  - Rereads the stored tool result if a concurrent request inserts the same key
  - Stores run plans, tool inputs, and results in JSON
  - Summarizes active runs and their tool calls
  - Deletes expired runs and tool calls

Example:

  Run the program using the default DSN (sampledb)

    java --module-path out:$TIMESTEN_HOME/install/lib/ttjdbc25.jar \
      --enable-native-access=timesten.jdbc \
      --module my.jdbc.app.module/jdbc.demo.AgentWorkflowState -u username [-p password]


**FeatureStore**

This sample demonstrates a real-time online feature store pattern in TimesTen. It creates a feature table with tenant, user, feature name, feature value, freshness, and expiration fields; upserts fresh feature values; stores a JSON audit trail for the resulting personalization decision; and removes stale rows before dropping the table. The demo uses simulated feature updates so the focus stays on low-latency reads, writes, and freshness management.

The sample performs the following steps:

  - Creates a `user_features` table
  - Creates indexes for tenant/user and freshness lookups
  - Seeds one stale feature row
  - Upserts fresh feature values for sample users
  - Fetches the current feature set for a user with low latency
  - Stores a JSON audit payload for the resulting personalization decision
  - Deletes stale feature rows

Example:

  Run the program using the default DSN (sampledb)

    java --module-path out:$TIMESTEN_HOME/install/lib/ttjdbc25.jar \
      --enable-native-access=timesten.jdbc \
      --module my.jdbc.app.module/jdbc.demo.FeatureStore -u username [-p password]


**PaymentAuthorizationState**

This sample demonstrates a real-time payment authorization state flow in TimesTen. It creates a payment authorization table with tenant, account, merchant, amount, risk, status, and expiration fields; applies deterministic authorization rules; stores request and decision metadata in JSON; replays repeated payment requests; and rereads the stored decision if concurrent requests insert the same key. It removes expired rows before dropping the table. The demo uses simulated rules so the focus stays on low-latency decisioning and state management.

The sample performs the following steps:

  - Creates a `payment_authorizations` table
  - Creates indexes for tenant/account, payment id, and expiration lookups
  - Seeds one expired authorization record
  - Processes payment authorization requests
  - Shows a repeated request being replayed from the existing state
  - Rereads the stored decision if a concurrent request inserts the same key
  - Stores request and decision metadata in JSON
  - Summarizes active authorizations by tenant/account/status
  - Deletes expired authorization records

Example:

  Run the program using the default DSN (sampledb)

    java --module-path out:$TIMESTEN_HOME/install/lib/ttjdbc25.jar \
      --enable-native-access=timesten.jdbc \
      --module my.jdbc.app.module/jdbc.demo.PaymentAuthorizationState -u username [-p password]


**TelecomCallRoutingState**

This sample demonstrates a real-time telecom call routing state flow in TimesTen. It creates a call routing table with tenant, subscriber, call, region, slice, priority, route state, and expiration fields; applies deterministic routing rules; stores request and decision metadata in JSON; replays repeated call requests; and rereads the stored decision if concurrent requests insert the same key. It removes expired rows before dropping the table. The demo uses simulated rules so the focus stays on low-latency routing decisions and state management.

The sample performs the following steps:

  - Creates a `call_routing_state` table
  - Creates indexes for tenant/subscriber, call id, and expiration lookups
  - Seeds one expired routing record
  - Processes call routing requests
  - Shows a repeated request being replayed from the existing state
  - Rereads the stored decision if a concurrent request inserts the same key
  - Stores request and decision metadata in JSON
  - Summarizes active routing decisions by tenant/subscriber/state
  - Deletes expired routing records

Example:

  Run the program using the default DSN (sampledb)

    java --module-path out:$TIMESTEN_HOME/install/lib/ttjdbc25.jar \
      --enable-native-access=timesten.jdbc \
      --module my.jdbc.app.module/jdbc.demo.TelecomCallRoutingState -u username [-p password]


For more information on Java programming with Oracle TimesTen, refer to the [Oracle TimesTen In-Memory Database Java Developer's Guide](https://docs.oracle.com/en/database/other-databases/timesten/26.1/java-developer/index.html).
