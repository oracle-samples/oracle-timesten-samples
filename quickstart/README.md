# TimesTen QuickStart and Scaleout Samples

The Oracle TimesTen In-memory Database can be deployed in two distinct modes; Classic and Scaleout.

Classic is equivalent to the original TimesTen In-Memory Database and TimesTen Application-Tier Database Cache products in releases prior to 18.1 and includes all of the functionality of those products plus additional enhancements. The focus is on delivering low, consistent latency for SQL operations and transactions, high throughput (within the constraints of a single computer system) and high performance high availability.

Scaleout is a new feature introduced in the 18.1 release. In this mode TimesTen is deployed as a distributed, shared nothing, elastically scalable, highly available, relational in-memory database. The focus is on very high throughput (utilizing the computing power of multiple individual computer systems), providing a single database image for applications (transparent data distribution), automatic high availability and elastic scalability.

This folder contains the original TimesTen Classic QuickStart (which also covers Application-Tier Database Cache) and some Scaleout sample programs. See the README files in the individual folders for more information.

| Repo/Folder name            | Description                                     |
| :-------------------------- | :---------------------------------------------- |
| [classic](./classic)        | Classic QuickStart                         |
| [scaleout](./scaleout)              | Scaleout sample programs.                      |

## More information
You can find more information on the Oracle TimesTen In-Memory Database on our [Product Portal](https://www.oracle.com/database/technologies/related/timesten.html)

## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/database/timesten-18.1/)
