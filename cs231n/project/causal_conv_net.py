import tensorflow as tf
from . import wavenet_ops

class CausalConvNetConfig:

    def __init__(
        self,
        filter_width,
        quantization_channels,
        residual_channels,
        dilation_channels,
        condition_channels,
        skip_channels,
        dilations,
        output_width,
        generator_batch_size
    ):
        self.filter_width = filter_width
        self.quantization_channels = quantization_channels
        self.residual_channels = residual_channels
        self.dilation_channels = dilation_channels
        self.condition_channels = condition_channels
        self.skip_channels = skip_channels
        self.dilations = dilations
        self.output_width = output_width
        self.generator_batch_size = generator_batch_size

class CausalConvNet:

    def __init__(self, config):
        self.config = config
        self.receptive_field = wavenet_ops.receptive_field(config.filter_width, config.dilations)

    def encode(self, inputs, inputs_condition):
        # Cut off the last sample of network input to preserve causality
        layer = tf.slice(inputs, [0, 0, 0], [-1, tf.shape(inputs)[1] - 1, -1])

        # Add zero pad at the beginning to line up with video frames
        # I.e. video frames at time t used to predict output audio at time t,
        # but only audio frames at time < t used to predict audio at time t
        layer = tf.pad(layer, [[0, 0], [1, 0], [0, 0]])

        layer = wavenet_ops.causal_layer(
            input_batch = layer, 
            filter_width = self.config.filter_width,
            quantization_channels = self.config.quantization_channels,
            residual_channels = self.config.residual_channels
        )

        with tf.variable_scope("conv_gc"):
            layer_gc = wavenet_ops.causal_layer(
                input_batch = inputs_condition, 
                filter_width = self.config.filter_width,
                quantization_channels = self.config.condition_channels,
                residual_channels = self.config.condition_channels
            )

        dilation_outputs = []
        for i, dilation in enumerate(self.config.dilations):
            with tf.variable_scope("conv_gc"):
                output_gc, layer_gc = self._create_dilation_layer(i, layer_gc, None, dilation, residual_channels = self.config.condition_channels)
            cond_cut = tf.shape(inputs_condition)[1] - tf.shape(layer_gc)[1]
            cond = tf.slice(inputs_condition, [0, cond_cut, 0], [-1, -1, -1]) + layer_gc

            output, layer = self._create_dilation_layer(i, layer, cond, dilation)
            dilation_outputs.append(output)

        output = self._create_output_layer(dilation_outputs)

        return output

    def generate(self, inputs, inputs_condition):
        init_ops = []
        push_ops = []
        outputs = []
        
        layer = inputs
        layer_gc = inputs_condition

        q = tf.FIFOQueue(
            1,
            dtypes=tf.float32,
            shapes=(self.config.generator_batch_size, self.config.quantization_channels))
        init = q.enqueue_many(
            tf.zeros((1, self.config.generator_batch_size, self.config.quantization_channels)))

        q_gc = tf.FIFOQueue(
            1,
            dtypes=tf.float32,
            shapes=(self.config.generator_batch_size, self.config.condition_channels))
        init_gc = q_gc.enqueue_many(
            tf.zeros((1, self.config.generator_batch_size, self.config.condition_channels)))

        current_state = q.dequeue()
        current_state_gc = q_gc.dequeue()
        push = q.enqueue([layer])
        push_gc = q_gc.enqueue([layer_gc])
        init_ops.extend([init, init_gc])
        push_ops.extend([push, push_gc])

        layer = wavenet_ops.generator_causal_layer(
            input_batch = layer,
            state_batch = current_state,
            filter_width = self.config.filter_width,
            quantization_channels = self.config.quantization_channels,
            residual_channels = self.config.residual_channels
        )

        with tf.variable_scope("conv_gc"):
            layer_gc = wavenet_ops.generator_causal_layer(
                input_batch = layer_gc,
                state_batch = current_state_gc,
                filter_width = self.config.filter_width,
                quantization_channels = self.config.condition_channels,
                residual_channels = self.config.condition_channels
            )

        dilation_outputs = []
        for i, dilation in enumerate(self.config.dilations):
            q = tf.FIFOQueue(dilation, dtypes=tf.float32, shapes=(self.config.generator_batch_size, self.config.residual_channels))
            init = q.enqueue_many(tf.zeros((dilation, self.config.generator_batch_size, self.config.residual_channels)))

            q_gc = tf.FIFOQueue(dilation, dtypes=tf.float32, shapes=(self.config.generator_batch_size, self.config.condition_channels))
            init_gc = q_gc.enqueue_many(tf.zeros((dilation, self.config.generator_batch_size, self.config.condition_channels)))

            current_state = q.dequeue()
            current_state_gc = q_gc.dequeue()
            push = q.enqueue([layer])
            push_gc = q_gc.enqueue([layer_gc])
            init_ops.extend([init, init_gc])
            push_ops.extend([push, push_gc])

            with tf.variable_scope("conv_gc"):
                output_gc, layer_gc = self._create_generator_dilation_layer(
                    i, layer_gc, current_state_gc, inputs_condition = None, dilation = dilation, residual_channels = self.config.condition_channels)
                cond = inputs_condition + layer_gc

            output, layer = self._create_generator_dilation_layer(i, layer, current_state, cond, dilation)
            dilation_outputs.append(output)

        output = self._create_generator_output_layer(dilation_outputs)

        return output, init_ops, push_ops

    def _create_dilation_layer(self, layer_idx, inputs, inputs_condition, dilation, residual_channels = None):
        return wavenet_ops.dilation_layer(
            layer_idx = layer_idx,
            input_batch = inputs,
            filter_width = self.config.filter_width,
            quantization_channels = self.config.quantization_channels,
            residual_channels = residual_channels if residual_channels is not None else self.config.residual_channels,
            dilation_channels = self.config.dilation_channels,
            skip_channels = self.config.skip_channels,
            dilation = dilation,
            output_width = self.config.output_width,
            global_condition_batch_and_channels = (inputs_condition, self.config.condition_channels) if inputs_condition is not None else None
        )

    def _create_generator_dilation_layer(self, layer_idx, inputs, current_state, inputs_condition, dilation, residual_channels = None):
        return wavenet_ops.generator_dilation_layer(
            layer_idx = layer_idx,
            input_batch = inputs,
            state_batch = current_state,
            filter_width = self.config.filter_width,
            quantization_channels = self.config.quantization_channels,
            residual_channels = residual_channels if residual_channels is not None else self.config.residual_channels,
            dilation_channels = self.config.dilation_channels,
            skip_channels = self.config.skip_channels,
            dilation = dilation,
            global_condition_batch_and_channels = (inputs_condition, self.config.condition_channels) if inputs_condition is not None else None
        )

    def _create_output_layer(self, dilation_outputs):
        return wavenet_ops.output_layer(
            dilation_outputs,
            quantization_channels = self.config.quantization_channels, 
            skip_channels = self.config.skip_channels
        )

    def _create_generator_output_layer(self, dilation_outputs):
        return wavenet_ops.generator_output_layer(
            dilation_outputs,
            quantization_channels = self.config.quantization_channels, 
            skip_channels = self.config.skip_channels
        )