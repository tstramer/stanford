from jinja2 import Template
import os
import random

dir = os.path.dirname(__file__)

DEFAULT_EMOJI = ["😜", "🙏", "😘", "👍", "😂", "😍", "😛", "🙌", "😉", "🐼", "👌", "👊", "💯"]
POLITICAL_EMOJI = ["😜", "💩", "😂", "😁", "😯", "🐼", "😪", "🇺🇸", "💯", "⚡", "💥", "🐷", "🔥", "🐸"]
DEFAULT_NUM_EMOJI = 3

class WithEmojiAction:
	def __init__(self, action, emojis = DEFAULT_EMOJI, num_emojis = DEFAULT_NUM_EMOJI):
		self.name = action.name
		self.action = action
		self.emojis = emojis
		self.num_emojis = num_emojis

	def get_tweet_text(self, tweet):
		num_to_take = random.choice([0,1,2,3,4])
		tweet_text = self.action.get_tweet_text(tweet)
		url = self.action.get_url(tweet)
		emoji_suffix = "".join(random.sample(self.emojis, num_to_take))
		return "{tweet_text} {emoji_suffix} {url}".format(
			tweet_text = tweet_text,
			url = url,
			emoji_suffix = emoji_suffix)