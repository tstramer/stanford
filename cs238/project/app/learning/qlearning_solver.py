import numpy as np
import pandas as pd
import math

class QLearningSolver:
    """
        Algorithm described in https://en.wikipedia.org/wiki/Q-learning
    """

    def __init__(self, data, num_states, num_actions, alpha, gamma, max_iterations):
        self.data = data
        self.num_states = num_states
        self.num_actions = num_actions
        self.alpha = alpha
        self.gamma = gamma
        self.max_iterations = max_iterations

    def solve(self, Q):
        for i in range(0, self.max_iterations):
            for (s, a, r, sa) in self.data:
                Q[s, a] = (1-self.alpha)*Q[s,a] + (self.alpha)*(r+self.gamma*np.max(Q[sa]))
        return Q

    def get_policy(self):
        return np.argmax(self.Q,1)+1