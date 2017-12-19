import os
import json

CONFIG = {}
USERS_CREDS = {}
CONFIG_NAMES = ['gcloud', 'twitter']

# Load configuration files (in $ROOT/config) into a dictionary
dir = os.path.dirname(__file__)
for config_name in CONFIG_NAMES:
	CONFIG[config_name] = {}
	for line in open(os.path.join(dir, '../config/', config_name)):
		parts = line.rstrip('\n').partition("=")
		CONFIG[config_name][parts[0]] = parts[2]

# Load twitter credentials for each bot (access key, secret)
with open(os.path.join(dir, '../config/users_credentials.json')) as creds_file:
	USERS_CREDS = json.load(creds_file)["users"]

# Decide which database to use based on whether DEVEL env var set
DEVEL_SQLALCHEMY_DATABASE_URI = 'sqlite:////tmp/develsql_v10.db'
PROD_SQLALCHEMY_DATABASE_URI = (
    'mysql+pymysql://{user}:{password}@localhost/{database}').format(
        user=CONFIG["gcloud"]["DB_USER"], password=CONFIG["gcloud"]["DB_PASSWORD"],
        database=CONFIG["gcloud"]["DB_DATABASE_NAME"])
if os.environ.get('DEVEL'):
	SQLALCHEMY_DATABASE_URI=DEVEL_SQLALCHEMY_DATABASE_URI
else:
	SQLALCHEMY_DATABASE_URI=PROD_SQLALCHEMY_DATABASE_URI

print (SQLALCHEMY_DATABASE_URI)