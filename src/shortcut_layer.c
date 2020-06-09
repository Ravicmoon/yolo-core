#include "shortcut_layer.h"

#include <assert.h>
#include <stdio.h>

#include "blas.h"
#include "convolutional_layer.h"
#include "dark_cuda.h"
#include "gemm.h"
#include "utils.h"

void FillShortcutLayer(layer* l, int batch, int n, int* input_layers,
    int* input_sizes, int w, int h, int c, float** layers_output,
    float** layers_delta, float** layers_output_gpu, float** layers_delta_gpu,
    WEIGHTS_TYPE_T weights_type, WEIGHTS_NORMALIZATION_T weights_normalization,
    ACTIVATION activation, int train)
{
  fprintf(stderr, "Shortcut Layer: ");
  for (int i = 0; i < n; ++i)
  {
    fprintf(stderr, "%d, ", input_layers[i]);
  }

  l->train = train;
  l->type = SHORTCUT;
  l->batch = batch;
  l->activation = activation;
  l->n = n;
  l->input_layers = input_layers;
  l->input_sizes = input_sizes;
  l->layers_output = layers_output;
  l->layers_delta = layers_delta;
  l->weights_type = weights_type;
  l->weights_normalization = weights_normalization;
  l->learning_rate_scale = 1;  // not necessary

  l->w = l->out_w = w;
  l->h = l->out_h = h;
  l->c = l->out_c = c;
  l->outputs = w * h * c;
  l->inputs = l->outputs;

  l->index = l->input_layers[0];

  if (train)
    l->delta = (float*)xcalloc(l->outputs * batch, sizeof(float));
  l->output = (float*)xcalloc(l->outputs * batch, sizeof(float));

  l->nweights = 0;
  if (l->weights_type == PER_FEATURE)
    l->nweights = (l->n + 1);
  else if (l->weights_type == PER_CHANNEL)
    l->nweights = (l->n + 1) * l->c;

  if (l->nweights > 0)
  {
    l->weights = (float*)calloc(l->nweights, sizeof(float));
    for (int i = 0; i < l->nweights; ++i)
    {
      l->weights[i] = 1;
    }

    if (train)
      l->weight_updates = (float*)calloc(l->nweights, sizeof(float));
    l->update = UpdateShortcutLayer;
  }

  l->forward = ForwardShortcutLayer;
  l->backward = BackwardShortcutLayer;

#ifndef GPU
  if (l->activation == SWISH || l->activation == MISH)
    l->activation_input = (float*)calloc(l->batch * l->outputs, sizeof(float));
#endif  // not GPU

#ifdef GPU
  if (l->activation == SWISH || l->activation == MISH)
    l->activation_input_gpu =
        cuda_make_array(l->activation_input, l->batch * l->outputs);

  l->forward_gpu = ForwardShortcutLayerGpu;
  l->backward_gpu = BackwardShortcutLayerGpu;

  if (l->nweights > 0)
  {
    l->update_gpu = UpdateShortcutLayerGpu;
    l->weights_gpu = cuda_make_array(l->weights, l->nweights);
    if (train)
      l->weight_updates_gpu = cuda_make_array(l->weight_updates, l->nweights);
  }

  if (train)
    l->delta_gpu = cuda_make_array(l->delta, l->outputs * batch);
  l->output_gpu = cuda_make_array(l->output, l->outputs * batch);

  l->input_sizes_gpu = cuda_make_int_array_new_api(input_sizes, l->n);
  l->layers_output_gpu =
      (float**)cuda_make_array_pointers((void**)layers_output_gpu, l->n);
  l->layers_delta_gpu =
      (float**)cuda_make_array_pointers((void**)layers_delta_gpu, l->n);
#endif  // GPU

  l->bflops = l->out_w * l->out_h * l->out_c * l->n / 1000000000.;
  if (l->weights_type)
    l->bflops *= 2;

  fprintf(stderr, " wt = %d, wn = %d, outputs:%4d x%4d x%4d %5.3f BF\n",
      l->weights_type, l->weights_normalization, l->out_w, l->out_h, l->out_c,
      l->bflops);
}

