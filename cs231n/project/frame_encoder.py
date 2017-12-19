import tensorflow as tf

class FrameEncoderConfig:

    def __init__(self, num_features):
        self.num_features = num_features

class FrameEncoder:

    def __init__(self, config):
        self.config = config

    def encode(self, frames):
        h = tf.layers.conv2d(frames, 8, [3, 3], activation=tf.nn.relu)
        h = tf.layers.batch_normalization(h)
        h = tf.layers.conv2d(h, 8, [3, 3], activation=tf.nn.relu)
        h = tf.layers.batch_normalization(h)
        h = tf.layers.max_pooling2d(h, [2, 2], [2, 2])

        h = tf.layers.conv2d(h, 16, [3, 3], activation=tf.nn.relu)
        h = tf.layers.batch_normalization(h)
        h = tf.layers.conv2d(h, 16, [3, 3], activation=tf.nn.relu)
        h = tf.layers.batch_normalization(h)
        h = tf.layers.max_pooling2d(h, [2, 2], [2, 2])

        h = tf.contrib.layers.flatten(h)
        h = tf.layers.dense(h, 256, activation=tf.nn.relu)
        h = tf.layers.batch_normalization(h)
        h = tf.layers.dense(h, self.config.num_features)

        return h