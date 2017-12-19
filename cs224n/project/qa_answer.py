from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import io
import os
import json
import sys
import random
from os.path import join as pjoin

from tqdm import tqdm
import numpy as np
from six.moves import xrange
import tensorflow as tf

from qa_model import *
from preprocessing.squad_preprocess import data_from_json, maybe_download, squad_base_url, \
    invert_map, tokenize, token_idx_map
import qa_data

from util import Progbar, get_minibatches

import logging

logging.basicConfig(level=logging.INFO)

FLAGS = tf.app.flags.FLAGS

tf.app.flags.DEFINE_float("learning_rate", 0.001, "Learning rate.")
tf.app.flags.DEFINE_float("dropout", 0.0, "Fraction of units randomly dropped on non-recurrent connections.")
tf.app.flags.DEFINE_integer("batch_size", 80, "Batch size to use during training.")
tf.app.flags.DEFINE_integer("epochs", 0, "Number of epochs to train.")
tf.app.flags.DEFINE_integer("state_size", 300, "Size of each model layer.")
tf.app.flags.DEFINE_string("glove_tokens", "840B", "Number of tokens in glove training.")
tf.app.flags.DEFINE_integer("embedding_size", 300, "Size of the pretrained vocabulary.")
tf.app.flags.DEFINE_integer("keep", 0, "How many checkpoints to keep, 0 indicates keep all.")
tf.app.flags.DEFINE_string("train_dir", "train", "Training directory (default: ./train).")
tf.app.flags.DEFINE_string("log_dir", "log", "Path to store log and flag files (default: ./log)")
tf.app.flags.DEFINE_string("vocab_path", "data/squad/vocab.dat", "Path to vocab file (default: ./data/squad/vocab.dat)")
tf.app.flags.DEFINE_string("embed_path", "", "Path to the trimmed GLoVe embedding (default: ./data/squad/glove.trimmed.{embedding_size}.npz)")
tf.app.flags.DEFINE_string("dev_path", "data/squad/dev-v1.1.json", "Path to the JSON dev set to evaluate against (default: ./data/squad/dev-v1.1.json)")

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


def read_dataset(dataset, tier, vocab):
    """Reads the dataset, extracts context, question, answer,
    and answer pointer in their own file. Returns the number
    of questions and answers processed for the dataset"""

    context_data = []
    query_data = []
    question_uuid_data = []

    for articles_id in tqdm(range(len(dataset['data'])), desc="Preprocessing {}".format(tier)):
        article_paragraphs = dataset['data'][articles_id]['paragraphs']
        for pid in range(len(article_paragraphs)):
            context = article_paragraphs[pid]['context']
            # The following replacements are suggested in the paper
            # BidAF (Seo et al., 2016)
            context = context.replace("''", '" ')
            context = context.replace("``", '" ')

            context_tokens = tokenize(context)
            context_ids = [vocab.get(w.decode("UTF-8"), qa_data.UNK_ID) for w in context_tokens]

            qas = article_paragraphs[pid]['qas']
            for qid in range(len(qas)):
                question = qas[qid]['question']
                question_tokens = tokenize(question)
                question_uuid = qas[qid]['id']

                qustion_ids = [vocab.get(w.decode("UTF-8"), qa_data.UNK_ID) for w in question_tokens]

                context_data.append(pad(context_ids, MAX_CONTEXT_LENGTH))
                query_data.append(pad(qustion_ids, MAX_QUESTION_LENGTH))
                question_uuid_data.append(question_uuid)

    return context_data, query_data, question_uuid_data

def pad(ids, max_len):
    if len(ids) > max_len:
        return ids[0:max_len]
    else:
        return np.pad(ids, (0, max_len-len(ids)), 'constant', constant_values=0)

def prepare_dev(prefix, dev_filename, vocab):
    # Don't check file size, since we could be using other datasets
    dev_dataset = maybe_download(squad_base_url, dev_filename, prefix)

    dev_data = data_from_json(os.path.join(prefix, dev_filename))
    context_data, question_data, question_uuid_data = read_dataset(dev_data, 'dev', vocab)

    return [np.array(context_data), np.array(question_data), question_uuid_data]


