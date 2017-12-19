import tensorflow as tf

class VideoEncoderConfig:

    def __init__(self, frame_width, frame_height):
        self.frame_width = frame_width
        self.frame_height = frame_height
        self.frame_dims = [self.frame_width, self.frame_height]

class VideoEncoder:

    def __init__(self, frame_encoder, config):
        self.frame_encoder = frame_encoder
        self.config = config

    def encode(self, frames):
        encoded_frames = self.frame_encoder.encode(tf.cast(frames[:, :, :, None], tf.float32))
        return encoded_frames

    def encode_sparse(self, frames, sparse_idxs, dense_shape):
        encoded_frames = self.frame_encoder.encode(tf.cast(frames[:, :, :, None], tf.float32))

        sparse_values = tf.reshape(encoded_frames, [-1])

        return tf.SparseTensor(
            sparse_idxs,
            sparse_values,
            dense_shape = dense_shape)