# Using the TimesTen container

The operation of the container is controlled by parameters specified at both build time and run time. A number of useful scripts can be found in the top level directory of this package. Use of these scripts is recommended until you become familiar with how things work.

## Important Note

It is not _required_ to use any of the provided convenience scripts to build the image or run the container. The scripts are provided as a convenience that leverages the central point of configuration (the **container.cfg** file) and they also serve to illustrate the required docker commands for the various functions.

## Container startup

The container should be started using the **ttstart** script:

    Usage:
    
        ttstart [--network] [--csport [<portno>]]
        
    --network
      Attaches the container to the custom docker network. If not specified
      the container uses the default docker network.
        
    --csport [<portno>]
      Maps the TimesTen client-server access port to a localhost port on
      the host. By default the host port used is 6625 but this can be
      overridden by specifying an alternate port (1025 <= portno <= 65535).
      A TimesTen client can then connect to TimesTen in the container via
      this port on the host

Running this script will start the container. The script waits until the container has successfully started and completed initialization (or has failed to start) before returning.

On startup, if there is an existing TimesTen instance then it is started, otherwise a new instance is created, configured and started. If there is an existing database then that database is started up, otherwise a new database is created and initialised.

## Container shutdown

The container should be stopped using the **ttstop** script:

    Usage:
    
        ttstop
        
    Cleanly terminates a running TimesTen container.

Running this script will stop the container. The script waits until the container has successfully stopped before returning.

When the container is stopped, the TimesTen database is first gracefully shutdown and then the TimesTen instance is stopped. This process needs a certain amount of time to complete, so the container stop operation may take several seconds.

## Accessing the container

You can access the container using the **ttconnect** script:

    Usage:

        ttconnect [<cmd> [<arg>...]]

    If no 'cmd' specified, open an interactive shell session to the
    TimesTen container. If 'cmd' (and optional args) is specified,
    run the command in the TimesTen container and display the output.
    
If no **cmd** is specified then you will be connected interactively to the container as the user 'timesten'. If **cmd** (and optional arguments) are specified then the command is run within the container, as the user 'timesten' and an interactive session is not established.   

## Viewing the container logs

You can view the container logs using the **ttlog** script:

    Usage:

        ttlog [-f|--follow]

    Displays the docker log for the TimesTen container. If '--follow'
    is specified, displays new log data as it is generated. Interrupt
    with 'Ctrl-C'.

This will display the docker log for the container. The **--follow** option will cause the display to continue, following the log output, until interrupted or the container terminates.

## Instance and database lifecycle

Once the TimesTen instance and database have been created, they will persist across container shutdown, startup and build operations _provided that you do not run the **rmvolume** script in the meantime._

If you successfully run the **rmvolume** script, the container's persistent storage is deleted. You must re-create the volume (using the **crvolume** script) before you will be able to run the container again. When you do run the container it will start with a new instance and a newly created database.



