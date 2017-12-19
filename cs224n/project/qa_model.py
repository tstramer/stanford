import time
import logging

import numpy as np
import numpy.linalg

from six.moves import xrange  # pylint: disable=redefined-builtin
import tensorflow as tf
from tensorflow.python.ops import variable_scope as vs

from evaluate import exact_match_score, f1_score
from util import Progbar, get_minibatches

from os.path import join as pjoin

from collections import Counter

from cells import *

from tf_util import *

logging.basicConfig(level=logging.INFO)
logging.basicConfig(format='%(message)s', level=logging.DEBUG)

logger = logging.getLogger("hw4.qa_model")
logger.setLevel(logging.DEBUG)

np.set_printoptions(threshold=np.inf)

MAX_CONTEXT_LENGTH = 300
MAX_QUESTION_LENGTH = 40

class Config:
    """Holds model hyperparams and data information.
    """

    def __init__(
        self, 
        max_question_length, 
        max_context_length, 
        embedding_size, 
        state_size,
        learning_rate, 
        min_learning_rate, 
        num_epochs, 
        batch_size, 
        max_grad_norm,
        dropout_rate,
        optimizer_name
    ):
        self.max_question_length = max_question_length
        self.max_context_length = max_context_length
        self.embedding_size = embedding_size
        self.state_size = state_size
        self.learning_rate = learning_rate
        self.min_learning_rate = min_learning_rate
        self.num_epochs = num_epochs
        self.batch_size = batch_size
        self.max_grad_norm = max_grad_norm
        self.dropout_rate = dropout_rate
        self.optimizer_name = optimizer_name

def get_optimizer(opt):
    if opt == "adam":
        optfn = tf.train.AdamOptimizer
    elif opt == "sgd":
        optfn = tf.train.GradientDescentOptimizer
    else:
        assert (False)
    return optfn

class QAModel(object):

    def __init__(self, size, vocab_dim):
        self.size = size
        self.vocab_dim = vocab_dim
        self.lstm_init = tf.contrib.layers.xavier_initializer()

    def encode(self, inputs, sequence_lengths, encoder_state_input = None, dropout_rate=None):
        """
        :param inputs: Symbolic representations of the input
        :param sequence_lengths: this is to make sure tf.nn.dynamic_rnn doesn't iterate
                      through masked steps
        :param encoder_state_input: (Optional) pass this as initial hidden state
                                    to tf.nn.dynamic_rnn to build conditional representations
        """

        Q, S = tf.nn.dynamic_rnn(
            self.lstm_cell(dropout_rate = dropout_rate), 
            inputs,
            dtype=tf.float32,
            sequence_length = sequence_lengths,
            initial_state = encoder_state_input)

        return Q, S

    def encode_bi(self, inputs, sequence_lengths, encoder_state_input = [None, None], dropout_rate=None):
        """
        :param inputs: Symbolic representations of the input
        :param sequence_lengths: this is to make sure tf.nn.dynamic_rnn doesn't iterate
                      through masked steps
        :param encoder_state_input: (Optional) pass this as initial hidden state
                                    to tf.nn.dynamic_rnn to build conditional representations
        """

        (Of, Ob), S = tf.nn.bidirectional_dynamic_rnn(
            self.lstm_cell(dropout_rate = dropout_rate), 
            self.lstm_cell(dropout_rate = dropout_rate),
            inputs,
            dtype=tf.float32,
            sequence_length = sequence_lengths,
            initial_state_fw = encoder_state_input[0],
            initial_state_bw = encoder_state_input[1])

        return tf.concat([Of, Ob], 2), S

    def match_lstm(self, H_q, H_q_size, H_p, sequence_lengths, log_question_masks, dropout_rate=None):
        attn_cell = self.attention_cell(H_q, H_q_size, log_question_masks, dropout_rate)

        (Of, Ob), S = bidirectional_dynamic_rnn_reuse_fw_bw(
            attn_cell,
            attn_cell,
            H_p,
            dtype=tf.float32,
            sequence_length = sequence_lengths)

        return tf.concat([Of, Ob], 2), S

    def decode(self, batch_size, attn_vectors, attn_size, log_context_masks, dropout_rate=None):
        ptr_cell = self.ptr_network_cell(attn_vectors, attn_size, log_context_masks, dropout_rate)

        init_state = ptr_cell.zero_state(batch_size, tf.float32)
        
        with tf.variable_scope("rnn_1"):
            B0, S0 = ptr_cell(inputs = None, state = init_state)
            tf.get_variable_scope().reuse_variables()
            B1, S1 = ptr_cell(inputs = None, state = S0)

        return B0, B1

    def ptr_network_cell(self, attn_vectors, attn_size, log_context_masks, dropout_rate=None):
        lstm_cell = self.lstm_cell(dropout_rate = dropout_rate, size_override = self.size)
        cell = PtrNetworkCell(
            attn_size,
            attn_vectors,
            self.size,
            lstm_cell,
            log_context_masks)
        return cell

    def attention_cell(self, H_q, H_q_size, log_question_masks, dropout_rate=None):
        lstm_cell = self.lstm_cell(dropout_rate = dropout_rate, size_override = self.size)
        cell = AttentionLSTMCell(
            H_q_size,
            H_q,
            self.size,
            lstm_cell,
            log_question_masks)
        return cell

    def lstm_cell(self, size_override=None, dropout_rate=None):
        cell = tf.contrib.rnn.LSTMCell(size_override or self.size, initializer=self.lstm_init)
        if dropout_rate is not None:
            return tf.contrib.rnn.DropoutWrapper(cell, output_keep_prob=1-dropout_rate)
        else:
            return cell
        