void ResizeShortcutLayer(layer* l, int w, int h, Network* net)
{
  l->w = l->out_w = w;
  l->h = l->out_h = h;
  l->outputs = w * h * l->out_c;
  l->inputs = l->outputs;
  if (l->train)
    l->delta =
        (float*)xrealloc(l->delta, l->outputs * l->batch * sizeof(float));
  l->output =
      (float*)xrealloc(l->output, l->outputs * l->batch * sizeof(float));

  int i;
  for (i = 0; i < l->n; ++i)
  {
    int index = l->input_layers[i];
    l->input_sizes[i] = net->layers[index].outputs;
    l->layers_output[i] = net->layers[index].output;
    l->layers_delta[i] = net->layers[index].delta;

    assert(
        l->w == net->layers[index].out_w && l->h == net->layers[index].out_h);
  }

  if (l->activation == SWISH || l->activation == MISH)
    l->activation_input = (float*)realloc(
        l->activation_input, l->batch * l->outputs * sizeof(float));

#ifdef GPU
  cuda_free(l->output_gpu);
  l->output_gpu = cuda_make_array(l->output, l->outputs * l->batch);

  if (l->train)
  {
    cuda_free(l->delta_gpu);
    l->delta_gpu = cuda_make_array(l->delta, l->outputs * l->batch);
  }

  float** layers_output_gpu = (float**)calloc(l->n, sizeof(float*));
  float** layers_delta_gpu = (float**)calloc(l->n, sizeof(float*));

  for (i = 0; i < l->n; ++i)
  {
    const int index = l->input_layers[i];
    layers_output_gpu[i] = net->layers[index].output_gpu;
    layers_delta_gpu[i] = net->layers[index].delta_gpu;
  }

  memcpy_ongpu(l->input_sizes_gpu, l->input_sizes, l->n * sizeof(int));
  memcpy_ongpu(l->layers_output_gpu, layers_output_gpu, l->n * sizeof(float*));
  memcpy_ongpu(l->layers_delta_gpu, layers_delta_gpu, l->n * sizeof(float*));

  free(layers_output_gpu);
  free(layers_delta_gpu);

  if (l->activation == SWISH || l->activation == MISH)
  {
    cuda_free(l->activation_input_gpu);
    l->activation_input_gpu =
        cuda_make_array(l->activation_input, l->batch * l->outputs);
  }
#endif
}

void ForwardShortcutLayer(layer* l, NetworkState state)
{
  int from_w = state.net->layers[l->index].w;
  int from_h = state.net->layers[l->index].h;
  int from_c = state.net->layers[l->index].c;

  if (l->nweights == 0 && l->n == 1 && from_w == l->w && from_h == l->h &&
      from_c == l->c)
  {
    int size = l->batch * l->w * l->h * l->c;
#pragma omp parallel for
    for (int i = 0; i < size; ++i)
      l->output[i] = state.input[i] + state.net->layers[l->index].output[i];
  }
  else
  {
    shortcut_multilayer_cpu(l->outputs * l->batch, l->outputs, l->batch, l->n,
        l->input_sizes, l->layers_output, l->output, state.input, l->weights,
        l->nweights, l->weights_normalization);
  }

  if (l->activation == SWISH)
    activate_array_swish(
        l->output, l->outputs * l->batch, l->activation_input, l->output);
  else if (l->activation == MISH)
    activate_array_mish(
        l->output, l->outputs * l->batch, l->activation_input, l->output);
  else
    activate_array_cpu_custom(l->output, l->outputs * l->batch, l->activation);
}

void BackwardShortcutLayer(layer* l, NetworkState state)
{
  if (l->activation == SWISH)
    gradient_array_swish(
        l->output, l->outputs * l->batch, l->activation_input, l->delta);
  else if (l->activation == MISH)
    gradient_array_mish(l->outputs * l->batch, l->activation_input, l->delta);
  else
    gradient_array(l->output, l->outputs * l->batch, l->activation, l->delta);

  backward_shortcut_multilayer_cpu(l->outputs * l->batch, l->outputs, l->batch,
      l->n, l->input_sizes, l->layers_delta, state.delta, l->delta, l->weights,
      l->weight_updates, l->nweights, state.input, l->layers_output,
      l->weights_normalization);
}