def generate_answers(sess, model, dataset, rev_vocab, batch_size):
    """
    Loop over the dev or test dataset and generate answer.

    Note: output format must be answers[uuid] = "real answer"
    You must provide a string of words instead of just a list, or start and end index

    In main() function we are dumping onto a JSON file

    evaluate.py will take the output JSON along with the original JSON file
    and output a F1 and EM

    You must implement this function in order to submit to Leaderboard.

    :param sess: active TF session
    :param model: a built QASystem model
    :param rev_vocab: this is a list of vocabulary that maps index to actual words
    :return:
    """

    answers = {}

    all_contexts, all_questions, all_question_uuids = dataset

    prog = Progbar(target=1 + int(len(dataset[0]) / batch_size))
    for i, batch in enumerate(get_minibatches([all_contexts, all_questions, all_question_uuids], batch_size, shuffle=False)):
        contexts, questions, question_uuids = batch
        context_lengths = (contexts != 0).sum(1)

        pred_spans = model.answer(sess, [questions, contexts])
        pred_spans[:, 0] = np.minimum(pred_spans[:, 0], pred_spans[:, 1])
        pred_spans[:, 1] = np.minimum(pred_spans[:, 1], context_lengths)

        for (s, e), uuid, context in zip(pred_spans, question_uuids, contexts):
            answers[uuid] = " ".join([rev_vocab[context[c]] for c in range(s, e+1)])

        prog.update(i + 1, [])

    return answers

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

def load_embeddings(embed_path):
    return np.load(embed_path)["glove"]

def main(_):

    vocab, rev_vocab = initialize_vocab(FLAGS.vocab_path)

    embed_path = FLAGS.embed_path or pjoin("data", "squad", "glove.trimmed.{}.{}.npz".format(FLAGS.glove_tokens, FLAGS.embedding_size))
    pretrained_embeddings = load_embeddings(embed_path)

    if not os.path.exists(FLAGS.log_dir):
        os.makedirs(FLAGS.log_dir)
    file_handler = logging.FileHandler(pjoin(FLAGS.log_dir, "log.txt"))
    logging.getLogger().addHandler(file_handler)

    print(vars(FLAGS))
    with open(os.path.join(FLAGS.log_dir, "flags.json"), 'w') as fout:
        json.dump(FLAGS.__flags, fout)

    # ========= Load Dataset =========
    # You can change this code to load dataset in your own way

    dev_dirname = os.path.dirname(os.path.abspath(FLAGS.dev_path))
    dev_filename = os.path.basename(FLAGS.dev_path)
    context_data, question_data, question_uuid_data = prepare_dev(dev_dirname, dev_filename, vocab)
    dataset = (context_data, question_data, question_uuid_data)

    # ========= Model-specific =========
    # You must change the following code to adjust to your model

    model = QAModel(size=FLAGS.state_size, vocab_dim=FLAGS.embedding_size)

    config = Config(
        max_question_length = MAX_QUESTION_LENGTH,
        max_context_length = MAX_CONTEXT_LENGTH, 
        embedding_size = FLAGS.embedding_size, 
        state_size = FLAGS.state_size,
        learning_rate = FLAGS.learning_rate, 
        min_learning_rate = .001, 
        num_epochs = FLAGS.epochs, 
        batch_size = FLAGS.batch_size, 
        max_grad_norm = 10.0,
        dropout_rate = FLAGS.dropout,
        optimizer_name = "adam"
    )

    qa = QASystem(model, config, pretrained_embeddings)

    with tf.Session() as sess:
        train_dir = FLAGS.train_dir#get_normalized_train_dir(FLAGS.train_dir)
        initialize_model(sess, qa, train_dir)
        answers = generate_answers(sess, qa, dataset, rev_vocab, FLAGS.batch_size)

        # write to json file to root dir
        with io.open('dev-prediction.json', 'w', encoding='utf-8') as f:
            f.write(str(json.dumps(answers, ensure_ascii=False)))


if __name__ == "__main__":
  tf.app.run()
