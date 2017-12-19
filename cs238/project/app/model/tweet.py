from services.flask_app import db
import json
from datetime import datetime
from dateutil import parser

class Tweet(db.Model):
    tweet_id = db.Column(db.BigInteger, primary_key=True)
    user_id = db.Column(db.BigInteger)
    timestamp = db.Column(db.DateTime)
    json_metadata = db.Column(db.Text())

    def __init__(self, metadata):
        self.tweet_id = metadata["id"]
        self.user_id = metadata["user"]["id"]
        self.json_metadata = json.dumps(metadata)
        if "created_at" in metadata:
            self.timestamp = parser.parse(metadata["created_at"])
        elif "timestamp_ms" in metadata:
            self.timestamp = datetime.fromtimestamp(int(metadata["timestamp_ms"])//1000.0)
        else:
            self.timestamp = datetime.now()
            print ("Unknown timestamp for {metadata}".format(
                metadata = json.dumps(metadata))
            )

    def __repr__(self):
        metadata = self.get_metadata()
        return ('{name} ({screen_name}): {text}'.format(
                name = metadata["user"]["name"],
                screen_name = metadata["user"]["screen_name"],
                text = metadata["text"].replace('\n', '. ')))

    def get_metadata(self):
        return json.loads(self.json_metadata)