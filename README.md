# oracle-timesten-samples

This repository stores a variety of examples demonstrating how to use the Oracle TimesTen In-Memory Database.

## Recommended starting points

If you are new to these samples, start with the language and deployment examples below. They highlight the most common application-development paths while keeping the classic QuickStart material available for deeper product coverage.

| Area | Start here | Description |
| :--- | :--------- | :---------- |
| Python | [languages/python](./languages/python) | Python samples using the `python-oracledb` driver. |
| Node.js | [languages/nodejs](./languages/nodejs) | Node.js samples using the `node-oracledb` driver. |
| Java | [quickstart/sample_code/jdbc](./quickstart/sample_code/jdbc) | Java samples using the TimesTen JDBC driver. |
| JSON | [Python JSON](./languages/python), [Node.js JSON](./languages/nodejs), [Java JSON](./quickstart/sample_code/jdbc) | JSON document storage, indexing, update, and query examples. |
| Containers | [containers](./containers) | Run TimesTen in a container with a persistent database. |
| Kubernetes | [kubernetes](./kubernetes) | Deploy TimesTen with the TimesTen Kubernetes Operator. |

## Repository layout

| Repo/Folder name            | Description                                     |
| :-------------------------- | :---------------------------------------------- |
| [languages](./languages) | Open source language samples for Python and Node.js. |
| [quickstart](./quickstart)  | Classic QuickStart, Scaleout, administration, and API samples. |
| [containers](./containers) | Sample showing how to run TimesTen in a container with a persistent database. |
| [kubernetes](./kubernetes) | Shows how to use the TimesTen Kubernetes Operator to deploy TimesTen in Kubernetes. |

## Classic and compatibility samples

The classic QuickStart tree remains part of this repository. It includes long-standing samples for JDBC, ODBC, OCI, PL/SQL, TTClasses, ODP.NET, ODPI-C, J2EE/ORM integrations, administration tasks, cache, replication, and performance testing. These samples are useful for existing deployments, compatibility testing, and product-feature coverage even when they are not the first recommended path for new application developers.

## Supported TimesTen Releases
In general these samples require a _minimum_ of **TimesTen 22.1.1.1.0** but our recommendation is to always use the most recent release available for your platform. Some specific samples may require a more recent TimesTen release; any such requirement will be detailed in the sample's README file. These samples all work equally well with regular TimesTen or the free TimesTen Express Edition (TimesTen XE).

## More information
You can find more information about the Oracle TimesTen In-Memory Database on our [Product Portal](https://www.oracle.com/database/technologies/related/timesten.html)

## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/en/database/other-databases/timesten/)

## Blogs
You can find interesting blogs relating to TimesTen on our [blogs channel](https://blogs.oracle.com/timesten)

## Contributing

This project welcomes contributions from the community. Before submitting a pull request, please [review our contribution guide](./CONTRIBUTING.md)

## Security

Please consult the [security guide](./SECURITY.md) for our responsible security vulnerability disclosure process

## License

Copyright (c) 2018, 2026 Oracle and/or its affiliates.

Released under the Universal Permissive License v1.0 as shown at
<https://oss.oracle.com/licenses/upl/>.
