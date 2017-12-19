#!/bin/bash

DIR="`dirname \"$0\"`"
source $DIR/../config/gcloud

gcloud beta sql instances delete $DB_INSTANCE_NAME