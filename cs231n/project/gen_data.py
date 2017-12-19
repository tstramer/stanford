"""
Audio Project - Virtual Piano Program
Stephanie Chan
09/14/12

Dependencies:
Python 2.7
NumPy, SciPy, PyGame, Scikits.samplerate
"""

import numpy as np
import pygame
import sys
import os
from pygame.locals import *
import mido
import time
import pygame.midi
import fluidsynth
from . import constants
import math
from .util import ensure_dir

DIR_PATH = os.path.dirname(os.path.realpath(__file__))

NOTES = constants.NUM_NOTES
NOTES_PER_OCTAVE = 12
SCALE = NOTES // NOTES_PER_OCTAVE
MIDI_OFFSET = 21

WHITE_NOTE_WIDTH = 100
BLACK_NOTE_WIDTH = 60

CHANNEL = 0

SOUND_FONT_FILEPATH = "/Users/tstramer/.fluidsynth/default_sound_font.sf2"

RESOURCES_DIR = os.path.join(DIR_PATH, "..", "resources")
TRAIN_DATA_DIR = os.path.join(DIR_PATH, "..", "data", "train")

def load_image(name):
    """
    Loads image into pygame and converts into a faster to render object
    """
    try:
        image = pygame.image.load(os.path.join(RESOURCES_DIR, name)).convert_alpha()
    except pygame.error as message:
        print('Cannot load image:', name)
        raise SystemExit(message)
    rect = image.get_rect()
    image = pygame.transform.scale(image, [rect.width // SCALE, rect.height // SCALE])
    image = image.convert()
    return image, image.get_rect()
        
class Keyboard(pygame.sprite.Sprite):
    """
    A keyboard sprite class for generating all the key images and making it
    light up on key press.
    
    """
    def __init__(self,note,x,y, sharp=False):
        """
        Input:
        :note: string name of the instance's note
        :x: :y: position values for rect area
        :sharp: boolean for differentiating between white and black keys
        """
        self.clock = pygame.time.Clock()
        pygame.sprite.Sprite.__init__(self)
        self.note = note
        self.sharp = sharp
        self.x = x
        self.y = y
        if sharp:
            self.image, self.rect = load_image('black-key.png')
        else:
            self.image, self.rect = load_image('key-left.png')
        screen = pygame.display.get_surface()
        self.area = screen.get_rect()
        self.rect.topleft = x, y

    def pressed(self, time):
        if self.sharp:
            self.image, self.rect = load_image('black-p-key.png')            
        else:
            self.image, self.rect = load_image('pressed-key.png')
        self.rect.topleft = self.x, self.y

    def unpressed(self):
        if self.sharp:
            self.image, self.rect = load_image('black-key.png')
        else:
            self.image, self.rect = load_image('key-left.png')
        self.rect.topleft = self.x, self.y

def setup_pygame():
    pygame.init()

    # Window initialization
    screen=pygame.display.set_mode((780,57))
    pygame.display.set_caption("Virtual Piano")

    # Background text layer initialization
    background = pygame.Surface(screen.get_size())
    background = background.convert()

    # Add background to screen. Added to hear to load before images.
    screen.blit(background, (0,0))
    pygame.display.flip()
    screen = pygame.display.get_surface()

    note_str = ['a', 'a_sharp', 'b', 'c', 'c_sharp', 'd', 'd_sharp', 'e','f', 'f_sharp', 'g',
                 'g_sharp']
    sprites = []
    # layers will be used to differentiate black key and white key layers
    layers = pygame.sprite.LayeredUpdates()
    x = 0
    y = 0

    # Generate keyboard objects that goes into the sprite dictionary with the
    # key being the string name and the value being the keyboard instance
    for i in range(NOTES):
        note = note_str[i % NOTES_PER_OCTAVE]
        z = 0
        if note.endswith('sharp'):
            z = x - BLACK_NOTE_WIDTH // (SCALE + 1) + 2
            sprite = Keyboard(note,z,y, sharp=True)
            sprites.append(sprite)
            layers.add(sprite)
            layers.change_layer(sprite,1)
        else:
            sprite = Keyboard(note,x,y)
            sprites.append(sprite)
            layers.add(sprite)
            x += WHITE_NOTE_WIDTH // SCALE + 1

    # Sprites will be ordered by layer which is exactly how we want to render it
    allsprites = pygame.sprite.OrderedUpdates(tuple(layers.sprites()))
    pygame.display.update()

    pygame.event.get()

    return screen, background, sprites, allsprites

def setup_audio():
    fl = fluidsynth.Synth(samplerate=constants.AUDIO_SAMPLE_RATE)
    sfid = fl.sfload(SOUND_FONT_FILEPATH)
    fl.program_select(0, sfid, 0, 0)

    return fl

def get_video_audio_frames(screen, background, sprites, allsprites, fl, mid):
    screen.blit(background, (0,0))
    allsprites.draw(screen)
    pygame.display.flip()

    audio_frames = []
    video_frames = []
    video_frame_times = []
    notes_pressed = []
    notes_released = []

    time_elapsed = 0
    last_note_change = 0
    last_notes_pressed = set()
    last_notes_released = set()

    for i, msg in enumerate(mid):
        time_elapsed += msg.time

        if msg.time > 0:
            print (time_elapsed)

            new_audio_frames = fl.get_samples(int(msg.time * constants.AUDIO_SAMPLE_RATE))
            audio_frames = np.append(audio_frames, new_audio_frames)
            
            if last_note_change != -1:
                video_frames.append(pygame.surfarray.array3d(screen))
                video_frame_times.append(int(last_note_change * constants.AUDIO_SAMPLE_RATE))
                notes_pressed.append(np.array(list(last_notes_pressed)))
                notes_released.append(np.array(list(last_notes_released)))
                last_note_change = -1
                last_notes_pressed = set()
                last_notes_released = set()

        if msg.type == 'note_on' or msg.type == 'note_off':
            note = msg.note - MIDI_OFFSET
            if msg.velocity == 0 or msg.type == 'note_off':
                sprites[note].unpressed()
                last_notes_released.add(note)
            else:
                sprites[note].pressed(time_elapsed)
                last_notes_pressed.add(note)
            if msg.type == 'note_on':
                fl.noteon(CHANNEL, msg.note, msg.velocity)
            else:
                fl.noteoff(CHANNEL, msg.note, msg.velocity)
            screen.blit(background, (0,0))
            allsprites.draw(screen)
            pygame.display.flip()
            last_note_change = time_elapsed

    video_frames = np.array(video_frames)
    audio_frames = np.array(audio_frames)
    video_frame_times = np.array(video_frame_times)
    notes_pressed = np.array(notes_pressed)
    notes_released = np.array(notes_released)

    return video_frames, audio_frames, video_frame_times, notes_pressed, notes_released

def convert_midi(screen, background, sprites, allsprites, fl, midi_filename):
    midi = mido.MidiFile(midi_filename)
    video, audio, video_frame_times, notes_pressed, notes_released = get_video_audio_frames(screen, background, sprites, allsprites, fl, midi)
    print (video.shape, audio.shape, video_frame_times.shape)

    base_name = os.path.splitext(os.path.basename(midi_filename))[0]
    array_path = os.path.join(TRAIN_DATA_DIR, "raw", base_name)

    ensure_dir(array_path)

    np.savez_compressed(
        array_path, 
        video_frames = video, 
        audio_frames = audio, 
        video_frame_times = video_frame_times,
        notes_pressed = notes_pressed,
        notes_released = notes_released
    )

def main(midi_filename):
    screen, background, sprites, allsprites = setup_pygame()
    fl = setup_audio()

    convert_midi(screen, background, sprites, allsprites, fl, midi_filename)

    fl.delete()

if __name__ == "__main__":
    main(sys.argv[1])
