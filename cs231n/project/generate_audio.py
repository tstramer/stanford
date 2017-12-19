from .synth_model import SynthModel, SynthModelConfig, DefaultSynthConfig
from . import constants
from .segments import Segments
import tensorflow as tf
import numpy as np
from .util import *
import logging
import os
from .audio_player import AudioPlayer
import scipy.sparse as sps
from . import wavenet_ops
import sys

np.random.seed(42)

logging.basicConfig(level=logging.INFO)
logging.basicConfig(format='%(message)s', level=logging.DEBUG)

logger = logging.getLogger("generate_audio")
logger.setLevel(logging.DEBUG)

logger.addHandler(logging.FileHandler('./log/generate.log')) 

class GenerateConfig:

    def __init__(
        self,
        model_config,
        model_dir
    ):
        self.model_config = model_config
        self.model_dir = model_dir

def get_config():
    return GenerateConfig(
        model_config = DefaultSynthConfig,
        model_dir = "train10"
    )

def initialize_model(session, saver, model_dir):
    ckpt = tf.train.get_checkpoint_state(model_dir)
    v2_path = ckpt.model_checkpoint_path + ".index" if ckpt else ""
    if ckpt and (tf.gfile.Exists(ckpt.model_checkpoint_path) or tf.gfile.Exists(v2_path)):
        logging.info("Reading model parameters from %s" % ckpt.model_checkpoint_path)
        saver.restore(session, ckpt.model_checkpoint_path)
    else:
        logging.info("Created model with fresh parameters.")
        session.run(tf.global_variables_initializer())
        logging.info('Num params: %d' % sum(v.get_shape().num_elements() for v in tf.trainable_variables()))

def load_data():
    return Segments.load_segments("data/train/segments/")

def generate_audio(model, config, session, video_segs, video_seg_times, note_segs, note_seg_times, audio_segs):
    session.run(model.init_ops)

    audio_len = config.model_config.audio_frames_per_segment - config.model_config.window_length
    window_len = config.model_config.window_length
    quant_channels = config.model_config.conv_net_config.quantization_channels
    num_segs = video_segs.shape[0]
    frame_width, frame_height = video_segs[0].shape[1:]
    num_notes = note_segs[0].shape[1]

    video_frames_win = np.zeros((window_len, num_segs, frame_width, frame_height))
    video_frames_rest = np.zeros((audio_len, num_segs, frame_width, frame_height))

    note_frames_win = np.zeros((window_len, num_segs, num_notes))
    note_frames_rest = np.zeros((audio_len, num_segs, num_notes))

    for i, times in enumerate(video_seg_times):
        idxs_w = np.where(video_seg_times[i] - window_len < 0)
        idxs_r = np.where(video_seg_times[i] - window_len >= 0)
        video_frames_win[video_seg_times[i][idxs_w], i, :, :] = video_segs[i][idxs_w]
        video_frames_rest[video_seg_times[i][idxs_r] - window_len, i, :, :] = video_segs[i][idxs_r]
        
        note_frames_win[note_seg_times[i][idxs_w], i, :] = note_segs[i][idxs_w]
        note_frames_rest[note_seg_times[i][idxs_r] - window_len, i, :] = note_segs[i][idxs_r]

    audio_segs = np.pad(audio_segs, [[0, 0], [1, 0]], 'constant')
    diff = 0
    for step in range(window_len):
        if constants.USE_VIDEO:
            condition_step = video_frames_win[step]
        else:
            condition_step = note_frames_win[step]
        predictions = model.generate(session, condition_step, audio_segs[:, step])
        sample = predictions_to_samples(predictions, quant_channels)
        diff += np.abs(sample - audio_segs[:, step+1])

    samples = np.zeros((audio_len + 1, num_segs))
    samples[0] = audio_segs[:, window_len]
    for step in range(audio_len):
        if constants.USE_VIDEO:
            condition_step = video_frames_rest[step]
        else:
            condition_step = note_frames_rest[step]
        if step % 2 == 0:
            prev = samples[step, :]
        else:
            prev = audio_segs[:, window_len+step]
        predictions = model.generate(session, condition_step, prev)
        sample = predictions_to_samples(predictions, quant_channels)
        samples[step+1, :] = sample
        if step % 100 == 0:
            print ("Step: {}".format(step))

    preds = np.transpose(samples[1:, :])
    actual = audio_segs[:, window_len+1:]

    print ("ave loss", np.mean(np.abs(preds - actual)))

    return preds, actual

def predictions_to_samples(predictions, quant_channels):
    samples = []
    for i in range(predictions.shape[0]):
        sample = np.random.choice(np.arange(quant_channels), p=predictions[i])
        samples.append(sample)
    return np.array(samples)

def main(i):
    config = get_config()
    config.model_config.batch_size = 1
    config.model_config.conv_net_config.generator_batch_size = 1

    model = SynthModel(config.model_config)
    data = load_data()

    tf_config = tf.ConfigProto()
    tf_config.gpu_options.allow_growth = True

    player = AudioPlayer(constants.AUDIO_SAMPLE_RATE, constants.SPEAKER_SAMPLE_RATE)
    with tf.Session(config=tf_config) as session:
        saver = tf.train.Saver()
        initialize_model(session, saver, config.model_dir)
        preds = []
        actuals = []
        pred, actual = generate_audio(
            model, 
            config, 
            session, 
            data.video_segments[i:i+1], 
            data.video_segment_times[i:i+1], 
            data.note_segments[i:i+1], 
            data.note_segment_times[i:i+1], 
            data.audio_segments[i:i+1])
        preds = np.append(preds, pred[0])
        actuals = np.append(actuals, actual[0])

        np.savez_compressed(
            "audio{}".format(i), 
            predicted = preds,
            actual = actuals
        )

if __name__ == "__main__":
    main(int(sys.argv[1]))