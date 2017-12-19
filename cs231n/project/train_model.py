from .synth_model import SynthModel, SynthModelConfig, DefaultSynthConfig
from . import constants
from .segments import Segments
import tensorflow as tf
import numpy as np
from .util import *
import logging
from os.path import join as pjoin
import os

# np.random.seed(42)

logging.basicConfig(level=logging.INFO)
logging.basicConfig(format='%(message)s', level=logging.DEBUG)

logger = logging.getLogger("train_model")
logger.setLevel(logging.DEBUG)

logger.addHandler(logging.FileHandler('./log/train.log')) 

class TrainConfig:

    def __init__(
        self,
        model_config,
        train_dir,
        num_epochs,
        train_percent
    ):
        self.model_config = model_config
        self.train_dir = train_dir
        self.num_epochs = num_epochs
        self.train_percent = train_percent

def get_config():
    return TrainConfig(
        model_config = DefaultSynthConfig,
        train_dir = "train10",
        num_epochs = 500,
        train_percent = 1
    )

def load_data():
    return Segments.load_segments_dir("data/train/segments")

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

def train_model(model, config, session, train_dataset, val_dataset, saver):
    best_score = 0

    if not os.path.exists(config.train_dir):
        os.makedirs(config.train_dir)

    for epoch in range(config.num_epochs):
        logger.info("\nEpoch %d out of %d", epoch + 1, config.num_epochs)
        
        score = run_epoch(model, config, session, train_dataset, val_dataset)
        
        save_path = saver.save(session, pjoin(config.train_dir, "model_epoch"), global_step=epoch)
        logger.info("Model saved in file: %s" % save_path)

        if score > best_score:
            best_score = score
            save_path = saver.save(session, pjoin(config.train_dir, "model_best"))
            logger.info("New best model (score=%d) saved in file: %s" % (best_score, save_path))

    return best_score

def run_epoch(model, config, session, train_dataset, val_dataset):
    prog = Progbar(target=1 + int(len(train_dataset[0]) / config.model_config.batch_size), logger=logger)
    loss = 0
    for i, train_batch in enumerate(get_minibatches(train_dataset, config.model_config.batch_size, shuffle=True)):
        if train_batch[0].shape[0] != config.model_config.batch_size:
            continue
        _, loss, grad_norm, repred_loss, l2_loss = model.optimize(session, train_batch)
        prog.update(i + 1, [
            ("train loss", loss), 
            ("pred loss", loss-repred_loss-l2_loss), 
            ("l2 loss", l2_loss), 
            ("grad norm", grad_norm)
        ])

    return -loss

def main():
    config = get_config()
    model = SynthModel(config.model_config)
    data = load_data()
    train_data, val_data = data.random_split(config.train_percent)

    train_data = train_data.to_array()
    val_data = val_data.to_array()

    tf_config = tf.ConfigProto()
    tf_config.gpu_options.allow_growth = True

    with tf.Session(config=tf_config) as session:
        saver = tf.train.Saver()
        initialize_model(session, saver, config.train_dir)
        train_model(model, config, session, train_data, val_data, saver)

if __name__ == "__main__":
    main()