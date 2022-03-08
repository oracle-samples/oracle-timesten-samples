# Build configuration, script and process

Building (and running) the image and container is performed from the top level directory in this package. 

The image configuration is of course specified via the **Dockerfile**. All the files referenced by the Dockerfile are located in the **content** directory.

The TimesTen image and container is built using the script **build**. The build process is configured by way of directives (environment variables) in the file **container.cfg**. 

## Container overview

The container image is based on the latest official TimesTen container image. By default the container uses the default Docker network connectivity, but it can optionally be configured to use a custom network that supports both IPv4 and IPv6. If you choose this option then you have full control over the IP subnets and addresses used for this network via the **container.cfg** file.

**NOTE:** In order to pull images from the [Oracle Container Registry](https://container-registry.oracle.com), you must (a) have a valid oracle.com login (free to create) and (b) have logged in to the container registry and accepted the license agreement. If you have not completed these steps, pull requests will fail.

Using a custom network can be very useful if you need the container to communicate with other containers, perhaps for the purpose of using TimesTen's cache and/or replication features, or allowing client-server access to the database from a different container.

The persistent storage needed for the TimesTen database is provided by a Docker volume. By default this volume is named **ttdb**, but an alternate name can be specified by way of a directive in the **container.cfg** file.

## The container.cfg file

This is a text file which contains environment variable definitions. It is used to configure many aspects of the container image and it is also used by the various support scripts .

The file contains detailed comments describing each configuration parameter. Most of the parameters are optional and have sensible defaults. There is one mandatory parameter (DOCKER_VOLUME) which specifies the name of the Docker volume used to provide persistent storage for the TimesTen database.

If you are using a custom network configuration then that is configured using parameters in this file.

## The build script

This script performs all necessary actions to build the image and the container. It is controlled by the parameters in the **build.cfg** file and also by some command line arguments:

    Usage:

        build [--login]

    Builds the TimesTen container image. Parameters are:

      --login
          Logs in to the container registry. Usually only required once.
          
The **--login** option causes the script to perform a login to the defined container registry. This is usually only required if this build needs to pull down the base image from the registry and if you have not already previously logged in.

## Build process

Customize the **container.cfg** file contents, if required (in most cases the file can be used 'as is'). Then run the build script.
