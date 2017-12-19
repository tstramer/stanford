from services.flask_app import app, db
from config import CONFIG, USERS_CREDS
import random
from components.tweetactions.tweet_actions import MARKOV_BOT_ACTIONS, QUOTE_TWEET_ACTIONS
import twitter
import os
from components.tweetsearch.search_tweets import *

TWEET_ACTIONS = MARKOV_BOT_ACTIONS + QUOTE_TWEET_ACTIONS

MAX_HOME_TIMELINE_TWEETS = 200
RETWEET_PROBABILITY = .5

def select_distractor_tweet(user_creds, rt_prob = RETWEET_PROBABILITY):
    api = twitter_api_for(user_creds)
    if random.uniform(0, 1) <= rt_prob:
        if not post_distractor_retweet(api):
            post_distractor_tweet(api)
    else:
        post_distractor_tweet(api)

def post_distractor_tweet(api):
    tweet_text = get_tweet_text()
    print ("post_distractor_tweet: {0}".format(tweet_text))
    api.PostUpdate(tweet_text)

def post_distractor_retweet(api):
    results = api.GetHomeTimeline(count=MAX_HOME_TIMELINE_TWEETS, include_entities=True)
    whitelist = load_whitelist(POLITICAL_WHITELIST)
    popular_retweets = search_popular_htl_retweets(results, whitelist)
    if len(popular_retweets) > 0:
        try:
            api.PostRetweet(popular_retweets[0]["id"])
            return True
        except:
            return False
    else:
        return False

def get_tweet_text():
    action = random.choice(TWEET_ACTIONS)
    return action.get_tweet_text({})

def twitter_api_for(user_creds):
    return twitter.Api(consumer_key=CONFIG["twitter"]["CONSUMER_KEY"],
                       consumer_secret=CONFIG["twitter"]["CONSUMER_SECRET"],
                       access_token_key=user_creds["key"],
                       access_token_secret=user_creds["secret"])