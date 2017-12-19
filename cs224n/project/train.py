
import os
import json
import sys

import tensorflow as tf
import numpy as np

from qa_model import *
from os.path import join as pjoin

import logging
from datetime import datetime

logging.basicConfig(level=logging.INFO)

tf.app.flags.DEFINE_float("learning_rate", 0.0007, "Learning rate.")
tf.app.flags.DEFINE_float("min_learning_rate", 0.0005, "Min learning rate.")
tf.app.flags.DEFINE_float("max_gradient_norm", 10.0, "Clip gradients to this norm.")
tf.app.flags.DEFINE_float("dropout", 0.3, "Fraction of units randomly dropped on non-recurrent connections.")
tf.app.flags.DEFINE_integer("batch_size", 50, "Batch size to use during training.")
tf.app.flags.DEFINE_integer("epochs", 10, "Number of epochs to train.")
tf.app.flags.DEFINE_integer("state_size", 300, "Size of each model layer.")
tf.app.flags.DEFINE_integer("embedding_size", 300, "Size of the pretrained vocabulary.")
tf.app.flags.DEFINE_string("glove_tokens", "840B", "Number of tokens in glove training.")
tf.app.flags.DEFINE_string("data_dir", "data/squad", "SQuAD directory (default ./data/squad)")
tf.app.flags.DEFINE_string("train_dir", "train", "Training directory to save the model parameters (default: ./train).")
tf.app.flags.DEFINE_string("load_train_dir", "", "Training directory to load model parameters from to resume training (default: {train_dir}).")
tf.app.flags.DEFINE_string("log_dir", "log", "Path to store log and flag files (default: ./log)")
tf.app.flags.DEFINE_string("optimizer", "adam", "adam / sgd")
tf.app.flags.DEFINE_integer("print_every", 1, "How many iterations to do per print.")
tf.app.flags.DEFINE_integer("keep", 0, "How many checkpoints to keep, 0 indicates keep all.")
tf.app.flags.DEFINE_string("vocab_path", "data/squad/vocab.dat", "Path to vocab file (default: ./data/squad/vocab.dat)")
tf.app.flags.DEFINE_string("embed_path", "", "Path to the trimmed GLoVe embedding (default: ./data/squad/glove.trimmed.{embedding_size}.npz)")

FLAGS = tf.app.flags.FLAGS

def initialize_model(session, model, train_dir):
    ckpt = tf.train.get_checkpoint_state(train_dir)
    v2_path = ckpt.model_checkpoint_path + ".index" if ckpt else ""

    if ckpt and (tf.gfile.Exists(ckpt.model_checkpoint_path) or tf.gfile.Exists(v2_path)):
        logging.info("Reading model parameters from %s" % ckpt.model_checkpoint_path)
        model.saver.restore(session, ckpt.model_checkpoint_path)
    else:
        logging.info("Created model with fresh parameters.")
        session.run(tf.global_variables_initializer())
        logging.info('Num params: %d' % sum(v.get_shape().num_elements() for v in tf.trainable_variables()))
    return model

def initialize_dataset(data_dir, dataset_name="train"):
    questions_path = pjoin(data_dir, "{}.ids.question".format(dataset_name))
    contexts_path = pjoin(data_dir, "{}.ids.context".format(dataset_name))
    spans_path = pjoin(data_dir, "{}.span".format(dataset_name))

    remove_idxs = []
    questions = load_ids_file(questions_path, remove_idxs, max_len=MAX_QUESTION_LENGTH)
    contexts = load_ids_file(contexts_path, remove_idxs, max_len=MAX_CONTEXT_LENGTH)
    spans = load_ids_file(spans_path, remove_idxs)

    questions = remove_indices(questions, remove_idxs)
    contexts = remove_indices(contexts, remove_idxs)
    spans = remove_indices(spans, remove_idxs)

    return [questions, contexts, spans]

def remove_indices(np_arr, remove_idxs):
    return np.delete(np_arr, remove_idxs, axis=0)

