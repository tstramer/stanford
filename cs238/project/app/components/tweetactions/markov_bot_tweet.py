import os
import random
from components.markovbot.markovbot import MarkovBot

dir = os.path.dirname(__file__)

DEFAULT_SEEDWORDS = [u'dream', u'psychoanalysis']

class MarkovBotTweet:
	def __init__(self, train_filename, prefixes = [], seedwords = DEFAULT_SEEDWORDS):
		self.train_filename = train_filename
		self.markov_bot = MarkovBot()
		self.name = "mbt_{0}".format(train_filename)
		self.prefixes = prefixes
		if (len(self.prefixes) == 0):
			self.prefixes = [""]
		self.seedwords = seedwords
		self.train_bot()

	def get_tweet_text(self, tweet):
		text = self.markov_bot.generate_text(20, seedword=self.seedwords)
		prefix = random.choice(self.prefixes)
		return "{prefix} \"{text}\"".format(
			prefix = prefix,
			text = text)

	def get_url(self, tweet):
		return ""

	def train_bot(self):
		file_path = os.path.join(dir, 'markovbot_data', self.train_filename)
		self.markov_bot.read(file_path)