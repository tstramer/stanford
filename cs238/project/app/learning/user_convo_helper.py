import json
from components.sentiment.vaderSentiment import sentiment
from .user_convo_state import UserConvoState
from .user_convo_action import UserConvoAction
import copy

def get_state_and_action_and_next_states(action_records, tweet_records, user_event_records):
    tweet_id_to_tweet = get_tweet_id_to_tweet(tweet_records, user_event_records)

    self_tweets = get_self_tweets(tweet_records, user_event_records)
    in_reply_to_chains = get_in_reply_to_chains(tweet_id_to_tweet)
    self_tweet_ids = set([tweet["id"] for tweet in self_tweets])

    tweet_id_to_faves = get_tweet_id_to_faves(user_event_records)
    tweet_id_to_rts = get_tweet_id_to_rts(user_event_records)
    tweet_id_to_quotes = get_tweet_id_to_quotes(user_event_records)
    tweet_id_to_replies = get_tweet_id_to_replies(user_event_records)
    
    follower_ids = get_follower_user_ids(user_event_records)

    state_and_action_and_next_states = []

    for action in action_records:
        tweet_id = action.tweet_id
        in_reply_to_tweet_id = action.in_reply_to_tweet_id
        
        in_reply_to_tweet = tweet_id_to_tweet[in_reply_to_tweet_id]
        in_reply_to_user = in_reply_to_tweet["user"]

        in_reply_tos = in_reply_to_chains.get(tweet_id, [])
        curr_self_tweet_ids = [tweet_id] + [tid for tid in in_reply_tos if tid in self_tweet_ids]
        curr_self_tweet_ids.sort()
        first_in_reply_to = tweet_id_to_tweet[tweet_id_to_tweet[curr_self_tweet_ids[0]]["in_reply_to_status_id"]]

        faves = tweet_id_to_faves.get(tweet_id, set())
        rts = tweet_id_to_rts.get(tweet_id, set())
        quotes = tweet_id_to_quotes.get(tweet_id, set())
        replies = tweet_id_to_replies.get(tweet_id, set())
        is_following = in_reply_to_user["id"] in follower_ids

        other_user_num_followers = in_reply_to_user["followers_count"] 
        other_user_num_statuses = in_reply_to_user["statuses_count"] 
        first_other_tweet_is_reply = first_in_reply_to["text"].startswith("@")
        convo_num_self_tweets = len(curr_self_tweet_ids)

        last_other_tweet_sentiment = sentiment(in_reply_to_tweet["text"])["compound"]
        reply_sentiment = 0
        if len(replies) > 0:
            reply_sentiment = sentiment(tweet_id_to_tweet[list(replies)[0]]["text"])["compound"]

        state = UserConvoState(
            other_user_num_followers, 
            other_user_num_statuses,
            last_other_tweet_sentiment,
            first_other_tweet_is_reply,
            convo_num_self_tweets
        )

        next_state = UserConvoState(
            other_user_num_followers, 
            other_user_num_statuses,
            reply_sentiment,
            first_other_tweet_is_reply,
            convo_num_self_tweets + 1
        )

        action = UserConvoAction(
            action, 
            len(faves), 
            len(rts),
            len(replies),
            reply_sentiment, 
            len(quotes), 
            is_following
        )

        state_and_action_and_next_states.append((state, action, next_state))

    return state_and_action_and_next_states

def get_self_tweets(tweets, user_events):
    self_tweet_ues = [ue for ue in user_events if ue.event == 'self_tweet']
    return [json.loads(ue.target_object) for ue in self_tweet_ues]

def get_tweet_id_to_tweet(tweet_records, user_event_records):
    tweets = [json.loads(tweet.json_metadata) for tweet in tweet_records]
    ue_targets = [json.loads(ue.target_object) for ue in user_event_records]
    tweet_ues = [ue for ue in ue_targets if "text" in ue]
    tweet_id_to_tweet = {}
    for tweet in (tweets + tweet_ues):
        tweet_id_to_tweet[tweet['id']] = tweet
    return tweet_id_to_tweet

