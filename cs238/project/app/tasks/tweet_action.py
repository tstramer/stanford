from config import CONFIG, USERS_CREDS
import random
from components.tweetactions.tweet_actions import TWEET_ACTIONS
import twitter
import time
from apscheduler.schedulers.background import BackgroundScheduler
import datetime
from model.tweet import Tweet
from model.action import Action
import os
from services.flask_app import app, db
from services.q_table import get_q_table
from learning.user_convo_state import UserConvoState 
from learning.user_convo_action import UserConvoAction 
from components.sentiment.vaderSentiment import sentiment

sched = BackgroundScheduler()

REPLY_DELAY_SECONDS = 60
FAVE_DELAY_SECONDS = 20
FOLLOW_DELAY_SECONDS = 40

all_states = UserConvoState.all_states()
all_actions = UserConvoAction.all_actions()

RANDOM_ACTION_PROB = .6

def tweet_action(tweet):
    user_creds = random.choice(USERS_CREDS)
    api = twitter_api_for(user_creds)

    screen_name = tweet["user"]["screen_name"]
    action = get_action(tweet)
    tweet_text = "@" + screen_name + " " + action.get_tweet_text(tweet)

    user_id = user_creds["id"]
    in_reply_to_tweet_id = tweet["id"]
    in_reply_to_user_id = tweet["user"]["id"]

    db.session.add(Tweet(tweet))
    db.session.commit()

    sched_favorite(api, in_reply_to_tweet_id)
    sched_follow(api, in_reply_to_user_id)
    sched_post_tweet(api, action, tweet_text, user_id, in_reply_to_tweet_id)

def sched_favorite(api, tweet_id, fave_prob=.8):
    if random.uniform(0, 1) <= fave_prob:
        sched.add_job(
            create_favorite, 
            args=[api, tweet_id],
            next_run_time = datetime.datetime.now() + datetime.timedelta(seconds=FAVE_DELAY_SECONDS)
        )

def sched_follow(api, user_id):
    sched.add_job(
        follow_user, 
        args=[api, user_id],
        next_run_time = datetime.datetime.now() + datetime.timedelta(seconds=FOLLOW_DELAY_SECONDS)
    )

def sched_post_tweet(api, action, tweet_text, user_id, in_reply_to_tweet_id):
    sched.add_job(
        post_tweet,
        args = [api, action, tweet_text, user_id, in_reply_to_tweet_id],
        next_run_time = datetime.datetime.now() + datetime.timedelta(seconds=REPLY_DELAY_SECONDS)
    )
    try:
        sched.start()
    except:
        print ("Already started scheduler")

def post_tweet(api, action, tweet_text, user_id, in_reply_to_tweet_id):
    print ("post_tweet: {0}".format(tweet_text))
    tweet = api.PostUpdate(tweet_text, in_reply_to_status_id=in_reply_to_tweet_id)
    db.session.add(Action(user_id, tweet.id, in_reply_to_tweet_id, action.name))
    db.session.commit()

def create_favorite(api, tweet_id):
    api.CreateFavorite(status_id = tweet_id)

def follow_user(api, user_id):
    api.CreateFriendship(user_id = user_id)

def get_action(tweet):
    Q = get_q_table()

    if random.uniform(0, 1) <= RANDOM_ACTION_PROB or len(Q) == 0:
        print ("Selecting random action")
        return random.choice(TWEET_ACTIONS)
    else:
        print ("Selecting optimal action")

        state = UserConvoState(
            tweet["user"]["statuses_count"],
            tweet["user"]["followers_count"],
            sentiment(tweet["text"])["compound"],
            # TODO: Fix these for multi-tweet convos
            tweet["text"].startswith("@"),
            1
        ).to_array()

        state_idx = all_states.index(state)
        best_action_idx = 0
        best_q_score = 0
        for action_idx in range(0, len(TWEET_ACTIONS)):
            if Q[state_idx, action_idx] >= best_q_score:
                best_action_idx = action_idx
                best_q_score = Q[state_idx, action_idx]
        return TWEET_ACTIONS[best_action_idx]

def twitter_api_for(user_creds):
    return twitter.Api(consumer_key=CONFIG["twitter"]["CONSUMER_KEY"],
                       consumer_secret=CONFIG["twitter"]["CONSUMER_SECRET"],
                       access_token_key=user_creds["key"],
                       access_token_secret=user_creds["secret"])