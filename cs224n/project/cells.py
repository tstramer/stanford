import tensorflow as tf
import collections

_AttentionStateTuple = collections.namedtuple("A", ("c_m", "h_m"))

class AttentionStateTuple(_AttentionStateTuple):
    __slots__ = ()

    @property
    def dtype(self):
        (c_m, h_m) = self
        return c_m.dtype

class AttentionLSTMCell(tf.contrib.rnn.RNNCell):
    def __init__(self, L, H_q, k, lstm_cell, log_question_masks):
        self.L = L # number of vetors of attention matrix
        self.H_q = H_q # question matrix, size: (batch_size, L, k)
        self.k = k # state size
        self.lstm_cell = lstm_cell
        self.log_question_masks = log_question_masks
        self.H_qt = tf.transpose(H_q, perm=[0,2,1]) # (batch_size, k, L)
        self.xiav_init = tf.contrib.layers.xavier_initializer()
        self.k_q = 2 * k # actual state size (2 for bidirectional)

    @property
    def state_size(self):
        return AttentionStateTuple(self.k, self.k)

    @property
    def output_size(self):
        return self.k 

    def __call__(self, inputs, state, scope=None):
        """Updates the state using the previous @state and @inputs."""

        scope = scope or type(self).__name__

        # It's always a good idea to scope variables in functions lest they
        # be defined elsewhere!
        with tf.variable_scope(scope):
            W_y = tf.get_variable("W_y", shape=[self.k_q, self.k], initializer=self.xiav_init)
            W_h = tf.get_variable("W_h", shape=[self.k, self.k], initializer=self.xiav_init)
            W_r = tf.get_variable("W_r", shape=[self.k, self.k], initializer=self.xiav_init)
            w = tf.get_variable("w", shape=[self.k, 1], initializer=self.xiav_init)
            b_p = tf.get_variable("b_p", shape=[1, 1, self.k], initializer=self.xiav_init)
            b = tf.get_variable("b", shape=[1, 1], initializer=self.xiav_init)

            M1 = self.batch_mul(self.H_q, W_y, self.L, self.k_q, self.k) # (batch, L, k)
            M2 = tf.reshape(tf.matmul(inputs, W_h) + tf.matmul(state.h_m, W_r), [-1, 1, self.k]) # (batch, k)
            M = tf.tanh((M1 + M2) + b_p) # (batch, L, k)
            Mw = tf.reshape(self.batch_mul(M, w, self.L, self.k, 1), [-1, self.L]) # (batch, L)
            alpha = tf.reshape(tf.nn.softmax((Mw + self.log_question_masks) + b), [-1, self.L, 1]) # (batch, L, 1)
            r_new = tf.reshape(tf.matmul(self.H_qt, alpha), [-1, self.k_q]) # (batch, k_q)

            h_m_in = tf.concat([r_new, inputs], axis=1) # (batch, 2k)
            h_m_out, h_m_state = self.lstm_cell(h_m_in, tf.contrib.rnn.LSTMStateTuple(state.c_m, state.h_m), scope + "_lstm_m")

            new_state = AttentionStateTuple(h_m_state.c, h_m_state.h)

        return h_m_out, new_state

    def batch_mul(self, Aijk, Bkl, j, k, l):
        Af = tf.reshape(Aijk, [-1, k])
        Cf = tf.matmul(Af, Bkl)
        return tf.reshape(Cf, [-1,j,l])

_PtrNetworkStateTuple = collections.namedtuple("P", ("b", "c", "h"))

class PtrNetworkStateTuple(_PtrNetworkStateTuple):
    __slots__ = ()

    @property
    def dtype(self):
        (b, c, h) = self
        return b.dtype

class PtrNetworkCell(tf.contrib.rnn.RNNCell):
    def __init__(self, P, Hr, k, lstm_cell, log_context_masks):
        self.P = P # number of vectors of attention matrix Hr
        self.Hr = Hr # matrix to attend over, size: (batch_size, P, 2k)
        self.k = k # state size
        self.lstm_cell = lstm_cell
        self.log_context_masks = log_context_masks
        self.Hrt = tf.transpose(Hr, perm=[0,2,1]) # (batch_size, 2k, P)
        self.xiav_init = tf.contrib.layers.xavier_initializer()

    @property
    def state_size(self):
        return PtrNetworkStateTuple(self.P, self.k, self.k)

    @property
    def output_size(self):
        return self.P

    #state = (a, h)
    def __call__(self, inputs, state, scope=None):
        """Updates the state using the previous @state and @inputs."""
        
        scope = scope or type(self).__name__

        # It's always a good idea to scope variables in functions lest they
        # be defined elsewhere!
        with tf.variable_scope(scope):
            V = tf.get_variable("V", shape=[2 * self.k, self.k], initializer=self.xiav_init)
            W_a = tf.get_variable("W_a", shape=[self.k, self.k], initializer=self.xiav_init)
            v = tf.get_variable("v", shape=[self.k, 1], initializer=self.xiav_init)
            b = tf.get_variable("b", shape=[1, 1, self.k], initializer=self.xiav_init)
            c = tf.get_variable("c", shape=[1, self.P], initializer=self.xiav_init)

            F1 = self.batch_mul(self.Hr, V, self.P, 2 * self.k, self.k) # (batch, P, k)

            F2 = tf.reshape(tf.matmul(state.h, W_a), [-1, 1, self.k]) # (batch, 1, k)
            F = tf.tanh((F1 + F2) + b) # (batch, P, k)
           
            beta = tf.reshape(self.batch_mul(F, v, self.P, self.k, 1), [-1, self.P]) + c # (batch, P)
            beta_sm = tf.nn.softmax(beta + self.log_context_masks)

            lstm_input = tf.reshape(
                tf.matmul(self.Hrt, tf.reshape(beta_sm, [-1, self.P, 1])),
                [-1, 2 * self.k]
            ) # (batch_size, 2k)
            h_out, h_state = self.lstm_cell(lstm_input, tf.contrib.rnn.LSTMStateTuple(state.c, state.h), scope + "_lstm")

            new_state = PtrNetworkStateTuple(beta_sm, h_state.c, h_state.h)

        return beta_sm, new_state

    def batch_mul(self, Aijk, Bkl, j, k, l):
        Af = tf.reshape(Aijk, [-1, k])
        Cf = tf.matmul(Af, Bkl)
        return tf.reshape(Cf, [-1,j,l])
