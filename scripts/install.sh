#!/bin/bash

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# Load UIO module
modprobe uio
if [ ! $? == 0 ]; then
  echo "Error: UIO kernel module did not load properly."
  echo " FAILED"
  exit 1
fi

# Load xdma_uio_kdrv
insmod ../xdma_uio.ko
if [ ! $? == 0 ]; then
  echo "Error: xdma_uio kernel module did not load properly."
  echo "Check if xdma_uio.ko is built with correct version of kernel headers"
  exit 1
fi

# Add new VID PID to xdma_uio
echo "10ee 7024" > /sys/bus/pci/drivers/xdma_uio/new_id


