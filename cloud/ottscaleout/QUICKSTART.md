###### Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
###### Licensed under the Universal Permissive License v 1.0 as shown at <http://oss.oracle.com/licenses/upl>  
    #     ___  ____     _    ____ _     _____
    #    / _ \|  _ \   / \  / ___| |   | ____|
    #   | | | | |_) | / _ \| |   | |   |  _|
    #   | |_| |  _ < / ___ | |___| |___| |___
    #    \___/|_| \_/_/   \_\____|_____|_____|
***
This quickstart guide describes utilities to rapidly deploy Oracle Times Ten Scaleout in Oracle Cloud Infrastructure (OCI).  For details on how the utility works and additional options please refer to the [README.md](./README.md) file.

Running the _provisionScaleoutOCI_ utility allocates the cloud resources necessary to deploy a running TimesTen Scaleout database.

### Prerequisites:
1. A compute instance running Oracle Linux 7, provisioned on Oracle OCI, that you can ssh into as the opc user.  
  This is known as the "bootstrap instance".  
  See the [Tutorial](https://docs.cloud.oracle.com/iaas/Content/GSG/Reference/overviewworkflow.htm) for creating a bootstrap instance.  
2. Access to an OCI compartment other than `root` or `PaaSManagedCompartment` with a policy that allows creating VCNs  
  Please see, `Identity->Compartments` and `Identity->Policies` in the [OCI Console](https://console.us-phoenix-1.oraclecloud.com "OCI Console Phoenix")  
  The user that runs the utility needs to be a member of a group with the following permissions for the chosen compartment:  
  `manage virtual-network-family`  
  `manage instance-family`  
  `manage volume-family`  
   Note that members of the administrator group already have permission and do not require explicitly created policies.  
3. Check Service limits  
  Please visit `Governance->Service Limits` in the [OCI Console](https://console.us-phoenix-1.oraclecloud.com "OCI Console Phoenix") to ensure adequate resources are available.  
  The minimum requirement is 1 VCN, and 4 compute instances.  
  In addition, if using standard shapes for database compute instances, 1 block volume for each database host instance is required.  
4. The following OCIDs from the OCI Console:  
  User ID, see `Identity-Users`  
  Tenancy ID, see `Administration->Tenancy Details`  
5. An Oracle TimesTen In-Memory Database 18c distribution.  
  The distribution on OTN is available at:  
  <https://www.oracle.com/technetwork/database/database-technologies/timesten/downloads/index.html>  
6. Optional but not required is one of either a JRE 8 or JDK 8 distribution, if needed for your applications.  
  The JRE distribution on OTN is available at:  
  <https://docs.oracle.com/javase/8/docs/technotes/guides/install/linux_jre.html>  
  The JDK distribution on OTN is available at:  
  <https://docs.oracle.com/javase/8/docs/technotes/guides/install/linux_jdk.html>  
7. These scripts, available at:  
  <https://github.com/oracle/oracle-timesten-samples>



### An example of using this utility  
* Copy a timesten distribution up to your bootstrap instance.  
 `scp timesten181130.server.linux8664.zip opc@123.123.123.123:`

* ssh into the bootstrap instance as the opc user, e.g  
`% ssh opc@123.123.123.123`

* Install terraform, the OCI Python SDK and CLI, and git.  
`% sudo yum install -y terraform python-oci-sdk python-oci-cli git`

* Create an .oci/config file for terraform to access OCI resources. 
  If you already have a .oci/config file then copy it up to the bootstrap instance.  
  `% scp -qr ~/.oci/config opc@123.123.123.123:.oci`   
  Otherwise generate one with the following.  
  `% oci setup config`  
  You will need to select a region and provide OCIDs for the user and tenancy.  
  For the cloud user, display the public API key and add it to Identity->Users->Add Public Key.  
  `% cat ~/.oci/oci_api_key_public.pem # paste to Identity->Users`  

* Download the scripts https://github.com/oracle/oracle-timesten-samples  
  `git clone https://github.com/oracle/oracle-timesten-samples`  

* Change to the ottscaleout directory  
  `% cd oracle-timesten-samples/cloud/ottscaleout`  

* Copy the timesten distribution into service/packages  
`% cp ~/timesten181130.server.linux8664.zip service/packages`

* Run the _provisionScaleoutOCI_ script from the ottscaleout directory  
`% ./provisionScaleoutOCI` 

### Example session
```
% scp timesten181130.server.linux8664.zip opc@123.123.123.123:
The authenticity of host '123.123.123.123 (123.123.123.123)' can't be established.
ECDSA key fingerprint is SHA256:...
ECDSA key fingerprint is MD5:...
Are you sure you want to continue connecting (yes/no)? yes
Warning: Permanently added '123.123.123.123' (ECDSA) to the list of known hosts.
% 
% ssh opc@123.123.123.123
Last login: Fri Dec  7 00:00:00 2018 from 8.8.8.8
[opc@bootstrap ~]$ 
[opc@bootstrap ~]$ sudo yum install -y terraform python-oci-sdk python-oci-cli git
Loaded plugins: langpacks, ulninfo
Complete!
[opc@bootstrap ~]$ 
[opc@bootstrap ~]$ oci setup config
    This command provides a walkthrough of creating a valid CLI config file.

    The following links explain where to find the information required by this
    script:

    User OCID and Tenancy OCID:

        https://docs.us-phoenix-1.oraclecloud.com/Content/API/Concepts/apisigningkey.htm#Other

    Region:

        https://docs.us-phoenix-1.oraclecloud.com/Content/General/Concepts/regions.htm

    General config documentation:

        https://docs.us-phoenix-1.oraclecloud.com/Content/API/Concepts/sdkconfig.htm


Enter a location for your config [/home/opc/.oci/config]: 
Enter a user OCID: ocid1.user.oc1.....
Enter a tenancy OCID: ocid1.tenancy.oc1.....
Enter a region (e.g. eu-frankfurt-1, uk-london-1, us-ashburn-1, us-phoenix-1): us-ashburn-1
Do you want to generate a new RSA key pair? (If you decline you will be asked to supply the path to an existing key.) [Y/n]: Y
Enter a directory for your keys to be created [/home/opc/.oci]: 
Enter a name for your key [oci_api_key]: 
Public key written to: /home/opc/.oci/oci_api_key_public.pem
Enter a passphrase for your private key (empty for no passphrase): 
Private key written to: /home/opc/.oci/oci_api_key.pem
Fingerprint: 1a:1a:1a:1a:...
Config written to /home/opc/.oci/config


    If you haven't already uploaded your public key through the console,
    follow the instructions on the page linked below in the section 'How to
    upload the public key':

        https://docs.us-phoenix-1.oraclecloud.com/Content/API/Concepts/apisigningkey.htm#How2


[opc@bootstrap ~]$ cat ~/.oci/oci_api_key_public.pem # paste to Identity->Users
-----BEGIN PUBLIC KEY-----
XYZ...
...
-----END PUBLIC KEY-----
[opc@bootstrap ~]$ 
[opc@bootstrap ~]$ git clone https://github.com/oracle/oracle-timesten-samples  
Cloning into 'oracle-timesten-samples'...
remote: Enumerating objects: nnn, done.
remote: Counting objects: 100%, done.
remote: Compressing objects: 100%, done.
remote: Total  (delta ), reused  (delta ), pack-reused 
Receiving objects: 100% (/),  MiB |  MiB/s, done.
Resolving deltas: 100% (/), done.
[opc@bootstrap ~]$ 
[opc@bootstrap ~]$ cd oracle-timesten-samples/cloud/ottscaleout
[opc@bootstrap ottscaleout]$
[opc@bootstrap ottscaleout]$ ./provisionScaleoutOCI

Checking OCI API configuration ...

Checking SSH configuration

Checking TimesTen scripts ...
- OK : scripts version v3_180123_18.1.2.0.0 installed.

Checking Terraform installation ...
- OK : Terraform version 0.11.11 in /usr/bin/terraform


Are you using an OCI trial or pay-as-you-go account ? [ no ]

What would you like to name this service : [ ttimdb1 ]

Service name set to 'ttimdb1'

Of the following 3 compartments :

1. dev1
2. dev2
3. QA1

Which compartment would you like to use ? [ 1 ] 2

Compartment set to 'dev2'

Of the following 4 regions :

1. eu-frankfurt-1
2. us-ashburn-1
3. uk-london-1
4. us-phoenix-1

Which region would you like to use ? [ 2 ]

Region set to 'us-ashburn-1'

Please select one of the following 'High Availability' options :

1. Best performance  - all data instances in one 'Availability Domain'
2. Best availability - data instances distributed across 'Availability Domains'

Which option would you like to choose ? [ 1 ]

Data instances will NOT span 'Availability Domains'

Which 'Availability Domain' do you want to use (1, 2, or 3) ? [ 1 ] 2

Found an existing TimesTen Scaleout distribution :

/home/opc/timesten/ttimdb1/service/packages/timesten181210.server.linux8664.zip

Would you like to use this TimesTen Scaleout distribution ? [ y ]

Number of Replica Sets
----------------------

* There are two data instances per replica set for high availability
* The number of data instances determines the database capacity and SQL parallelism

NOTE: Please check the OCI console's "Governance->Service Limits" page to ensure that
      you have sufficient resources to provision the desired number and type of compute instances.

How many replica sets would you like to create (1 replica set = 2 data instances) ? [ 2 ]

Number of replica sets = 2 (4 data instances)

**** Fast IO [NVMe disk]

Shape                | Memory  | OCPU | DB SIZE
-----------------------------------------------

1. VM.DenseIO2.8     | 120 GB  |    8 |   60 GB
2. VM.DenseIO2.16    | 240 GB  |   16 |  120 GB
3. VM.DenseIO2.24    | 320 GB  |   24 |  160 GB
4. BM.DenseIO2.52    | 768 GB  |   52 |  384 GB

**** Standard IO [Block Storage]

Shape                | Memory  | OCPU | DB SIZE
-----------------------------------------------

5. VM.Standard2.2    | 30 GB   |    2 |   15 GB
6. VM.Standard2.4    | 60 GB   |    4 |   30 GB

NOTE: Please check the OCI console's "Governance->Service Limits" or "Tenancy->Service Limits" page
      to ensure that you have sufficient resources to provision the desired VM shape.


Choose the shape for data instances [ 1 ] 5

Data instances will use shape : VM.Standard2.2

Provide the block volume size in GB (more than 50GB, less than 5000GB) ? [ 90 ]

Block volume size will be 90GB

-- SUMMARY --

Service Name                          ttimdb1                            
Scaleout Install                      timesten181210.server.linux8664.zip    
Compartment                           dev2                                   
Region                                us-ashburn-1                           
OS Image                              Oracle-Linux-7.6-2019.01.17-0          
Data Instance Shape                   VM.Standard2.2                         
Span Availability Domains             No (AD-2)                              
Database Size                         15GB                                   
Block volume size                     90GB                                   
Number of Replica Sets                2                                      
Number of Data Instances              4                                      
TOTAL NUMBER OF COMPUTE INSTANCES     5 (4 data + 1 bastion)                 

Proceed ? [ y ] y

Running Terraform ...
- 'terraform init' :  OK (/home/opc/timesten/ttimdb1/init.out)
- 'terraform plan' :  OK (/home/opc/timesten/ttimdb1/plan.out)
- 'terraform apply':  OK (/home/opc/timesten/ttimdb1/apply.out)

Terraform runtime was 0:04:02

Running Ansible scripts on remote host (/home/opc/timesten/ttimdb1/ansible.out)

This step may take longer than 10 minutes.

OK (/home/opc/timesten/ttimdb1/ansible.out)

Ansible runtime was 0:08:52

########################################
# Bastion Hosts
########################################

Hostname            Public IP        Private IP       Shape         
-----------------------------------  ---------------  ---------------
ttimdb1-bs-001            1.2.3.4    172.16.0.2       VM.Standard2.1

########################################
# Management Instances
########################################

Hostname            Public IP        Private IP       Shape         
-----------------------------------  ---------------  ---------------
ttimdb1-di-001                   

Total provisioning time was 0:12:54

### Example of accessing the provisioned resources.

[opc@bootstrap ottscaleout]$ 
[opc@bootstrap ottscaleout]$ cd ~/timesten/ttimdb1
[opc@bootstrap ttimdb1]$ ls
ansible.out  env-vars.orig  plan.out              public.tf     service            variables.tf
apply.out    init.out       private.tf            README.md     system-config.tf   variables.tf.orig
env-vars     outputs.tf     provisionScaleoutOCI  scaleout.out  terraform.tfstate
[opc@bootstrap ttimdb1]$ 
[opc@bootstrap ttimdb1]$ terraform output
InstanceIPAddresses = [
    bastion host instances (public addresses): 
    ssh opc@1.2.3.4
    ,
    database [mgmt|zookeeper] hosts (private addresses):
    ttimdb1-di-001 172.16.48.2
    ttimdb1-di-002 172.16.64.2
    ttimdb1-di-003 172.16.48.3
    ttimdb1-di-004 172.16.64.3
    ,
    client host instances (private addresses):
    
]
[opc@bootstrap ttimdb1]$ ssh opc@1.2.3.4
Last login: Wed Dec  5 20:42:05 2018 from 123.123.123.123
[opc@ttimdb1-bs-001 ~]$
[opc@ttimdb1-bs-001 ~]$ ssh -tt ttimdb1-di-001 sudo su - oracle
[oracle@ttimdb1-di-001 ~]$ 
[oracle@ttimdb1-di-001 ~]$ /u10/TimesTen/ttimdb1/instance1/bin/ttenv ttisql dsn=ttimdb1
...
connect "dsn=ttimdb1";
...
Command> 
...
```
###

See the accompanying [README.md](./README.md) file for more information on using the provisioned resources and TimesTen Scaleout database.