def get_in_reply_to_chains(tweet_id_to_tweet):
    in_reply_to_chains_set = {}
    for (tweet_id, tweet) in tweet_id_to_tweet.items():
        all_in_reply_tos = set()
        curr_tweet = tweet
        while True:
            if not "in_reply_to_status_id" in curr_tweet:
                break
            in_reply_to = curr_tweet["in_reply_to_status_id"]
            if in_reply_to == None or (not in_reply_to in tweet_id_to_tweet):
                break
            curr_tweet = tweet_id_to_tweet[in_reply_to]
            all_in_reply_tos.add(in_reply_to)
        in_reply_to_chains_set[tweet_id] = all_in_reply_tos

    in_reply_to_chains = {}
    for (tweet_id, replies) in in_reply_to_chains_set.items():
        in_reply_to_chains[tweet_id] = list(replies)
        in_reply_to_chains[tweet_id].sort()
    return in_reply_to_chains

def get_tweet_id_to_faves(user_events):
    return get_tweet_id_to_engagement(user_events, 'favorite')

def get_tweet_id_to_replies(user_events):
    reply_ues = [ue for ue in user_events if ue.event == 'other_reply']
    tweet_id_to_replies = {}
    for reply_ue in reply_ues:
        tweet = json.loads(reply_ue.target_object)
        if not tweet["in_reply_to_status_id"] in tweet_id_to_replies:
            tweet_id_to_replies[tweet["in_reply_to_status_id"]] = set()
        tweet_id_to_replies[tweet["in_reply_to_status_id"]].add(tweet["id"])
    return tweet_id_to_replies

    return get_tweet_id_to_engagement(user_events, 'other_reply')

def get_tweet_id_to_quotes(user_events):
    quotetweet_ues = [u for u in user_events if u.event == "quoted_tweet"]
    tweet_id_to_quote = {}
    for quotetweet_ue in quotetweet_ues:
        user = json.loads(quotetweet_ue.source)
        tweet = json.loads(quotetweet_ue.target_object)
        if "quoted_status" in tweet:
            quoted_tweet = tweet["quoted_status"]
            if not quoted_tweet["id"] in tweet_id_to_quote:
                tweet_id_to_quote[quoted_tweet["id"]] = set()
            tweet_id_to_quote[quoted_tweet["id"]].add(user["id"])
    return tweet_id_to_quote

def get_tweet_id_to_rts(user_events):
    othertweet_ues = [u for u in user_events if u.event == "other_tweet"]
    tweet_id_to_rts = {}
    for othertweet_ue in othertweet_ues:
        user = json.loads(othertweet_ue.source)
        tweet = json.loads(othertweet_ue.target_object)
        if "retweeted_status" in tweet:
            retweet = tweet["retweeted_status"]
            if not retweet["id"] in tweet_id_to_rts:
                tweet_id_to_rts[retweet["id"]] = set()
            tweet_id_to_rts[retweet["id"]].add(user["id"])
    return tweet_id_to_rts

def get_tweet_id_to_engagement(user_events, engagement_event):
    engagement_ues = [ue for ue in user_events if ue.event == engagement_event]
    tweet_id_to_engagements = {}
    for engagement_ue in engagement_ues:
        user = json.loads(engagement_ue.source)
        tweet = json.loads(engagement_ue.target_object)
        if not tweet["id"] in tweet_id_to_engagements:
            tweet_id_to_engagements[tweet["id"]] = set()
        tweet_id_to_engagements[tweet["id"]].add(user["id"])
    return tweet_id_to_engagements

def get_follower_user_ids(user_events):
    follower_user_ids = set()
    follow_ues = [ue for ue in user_events if ue.event == 'follow']
    for follow_ue in follow_ues:
        user = json.loads(follow_ue.source)
        follower_user_ids.add(user["id"])
    return follower_user_ids