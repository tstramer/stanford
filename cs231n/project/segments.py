import numpy as np
from .util import ensure_dir
import os

# Represents segments of a video consisting of video frames and audio frames
# Each song is split into 7 second segments with a two second overlapping section
# The first and last segment are padded with zeros to make the 7 second size
class Segments:
    
    @staticmethod
    def load_segments(filename):
        npz = np.load(filename)
        return Segments(
            npz["video_segments"], 
            npz["video_segment_times"],
            npz["note_segments"], 
            npz["note_segment_times"], 
            npz["audio_segments"])

    @staticmethod
    def load_segments_dir(dir):
        segments = [Segments.load_segments(os.path.join(dir, f)) for f in os.listdir(dir)]
        return Segments.join(segments)

    @staticmethod
    def join(segments):
        return Segments(
            video_segments = np.concatenate([s.video_segments for s in segments], axis=0),
            video_segment_times = np.concatenate([s.video_segment_times for s in segments], axis=0),
            note_segments = np.concatenate([s.note_segments for s in segments], axis=0),
            note_segment_times = np.concatenate([s.note_segment_times for s in segments], axis=0),
            audio_segments = np.concatenate([s.audio_segments for s in segments], axis=0)
        )

    @staticmethod
    def save_segments(save_path, video_segments, video_segment_times, note_segments, note_segment_times, audio_segments):
        ensure_dir(save_path)
        np.savez_compressed(
            save_path, 
            video_segments = video_segments,
            video_segment_times = video_segment_times,
            note_segments = note_segments, 
            note_segment_times = note_segment_times,
            audio_segments = audio_segments
        )

    def __init__(self, video_segments, video_segment_times, note_segments, note_segment_times, audio_segments):
        self.video_segments = video_segments
        self.video_segment_times = video_segment_times
        self.note_segments = note_segments
        self.note_segment_times = note_segment_times
        self.audio_segments = audio_segments

    def random_split(self, percent_1):
        num_segments = self.audio_segments.shape[0]
        size_1 = int(num_segments * percent_1)
        idxs = np.random.permutation(num_segments)
        idxs_1 = idxs[:size_1]
        idxs_2 = idxs[size_1:]

        segs_1 = Segments(
            video_segments = self.video_segments[idxs_1],
            video_segment_times = self.video_segment_times[idxs_1],
            note_segments = self.note_segments[idxs_1],
            note_segment_times = self.note_segment_times[idxs_1],
            audio_segments = self.audio_segments[idxs_1]
        )

        segs_2 = Segments(
            video_segments = self.video_segments[idxs_2],
            video_segment_times = self.video_segment_times[idxs_2],
            note_segments = self.note_segments[idxs_2],
            note_segment_times = self.note_segment_times[idxs_2],
            audio_segments = self.audio_segments[idxs_2]
        )

        return segs_1, segs_2

    def to_array(self):
        return [self.video_segments, self.video_segment_times, self.note_segments, self.note_segment_times, self.audio_segments]