void UpdateShortcutLayer(
    layer* l, int batch, float learning_rate_init, float momentum, float decay)
{
  if (l->nweights > 0)
  {
    float learning_rate = learning_rate_init * l->learning_rate_scale;
    // float momentum = a.momentum;
    // float decay = a.decay;
    // int batch = a.batch;

    axpy_cpu(l->nweights, -decay * batch, l->weights, 1, l->weight_updates, 1);
    axpy_cpu(l->nweights, learning_rate / batch, l->weight_updates, 1,
        l->weights, 1);
    scal_cpu(l->nweights, momentum, l->weight_updates, 1);
  }
}

#ifdef GPU
void ForwardShortcutLayerGpu(layer* l, NetworkState state)
{
  shortcut_multilayer_gpu(l->outputs, l->batch, l->n, l->input_sizes_gpu,
      l->layers_output_gpu, l->output_gpu, state.input, l->weights_gpu,
      l->nweights, l->weights_normalization);

  if (l->activation == SWISH)
    activate_array_swish_ongpu(l->output_gpu, l->outputs * l->batch,
        l->activation_input_gpu, l->output_gpu);
  else if (l->activation == MISH)
    activate_array_mish_ongpu(l->output_gpu, l->outputs * l->batch,
        l->activation_input_gpu, l->output_gpu);
  else
    activate_array_ongpu(l->output_gpu, l->outputs * l->batch, l->activation);
}

void BackwardShortcutLayerGpu(layer* l, NetworkState state)
{
  if (l->activation == SWISH)
    gradient_array_swish_ongpu(l->output_gpu, l->outputs * l->batch,
        l->activation_input_gpu, l->delta_gpu);
  else if (l->activation == MISH)
    gradient_array_mish_ongpu(
        l->outputs * l->batch, l->activation_input_gpu, l->delta_gpu);
  else
    gradient_array_ongpu(
        l->output_gpu, l->outputs * l->batch, l->activation, l->delta_gpu);

  backward_shortcut_multilayer_gpu(l->outputs, l->batch, l->n,
      l->input_sizes_gpu, l->layers_delta_gpu, state.delta, l->delta_gpu,
      l->weights_gpu, l->weight_updates_gpu, l->nweights, state.input,
      l->layers_output_gpu, l->weights_normalization);
}

void UpdateShortcutLayerGpu(layer* l, int batch, float learning_rate_init,
    float momentum, float decay, float loss_scale)
{
  if (l->nweights > 0)
  {
    float learning_rate = learning_rate_init * l->learning_rate_scale;

    // Loss scale for Mixed-Precision on Tensor-Cores
    if (loss_scale != 1.0)
    {
      if (l->weight_updates_gpu && l->nweights > 0)
        scal_ongpu(l->nweights, 1.0 / loss_scale, l->weight_updates_gpu, 1);
    }

    reset_nan_and_inf(l->weight_updates_gpu, l->nweights);
    fix_nan_and_inf(l->weights_gpu, l->nweights);

    constrain_ongpu(l->nweights, 1, l->weight_updates_gpu, 1);

    axpy_ongpu(l->nweights, learning_rate / batch, l->weight_updates_gpu, 1,
        l->weights_gpu, 1);
    scal_ongpu(l->nweights, momentum, l->weight_updates_gpu, 1);
  }
}

void PushShortcutLayer(layer* l)
{
  cuda_push_array(l->weights_gpu, l->weights, l->nweights);
  CHECK_CUDA(cudaPeekAtLastError());
}

void PullShortcutLayer(layer* l)
{
  constrain_ongpu(l->nweights, 1, l->weight_updates_gpu, 1);
  cuda_pull_array_async(l->weight_updates_gpu, l->weight_updates, l->nweights);
  cuda_pull_array_async(l->weights_gpu, l->weights, l->nweights);
  CHECK_CUDA(cudaPeekAtLastError());
  CHECK_CUDA(cudaStreamSynchronize(get_cuda_stream()));
}
#endif
