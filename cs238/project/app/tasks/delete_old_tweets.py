import json
from services.flask_app import app, db
from components.tweetsearch.search_tweets import *
from model.tweet import Tweet
from .tweet_action import tweet_action
import twitter
from config import CONFIG
import time
import datetime

MAX_USER_TIMELINE_TWEETS = 200

def delete_old_replies(user_creds, max_reply_age):
    api = twitter.Api(consumer_key=CONFIG["twitter"]["CONSUMER_KEY"],
                  consumer_secret=CONFIG["twitter"]["CONSUMER_SECRET"],
                  access_token_key=user_creds["key"],
                  access_token_secret=user_creds["secret"])
    tweets = api.GetUserTimeline(user_id = user_creds["id"], count=MAX_USER_TIMELINE_TWEETS)
    num_deleted = 0
    for tweet in tweets:
        if tweet.text.startswith("@") and tweet.retweeted_status == None:
            timestamp = datetime.datetime.fromtimestamp(tweet.created_at_in_seconds)
            if timestamp < datetime.datetime.now()-max_reply_age:
                num_deleted = num_deleted + 1
                api.DestroyStatus(tweet.id)
                print ("Deleting tweet id: {0}".format(tweet.id))
    print ("Deleted {0} of {1} tweets fetched (max {2})".format(num_deleted, len(tweets), MAX_USER_TIMELINE_TWEETS))