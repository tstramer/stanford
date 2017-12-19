from components.tweetactions.tweet_actions import TWEET_ACTIONS

class UserConvoAction:
	def __init__(self, action, fave_count, rt_count, reply_count, reply_sentiment, quote_count, is_following):
		self.action = action
		self.fave_count = fave_count
		self.rt_count = rt_count
		self.reply_count = reply_count
		self.reply_sentiment = reply_sentiment # only valid if reply_count > 0
		self.quote_count = quote_count
		self.is_following = is_following

	def __repr__(self):
		return "action={action}, fave_count={fave_count}, rt_count={rt_count}, reply_count={reply_count}, reply_sentiment={reply_sentiment}, quote_count={quote_count}, is_following={is_following}".format(
					action = self.action,
					fave_count = self.fave_count,
					rt_count = self.rt_count,
					reply_count = self.reply_count,
					reply_sentiment = self.reply_sentiment,
					quote_count = self.quote_count,
					is_following = self.is_following
				)

	def to_array(self):
		for idx in range(0, len(TWEET_ACTIONS)):
			if self.action.action == TWEET_ACTIONS[idx].name:
				return [idx]
		return [-1]

	@classmethod
	def all_actions(cls):
		actions = []
		for action_idx in range(0, len(TWEET_ACTIONS)):
			actions.append([action_idx])
		return actions