def load_ids_file(path, remove_idxs, max_len = None):
    all_ids = []
    for line in open(path):
        ids = [int(x) for x in line.strip().split()]
        all_ids.append(ids)

    padded_ids = []
    for i, ids in enumerate(all_ids):
        if max_len is not None:
            if len(ids) > max_len:
                padded_ids.append(ids[0:max_len])
                remove_idxs.append(i)
            else:
                padded_ids.append(np.pad(ids, (0, max_len-len(ids)), 'constant', constant_values=0))
        else:
            return np.array(all_ids)
    return np.array(padded_ids)

def initialize_vocab(vocab_path):
    if tf.gfile.Exists(vocab_path):
        rev_vocab = []
        with tf.gfile.GFile(vocab_path, mode="rb") as f:
            rev_vocab.extend(f.readlines())
        rev_vocab = [line.strip('\n') for line in rev_vocab]
        vocab = dict([(x, y) for (y, x) in enumerate(rev_vocab)])
        return vocab, rev_vocab
    else:
        raise ValueError("Vocabulary file %s not found.", vocab_path)

def load_embeddings(embed_path):
    return np.load(embed_path)["glove"]

def get_normalized_train_dir(train_dir):
    """
    Adds symlink to {train_dir} from /tmp/cs224n-squad-train to canonicalize the
    file paths saved in the checkpoint. This allows the model to be reloaded even
    if the location of the checkpoint files has moved, allowing usage with CodaLab.
    This must be done on both train.py and qa_answer.py in order to work.
    """
    global_train_dir = '/tmp/cs224n-squad-train'
    if os.path.exists(global_train_dir):
        os.unlink(global_train_dir)
    if not os.path.exists(train_dir):
        os.makedirs(train_dir)
    os.symlink(os.path.abspath(train_dir), global_train_dir)
    return global_train_dir

def add_timestamp_to_train_dir(train_base_dir):
    train_dir = os.path.join(
        train_base_dir, 
        datetime.now().strftime('%Y-%m-%d_%H-%M-%S'))

    return train_dir

def main(_):

    embed_path = FLAGS.embed_path or pjoin("data", "squad", "glove.trimmed.{}.{}.npz".format(FLAGS.glove_tokens, FLAGS.embedding_size))
    vocab_path = FLAGS.vocab_path or pjoin(FLAGS.data_dir, "vocab.dat")
    vocab, rev_vocab = initialize_vocab(vocab_path)

    train_dataset = initialize_dataset(FLAGS.data_dir, dataset_name="train")
    val_dataset = initialize_dataset(FLAGS.data_dir, dataset_name="val") # validation

    pretrained_embeddings = load_embeddings(embed_path)

    model = QAModel(size=FLAGS.state_size, vocab_dim=FLAGS.embedding_size)

    config = Config(
        max_question_length = MAX_QUESTION_LENGTH,
        max_context_length = MAX_CONTEXT_LENGTH, 
        embedding_size = FLAGS.embedding_size,
        state_size = FLAGS.state_size,
        learning_rate = FLAGS.learning_rate, 
        min_learning_rate = FLAGS.min_learning_rate, 
        num_epochs = FLAGS.epochs, 
        batch_size = FLAGS.batch_size, 
        max_grad_norm = FLAGS.max_gradient_norm,
        dropout_rate = FLAGS.dropout,
        optimizer_name = FLAGS.optimizer
    )

    qa = QASystem(model, config, pretrained_embeddings)

    if not os.path.exists(FLAGS.log_dir):
        os.makedirs(FLAGS.log_dir)
    file_handler = logging.FileHandler(pjoin(FLAGS.log_dir, "log.txt"))
    logging.getLogger().addHandler(file_handler)

    print(vars(FLAGS))
    with open(os.path.join(FLAGS.log_dir, "flags.json"), 'w') as fout:
        json.dump(FLAGS.__flags, fout)

    tf_config = tf.ConfigProto()
    tf_config.gpu_options.allow_growth = True
    tf_config.allow_soft_placement = True

    with tf.Session(config=tf_config) as sess:
        sess.run(tf.global_variables_initializer())

        load_train_dir = get_normalized_train_dir(FLAGS.load_train_dir or FLAGS.train_dir)
        initialize_model(sess, qa, load_train_dir)

        save_train_dir = get_normalized_train_dir(add_timestamp_to_train_dir(FLAGS.train_dir))

        with tf.device('/gpu:0'):
            qa.train(sess, train_dataset, val_dataset, save_train_dir)

if __name__ == "__main__":
    tf.app.run()
