# Oracle TimesTen In-Memory Database Container Examples

This directory has two container paths:

- [modern-demos](./modern-demos) is the quick start for the modern Python,
  Node.js, and Java samples. It uses Podman by default and also supports Docker.
- The files in this directory are the original persistent-container example.
  Its scripts invoke Docker directly and show how to extend the official image
  so a database survives container start/stop events.

Both paths use the official Oracle TimesTen In-Memory Database container images.

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
| [README_quickstart](./README_quickstart.md) | Step-by-step guide for the original persistent Docker container example. |
| [README_content](./README_content.md) | README file that describes the files in the [content](./content) directory. |
| [README_build](./README_build.md) | README file that describes the build process and configuration options. | 
| [README_usage](./README_usage.md) | README file that provides detailed usage information for the container and associated scripts. | 
| [README_platforms](./README_platforms.md) | README file that provides important information for using Docker on non-Linux hosts, for example Docker Desktop for Mac. |   
| [FAQ](./FAQ.md) | Frequently Asked Questions. |

## Official TimesTen Container Images
These can be found in the [Oracle Container Registry](https://container-registry.oracle.com).

## Supported TimesTen Releases
The original persistent example extends the official TimesTen Docker image and
supports the current variants of that image.

## More information
You can find more information about the Oracle TimesTen In-Memory Database on our [Product Portal](https://www.oracle.com/database/technologies/related/timesten.html)

## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/en/database/other-databases/timesten/)

## Blogs
You can find interesting blogs relating to TimesTen on our [blogs channel](https://blogs.oracle.com/timesten)
