# taken from https://github.com/ibab/tensorflow-wavenet/blob/master/wavenet/ops.py
# and slightly modified

import numpy as np
import tensorflow as tf

MAX_AUDIO_LEVEL = 32768.0 # 16 bit signed integer which has range (-32768, 32767)

def time_to_batch(value, dilation, name=None):
    with tf.name_scope('time_to_batch'):
        shape = tf.shape(value)
        pad_elements = dilation - 1 - (shape[1] + dilation - 1) % dilation
        padded = tf.pad(value, [[0, 0], [0, pad_elements], [0, 0]])
        reshaped = tf.reshape(padded, [-1, dilation, shape[2]])
        transposed = tf.transpose(reshaped, perm=[1, 0, 2])
        return tf.reshape(transposed, [shape[0] * dilation, -1, shape[2]])

def batch_to_time(value, dilation, name=None):
    with tf.name_scope('batch_to_time'):
        shape = tf.shape(value)
        prepared = tf.reshape(value, [dilation, -1, shape[2]])
        transposed = tf.transpose(prepared, perm=[1, 0, 2])
        return tf.reshape(transposed,
                          [tf.div(shape[0], dilation), -1, shape[2]])

def causal_conv(value, filter_, dilation, name='causal_conv'):
    with tf.name_scope(name):
        filter_width = tf.shape(filter_)[0]
        if dilation > 1:
            transformed = time_to_batch(value, dilation)
            conv = tf.nn.conv1d(transformed, filter_, stride=1,
                                padding='VALID')
            restored = batch_to_time(conv, dilation)
        else:
            restored = tf.nn.conv1d(value, filter_, stride=1, padding='VALID')
        # Remove excess elements at the end.
        out_width = tf.shape(value)[1] - (filter_width - 1) * dilation
        result = tf.slice(restored,
                          [0, 0, 0],
                          [-1, out_width, -1])
        return result

def generator_causal_conv(input_batch, state_batch, weights):
    '''Perform convolution for a single convolutional processing step.'''
    past_weights = weights[0, :, :]
    curr_weights = weights[1, :, :]
    output = tf.matmul(state_batch, past_weights) + tf.matmul(
        input_batch, curr_weights)
    return output

def create_variable(name, shape):
    '''Create a convolution filter variable with the specified name and shape,
    and initialize it using Xavier initialition.'''
    initializer = tf.contrib.layers.xavier_initializer_conv2d()
    variable = tf.get_variable(name = name, shape = shape, initializer = initializer)
    return variable

def create_bias_variable(name, shape):
    '''Create a bias variable with the specified name and shape and initialize
    it to zero.'''
    initializer = tf.constant_initializer(value=0.0, dtype=tf.float32)
    return tf.get_variable(name = name, shape = shape, initializer = initializer)

def causal_layer(
    input_batch,
    filter_width,
    quantization_channels,
    residual_channels
):
    with tf.variable_scope("causal_layer"):
        weights_filter = create_variable(
            "weights_filter", [filter_width, quantization_channels, residual_channels])
        return causal_conv(input_batch, weights_filter, dilation = 1)

def generator_causal_layer(
    input_batch, 
    state_batch, 
    filter_width,
    quantization_channels,
    residual_channels
):
    with tf.variable_scope('causal_layer'):
        weights_filter = create_variable(
            "weights_filter", [filter_width, quantization_channels, residual_channels])
        output = generator_causal_conv(input_batch, state_batch, weights_filter)
    return output

def dilation_layer(
    layer_idx,
    input_batch,
    filter_width,
    quantization_channels,
    residual_channels,
    dilation_channels,
    skip_channels,
    dilation,
    output_width,
    global_condition_batch_and_channels = None
):
    with tf.variable_scope("layer{}".format(layer_idx)):
        weights_filter = create_variable(
            "weights_filter", [filter_width, residual_channels, dilation_channels])
        weights_gate = create_variable(
            "weights_gate", [filter_width, residual_channels, dilation_channels])
        weights_dense = create_variable(
            "weights_dense", [1, dilation_channels, residual_channels])
        weights_skip = create_variable(
            "weights_skip", [1, dilation_channels, skip_channels])
        filter_bias = create_bias_variable(
            "filter_bias", [dilation_channels])
        gate_bias = create_bias_variable(
            "gate_bias", [dilation_channels])
        dense_bias = create_bias_variable(
            "dense_bias", [residual_channels])
        skip_bias = create_bias_variable(
            "skip_bias", [skip_channels])

        global_condition_batch_and_weights = None 
        if global_condition_batch_and_channels is not None:
            gc_batch, gc_channels = global_condition_batch_and_channels
            weights_gc_filter = create_variable(
                "weights_gc_filter", [1, gc_channels, dilation_channels])
            weights_gc_gate = create_variable(
                "weights_gc_gate", [1, gc_channels, dilation_channels])
            global_condition_batch_and_weights = (gc_batch, weights_gc_filter, weights_gc_gate)

        return _create_dilation_layer(
            input_batch,
            weights_filter,
            weights_gate,
            weights_dense,
            weights_skip,
            (filter_bias, gate_bias),
            (dense_bias, skip_bias),
            dilation,
            output_width,
            global_condition_batch_and_weights
        )

