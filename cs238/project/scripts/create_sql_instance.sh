#!/bin/bash

DIR="`dirname \"$0\"`"
source $DIR/../config/gcloud

set -v

gcloud beta sql instances create $DB_INSTANCE_NAME \
    --tier=db-f1-micro \
    --region=$REGION1 \
    --activation-policy=ALWAYS
  
gcloud sql instances set-root-password $DB_INSTANCE_NAME --password=$DB_PASSWORD