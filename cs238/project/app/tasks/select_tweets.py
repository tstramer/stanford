import json
from services.flask_app import app, db
from components.tweetsearch.search_tweets import *
from model.tweet import Tweet
from .tweet_action import tweet_action
import twitter
from config import CONFIG, USERS_CREDS
import time

api = twitter.Api(consumer_key=CONFIG["twitter"]["CONSUMER_KEY"],
                  consumer_secret=CONFIG["twitter"]["CONSUMER_SECRET"],
                  access_token_key=CONFIG["twitter"]["ACCESS_TOKEN_KEY"],
                  access_token_secret=CONFIG["twitter"]["ACCESS_TOKEN_SECRET"])

MIN_RETWEETS_POPULAR = 20
MAX_SEARCH_TWEETS = 100
MAX_HOME_TIMELINE_TWEETS = 200
MAX_TWEETS_TO_PROCESS = 4
MIN_TIME_SECS_TO_RESET_COUNT = 320
SENTIMENT_SCORE_THRESH = -.75

def select_tweets_popular():
    term = create_query(load_whitelist(POLITICAL_WHITELIST), min_retweets = MIN_RETWEETS_POPULAR)
    results = api.GetSearch(term, count = MAX_SEARCH_TWEETS, result_type = "recent")
    process_tweets([r.AsDict() for r in results])

def select_tweets_recent():
    process_tweets(search_tweets(api, load_whitelist(POLITICAL_WHITELIST)))

def select_tweets_home_timeline():
    api_user = twitter.Api(consumer_key=CONFIG["twitter"]["CONSUMER_KEY"],
                  consumer_secret=CONFIG["twitter"]["CONSUMER_SECRET"],
                  access_token_key=USERS_CREDS[0]["key"],
                  access_token_secret=USERS_CREDS[0]["secret"])
    results = api_user.GetHomeTimeline(count=MAX_HOME_TIMELINE_TWEETS, include_entities=True)
    tweets = [r.AsDict() for r in results]

    whitelist = load_whitelist(POLITICAL_WHITELIST)
    popular_retweets = search_popular_htl_retweets(results, whitelist)
    
    process_tweets(popular_retweets, filter = lambda a, b: True) # override negative sentiment filter
    process_tweets(tweets)

def tweet_filter(tweet, sentiment_score_thresh):
     # is_reply(tweet) and \
    return (not is_retweet(tweet)) and \
                 ("followers_count" in tweet["user"] and tweet["user"]["followers_count"] > 800) and \
                 (is_hatespeech(tweet) or has_negative_sentiment(tweet, sentiment_score_thresh))

def process_tweets(tweets, sentiment_score_thresh = SENTIMENT_SCORE_THRESH, max = MAX_TWEETS_TO_PROCESS, filter = tweet_filter):
    count = 0
    start = time.time()
    for tweet in tweets:
        if count >= max:
            end = time.time()
            if (end - start) >= MIN_TIME_SECS_TO_RESET_COUNT:
                print ("Reseting count")
                count = 0
                start = time.time()
        if already_processed_tweet(tweet):
            print ("Already processed: {sn}: {text}".format(sn = tweet["user"]["screen_name"], text = tweet["text"]))
        elif filter(tweet, sentiment_score_thresh) and count <= max:
            count = count + 1
            tweet_action(tweet)
            print ("Processing: {sn}: {text}".format(sn = tweet["user"]["screen_name"], text = tweet["text"]))

# TODO: Batch version
def already_processed_tweet(tweet):
    q = db.session.query(Tweet).filter(Tweet.tweet_id==tweet["id"])
    return db.session.query(q.exists()).scalar()

