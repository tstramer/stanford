from enum import Enum

class CountCategory(Enum):
	low = 0
	medium = 1
	high = 2

class SentimentCategory(Enum):
	negative = 0
	neutral = 1
	positive = 2

	@classmethod
	def from_score(cls, score):
		if score < 0:
			return cls.negative
		elif score == 0:
			return cls.neutral
		else:
			return cls.positive

class UserConvoState:
	def __init__(
		self, 
		other_user_num_followers, 
		other_user_num_statuses,
		last_other_tweet_sentiment,
		first_other_tweet_is_reply,
		convo_num_self_tweets
	):
		self.other_user_num_followers = other_user_num_followers
		self.other_user_num_statuses = other_user_num_statuses
		self.last_other_tweet_sentiment = last_other_tweet_sentiment
		self.first_other_tweet_is_reply = first_other_tweet_is_reply
		self.convo_num_self_tweets = convo_num_self_tweets

	def __repr__(self):
		return "other_user_num_followers={other_user_num_followers}, other_user_num_statuses={other_user_num_statuses}, last_other_tweet_sentiment={last_other_tweet_sentiment}, first_other_tweet_is_reply={first_other_tweet_is_reply}, convo_num_self_tweets={convo_num_self_tweets}".format(
					other_user_num_followers = self.other_user_num_followers,
					other_user_num_statuses = self.other_user_num_statuses,
					last_other_tweet_sentiment = self.last_other_tweet_sentiment,
					first_other_tweet_is_reply = self.first_other_tweet_is_reply,
					convo_num_self_tweets = self.convo_num_self_tweets,
				)

	def followers_count_category(self):
		if self.other_user_num_followers < 100:
			return CountCategory.low
		elif self.other_user_num_followers < 2000:
			return CountCategory.medium
		else:
			return CountCategory.high

	def statuses_count_category(self):
		if self.other_user_num_statuses < 100:
			return CountCategory.low
		elif self.other_user_num_statuses < 1000:
			return CountCategory.medium
		else:
			return CountCategory.high

	def last_other_tweet_sentiment_category(self):
		return SentimentCategory.from_score(self.last_other_tweet_sentiment)

	def to_array(self):
		return [
			self.followers_count_category().value, 
			self.statuses_count_category().value,
			self.last_other_tweet_sentiment_category().value,
			self.first_other_tweet_is_reply,
			int(self.convo_num_self_tweets > 1)
		]

	@classmethod
	def all_states(cls):
		states = []
		for num_followers in CountCategory:
			for num_statuses in CountCategory:
				for sentiment in SentimentCategory:
					for first_other_tweet_is_reply in [False, True]:
						for convo_num_self_tweets in [False, True]:
							states.append([
								num_followers.value, 
								num_statuses.value,
								sentiment.value,
								int(first_other_tweet_is_reply),
								int(convo_num_self_tweets)
							])
		return states