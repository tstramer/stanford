#!/bin/bash

DIR="`dirname \"$0\"`"
source $DIR/../config/gcloud

set -v

# create the instance
gcloud compute instances create $1 \
  --image-project ubuntu-os-cloud \
  --image-family ubuntu-1404-lts \
  --machine-type f1-micro \
  --scopes userinfo-email,cloud-platform \
  --metadata-from-file startup-script=$DIR/startup_compute_instance.sh \
  --zone $ZONE \
  --tags http-server,https-server
