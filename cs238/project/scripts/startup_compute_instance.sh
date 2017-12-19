#!/bin/bash

PROJECT_ID="heartroll-3"
DB_CONNECTION_NAME="$PROJECT_ID:us-central1:datastore5"

set -v

# Install logging monitor. The monitor will automatically pickup logs sent to syslog
curl -s "https://storage.googleapis.com/signals-agents/logging/google-fluentd-install.sh" | bash
service google-fluentd restart &

# Install dependencies
apt-get update
apt-get install -yq git python3 python-dev python3-pip python-virtualenv build-essential supervisor mysql-client

# pip from apt is out of date, so make it update itself and install virtualenv.
pip3 install --upgrade pip virtualenv

# download & start the cloud sql proxy
wget https://dl.google.com/cloudsql/cloud_sql_proxy.linux.amd64
mv cloud_sql_proxy.linux.amd64 cloud_sql_proxy
chmod +x cloud_sql_proxy
./cloud_sql_proxy -instances=$DB_CONNECTION_NAME=tcp:3306 &

# create the app database
# TODO: Make this less hacky... it's going to fail after the first time
echo "CREATE DATABASE app" | mysql --user=root --password=root --host=127.0.0.1

# Get the source code from the Google Cloud Repository
# git requires $HOME and it's not set during the startup script.
export HOME=/root
git config --global credential.helper gcloud.sh
rm -rf /opt/app
git clone https://source.developers.google.com/p/$PROJECT_ID /opt/app

cd /opt/app/src
pip3 install -r requirements.txt