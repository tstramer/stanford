class SimpleAction:
	def __init__(self, name, text, weight = 1.0, url = ""):
		self.text = text
		self.name = name
		self.weight = weight
		self.url = ""
		
	def get_tweet_text(self, tweet):
		return self.text

	def get_url(self, tweet):
		return self.url