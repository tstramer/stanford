from services.flask_app import db
import json
from datetime import datetime
from dateutil import parser

class UserEvent(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.BigInteger)
    event = db.Column(db.String(80))
    source = db.Column(db.Text())
    target_object = db.Column(db.Text())
    timestamp = db.Column(db.DateTime)

    def __init__(self, user_id, metadata):
        self.user_id = user_id
        if "in_reply_to_status_id" in metadata:
            if metadata["user"]["id"] == user_id: # sent by ourselves
                self.event = "self_tweet"
            elif metadata["in_reply_to_status_id"] != None: # reply from someone else
                self.event = "other_reply"
            else: # other, not sure when this will actually happen...
                self.event = "other_tweet"
            self.source = json.dumps(metadata["user"])
            self.target_object = json.dumps(metadata)
        elif "event" in metadata:
            self.event = metadata["event"]
            self.source = json.dumps(metadata["source"])
            if "target_object" in metadata:
                self.target_object = json.dumps(metadata["target_object"])
            else:
                self.target_object = "{}"
        else:
            # TODO: Scrub tweet deletes
            self.event = "unknown"
            self.source = "{}"
            self.target_object = json.dumps(metadata)
            print ("Unknown user event ({user_id}): {metadata}".format(
                user_id = user_id,
                metadata = json.dumps(metadata)))
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
        source = json.loads(self.source)
        target_object =json.loads(self.target_object)
        target = ""
        if "text" in target:
            target = target_object["text"].replace('\n', '. ')
        elif "screen_name" in target:
            target = "@{0}".format(target_object["screen_name"])
        return ('{event} ({source}): {target}'.format(
                event = self.event,
                source = source["screen_name"],
                target = target))