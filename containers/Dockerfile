# LICENSE UPL 1.0
#
# Copyright (c) 2022 Oracle and/or its affiliates.
#
# ORACLE DOCKERFILES PROJECT
# --------------------------
# This is the Dockerfile for Oracle TimesTen In-Memory Database.
# This Dockerfile supports TimesTen release 18.1 and later.

# Global build args
# ---------------
ARG BASEIMAGE=container-registry.oracle.com/timesten/timesten:22.1.1.1.0

# Define base image
# ---------------
FROM ${BASEIMAGE}

# Label
# ---------------
LABEL "provider"="Oracle"          \
      "volume.data"="/ttdb"        \
      "port.daemon"="6624"         \
      "port.server"="6625"         \
      "port.replication"="3754"

# Switch to 'root' as we need to install stuff and so on.
# ---------------
USER root

# Create the init directory and the volume mount point.
# Also remove the pre-defined TimesTen instance.
# ---------------
RUN mkdir -m 700 /home/timesten/ttinit && mkdir -m 775 /ttdb && rm -rf /timesten/instance1

# Copy the bash profile files
# ---------------
COPY content/bashrc /home/timesten/.bashrc
COPY content/bash_profile /home/timesten/.bash_profile

# Copy the remenv convenience script
# ---------------
COPY content/remenv /usr/local/bin/remenv

# Copy the database config and init files.
# ---------------
COPY content/init.sql content/sys.odbc.ini /home/timesten/ttinit/

# Copy the control scripts.
# ---------------
COPY content/init-tt content/start-tt content/stop-tt /etc/init.d/

# Set ownership and permissions
# ---------------
RUN chown -R timesten:timesten /ttdb /home/timesten && chmod 644 /home/timesten/.bashrc /home/timesten/.bash_profile && chmod 755 /usr/local/bin/remenv /etc/init.d/init-tt /etc/init.d/start-tt /etc/init.d/stop-tt

# Expose the necessary ports.
# ---------------
EXPOSE 3754 6624 6625

# Switch to user 'timesten' for execution
# ---------------
USER timesten

# Set container entry point.
# ---------------
ENTRYPOINT [ "/usr/bin/bash", "-c", "/etc/init.d/init-tt" ]
