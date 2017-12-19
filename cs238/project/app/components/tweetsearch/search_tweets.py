import json
import os
from components.sentiment.vaderSentiment import sentiment, word_valence_dict
from components.hatespeech.app import classify_tweet
import random

POLITICAL_WHITELIST = "political"
PROFANITIES_WHITELIST=  "profanities"
SLURS_WHITELIST = "slurs"

LANGUAGES = ['en']

MAX_WHITELIST_ITEMS = 500
NEGATIVE_SENTIMENT_SCORE_THRESHOLD = -.75 # score between (-1, 1)
HATESPEECH_CONFIDENCE_THRESH = 60

MIN_RETWEETS_HTL=5

def search_tweets(api, whitelist, languages=LANGUAGES):
    for tweet in api.GetStreamFilter(track=whitelist, languages=languages):
        if "text" in tweet:
            yield tweet

def search_popular_htl_retweets(results, whitelist, min_retweets=MIN_RETWEETS_HTL):
    tweets = [result.AsDict() for result in results]
    filtered_tweets = [tweet for tweet in tweets if tweet_whitelist_filter(tweet, whitelist)]
    retweets = [tweet["retweeted_status"] for tweet in tweets if "retweeted_status" in tweet]
    return [rt for rt in retweets if "retweet_count" in rt and rt["retweet_count"] >= min_retweets]

def tweet_whitelist_filter(tweet, whitelist, include_name_and_bio = True):
    text = tweet["text"]
    if include_name_and_bio:
        text = text + " " + tweet["user"]["name"]
        if "description" in tweet["user"]:
            text = text + " " + tweet["user"]["description"]
    text = text.lower()
    for word in whitelist:
        if word.lower() in text:
            return True
    return False

def is_hatespeech(tweet, confidence_threshold = HATESPEECH_CONFIDENCE_THRESH):
    result = classify_tweet(tweet["text"])
    return result == 0 or result == 2 # 0=hate speech, 2=offensive language

def has_negative_sentiment(tweet, score_threshold = NEGATIVE_SENTIMENT_SCORE_THRESHOLD):
    result = sentiment(tweet["text"])
    return result["compound"] < score_threshold

def is_reply(tweet):
    return tweet["in_reply_to_status_id"] != None or tweet["in_reply_to_user_id"] != None

def is_retweet(tweet):
    return tweet["text"].startswith("RT ")

def create_query(terms, min_retweets = 1, min_favorites = 1):
    return "({terms} min_retweets:{min_retweets} min_faves:{min_favorites}".format(
        terms = " OR ".join(["\"{0}\"".format(t) for t in terms]), 
        min_retweets = min_retweets, 
        min_favorites = min_favorites
    )

def load_whitelist(name, limit=MAX_WHITELIST_ITEMS):
    dir = os.path.dirname(__file__)
    whitelist_file = open(os.path.join(dir, name), 'r')
    lines = [w.rstrip('\n').lower() for w in whitelist_file]
    return random.sample(lines, min(len(lines), limit))
