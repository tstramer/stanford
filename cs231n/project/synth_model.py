import numpy as np
import tensorflow as tf
from .frame_encoder import FrameEncoder, FrameEncoderConfig
from .video_encoder import VideoEncoder, VideoEncoderConfig
from .causal_conv_net import CausalConvNet, CausalConvNetConfig
from . import wavenet_ops
from . import constants

class SynthModelConfig:

    def __init__(
        self, 
        frame_config, 
        video_config,
        conv_net_config,
        num_notes,
        batch_size,
        window_length,
        audio_frames_per_segment,
        l2_reg_strength,
        learning_rate,
        learning_rate_decay_rate,
        learning_rate_decay_steps,
        max_grad_norm,
        repred_strength,
        dropout_prob
    ):
        self.frame_config = frame_config
        self.video_config = video_config
        self.conv_net_config = conv_net_config
        self.num_notes = num_notes
        self.batch_size = batch_size
        self.window_length = window_length
        self.audio_frames_per_segment = audio_frames_per_segment
        self.l2_reg_strength = l2_reg_strength
        self.learning_rate = learning_rate
        self.learning_rate_decay_rate = learning_rate_decay_rate
        self.learning_rate_decay_steps = learning_rate_decay_steps
        self.max_grad_norm = max_grad_norm
        self.repred_strength = repred_strength
        self.dropout_prob = dropout_prob

DefaultSynthConfig = SynthModelConfig(
    frame_config = FrameEncoderConfig(
        num_features = 128),
    video_config = VideoEncoderConfig(
        frame_width = 780,
        frame_height = 57),
    conv_net_config = CausalConvNetConfig(
        filter_width = 2,
        quantization_channels = 256,
        residual_channels = 64,
        dilation_channels = 64,
        condition_channels = 2 * constants.NUM_NOTES + 1,#128,
        skip_channels = 256,
        dilations = [2**i for i in range(10)] * 6,
        output_width = constants.AUDIO_SEGMENT_LEN * constants.AUDIO_SAMPLE_RATE,
        generator_batch_size = 1),
    num_notes = 2 * constants.NUM_NOTES, # 2 because press and unpress
    batch_size = 3,
    window_length = constants.AUDIO_WINDOW_LEN * constants.AUDIO_SAMPLE_RATE,
    audio_frames_per_segment = (constants.AUDIO_SEGMENT_LEN + constants.AUDIO_WINDOW_LEN) * constants.AUDIO_SAMPLE_RATE,
    l2_reg_strength = .0001,
    learning_rate = .002,
    learning_rate_decay_rate = .3,
    learning_rate_decay_steps = 8000,
    max_grad_norm = 10,
    repred_strength = 0,#.25
    dropout_prob = 0#.2
)

