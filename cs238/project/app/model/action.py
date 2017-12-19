from services.flask_app import db
from datetime import datetime

class Action(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.BigInteger)
    tweet_id = db.Column(db.BigInteger)
    in_reply_to_tweet_id = db.Column(db.BigInteger)
    action = db.Column(db.String)
    timestamp = db.Column(db.DateTime)

    def __init__(self, user_id, tweet_id, in_reply_to_tweet_id, action):
        self.user_id = user_id
        self.tweet_id = tweet_id
        self.in_reply_to_tweet_id = in_reply_to_tweet_id
        self.action = action
        self.timestamp = datetime.now()

    def __repr__(self):
        return ('user_id={user_id}, tweet_id={tweet_id}, action={action}'.format(
                    user_id = self.user_id,
                    tweet_id = self.tweet_id,
                    action = self.action))