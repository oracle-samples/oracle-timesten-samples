# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
# variables for terraform, ansible
# configure 
#  bastion hosts, mgmt instances
#  db connection attributes
#  os images  
#  network parameters, storage
#

### Instance configuration
# Creating an NxK TimesTen Scaleout Cluster, where K=2
# Creates VCN, subnets, compute resources, Object Storage bucket
# the N in NxK
variable  "diInstanceCount" { default = "2" }

# Compute instance shape for data instances
# N*K VMs/BMs are provisioned for data instances
# Must use NVMe shape (DenseIO, HighIO)
variable  "diInstanceShape" { default = "VM.DenseIO1.4" }

# ZooKeeper is allocated on zkInstanceCount VMs
# To co-locate Zk VMs with mgmt/data VMs set zkInstanceCount=0
# Otherwise set zkInstanceCount=3 for stand-alone VMs.
variable  "zkInstanceCount" { default = "0" }
variable  "zkInstanceShape" { default = "VM.Standard1.1" }

# Number of hosts for mgmt instances {0|2}
# 0 = co-located with data instance VMs
variable  "mgInstanceCount" { default = "0" }
variable  "mgInstanceShape" { default = "VM.Standard1.1" }

# Number and shape of Bastion hosts {1|2|3}
variable  "bsInstanceCount" { default = "1" }
variable  "bsInstanceShape" { default = "VM.Standard1.1" }

### Which AD(s)
# Compute instances provisioned round robin to AD mod ksafety
# For example, to start provisioning in AD3, inital_AD=3
# Valid values are 1-3 otherwise 1 is used
variable  "initial_AD" { default = "1" }

# What OS Image to use
variable "InstanceImageOCID" {
  type = "map"
  default = {
    #  Oracle-Linux-7.5-2018.06.14-0
    // See https://docs.us-phoenix-1.oraclecloud.com/images/
    "us-phoenix-1"   = "ocid1.image.oc1.phx.aaaaaaaaxyc7rpmh3v4yyuxcdjndofxuuus4iwd7a7wjc63u2ykycojr5djq"
    "us-ashburn-1"   = "ocid1.image.oc1.iad.aaaaaaaazq7xlunevyn3cf4wppcx2j53eb26pnnc4ukqtfj4tbjjcklnhpaa"
    "eu-frankfurt-1" = "ocid1.image.oc1.eu-frankfurt-1.aaaaaaaa7qdjjqlvryzxx4i2zs5si53edgmwr2ldn22whv5wv34fc3sdsova"
    "uk-london-1"    = "ocid1.image.oc1.uk-london-1.aaaaaaaas5vonrmseff5fljdmpffffqotcqdrxkbsctotrmqfrnbjd6wwsfq"
  }
}

# Variables after this point written to ansible vars file
#
# Networking
# VCN addressing
# 
variable "network" {
  type = "map"
  default = {
    "cidr"          = "172.16.0.0/16"
    "cidr_prefix"   = "172.16"
    # Subnets ${cidr_prefix}.X.0/${subnet_mask}, X=1..3,11..13
    "public_octet"  = "1"
    "private_octet" = "11"
    "subnet_mask"   = "24"
  }
}

# Oracle Timesten Scaleout
variable "timesten" {
  type = "map"
  default = {
    "databasecharacterset" = "AL32UTF8"
    "durability"           =  0
    "permsize"             =  4096
    "tempsize"             =  400
    "restarttimeout"       =  ""
    "stoptimeout"          =  300
    "mgmtdaemonport"       =  6624
    "mgmtcsport"           =  6625
    "mgmtreplport"         =  3754
    "ephemeral"            = "32768-61000"
    "dsdaemonport"         =  46464
    "dscsport"             =  46465
  }
}

variable "java" {
  type = "map"
  default = {
    "javabase" = "/opt"
  }
}

# Storage Configuration
# Uses no more than 4 devices
# LVM only or mdraid (mdadm)
# LVM-RAID-0, MD-RAID-10
# Creating a file system on mdraid takes 10-30 minutes
# Default is RAID0 (striped) using LVM
# 
variable "system" {
  type = "map"
  default = {
    "fsname"     =      "/u10"
    "storage"    =      "LVM-RAID-0"
  }
}

# opc
variable "opc" {
  type = "map"
  default = {
    opchome   = "/home/opc"
    scriptdir = "service/scripts"
    securityupdates = "false"
  }
}

# Begin section of: do not modify here variables
# Begin Defined in env-vars
# oci 
variable "tenancy_ocid" {}
variable "user_ocid" {}
variable "fingerprint" {}
variable "private_key_path" {}
variable "compartment_ocid" {}
variable "region" {}
# keys
variable "ssh_public_key" {}
variable "ssh_private_key" {}
# name
variable "service_name" {}
# end Defined in env-vars
# end do not modify here