def _create_dilation_layer(
    input_batch,
    weights_filter,
    weights_gate,
    weights_dense,
    weights_skip,
    filter_and_gate_biases,
    dense_and_skip_biases,
    dilation,
    output_width,
    global_condition_batch_and_weights
):
    '''Creates a single causal dilated convolution layer.
    Args:
         input_batch: Input to the dilation layer.
         weights_filter: filter tensor of shape (filter_width, in_channels, out_channels)
         weights_gate: gate tensor of shape (filter_width, in_channels, out_channels)
         weights_dense: ...
         weights_skip: ...
         filter_and_gate_biases: ...
         dense_and_skip_biases: ...
         dilation: Integer specifying the dilation size.
         output_width: ...
         global_condition_batch_and_weights: Tuple of tensor, filter weights, and gate weights
             for global conditioning. Tensor contains the global data upon
             which the output is to be conditioned upon. Shape:
             [batch size, 1, channels]. The 1 is for the axis
             corresponding to time so that the result is broadcast to
             all time steps.
    The layer contains a gated filter that connects to dense output
    and to a skip connection:
           |-> [gate]   -|        |-> 1x1 conv -> skip output
           |             |-> (*) -|
    input -|-> [filter] -|        |-> 1x1 conv -|
           |                                    |-> (+) -> dense output
           |------------------------------------|
    Where `[gate]` and `[filter]` are causal convolutions with a
    non-linear activation at the output. Biases and global conditioning
    are omitted due to the limits of ASCII art.
    '''

    conv_filter = causal_conv(input_batch, weights_filter, dilation)
    conv_gate = causal_conv(input_batch, weights_gate, dilation)

    if global_condition_batch_and_weights is not None:
        global_condition_batch, weights_gc_filter, weights_gc_gate = global_condition_batch_and_weights
        
        out_cut = tf.shape(global_condition_batch)[1] - tf.shape(conv_filter)[1]
        global_condition_batch = tf.slice(global_condition_batch, [0, out_cut, 0], [-1, -1, -1])

        conv_filter = conv_filter + tf.nn.conv1d(global_condition_batch,
                                                 weights_gc_filter,
                                                 stride=1,
                                                 padding="SAME",
                                                 name="gc_filter")
        conv_gate = conv_gate + tf.nn.conv1d(global_condition_batch,
                                             weights_gc_gate,
                                             stride=1,
                                             padding="SAME",
                                             name="gc_gate")

    if filter_and_gate_biases is not None:
        filter_bias, gate_bias = filter_and_gate_biases
        conv_filter = tf.add(conv_filter, filter_bias)
        conv_gate = tf.add(conv_gate, gate_bias)

    out = tf.tanh(conv_filter) * tf.sigmoid(conv_gate)

    # The 1x1 conv to produce the residual output
    transformed = tf.nn.conv1d(
        out, weights_dense, stride=1, padding="SAME", name="dense")

    # The 1x1 conv to produce the skip output
    skip_cut = tf.shape(out)[1] - output_width
    out_skip = tf.slice(out, [0, skip_cut, 0], [-1, -1, -1])
    skip_contribution = tf.nn.conv1d(
        out_skip, weights_skip, stride=1, padding="SAME", name="skip")

    if dense_and_skip_biases is not None:
        dense_bias, skip_bias = dense_and_skip_biases
        transformed = transformed + dense_bias
        skip_contribution = skip_contribution + skip_bias

    input_cut = tf.shape(input_batch)[1] - tf.shape(transformed)[1]
    input_batch = tf.slice(input_batch, [0, input_cut, 0], [-1, -1, -1])

    return skip_contribution, input_batch + transformed

def generator_dilation_layer(
    layer_idx,
    input_batch,
    state_batch,
    filter_width,
    quantization_channels,
    residual_channels,
    dilation_channels,
    skip_channels,
    dilation,
    global_condition_batch_and_channels = None
):
    with tf.variable_scope("layer{}".format(layer_idx)):
        weights_filter = create_variable(
            "weights_filter", [filter_width, residual_channels, dilation_channels])
        weights_gate = create_variable(
            "weights_gate", [filter_width, residual_channels, dilation_channels])
        weights_dense = create_variable(
            "weights_dense", [1, dilation_channels, residual_channels])
        weights_skip = create_variable(
            "weights_skip", [1, dilation_channels, skip_channels])
        filter_bias = create_bias_variable(
            "filter_bias", [dilation_channels])
        gate_bias = create_bias_variable(
            "gate_bias", [dilation_channels])
        dense_bias = create_bias_variable(
            "dense_bias", [residual_channels])
        skip_bias = create_bias_variable(
            "skip_bias", [skip_channels])

        global_condition_batch_and_weights = None 
        if global_condition_batch_and_channels is not None:
            gc_batch, gc_channels = global_condition_batch_and_channels
            weights_gc_filter = create_variable(
                "weights_gc_filter", [1, gc_channels, dilation_channels])
            weights_gc_gate = create_variable(
                "weights_gc_gate", [1, gc_channels, dilation_channels])
            global_condition_batch_and_weights = (gc_batch, weights_gc_filter, weights_gc_gate)

        return _create_generator_dilation_layer(
            input_batch,
            state_batch,
            weights_filter,
            weights_gate,
            weights_dense,
            weights_skip,
            (filter_bias, gate_bias),
            (dense_bias, skip_bias),
            dilation,
            global_condition_batch_and_weights)

