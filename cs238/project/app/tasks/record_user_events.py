import json
from services.flask_app import app, db
import twitter
from config import CONFIG
from model.user_event import UserEvent
from .tweet_action import tweet_action, sched_favorite
import random

def record_user_events(user_creds):
    api = twitter.Api(consumer_key=CONFIG["twitter"]["CONSUMER_KEY"],
                      consumer_secret=CONFIG["twitter"]["CONSUMER_SECRET"],
                      access_token_key=user_creds["key"],
                      access_token_secret=user_creds["secret"])
    for user_event in api.GetUserStream():
        user_event_db = UserEvent(user_creds["id"], user_event)
        print ("user event: {0}".format(user_event_db.event))
        if user_event_db.event != "unknown":
            db.session.add(user_event_db)
            db.session.commit()
            if user_event_db.event == "other_reply" and random.uniform(0,1) < .75:
                tweet_action(user_event)