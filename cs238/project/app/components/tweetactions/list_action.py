import random

class ListAction:
	def __init__(self, name, texts, url = ""):
		self.texts = texts
		self.name = "la_{0}".format(name)
		self.url = ""

	def get_tweet_text(self, tweet):
		return random.choice(self.texts)

	def get_url(self, tweet):
		return self.url