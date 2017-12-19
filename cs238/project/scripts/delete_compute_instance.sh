#!/bin/bash

DIR="`dirname \"$0\"`"
source $DIR/../config/gcloud

gcloud compute instances delete --zone $ZONE $1