def _create_generator_dilation_layer(
    input_batch,
    state_batch,
    weights_filter,
    weights_gate,
    weights_dense,
    weights_skip,
    filter_and_gate_biases,
    dense_and_skip_biases,
    dilation,
    global_condition_batch_and_weights):

    output_filter = generator_causal_conv(input_batch, state_batch, weights_filter)
    output_gate = generator_causal_conv(input_batch, state_batch, weights_gate)

    if global_condition_batch_and_weights is not None:
        global_condition_batch, weights_gc_filter, weights_gc_gate = global_condition_batch_and_weights
        global_condition_batch = tf.reshape(global_condition_batch, shape=(1, -1))

        weights_gc_filter = weights_gc_filter[0, :, :]
        output_filter += tf.matmul(global_condition_batch, weights_gc_filter)

        weights_gc_gate = weights_gc_gate[0, :, :]
        output_gate += tf.matmul(global_condition_batch, weights_gc_gate)

    if filter_and_gate_biases is not None:
        filter_bias, gate_bias = filter_and_gate_biases
        output_filter = tf.add(output_filter, filter_bias)
        output_gate = tf.add(output_gate, gate_bias)

    out = tf.tanh(output_filter) * tf.sigmoid(output_gate)

    transformed = tf.matmul(out, weights_dense[0, :, :])
    skip_contribution = tf.matmul(out, weights_skip[0, :, :])

    if dense_and_skip_biases is not None:
        dense_bias, skip_bias = dense_and_skip_biases
        transformed = transformed + dense_bias
        skip_contribution = skip_contribution + skip_bias

    return skip_contribution, input_batch + transformed

def output_layer(dilation_outputs, quantization_channels, skip_channels):
    with tf.variable_scope("postprocessing"):
        w1 = create_variable("w1", [1, skip_channels, skip_channels])
        w2 = create_variable("w2", [1, skip_channels, quantization_channels])
        b1 = create_bias_variable("b1", [skip_channels])
        b2 = create_bias_variable("b2", [quantization_channels])

        return _create_output_layer(dilation_outputs, w1, w2, b1, b2)

def _create_output_layer(dilation_outputs, w1, w2, b1, b2):
    total = sum(dilation_outputs)
    transformed1 = tf.nn.relu(total)
    conv1 = tf.nn.conv1d(transformed1, w1, stride=1, padding="SAME")
    conv1 = tf.add(conv1, b1)

    transformed2 = tf.nn.relu(conv1)
    conv2 = tf.nn.conv1d(transformed2, w2, stride=1, padding="SAME")
    conv2 = tf.add(conv2, b2)

    return conv2

def generator_output_layer(dilation_outputs, quantization_channels, skip_channels):
    with tf.variable_scope("postprocessing"):
        w1 = create_variable("w1", [1, skip_channels, skip_channels])
        w2 = create_variable("w2", [1, skip_channels, quantization_channels])
        b1 = create_bias_variable("b1", [skip_channels])
        b2 = create_bias_variable("b2", [quantization_channels])

        return _create_generator_output_layer(dilation_outputs, w1, w2, b1, b2)

def _create_generator_output_layer(dilation_outputs, w1, w2, b1, b2):
    total = sum(dilation_outputs)
    transformed1 = tf.nn.relu(total)
    conv1 = tf.matmul(transformed1, w1[0, :, :])
    conv1 = tf.add(conv1, b1)

    transformed2 = tf.nn.relu(conv1)
    conv2 = tf.matmul(transformed2, w2[0, :, :])
    conv2 = tf.add(conv2, b2)

    return conv2

def receptive_field(filter_width, dilations):
    return (filter_width - 1) * sum(dilations) + filter_width

def one_hot(input_batch, quantization_channels):
    '''One-hot encodes the waveform amplitudes.
    This allows the definition of the network as a categorical distribution
    over a finite set of possible amplitudes.
    '''
    with tf.name_scope('one_hot_encode'):
        encoded = tf.one_hot(
            input_batch,
            depth=quantization_channels,
            dtype=tf.float32)
        shape = [tf.shape(input_batch)[0], -1, quantization_channels]
        encoded = tf.reshape(encoded, shape)
    return encoded

def to_audio(samples, quant_channels):
    audio = 2 * (samples / quant_channels) - 1
    signs = np.sign(audio) 
    audio *= signs * np.log(1 + quant_channels)
    audio = (np.exp(audio) - 1) / quant_channels
    audio *= MAX_AUDIO_LEVEL * signs

    return audio