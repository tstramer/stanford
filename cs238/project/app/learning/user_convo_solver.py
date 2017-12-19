import numpy as np
import pandas as pd
from components.tweetactions.tweet_actions import TWEET_ACTIONS
from .qlearning_solver import QLearningSolver
from services.flask_app import db
from model.action import Action
from model.tweet import Tweet
from model.user_event import UserEvent
from .user_convo_helper import get_state_and_action_and_next_states
from .user_convo_state import UserConvoState
from .user_convo_action import UserConvoAction

DEFAULT_ALPHA = .1
DEFAULT_GAMMA = .9
DEFAULT_MAX_ITERS = 25

FAVE_WEIGHT = 1
RT_WEIGHT = 3
REPLY_WEIGHT = 5
QUOTE_WEIGHT = 7
IS_FOLLOWING_WEIGHT = 3
CONVO_SIZE_REWARD = 5

class UserConvoSolver():
	def __init__(self, alpha = DEFAULT_ALPHA, gamma = DEFAULT_GAMMA, max_iters = DEFAULT_MAX_ITERS):
		self.all_states = UserConvoState.all_states()
		self.all_actions = UserConvoAction.all_actions()
		self.alpha = alpha
		self.gamma = gamma
		self.max_iters = max_iters

	def solve(self):
		(data, ave_reward_per_action) = self.generate_data()
		solver = QLearningSolver(data, len(self.all_states), len(self.all_actions), self.alpha, self.gamma, self.max_iters)
		
		Qinit = np.zeros((len(self.all_states), len(self.all_actions)))
		for i in range(0, len(self.all_states)):
			for j in range(0, len(self.all_actions)):
				Qinit[i, j] = .3 * ave_reward_per_action[j]
		Q = solver.solve(Qinit)
		Q = self.interpolate(Q)
		return Q

	def generate_data(self):
		actions = Action.query.all()
		tweets = Tweet.query.all()
		user_events = UserEvent.query.all()
		state_and_action_and_next_states = get_state_and_action_and_next_states(actions, tweets, user_events)
		data = []
		ave_reward_per_action = np.zeros(len(self.all_actions))
		action_taken = np.zeros(len(self.all_actions))
		for (state, action, next_state) in state_and_action_and_next_states:
			reward = self.calculate_reward(state, action)
			state_idx = self.all_states.index(state.to_array())
			next_state_idx = self.all_states.index(next_state.to_array())
			action_idx = self.all_actions.index(action.to_array())
			ave_reward_per_action[action_idx] += reward
			action_taken[action_idx] += 1
			data.append([state_idx, action_idx, reward, next_state_idx])
		for i in range(0, len(ave_reward_per_action)):
			if action_taken[i] > 0:
				ave_reward_per_action[i] = ave_reward_per_action[i] / action_taken[i]
		return (data, ave_reward_per_action)

	def calculate_reward(self, state, action):
		reward = 0
		reward += action.fave_count * FAVE_WEIGHT
		reward += action.rt_count * RT_WEIGHT
		reward += action.reply_count * REPLY_WEIGHT
		reward += action.quote_count * QUOTE_WEIGHT
		reward += int(action.is_following) * IS_FOLLOWING_WEIGHT
		reward += (state.convo_num_self_tweets-1) * CONVO_SIZE_REWARD
		return reward

	def interpolate(self, Q):
	    # has_non_zero_value = np.any(Q != 0, axis=1)
	    # filled_states = np.where(has_non_zero_value)[0]
	    # missing_states = np.where(np.logical_not(has_non_zero_value))[0]
	    return Q