import pyaudio
import fluidsynth

class AudioPlayer:

	def __init__(self, audio_sample_rate, speaker_sample_rate):
		self.audio_sample_rate = audio_sample_rate
		self.speaker_sample_rate = speaker_sample_rate
		self.pa, self.strm = self._setup_audio()

	def play_sample(self, audio):
	    upsampled = audio.repeat(self.speaker_sample_rate // self.audio_sample_rate, axis=0)
	    samps = fluidsynth.raw_audio_string(upsampled)
	    print ('Starting playback')
	    self.strm.write(samps)

	def _setup_audio(self):
	    pa = pyaudio.PyAudio()
	    strm = pa.open(
	        format = pyaudio.paInt16,
	        channels = 2, 
	        rate = self.speaker_sample_rate, 
	        output = True)

	    return pa, strm

