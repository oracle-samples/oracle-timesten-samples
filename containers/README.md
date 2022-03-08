# Oracle TimesTen In-Memory Database on Docker

Sample Dockerfile and support scripts for building a container image to run the Oracle TimesTen In-Memory Database. This sample is based on the official Oracle TimesTen In-Memory Database container images. It shows how you can extend the official image to support a persistent database that survives container start/stop events.

| File / folder name            | Description                                     |
| :-------------------------- | :---------------------------------------------- |
| [container.cfg](./container.cfg) | Configuration file. |
| [Dockerfile](./Dockerfile) | Dockerfile used to build the TimesTen image. |
| [content](./content) | Files and scripts to populate the container. |
| [build](./build) | Script to build the TimesTen image. |
| [crvolume](./crvolume) | Script to create the container's persistence volume. The volume must exist before you can run the container. |
| [rmvolume](./rmvolume) | Script to delete the container's persistence volume. Only succeeds if the container does not exist. |
| [crnetwork](./crnetwork) | Script to create the (optional) custom docker network. |
| [rmnetwork](./rmnetwork) | Script to delete the custom network. |
| [rmcontainer](./rmcontainer) | Script to delete the container. The container is implicitly created when you execute the 'ttstart' script (docker run). |
| [rmimage](./rmimage) | Script to delete the TimesTen image. Only succeeds if there is no existing container. |
| [ttstart](./ttstart) | Script to start the container. Implicitly creates a container from the image. | 
| [ttstop](./ttstop) | Script to stop the container. | 
| [ttconnect](./ttconnect) | Script to open an interactive session to the running container, or to run a command in the running container. | 
| [ttlog](./ttlog) | Script to display the logs for the running container. | 
| [README](./README.md) | This README file. |
| [README_quickstart](./README_quickstart.md) | README file that takes you step by step through the (simple) process to create and start a TimesTen container. It is recommended that you start by following the process detailed in this file before emarking on any customizations.|
| [README_content](./README_content.md) | README file that describes the files in the [content](./content) directory. |
| [README_build](./README_build.md) | README file that describes the build process and configuration options. | 
| [README_usage](./README_usage.md) | README file that provides detailed usage information for the container and associated scripts. | 
| [README_platforms](./README_platforms.md) | README file that provides important information for using Docker on non-Linux hosts, for example Docker Desktop for Mac. |   
| [FAQ](./FAQ.md) | Frequently Asked Questions. |

## Official TimesTen Container Images
These can be found in the [Oracle Container Registry](https://container-registry.oracle.com).

## Supported TimesTen Releases
This Dockerfile extends the official TimesTen docker container image and supports all current flavours of that image.

## More information
You can find more information about the Oracle TimesTen In-Memory Database on our [Product Portal](https://www.oracle.com/database/technologies/related/timesten.html)

## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/en/database/other-databases/timesten/)

## Blogs
You can find interesting blogs relating to TimesTen on our [blogs channel](https://blogs.oracle.com/timesten)

