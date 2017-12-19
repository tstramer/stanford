#!/bin/bash

DIR="`dirname \"$0\"`"
source $DIR/../config/gcloud

gcloud compute ssh --zone $ZONE $1