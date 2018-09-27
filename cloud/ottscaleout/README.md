# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
# Version 180720
    #     ___  ____     _    ____ _     _____
    #    / _ \|  _ \   / \  / ___| |   | ____|
    #   | | | | |_) | / _ \| |   | |   |  _|
    #   | |_| |  _ < / ___ | |___| |___| |___
    #    \___/|_| \_/_/   \_\____|_____|_____|
***
This example uses Terraform to allocate OCI resources for running the Oracle TimesTen Scaleout [InMemory] Database.
Ansible scripts are also provided to rollout a database across the provisioned infrastructure.
This is a bring-your-own-license (BYOL) solution for Oracle TimesTen Scaleout.
Please see the whitepaper, "[Deploying Oracle TimesTen Scaleout Database on OCI](http://www.oracle.com/technetwork/database/database-technologies/timesten/overview/wp-deployingtimestenscaleoutonoci-5069015.pdf)".

The example creates a VCN with one or more public subnets running bastion hosts.
Private subnets are created to run the hosts needed for TimesTen Scaleout.


### Using this example
* Copy TimesTen Scaleout and JDK distributions (.zip or .tar.gz files) to the service/packages directory.
* Update env-vars with the required information.
This example uses the same set of environment variables as other terraform examples, so you only need to do this once.
* Source env-vars
* `$ . env-vars`
* Update variables in variables.tf as applicable to your target environment.
* terraform plan
* terraform apply [--auto-approve]

To destroy all the resources created
* terraform destroy -force

Bastion host instances are configured as a NAT instances (by enabling forwarding and configuring firewall to do forwarding/masquerading).
The private subnet's route table is configured to use the NAT instance's private IP address as the default route target.
See [Using a Private IP as a Route Target](https://docs.us-phoenix-1.oraclecloud.com/Content/Network/Tasks/managingroutetables.htm#privateip) for more details on this feature.
Once the environment is built, the compute instances on the private network have Internet connectivity.
A private instance doesn't have a public IP address and it's subnet's route table doesn't contain Internet gateway.  
You can login into the private instance via ssh from the nat instance.
To verify connectivity, you can then run a command like 'ping oracle.com'

### Files in the configuration

#### `env-vars`
Is used to export the environmental variables used in the configuration.
These are intended to be the most common variables used.
They also include references to private key file information for access to the OCI infrastructure.

Before you plan, apply, or destroy the configuration source the file -  
`$ . env-vars`

#### `variables.tf`
Defines less frequently used variables.

### `public.tf`
Creates the VCN, the public subnets, and the compute instances used as bastion hosts.

### `private.tf`
Creates the private subnets, and the compute instances used as zookeeper servers, management instances, or database instances.

### `system-config.tf`
Contains terraform resources for initial configuration of the compute instances.
Ansible on 1 bastion host.
Variables and configuration files for ansible are set up as well.

### `service` directory
* ansible  - Ansible scripts
* packages - Required BYOL software: Timesten Scaleout distribution, JDK
* scripts  - Miscellaneous configuration scripts

###
Using Ansible to set up hosts and rollout TimesTen Scaleout
* `$ ssh opc@<bastion-host address> e.g. 129.146.2.3
* `$ cd service/ansible`
* `$ ansible-playbook -i hosts rollout.yml`

To scale out the number of data instances
* Modify variables.tf to set diInstanceCount=<bigger than before>
* terraform plan
* terraform apply --auto-approve
Then ssh to the bastion host,
* `$ cd service/ansible
* `$ ansible-playbook -i hosts scaleout.yml

To stop the zookeeper servers, management instances and database,
* `$ ansible-playbook -i hosts stop.yml
Note that this does not stop any cloud resources provisioned and charging continues.

To destroy the database, remove data and mgmt instance directories
* `$ cd service/ansible
* `$ ansible-playbook -i hosts dbdestroy .yml