class QASystem(object):

    def __init__(self, model, config, pretrained_embeddings):
        """
        Initializes the system
        :param model: an model that you constructed in train.py
        :param args: pass in more arguments as needed
        """

        self.model = model
        self.pretrained_embeddings = pretrained_embeddings
        self.config = config
        
        # ==== set up placeholder tokens ========

        self.questions = tf.placeholder(tf.int32, shape=(None, self.config.max_question_length))
        self.question_lengths = tf.placeholder(tf.int32, shape=(None))
        self.contexts = tf.placeholder(tf.int32, shape=(None, self.config.max_context_length))
        self.context_lengths = tf.placeholder(tf.int32, shape=(None))
        self.labels = tf.placeholder(tf.int32, shape=(None, 2)) # (start, end)

        self.context_masks = tf.placeholder(tf.bool, shape=(None, self.config.max_context_length))
        self.log_context_masks = tf.log(tf.cast(self.context_masks, tf.float32))

        self.question_masks = tf.placeholder(tf.bool, shape=(None, self.config.max_question_length))
        self.log_question_masks = tf.log(tf.cast(self.question_masks, tf.float32))

        self.dropout_rate = tf.placeholder(tf.float32, shape=[])

        # ==== assemble pieces ====
        with tf.variable_scope("qa", initializer=tf.uniform_unit_scaling_initializer(1.0)):
            self.setup_embeddings()

            self.pred_op = self.get_pred_op(
                self.question_embeddings, self.question_lengths,
                self.context_embeddings, self.context_lengths)

            self.loss = self.get_loss(self.pred_op, self.labels)
            
            self.train_op = self.get_training_op(self.loss)

        self.saver = tf.train.Saver()

    def get_pred_op(self, question_embeddings, question_lengths, context_embeddings, context_lengths):
        """
        Assemble the reading comprehension system
        """

        with tf.variable_scope("question"):
            H_q, _ = self.model.encode_bi(question_embeddings, question_lengths, dropout_rate=self.dropout_rate)

        with tf.variable_scope("passage"):
            H_p, _ = self.model.encode(context_embeddings, context_lengths, dropout_rate=self.dropout_rate)

        with tf.variable_scope("match_lstm"):
            M, _ = self.model.match_lstm(
                H_q,
                self.config.max_question_length,
                H_p,
                context_lengths,
                self.log_question_masks,
                dropout_rate=self.dropout_rate)

        with tf.variable_scope("ptr_network"):
            B0, B1 = self.model.decode(
                tf.shape(context_embeddings)[0],
                M,
                self.config.max_context_length,
                self.log_context_masks,
                dropout_rate=self.dropout_rate)

        return B0, B1

    def get_loss(self, preds, labels):
        """
        Loss computation for training
        """

        with tf.variable_scope("loss"):
            example_idxs = tf.range(0, tf.shape(labels)[0])
            start_indices = tf.stack([example_idxs, labels[:, 0]], axis=1)  
            end_indices = tf.stack([example_idxs, labels[:, 1]], axis=1)

            start_preds = tf.gather_nd(preds[0], start_indices)
            end_preds = tf.gather_nd(preds[1], end_indices)
            
            losses = -(tf.log(start_preds + 1e-8) + tf.log(end_preds + 1e-8))

            return tf.reduce_mean(losses)

    def setup_embeddings(self):
        """
        Loads distributed word representations based on placeholder tokens
        """

        with tf.variable_scope("embeddings"):
            embeddings_tensor = self.pretrained_embeddings.astype(np.float32)
            self.question_embeddings = tf.nn.embedding_lookup(embeddings_tensor, self.questions)
            self.context_embeddings = tf.nn.embedding_lookup(embeddings_tensor, self.contexts)

    def get_training_op(self, loss):
        params = tf.trainable_variables()

        with tf.variable_scope("train"):
            optimizer = get_optimizer(self.config.optimizer_name)(learning_rate=self.config.learning_rate)

            gradients, self.grad_norm = tf.clip_by_global_norm(
                tf.gradients(loss, params), self.config.max_grad_norm)

            return optimizer.apply_gradients(zip(gradients, params))

    def optimize(self, session, batch):
        """
        Takes in actual data to optimize the model
        This method is equivalent to a step() function
        """

        questions, contexts, spans = batch

        input_feed = self.base_input_feed(questions, contexts, dropout_rate = self.config.dropout_rate)
        input_feed[self.labels] = spans

        output_feed = [
            self.train_op,
            self.loss,
            self.grad_norm
        ]

        outputs = session.run(output_feed, input_feed)

        return outputs

    def test(self, session, valid_x, valid_y):
        """
        Computes a cost for the validation set
        """

        questions, contexts = valid_x

        input_feed = self.base_input_feed(questions, contexts, dropout_rate = 0)
        input_feed[self.labels] = valid_y

        output_feed = [
            self.loss
        ]

        outputs = session.run(output_feed, input_feed)

        return outputs[0]

    def decode(self, session, test_x):
        """
        Returns the probability distribution over different positions in the paragraph
        so that other methods like self.answer() will be able to work properly
        """

        questions, contexts = test_x

        input_feed = self.base_input_feed(questions, contexts, dropout_rate = 0)

        output_feed = [self.pred_op]

        outputs = session.run(output_feed, input_feed)

        return outputs[0]

    def answer(self, session, test_x):
        questions, contexts = test_x
        preds = self.decode(session, [questions, contexts])
        return self._preds_to_spans(preds)

    def _preds_to_spans(self, preds):
        start = np.argmax(preds[0], axis=1)
        mask = np.zeros_like(preds[0])
        for i in range(np.shape(preds[0])[0]):
            mask[i, start[i]:] = 1
        end = np.argmax(preds[1] * mask, axis=1)
        return np.stack([start, end], axis=1)

        start = np.argmax(preds[0], axis=1)
        end = np.argmax(preds[1], axis=1)
        return np.stack([start, end], axis=1)

    def validate(self, session, valid_dataset):
        """
        Iterate through the validation dataset and determine what
        the validation cost is.

        This method calls self.test() which explicitly calculates validation cost.
        """

        valid_cost = 0

        for valid_x, valid_y in valid_dataset:
          valid_cost = self.test(session, valid_x, valid_y)

        return valid_cost

    def evaluate_answer(self, session, dataset, sample=100, log=False, ds_type="dev"):
        """
        Evaluate the model's performance using the harmonic mean of F1 and Exact Match (EM)
        with the set of true answer labels

        This step actually takes quite some time. So we can only sample 100 examples
        from either training or testing set.

        :param session: session should always be centrally managed in train.py
        :param dataset: a representation of our data, in some implementations, you can
                        pass in multiple components (arguments) of one dataset to this function
        :param sample: how many examples in dataset we look at
        :param log: whether we print to std out stream
        """

        f1 = 0.
        em = 0.

        sampled_dataset = next(get_minibatches(dataset, sample, shuffle=True))
        questions, contexts, spans = sampled_dataset

        tic = time.time()
        preds = self.decode(session, [questions, contexts])
        pred_spans = self._preds_to_spans(preds)
        toc = time.time()
        logger.info("evaluate_answer pred_spans: took %f secs" % (toc - tic))

        logger.info ("Predicted/actual spans: ")
        for p, a in zip(pred_spans, spans):
            logger.info (str(p) + " || " + str(a))

        for context, pred_span, actual_span in zip(contexts, pred_spans, spans):
            em += self.exact_match_score(pred_span, actual_span)

            tokens_pred = context[pred_span[0]:pred_span[1]+1]
            tokens_actual = context[actual_span[0]:actual_span[1]+1]
            f1 += self.f1_score(tokens_pred, tokens_actual)

        if log:
            logger.info("{}: F1: {}, EM: {}, for {} samples".format(ds_type, f1, em, sample))

        return f1, em

    def exact_match_score(self, pred_span, actual_span):
        if np.array_equal(pred_span, actual_span):
            return 1
        else:
            return 0

    def f1_score(self, prediction_tokens, ground_truth_tokens):
        common = Counter(prediction_tokens) & Counter(ground_truth_tokens)
        num_same = sum(common.values())
        if num_same == 0:
            return 0
        precision = 1.0 * num_same / len(prediction_tokens)
        recall = 1.0 * num_same / len(ground_truth_tokens)
        f1 = (2 * precision * recall) / (precision + recall)
        return f1

    def train(self, session, train_dataset, val_dataset, train_dir):
        """
        Implement main training loop

        :param session: it should be passed in from train.py
        :param dataset: a representation of our data, in some implementations, you can
                        pass in multiple components (arguments) of one dataset to this function
        :param train_dir: path to the directory where you should save the model checkpoint
        """

        tic = time.time()
        params = tf.trainable_variables()
        num_params = sum(map(lambda t: np.prod(tf.shape(t.value()).eval()), params))
        toc = time.time()
        logger.info("Number of params: %d (retreival took %f secs)" % (num_params, toc - tic))

        print("Variables: %s" % [c.name for c in tf.get_collection(tf.GraphKeys.GLOBAL_VARIABLES)])

        best_score = 0

        for epoch in range(self.config.num_epochs):
            logger.info("\nEpoch %d out of %d", epoch + 1, self.config.num_epochs)
            score = self.run_epoch(session, train_dataset, val_dataset)
            save_path = self.saver.save(session, pjoin(train_dir, "model_epoch"), global_step=epoch)
            logger.info("Model saved in file: %s" % save_path)

            if score > best_score:
                best_score = score
                save_path = self.saver.save(session, pjoin(train_dir, "model_best"))
                logger.info("New best model (score=%d) saved in file: %s" % (best_score, save_path))

        return best_score

    def run_epoch(self, session, train_dataset, val_dataset):
        prog = Progbar(target=1 + int(len(train_dataset[0]) / self.config.batch_size), logger=logger)
        for i, train_batch in enumerate(get_minibatches(train_dataset, self.config.batch_size, shuffle=True)):
            _, loss, grad_norm = self.optimize(session, train_batch)
            prog.update(i + 1, [("train loss", loss), ("grad norm", grad_norm)])

        self.evaluate_answer(session, train_dataset, log=True, ds_type="Train dataset")
        f1, em = self.evaluate_answer(session, val_dataset, log=True, ds_type="Dev dataset")
        return f1

    def base_input_feed(self, questions, contexts, dropout_rate):
        question_lengths = (questions != 0).sum(1)
        context_lengths = (contexts != 0).sum(1)

        context_masks = np.zeros(np.shape(contexts), dtype=bool)
        for i in range(np.shape(contexts)[0]):
            context_masks[i][0:context_lengths[i]] = True

        question_masks = np.zeros(np.shape(questions), dtype=bool)
        for i in range(np.shape(questions)[0]):
            question_masks[i][0:question_lengths[i]] = True

        input_feed = {
            self.questions: questions,
            self.question_lengths: question_lengths,
            self.question_masks: question_masks,
            self.contexts: contexts,
            self.context_lengths: context_lengths,
            self.context_masks: context_masks,
            self.dropout_rate: dropout_rate
        }

        return input_feed
