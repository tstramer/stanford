import numpy as np
import math
from . import constants
import os
import sys
from .segments import Segments
import skimage.color

DIR_PATH = os.path.dirname(os.path.realpath(__file__))
TRAIN_DATA_DIR = os.path.join(DIR_PATH, "..", "data", "train")

QUANT_LEVEL = 256
MAX_AUDIO_LEVEL = 32768.0 # 16 bit signed integer which has range (-32768, 32767)

def main(npz_filename):
    npz = np.load(npz_filename)
    (video_segments, video_segment_times), (notes, note_times), audio_segments = get_video_and_audio_segments(npz)

    base_name = os.path.splitext(os.path.basename(npz_filename))[0]
    array_path = os.path.join(TRAIN_DATA_DIR, "segments", base_name)

    Segments.save_segments(
        array_path, 
        video_segments, 
        video_segment_times, 
        notes,
        note_times,
        audio_segments
    )

def get_video_and_audio_segments(npz):
    audio_frames = npz["audio_frames"]
    video_frames = npz["video_frames"]
    video_frame_times = npz["video_frame_times"]
    notes_pressed = one_hot(npz["notes_pressed"], constants.NUM_NOTES)
    notes_released = one_hot(npz["notes_released"], constants.NUM_NOTES)

    notes = np.concatenate([notes_pressed, notes_released], axis=1)

    audio_frames = np.average(audio_frames.reshape([-1, 2]), axis=1)

    audio_segments = get_segments(
        audio_frames, 
        constants.AUDIO_SAMPLE_RATE, 
        constants.AUDIO_SEGMENT_LEN, 
        constants.AUDIO_WINDOW_LEN,
        postprocess_audio_frames
    )

    video_segments, video_segment_times = get_segments_sparse(
        video_frames, 
        constants.AUDIO_SAMPLE_RATE, 
        constants.AUDIO_SEGMENT_LEN, 
        constants.AUDIO_WINDOW_LEN,
        postprocess_video_frames,
        video_frame_times,
        audio_segments.shape[0]
    )

    notes, note_times = get_segments_sparse(
        notes, 
        constants.AUDIO_SAMPLE_RATE, 
        constants.AUDIO_SEGMENT_LEN, 
        constants.AUDIO_WINDOW_LEN,
        lambda x: x,
        video_frame_times,
        audio_segments.shape[0]
    )

    print ("audio: ", audio_segments.shape)
    print ("video: ", video_segments.shape)

    return (video_segments, video_segment_times), (notes, note_times), audio_segments

def get_segments(all_frames, frame_rate, seg_len, window_len, postprocess):
    def create_pad(num):
        return np.zeros(([num] + list(all_frames[0].shape)))

    curr = 0
    segments = []

    while curr <= all_frames.shape[0]:
        frames = []
        start = curr - frame_rate * window_len
        if start < 0:
            frames = create_pad(-start)
            start = 0
        curr += seg_len * frame_rate

        if len(frames) > 0:
            frames = np.concatenate([frames, all_frames[start:curr]], axis=0)
        else:
            frames = all_frames[start:curr]

        if curr >= all_frames.shape[0]:
            frames = np.concatenate([frames, create_pad(curr - all_frames.shape[0])], axis=0)

        segments.append(postprocess(frames))

    return np.array(segments)

def get_segments_sparse(all_frames, frame_rate, seg_len, window_len, postprocess, sparse_times, num_segments):
    segments = []
    segment_times = []

    j = 0
    for i in range(num_segments):
        curr_min = (seg_len * i - window_len) * frame_rate
        curr_max = (seg_len * (i + 1)) * frame_rate

        idxs = []
        j = 0
        while j < len(sparse_times) and sparse_times[j] < curr_max:
            if sparse_times[j] >= curr_min:
                idxs.append(j)
            j += 1

        segments.append(postprocess(all_frames[idxs]))
        segment_times.append(sparse_times[idxs] - curr_min)

    return np.array(segments), np.array(segment_times)

def one_hot(els_list, buckets):
    output = np.zeros((els_list.shape[0], buckets))
    for i, els in enumerate(els_list):
        if len(els) > 0:
            output[i, els] = 1
    return output

def postprocess_video_frames(frames):
    # extract y-channel
    yuv_frames = skimage.color.rgb2yuv(frames)
    y_frames = yuv_frames[:, :, :, 0]
    return y_frames

def postprocess_audio_frames(frames):
    # apply mu-law companding transform and quantize into QUANT_LEVEL bucket indices
    frames = frames / MAX_AUDIO_LEVEL
    frames = np.sign(frames) * np.log(1 + QUANT_LEVEL * np.abs(frames)) / np.log(1 + QUANT_LEVEL)
    buckets = 2 * np.array(range(QUANT_LEVEL)) / QUANT_LEVEL - 1
    return np.maximum(np.digitize(frames, buckets) - 1, 0)

if __name__ == "__main__":
    main(sys.argv[1])