class SynthModel:

    def __init__(self, config):
        self.config = config
        self.quant_channels = self.config.conv_net_config.quantization_channels

        np_notes_one_pad = np.zeros(
            (self.config.batch_size, self.config.audio_frames_per_segment, self.config.num_notes + 1),
            np.float32
        )
        np_notes_one_pad[:, :, 0] = 1
        self.notes_one_pad = tf.constant(np_notes_one_pad)
        
        self.frame_encoder = FrameEncoder(config.frame_config)
        self.video_encoder = VideoEncoder(self.frame_encoder, config.video_config)
        self.conv_net = CausalConvNet(config.conv_net_config)

        self.notes = tf.placeholder(tf.float32, shape=[None, config.num_notes])
        self.note_sparse_idxs = tf.placeholder(tf.int64, shape=[None, 3])

        self.video_frames = tf.placeholder(tf.float32, shape=[None] + config.video_config.frame_dims)
        self.audio_frames = tf.placeholder(tf.int32, shape=[None, config.audio_frames_per_segment])
        self.sparse_idxs = tf.placeholder(tf.int64, shape=[None, 3])

        self.video_frames_gen = tf.placeholder(tf.float32, shape=[None] + config.video_config.frame_dims)
        self.notes_gen = tf.placeholder(tf.float32, shape=[None, config.num_notes + 1])
        self.audio_frames_gen = tf.placeholder(tf.int32, shape=[None])
        self.audio_frames_gen_one_hot = tf.one_hot(self.audio_frames_gen, self.quant_channels)

        self.pred_op = self.get_predict_op()
        self.repred_op = self.get_repredict_op(self.pred_op)
        self.loss_op = self.get_loss_op(self.pred_op, self.repred_op)
        self.train_op = self.get_training_op(self.loss_op)
        self.generator_op, self.init_ops, self.push_ops = self.get_generator_op()

    def get_predict_op(self):
        with tf.variable_scope("pred"):
            condition_tensor = self._get_condition_tensor()
            audio_frames_one_hot = wavenet_ops.one_hot(self.audio_frames, self.quant_channels)
            if self.config.dropout_prob > 0:
                audio_frames_one_hot = tf.nn.dropout(audio_frames_one_hot, keep_prob = 1 - self.config.dropout_prob)
            audio_preds = self.conv_net.encode(audio_frames_one_hot, condition_tensor)
            return audio_preds

    def get_repredict_op(self, pred_op):
        with tf.variable_scope("pred", reuse=True):
            condition_tensor = self._get_condition_tensor()
            audio_frames_one_hot = wavenet_ops.one_hot(self.audio_frames[:, 0:self.config.window_length], self.quant_channels)
            pred_op_with_window = tf.concat([audio_frames_one_hot, pred_op], axis = 1)
            audio_preds = self.conv_net.encode(pred_op_with_window, condition_tensor)
            return audio_preds

    def get_generator_op(self):
        with tf.variable_scope("pred", reuse=True):
            if constants.USE_VIDEO:
                condition_features = self.video_encoder.encode(self.video_frames_gen)
            else:
                condition_features = self.notes_gen

            audio_preds, init_ops, push_ops = self.conv_net.generate(self.audio_frames_gen_one_hot, condition_features)

            return tf.nn.softmax(audio_preds), init_ops, push_ops

    def get_training_op(self, loss):
        with tf.variable_scope("train"):
            params = tf.trainable_variables()

            global_step = tf.Variable(0, trainable=False)
            learning_rate = tf.train.exponential_decay(self.config.learning_rate, 
                                                       global_step = global_step,
                                                       decay_steps = self.config.learning_rate_decay_steps, 
                                                       decay_rate = self.config.learning_rate_decay_rate, 
                                                       staircase=True)

            optimizer = tf.train.AdamOptimizer(learning_rate=learning_rate, epsilon=1e-4)

            gradients, self.grad_norm = tf.clip_by_global_norm(
                tf.gradients(loss, params), self.config.max_grad_norm)

            return optimizer.apply_gradients(zip(gradients, params), 
                                             global_step = global_step)

    def get_loss_op(self, pred_op, repred_op):
        with tf.variable_scope("loss"):
            audio_frames_one_hot = wavenet_ops.one_hot(self.audio_frames, self.quant_channels)
            target_audio = tf.slice(audio_frames_one_hot, 
                                   [0, self.config.window_length, 0], 
                                   [-1, -1, -1])

            logits_pred = tf.reshape(pred_op, [-1, self.quant_channels])
            logits_repred = tf.reshape(repred_op, [-1, self.quant_channels])
            labels = tf.reshape(target_audio, [-1, self.quant_channels])

            loss_pred = tf.nn.softmax_cross_entropy_with_logits(logits = logits_pred, labels = labels)
            reduced_loss_pred = tf.reduce_mean(loss_pred)

            loss_repred = tf.nn.softmax_cross_entropy_with_logits(logits = logits_repred, labels = labels)
            reduced_loss_repred = tf.reduce_mean(loss_repred)

            if self.config.l2_reg_strength > 0:
                self.l2_loss = self.config.l2_reg_strength * tf.add_n(
                    [tf.nn.l2_loss(v) for v in tf.trainable_variables() if not('bias' in v.name)])
            else:
                self.l2_loss = tf.constant(0.0)

            if self.config.repred_strength > 0:
                self.repred_loss = self.config.repred_strength * reduced_loss_repred
            else:
                self.repred_loss = tf.constant(0.0)

            total_loss = (reduced_loss_pred + self.repred_loss + self.l2_loss)

            return total_loss

    def generate(self, session, condition_frames, prev_audio_frames):
        input_feed = {
            self.audio_frames_gen: prev_audio_frames
        }
        if constants.USE_VIDEO:
            input_feed[self.video_frames_gen] = condition_frames
        else:
            condition_frames = np.pad(condition_frames, [[0, 0], [1, 0]], 'constant', constant_values = 1)
            input_feed[self.notes_gen] = condition_frames

        output_feed = [
            self.generator_op
        ] + self.push_ops

        return session.run(output_feed, input_feed)[0]

    def optimize(self, session, data):
        VIDEO_SEGS = 0
        VIDEO_TIMES = 1
        NOTE_SEGS = 2
        NOTE_TIMES = 3
        AUDIO_SEGS = 4

        idxs = self._get_sparse_idxs(data[VIDEO_SEGS], data[VIDEO_TIMES], self.config.frame_config.num_features)
        idxs_notes = self._get_sparse_idxs(data[NOTE_SEGS], data[NOTE_TIMES], self.config.num_notes)

        input_feed = {
            self.video_frames: np.concatenate(data[VIDEO_SEGS]),
            self.audio_frames: data[AUDIO_SEGS],
            self.sparse_idxs: idxs,
            self.notes: np.concatenate(data[NOTE_SEGS]),
            self.note_sparse_idxs: idxs_notes
        }

        output_feed = [
            self.train_op,
            self.loss_op,
            self.grad_norm,
            self.repred_loss,
            self.l2_loss
        ]

        return session.run(output_feed, input_feed)

    def _get_sparse_idxs(self, segments, segment_times, num_features):
        frames_per_segment = np.array([len(frames) for frames in segments])
        frame_times = np.concatenate(segment_times)

        splits = np.cumsum(frames_per_segment)[0:-1]
        segments = np.split(frame_times, splits)
        segment_idxs = []
        for i, segment in enumerate(segments):
            idxs = np.stack([
                np.repeat(i, num_features * len(segment)),
                np.repeat(segment, num_features),
                np.tile(range(num_features), len(segment))
            ]).T
            segment_idxs.append(idxs)

        return np.concatenate(segment_idxs, axis=0)

    def _get_condition_tensor(self):
        if constants.USE_VIDEO:
            dense_video_shape = [
                self.config.batch_size, 
                self.config.audio_frames_per_segment, 
                self.config.frame_config.num_features
            ]
            condition_tensor = self.video_encoder.encode_sparse(self.video_frames,
                                                                self.sparse_idxs,
                                                                dense_shape = dense_video_shape)

            return tf.sparse_tensor_to_dense(condition_tensor)
        else:
            dense_notes_shape = [
                self.config.batch_size, 
                self.config.audio_frames_per_segment, 
                self.config.num_notes
            ]

            condition_tensor = tf.SparseTensor(
                self.note_sparse_idxs,
                tf.reshape(self.notes, [-1]),
                dense_shape = dense_notes_shape)
            dense = tf.sparse_tensor_to_dense(condition_tensor)
            dense = tf.pad(dense, [[0, 0], [0, 0], [1, 0]])

            return dense + self.notes_one_pad


    def _get_condition_width(self):
        if constants.USE_VIDEO:
            return self.config.frame_config.num_features
        else:
            return self.config.num_notes + 1
