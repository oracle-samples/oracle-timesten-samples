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
# Creating an NxK TimesTen Scaleout Cluster (default 2x2)
# Creates VCN, subnets, compute resources
# The N in NxK (replica sets)
variable  "diInstanceCount" { default = "2" }

# Compute instance shape for data instances
# N*K VMs/BMs are provisioned for data instances
# Not allowed to use VM.Standard1.1 shape
# Recommended to use NVMe shape (DenseIO, HighIO) for best performance
variable  "diInstanceShape" { default = "VM.DenseIO1.4" }

### K-Safety
# The K in NxK (copies of data)
# Supports {1|2}
# By default, K==2 spans ADs; see singleAD variable below
variable  "ksafety" { default = "2" }

# ZooKeeper is allocated on zkInstanceCount VMs
# To co-locate Zk VMs with mgmt/data VMs set zkInstanceCount=0
# Otherwise set zkInstanceCount=3 for stand-alone VMs
variable  "zkInstanceCount" { default = "0" }
variable  "zkInstanceShape" { default = "VM.Standard1.1" }

# Number of hosts for mgmt instances {0|2}
# 0 = co-located with data instance VMs
variable  "mgInstanceCount" { default = "0" }
variable  "mgInstanceShape" { default = "VM.Standard1.1" }

# Client only installations are allocated on clInstanceCount VMs
# clients are provisioned on private subnet
variable "clInstanceCount" { default = "0" }
variable "clInstanceShape" { default = "VM.Standard1.1" }

# Number and shape of Bastion hosts {1|2|3}
variable  "bsInstanceCount" { default = "1" }
variable  "bsInstanceShape" { default = "VM.Standard1.1" }

### Which AD(s)
# Compute instances provisioned round robin to AD mod ksafety
# To start provisioning in AD3, initialAD=3
# Valid values are 1-3 otherwise 1 is used
variable  "initialAD" { default = "1" }
# Co-locate data spaces and management instances in same AD
# singleAD == { "true" | "false" }
variable  "singleAD"   { default = "false" }

### Block Volumes if any
# One block volume for each data compute instance (NxK)
# To use block volumes, instance shape must be standard
# If both block and nvme devices exist, DB is created on nvme only
#
# Size of block volume in GB to attach to each data compute instance
# diBlockVolumeSizeGB < 50 means do not attach block volumes to any data compute instance
# Minimum allocation is 50 GB
variable "diBlockVolumeSizeGB" { default = "0" }

# What OS Image to use
variable "InstanceImageOCID" {
  type = "map"
  default = {
    #  Oracle-Linux-7.5-2018.09.25-0
    // See https://docs.us-phoenix-1.oraclecloud.com/images/
    "us-phoenix-1"   = "ocid1.image.oc1.phx.aaaaaaaadv2bbpnkivhi2rtfuymu2a4bvaxaj567vqdmjjffrq2vswmyr5qa"
    "us-ashburn-1"   = "ocid1.image.oc1.iad.aaaaaaaaiu73xa6afjzskjwvt3j5shpmboxtlo7yw4xpeqpdz5czpde7px2a"
    "eu-frankfurt-1" = "ocid1.image.oc1.eu-frankfurt-1.aaaaaaaabfxzgyg2gbwxgkfeir3xfqtkdey3g6k6cmri7wuost35vnrmo4zq"
    "uk-london-1"    = "ocid1.image.oc1.uk-london-1.aaaaaaaaynxdelzap4cpziba7ahd2jbfdf5ss4r4wjmqefm6wwftrmn25vbq"
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
    "subnets"       = "4"         
    # logbase2(subnets in network)
    # enables 2^16 - 2^subnets host systems
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
    # entire ephemeral range req'd for channel ports at present
    "chnlportlo"           =  32768
    "chnlporthi"           =  61000
    # daemon and cs ports must be in range chnlportlo-chnlporthi
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
