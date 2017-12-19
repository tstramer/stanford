from jinja2 import Template
import os
import random

dir = os.path.dirname(__file__)

class QuoteRandomAccountTweet:
	def __init__(self, category, prefixes = []):
		self.category = category
		self.tweet_ids = self.load_tweet_ids(category)
		self.name = "qrat_{0}".format(category)
		self.prefixes = prefixes
		if (len(self.prefixes) == 0):
			self.prefixes = [""]

	def get_tweet_text(self, tweet):
		return random.choice(self.prefixes)

	def get_url(self, tweet):
		tweet_id = self.weighted_choice(self.tweet_ids_with_weights())
		return "https://twitter.com/asdf/status/{0}".format(tweet_id)

	def load_tweet_ids(self, category):
		path = os.path.join(dir, 'account_tweets', self.category)
		return [int(line.rstrip('\n')) for line in open(path, 'r')]

	def tweet_ids_with_weights(self):
		return list(zip(self.tweet_ids, self.get_weights()))

	def get_weights(self):
		self.tweet_ids.sort()
		weights = [0 for i in range(0, len(self.tweet_ids))]
		for i in range(0, len(self.tweet_ids)):
			weights[i] = (self.tweet_ids[i] - self.tweet_ids[0]) / self.tweet_ids[-1]
			weights[i] = min(1, weights[i] + .25)
		return weights

	def weighted_choice(self, choices):
	   total = sum(w for c, w in choices)
	   r = random.uniform(0, total)
	   upto = 0
	   for c, w in choices:
	      if upto + w >= r:
	         return c
	      upto += w
	   assert False, "Shouldn't get here"