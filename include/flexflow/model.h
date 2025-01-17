/* Copyright 2021 Stanford, Facebook, LANL
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _FLEXFLOW_MODEL_H_
#define _FLEXFLOW_MODEL_H_
#include "accessor.h"
#include "config.h"
#include "device.h"
#include "flexflow/node.h"
#include "flexflow/operator_params.h"
#include "flexflow/utils/hash_utils.h"
#include "flexflow/utils/tuple.h"
#include "initializer.h"
#include "layer.h"
#include "legion.h"
#include "loss_functions.h"
#include "metrics_functions.h"
#include "optimizer.h"
#include "parallel_tensor.h"
#include "recompile.h"
#include "simulator.h"
#include "tensor.h"
#include "tl/optional.hpp"
#include "utils/dot/record_formatter.h"
#include <functional>
#include <unistd.h>
#include <utility>

#include "ffconst.h"
#include "fftype.h"

namespace FlexFlow {

enum TaskIDs {
  TOP_LEVEL_TASK_ID,
  FF_INIT_TASK_ID,
  IMAGE_INIT_TASK_ID,
  LABEL_INIT_TASK_ID,
  LOAD_IMAGES_TASK_ID,
  NORMALIZE_IMAGES_TASK_ID,
  ELEMENTBINARY_INIT_TASK_ID,
  ELEMENTBINARY_FWD_TASK_ID,
  ELEMENTBINARY_BWD_TASK_ID,
  ELEMENTUNARY_INIT_TASK_ID,
  ELEMENTUNARY_FWD_TASK_ID,
  ELEMENTUNARY_BWD_TASK_ID,
  CONV2D_INIT_TASK_ID,
  CONV2D_INIT_PARA_TASK_ID,
  CONV2D_FWD_TASK_ID,
  CONV2D_BWD_TASK_ID,
  CONV2D_UPD_TASK_ID,
  DROPOUT_INIT_TASK_ID,
  DROPOUT_FWD_TASK_ID,
  DROPOUT_BWD_TASK_ID,
  EMBED_INIT_TASK_ID,
  EMBED_FWD_TASK_ID,
  EMBED_BWD_TASK_ID,
  GROUP_BY_INIT_TASK_ID,
  GROUP_BY_FWD_TASK_ID,
  GROUP_BY_BWD_TASK_ID,
  CACHE_INIT_TASK_ID,
  CACHE_FWD_TASK_ID,
  CACHE_UPDATE_TASK_ID,
  CAST_INIT_TASK_ID,
  CAST_FWD_TASK_ID,
  CAST_BWD_TASK_ID,
  AGGREGATE_INIT_TASK_ID,
  AGGREGATE_FWD_TASK_ID,
  AGGREGATE_BWD_TASK_ID,
  AGG_SPEC_INIT_TASK_ID,
  AGG_SPEC_FWD_TASK_ID,
  AGG_SPEC_BWD_TASK_ID,
  POOL2D_INIT_TASK_ID,
  POOL2D_FWD_TASK_ID,
  POOL2D_BWD_TASK_ID,
  BATCHNORM_INIT_TASK_ID,
  BATCHNORM_INIT_PARA_TASK_ID,
  BATCHNORM_FWD_TASK_ID,
  BATCHNORM_BWD_TASK_ID,
  BATCHMATMUL_INIT_TASK_ID,
  BATCHMATMUL_FWD_TASK_ID,
  BATCHMATMUL_BWD_TASK_ID,
  LAYERNORM_INIT_TASK_ID,
  LAYERNORM_FWD_TASK_ID,
  LAYERNORM_BWD_TASK_ID,
  LINEAR_INIT_TASK_ID,
  LINEAR_INIT_PARA_TASK_ID,
  LINEAR_FWD_TASK_ID,
  LINEAR_BWD_TASK_ID,
  LINEAR_BWD2_TASK_ID,
  LINEAR_UPD_TASK_ID,
  FLAT_INIT_TASK_ID,
  FLAT_FWD_TASK_ID,
  FLAT_BWD_TASK_ID,
  SOFTMAX_INIT_TASK_ID,
  SOFTMAX_FWD_TASK_ID,
  SOFTMAX_BWD_TASK_ID,
  CONCAT_INIT_TASK_ID,
  CONCAT_FWD_TASK_ID,
  CONCAT_BWD_TASK_ID,
  SPLIT_INIT_TASK_ID,
  SPLIT_FWD_TASK_ID,
  SPLIT_BWD_TASK_ID,
  RESHAPE_INIT_TASK_ID,
  RESHAPE_FWD_TASK_ID,
  RESHAPE_BWD_TASK_ID,
  REVERSE_INIT_TASK_ID,
  REVERSE_FWD_TASK_ID,
  REVERSE_BWD_TASK_ID,
  TOPK_INIT_TASK_ID,
  TOPK_FWD_TASK_ID,
  TOPK_BWD_TASK_ID,
  TRANSPOSE_INIT_TASK_ID,
  TRANSPOSE_FWD_TASK_ID,
  TRANSPOSE_BWD_TASK_ID,
  ATTENTION_INIT_TASK_ID,
  ATTENTION_FWD_TASK_ID,
  ATTENTION_BWD_TASK_ID,
  MSELOSS_BWD_TASK_ID,
  FUSEDOP_INIT_TASK_ID,
  FUSEDOP_FWD_TASK_ID,
  FUSEDOP_BWD_TASK_ID,
  NOOP_INIT_TASK_ID,
  // Metrics tasks
  METRICS_COMP_TASK_ID,
  UPDATE_METRICS_TASK_ID,
  // Parameter server prefetch task
  PS_PREFETCH_TASK_ID,
  // Loss
  LOSS_BWD_TASK_ID,
  // Optimizer with PS
  SGD_UPD_PS_TASK_ID,
  ADAM_UPD_PS_TASK_ID,
  // Optimizer with NCCL
  SGD_UPD_NCCL_TASK_ID,
  ADAM_UPD_NCCL_TASK_ID,
  // Initializer
  GLOROT_INIT_TASK_ID,
  ZERO_INIT_TASK_ID,
  CONSTANT_INIT_TASK_ID,
  UNIFORM_INIT_TASK_ID,
  NORMAL_INIT_TASK_ID,
  // NCCL tasks
  NCCL_GETUNIQUEID_TASK_ID,
  NCCL_INIT_COMMS_TASK_ID,
  // Search
  STRATEGY_SEARCH_TASK_ID,
  // Graph
  GRAPH_OPTIMIZE_TASK_ID,
  // Python data loader
  PY_DL_FLOAT_LOAD_ENTIRE_CPU_TASK_ID,
  PY_DL_INT32_LOAD_ENTIRE_CPU_TASK_ID,
  PY_DL_INT64_LOAD_ENTIRE_CPU_TASK_ID,
  PY_DL_FLOAT_INDEX_LOAD_ENTIRE_CPU_TASK_ID,
  PY_DL_INT32_INDEX_LOAD_ENTIRE_CPU_TASK_ID,
  PY_DL_INT64_INDEX_LOAD_ENTIRE_CPU_TASK_ID,
  PY_DL_FLOAT_LOAD_BATCH_GPU_TASK_ID,
  PY_DL_INT32_LOAD_BATCH_GPU_TASK_ID,
  PY_DL_INT64_LOAD_BATCH_GPU_TASK_ID,
  // Parallel Ops
  REPARTITION_INIT_TASK_ID,
  REPARTITION_FWD_TASK_ID,
  REPARTITION_BWD_TASK_ID,
  COMBINE_INIT_TASK_ID,
  COMBINE_FWD_TASK_ID,
  COMBINE_BWD_TASK_ID,
  REPLICATE_INIT_TASK_ID,
  REPLICATE_FWD_TASK_ID,
  REPLICATE_BWD_TASK_ID,
  REDUCTION_INIT_TASK_ID,
  REDUCTION_FWD_TASK_ID,
  REDUCTION_BWD_TASK_ID,
  PIPELINE_INIT_TASK_ID,
  PIPELINE_FWD_TASK_ID,
  PIPELINE_BWD_TASK_ID,
  FUSED_PARALLELOP_INIT_TASK_ID,
  FUSED_PARALLELOP_FWD_TASK_ID,
  FUSED_PARALLELOP_BWD_TASK_ID,
  // Custom tasks
  CUSTOM_GPU_TASK_ID_FIRST,
  CUSTOM_GPU_TASK_ID_1,
  CUSTOM_GPU_TASK_ID_2,
  CUSTOM_GPU_TASK_ID_3,
  CUSTOM_GPU_TASK_ID_4,
  CUSTOM_GPU_TASK_ID_5,
  CUSTOM_GPU_TASK_ID_6,
  CUSTOM_GPU_TASK_ID_7,
  CUSTOM_GPU_TASK_ID_8,
  CUSTOM_GPU_TASK_ID_LAST,
  CUSTOM_CPU_TASK_ID_FIRST,
  CUSTOM_CPU_TASK_ID_1,
  CUSTOM_CPU_TASK_ID_2,
  CUSTOM_CPU_TASK_ID_3,
  CUSTOM_CPU_TASK_ID_4,
  CUSTOM_CPU_TASK_ID_5,
  CUSTOM_CPU_TASK_ID_6,
  CUSTOM_CPU_TASK_ID_7,
  CUSTOM_CPU_TASK_ID_LAST,
  // Make sure PYTHON_TOP_LEVEL_TASK_ID is
  // consistent with python/main.cc
  PYTHON_TOP_LEVEL_TASK_ID = 11111,
};

enum ShardingID {
  DataParallelShardingID = 135,
};

// namespace Legion {
//   class Serializer;
// }

namespace PCG {
class SearchHelper;
class GraphSearchHelper;
class Graph;
}; // namespace PCG

class FFModel;
class ParallelOp;

std::string optype_to_string(OperatorType);

void solve_parallel_dim_mappings(
    std::vector<ParallelDimMappingRecord> const &mapping,
    std::vector<ParallelDim const *> const &inputs,
    std::vector<ParallelDim *> const &weights,
    std::vector<ParallelDim *> const &outputs);
std::unordered_map<int, int> output_to_input_mapping(
    std::vector<ParallelDimMappingRecord> const &mapping);
std::unordered_map<int, int> input_to_output_mapping(
    std::vector<ParallelDimMappingRecord> const &mapping);

class NoOp;

ParallelConfig get_basic_data_parallel_config(int num_parts, int dims);

class Cast;
class Concat;
class Conv2D;
class Conv2DParams;
class Dropout;
class DropoutParams;
class ElementBinary;
class ElementUnary;
class Embedding;
class Flat;
class Linear;
class LinearParams;
class MultiHeadAttention;
class Pool2D;
class Pool2DParams;
class Reshape;
class ReshapeParams;
class Softmax;
class Split;
class Combine;
class Repartition;
class Reduction;
class Replicate;
class FusedParallelOp;
class ParallelOpInfo;

// TODO: Move to an appropriate place
/*
  This is used to create a type that recursively replaces value type
  ParallelTensor by ParallelTensorShape in T. E.g., ToShape<std::tuple<int,
  ParallelTensor>>::type gives std::tuple<int, ParallelTensorShape>
*/
template <typename T>
struct ToShape {
  using type = T;
};

template <>
struct ToShape<ParallelTensor> {
  using type = ParallelTensorShape;
};

template <typename... Args, template <typename...> typename Container>
struct ToShape<Container<Args...>> {
  using type = Container<typename ToShape<Args>::type...>;
};

// TODO: Move to an appropriate place
template <typename Input>
typename ToShape<Input>::type get_input_shape(Input const &input) = delete;

template <>
std::tuple<> get_input_shape(std::tuple<> const &);

template <>
ParallelTensorShape get_input_shape(ParallelTensor const &input);

template <>
std::pair<ParallelTensorShape, ParallelTensorShape>
    get_input_shape(std::pair<ParallelTensor, ParallelTensor> const &inputs);

template <>
std::vector<ParallelTensorShape>
    get_input_shape(std::vector<ParallelTensor> const &inputs);

class FFModel {
public:
  FFModel(FFConfig &config);

  static constexpr float PROPAGATION_CHANCE = 0.25;
  static constexpr float CONTINUE_PROPAGATION_CHANCE = 0.75;
  static constexpr float PROPAGATION_SIZE_WEIGHT = 1.0;

  // C++ APIs for constructing models
  // Add an exp layer
  Tensor exp(const Tensor x, char const *name = NULL);
  // Add an add layer
  Tensor add(const Tensor x,
             const Tensor y,
             bool inplace_a = false,
             char const *name = NULL);
  // Add a subtract layer
  Tensor subtract(const Tensor x,
                  const Tensor y,
                  bool inplace_a = false,
                  char const *name = NULL);
  // Add a multiply layer
  Tensor multiply(const Tensor x,
                  const Tensor y,
                  bool inplace_a = false,
                  char const *name = NULL);
  // Add a divide layer
  Tensor divide(const Tensor x,
                const Tensor y,
                bool inplace_a = false,
                char const *name = NULL);
  // Add a rsqrt layer
  Tensor rsqrt(const Tensor x, bool inplace = true, char const *name = NULL);
  // Add a pow layer
  Tensor pow(const Tensor x,
             float const exponent,
             bool inplace = true,
             char const *name = NULL);
  // Add a scalar multiply layer
  Tensor scalar_multiply(const Tensor x,
                         float const scalar,
                         bool inplace = true,
                         char const *name = NULL);
  Tensor scalar_add(const Tensor x,
                    float const scalar,
                    bool inplace = true,
                    char const *name = NULL);
  Tensor scalar_sub(const Tensor x,
                    float const scalar,
                    bool inplace = true,
                    char const *name = NULL);
  Tensor scalar_truediv(const Tensor x,
                        float const scalar,
                        bool inplace = true,
                        char const *name = NULL);
  // Add an activation layer
  Tensor relu(const Tensor x, bool inplace = true, char const *name = NULL);
  Tensor identity(const Tensor x, char const *name = NULL);
  Tensor gelu(const Tensor x, char const *name = NULL);
  Tensor sigmoid(const Tensor x, char const *name = NULL);
  Tensor tanh(const Tensor x, char const *name = NULL);
  Tensor elu(const Tensor x, bool inplace = true, char const *name = NULL);
  // Add a 2D convolutional layer
  Tensor conv2d(const Tensor input,
                int outChannels,
                int kernelH,
                int kernelW,
                int strideH,
                int strideW,
                int paddingH,
                int paddingW,
                ActiMode activation = AC_MODE_NONE,
                int groups = 1,
                bool use_bias = true,
                Layer const *shared_op = NULL,
                Initializer *krenel_initializer = NULL,
                Initializer *bias_initializer = NULL,
                char const *name = NULL);
  // Add a dropout layer
  Tensor dropout(const Tensor input,
                 float rate,
                 unsigned long long seed = 0,
                 char const *name = NULL);
  // Add an embedding layer
  Tensor embedding(const Tensor input,
                   int num_entires,
                   int outDim,
                   AggrMode aggr,
                   Layer const *shared_op = NULL,
                   Initializer *kernel_initializer = NULL,
                   char const *name = NULL);
  // Add a group_by layer
  void group_by(const Tensor data,
                const Tensor assign,
                Tensor *outputs,
                int n,
                float alpha,
                char const *name = NULL);
  // Add a cache layer
  Tensor cache(Tensor const &input,
               int num_batches,
               std::function<float(float *, void const *, void const *, int)>
                   score_f = {},
               char const *name = NULL);
  // Add aggregate layer
  Tensor aggregate(Tensor const *inputs,
                   int n,
                   float lambda_bal,
                   char const *name = NULL);
  // Add aggregate_spec layer
  Tensor aggregate_spec(Tensor const *inputs,
                        int n,
                        float lambda_bal,
                        char const *name = NULL);
  // Add a 2D pooling layer
  Tensor pool2d(const Tensor input,
                int kernelH,
                int kernelW,
                int strideH,
                int strideW,
                int paddingH,
                int paddingW,
                PoolType type = POOL_MAX,
                ActiMode activation = AC_MODE_NONE,
                char const *name = NULL);
  // Add a batch_norm layer
  Tensor layer_norm(const Tensor input,
                    std::vector<int> const &axes,
                    bool elementwise_affine,
                    float eps,
                    char const *name = NULL);
  // Add a batch_norm layer
  Tensor
      batch_norm(const Tensor input, bool relu = true, char const *name = NULL);
  // Add a batch_matmul layer
  Tensor batch_matmul(const Tensor A,
                      const Tensor B,
                      int a_seq_length_dim = -1,
                      int b_seq_length_dim = -1,
                      char const *name = nullptr);
  // Add a dense layer
  Tensor dense(const Tensor input,
               int outDim,
               ActiMode activation = AC_MODE_NONE,
               bool use_bias = true,
               DataType data_type = DT_FLOAT,
               Layer const *shared_op = NULL,
               Initializer *kernel_initializer = NULL,
               Initializer *bias_initializer = NULL,
               char const *name = NULL);
  // Add a cast layer
  Tensor cast(const Tensor input, DataType dtype, char const *name);
  // Add a concat layer
  Tensor
      concat(int n, Tensor const *tensors, int axis, char const *name = NULL);
  // Add a mean layer
  Tensor mean(const Tensor input,
              std::vector<int> const &dims,
              bool keepdims,
              char const *name);
  // Add a split layer
  void split(const Tensor input,
             Tensor *outputs,
             std::vector<int> const &split,
             int axis,
             char const *name = NULL);
  // Add a flat layer
  Tensor flat(const Tensor input, char const *name = NULL);
  // Add a softmax layer
  Tensor softmax(const Tensor input, int dim = -1, char const *name = NULL);
  // Create input tensors and constants
  Tensor transpose(const Tensor input,
                   std::vector<int> const &perm,
                   char const *name = NULL);
  Tensor reshape(const Tensor input,
                 std::vector<int> const &shape,
                 char const *name = NULL);
  Tensor reverse(const Tensor input, int axis, char const *name = NULL);
  void top_k(const Tensor input,
             Tensor *outputs,
             int k,
             bool sorted,
             char const *name = NULL);
  Tensor multihead_attention(const Tensor query,
                             const Tensor key,
                             const Tensor value,
                             int embed_dim,
                             int num_heads,
                             int kdim = 0,
                             int vdim = 0,
                             float dropout = 0.0f,
                             bool bias = true,
                             bool add_bias_kv = false,
                             bool add_zero_attn = false,
                             Initializer *kernel_initializer = NULL,
                             char const *name = NULL);
  Tensor create_tensor_legion_ordering(int num_dim,
                                       int const dims[],
                                       DataType data_type,
                                       Layer const *owner_op = NULL,
                                       int owner_idx = 0,
                                       bool create_grad = true);
  ParallelTensor
      create_parallel_tensor_legion_ordering(int num_dim,
                                             const ParallelDim dims[],
                                             DataType data_type,
                                             Op const *owner_op = NULL,
                                             int owner_idx = 0,
                                             bool create_grad = true,
                                             size_t input_tensor_guid = 0);
  Tensor create_tensor(int num_dim,
                       int const dims[],
                       DataType data_type,
                       Layer const *owner_op = NULL,
                       int owner_idx = 0,
                       bool create_grad = true);
  ParallelTensor create_parallel_tensor(int num_dim,
                                        const ParallelDim dims[],
                                        DataType data_type,
                                        Op const *owner_op = NULL,
                                        int owner_idx = 0,
                                        bool create_grad = true,
                                        size_t input_tensor_guid = 0);
  template <int NDIM>
  Tensor create_tensor(int const dims[],
                       DataType data_type,
                       Layer const *owner_op = NULL,
                       int owner_idx = 0,
                       bool create_grad = true);
  template <int NDIM>
  ParallelTensor create_parallel_tensor(const ParallelDim dims[],
                                        DataType data_type,
                                        Op const *owner_op = NULL,
                                        int owner_idx = 0,
                                        bool create_grad = true,
                                        size_t input_tensor_guid = 0);
  Parameter
      create_weight(int numdim,
                    int const dims[],
                    DataType data_type,
                    Layer const *owner_op = NULL,
                    bool create_grad = true,
                    Initializer *initializer = NULL,
                    ParameterSyncType sync_type = ParameterSyncType::NONE);
  Parameter create_weight_legion_ordering(
      int numdim,
      int const dims[],
      DataType data_type,
      Layer const *owner_op = NULL,
      bool create_grad = true,
      Initializer *initializer = NULL,
      ParameterSyncType sync_type = ParameterSyncType::NONE);
  template <int NDIM>
  ParallelParameter create_parallel_weight(
      const ParallelDim dims[],
      DataType data_type,
      Op const *owner_op = NULL,
      bool create_grad = true,
      Initializer *initializer = NULL,
      ParameterSyncType sync_type = ParameterSyncType::NONE);
  ParallelParameter create_parallel_weight(
      int numdim,
      const ParallelDim dims[],
      DataType data_type,
      Op const *owner_op = NULL,
      bool create_grad = true,
      Initializer *initializer = NULL,
      ParameterSyncType sync_type = ParameterSyncType::NONE);
  ParallelParameter create_parallel_weight_legion_ordering(
      int numdim,
      const ParallelDim dims[],
      DataType data_type,
      Op const *owner_op = NULL,
      bool create_grad = true,
      Initializer *initializer = NULL,
      ParameterSyncType sync_type = ParameterSyncType::NONE);

  void map_tensor(ParallelTensor tensor, Op const *parallel_op);
  void map_weight(ParallelTensor tensor, Op const *parallel_op);
  bool get_parallel_tensor_from_tensor(const Tensor tensor,
                                       ParallelTensor &parallel_tensor) const;

  template <int NDIM>
  Tensor create_constant(int const dims[], float value, DataType date_type);
  // ========================================
  // Parallel APIs
  // ========================================
  ParallelTensor repartition(const ParallelTensor input,
                             int partition_legion_dim,
                             int partition_degree,
                             char const *name = NULL);
  ParallelTensor combine(const ParallelTensor input,
                         int combine_legion_dim,
                         int combine_degree,
                         char const *name = NULL);
  ParallelTensor replicate(const ParallelTensor input,
                           int replicate_legion_dim,
                           int replicate_degree,
                           char const *name = NULL);
  ParallelTensor reduction(const ParallelTensor input,
                           int reduction_legion_dim,
                           int reduction_degree,
                           char const *name = NULL);
  // ========================================
  // Graph APIs
  // ========================================
  float graph_cost(const PCG::Graph *graph,
                   const PCG::Node &sink_node,
                   MachineView const &sink_view,
                   const PCG::Node &source_node,
                   MachineView const &source_view,
                   MachineResource const &resources,
                   bool include_sink_compute_time,
                   bool constructing_optimal_view = false);
  void construct_optimal_view(
      const PCG::Graph *graph,
      const PCG::Node &sink_node,
      MachineView const &sink_view,
      const PCG::Node &source_node,
      MachineView const &source_view,
      MachineResource const &resources,
      bool include_sink_compute_time,
      float optimal_cost,
      std::unordered_map<PCG::Node, MachineView> &optimal_views);
  void deserialize_graph_optimal_view(
      Legion::Deserializer &dez,
      PCG::Graph *graph,
      std::unordered_map<PCG::Node, MachineView> &optimal_views);
  bool convert_graph_to_operators(
      const PCG::Graph *graph,
      std::unordered_map<PCG::Node, MachineView> const &optimal_views);
  static void register_all_machine_views(int num_nodes,
                                         int gpus_per_node,
                                         int cpus_per_node,
                                         std::vector<MachineView> &valid_views);
  // ========================================
  // Internal PCG::Node creation APIs
  // ========================================
  template <typename T>
  PCG::Node get_or_create_node(const typename T::Input &input,
                               typename T::Params const &params) {
    using Params = typename T::Params;

    auto input_shapes = get_input_shape<typename T::Input>(input);

    if (!params.is_valid(input_shapes)) {
      return PCG::Node::INVALID_NODE;
    }

    T *op = nullptr;

    std::pair<typename ToShape<typename T::Input>::type, Params> key{
        input_shapes, params};
    auto &cache = get<std::unordered_map<
        std::pair<typename ToShape<typename T::Input>::type, Params>,
        T *>>(this->cached_ops);
    auto const &it = cache.find(key);
    if (it != cache.end()) {
      op = it->second;
    } else {
      op = new T(*this, params, input);
      cache[key] = op;
    }

    assert(op->get_params() == params);
    return this->new_node(op);
  }

  PCG::Node get_or_create_noop_node(const ParallelTensor input);
  PCG::Node get_or_create_input_node(ParallelTensorShape const &);
  PCG::Node get_or_create_cast_node(const ParallelTensor input, DataType dtype);
  PCG::Node get_or_create_concat_node(int num_inputs,
                                      ParallelTensor const *inputs,
                                      int legion_axis);
  PCG::Node get_or_create_element_binary_node(const ParallelTensor input1,
                                              const ParallelTensor input2,
                                              OperatorType type);
  PCG::Node get_or_create_embedding_node(LayerID const &layer_guid,
                                         const ParallelTensor input,
                                         int num_entries,
                                         int out_channels,
                                         AggrMode aggr);
  PCG::Node get_or_create_linear_node(LayerID const &layer_guid,
                                      const ParallelTensor input,
                                      int out_dim,
                                      ActiMode activation,
                                      bool use_bias);
  PCG::Node get_or_create_multihead_attn_node(LayerID const &layer_guid,
                                              const ParallelTensor query,
                                              const ParallelTensor key,
                                              const ParallelTensor value,
                                              int embed_dim,
                                              int num_heads,
                                              int kdim,
                                              int vdim,
                                              float dropout,
                                              bool bias,
                                              bool add_bias_kv,
                                              bool add_zero_attn);
  PCG::Node get_or_create_reshape_node(const ParallelTensor input,
                                       ReshapeParams const &shape);
  PCG::Node get_or_create_reshape_node(const ParallelTensor input,
                                       std::vector<int> const &shape);
  PCG::Node get_or_create_softmax_node(const ParallelTensor input,
                                       int softmax_dim);
  PCG::Node get_or_create_split_node(const ParallelTensor input,
                                     std::vector<int> const &splits,
                                     int legion_axis);
  PCG::Node get_or_create_repartition_node(const ParallelTensor input,
                                           int repartition_dim,
                                           int repartition_degree);
  PCG::Node get_or_create_replicate_node(const ParallelTensor input,
                                         int replicate_dim,
                                         int replicate_degree);
  PCG::Node get_or_create_reduction_node(const ParallelTensor input,
                                         int reduction_dim,
                                         int reduction_degree);
  PCG::Node get_or_create_combine_node(const ParallelTensor input,
                                       int combine_dim,
                                       int combine_degree);
  PCG::Node get_or_create_fused_parallel_node(
      const ParallelTensor input,
      std::vector<ParallelOpInfo> const &parallel_ops);
  PCG::Node get_or_create_conv2d_node(LayerID const &layer_guid,
                                      const ParallelTensor input,
                                      int out_channels,
                                      int kernel_h,
                                      int kernel_w,
                                      int stride_h,
                                      int stride_w,
                                      int padding_h,
                                      int padding_w,
                                      ActiMode activation,
                                      int groups,
                                      bool use_bias);
  PCG::Node get_or_create_dropout_node(const ParallelTensor input,
                                       DropoutParams const &params);
  PCG::Node get_or_create_pool2d_node(const ParallelTensor input,
                                      int kernelH,
                                      int kernelW,
                                      int strideH,
                                      int strideW,
                                      int paddingH,
                                      int paddingW,
                                      PoolType type,
                                      ActiMode activation);
  PCG::Node get_or_create_flat_node(const ParallelTensor input);
  PCG::Node get_or_create_element_unary_node(const ParallelTensor input,
                                             OperatorType type,
                                             bool inplace,
                                             float scalar);
  PCG::Node get_or_create_parallel_op_node(const ParallelTensor input,
                                           ParallelOpInfo const &);
  // ========================================
  // Internal APIs that should not be invoked from applications
  // ========================================
  void create_disjoint_partition(int num_dims,
                                 const ParallelDim dims[],
                                 Legion::IndexSpace const &part_is,
                                 Legion::LogicalRegion const &region,
                                 Legion::LogicalPartition &part);
  template <int NDIM, int TDIM>
  void create_disjoint_partition_with_dim2(
      const ParallelDim dims[],
      Legion::IndexSpaceT<TDIM> const &part_is,
      Legion::LogicalRegion const &region,
      Legion::LogicalPartition &part);
  void create_aliased_partition(int num_dims,
                                const ParallelDim dims[],
                                int aliased_dim,
                                Legion::IndexSpace const &part_is,
                                Legion::LogicalRegion const &region,
                                Legion::LogicalPartition &part);
  template <int NDIM, int TDIM>
  void create_aliased_partition_with_dim2(
      const ParallelDim dims[],
      int aliased_dim,
      Legion::IndexSpaceT<TDIM> const &part_is,
      Legion::LogicalRegion const &region,
      Legion::LogicalPartition &part);

  template <int NDIM>
  void create_disjoint_partition(const ParallelTensor tensor,
                                 Legion::IndexSpaceT<NDIM> const &part_is,
                                 Legion::LogicalPartition &part_fwd,
                                 Legion::LogicalPartition &part_bwd);

  template <int NDIM, int TDIM>
  void create_data_parallel_partition_with_diff_dims(
      const ParallelTensor tensor,
      Legion::IndexSpaceT<TDIM> const &task_is,
      Legion::LogicalPartition &part_fwd,
      Legion::LogicalPartition &part_bwd);
  template <int NDIM>
  void map_conv_weight(ParallelTensor p, Op const *parallel_op);
  template <int NDIM, int TDIM>
  void map_linear_weight(ParallelTensor p, Op const *parallel_op);
  template <int NDIM, int TDIM>
  ParallelTensor create_linear_replica(int const *dims,
                                       Legion::IndexSpaceT<TDIM> const &part_is,
                                       DataType data_type);
  static PerfMetrics
      update_metrics_task(Legion::Task const *task,
                          std::vector<Legion::PhysicalRegion> const &regions,
                          Legion::Context ctx,
                          Legion::Runtime *runtime);
  void reset_metrics();
  void init_operators();
  void prefetch();
  void forward(int seq_length = -1);
  void compute_metrics();
  void get_metrics();
  void backward(int seq_length = -1);
  void update();
  bool apply_fusion(std::vector<Op *> const &operators,
                    std::vector<Op *> &new_operators);
  Op *get_final_operator() const;
  void compile(LossType loss_type,
               std::vector<MetricsType> const &metrics,
               CompMode comp_mode = COMP_MODE_TRAINING);
  void compile(Optimizer *optimizer,
               LossType loss_type,
               std::vector<MetricsType> const &metrics,
               CompMode comp_mode = COMP_MODE_TRAINING);
  void graph_optimize(size_t budget,
                      bool only_data_parallel,
                      std::unique_ptr<PCG::Graph> &best_graph,
                      std::unordered_map<PCG::Node, MachineView> &optimal_view);
  void mcmc_optimize(std::map<Op const *, ParallelConfig> &best,
                     size_t budget,
                     float alpha,
                     CompMode comp_mode,
                     bool use_propagation) const;
#ifdef FF_USE_NCCL
  ncclComm_t *find_nccl_comms(MachineView const &view) const;
#endif
#ifdef FF_USE_PROPAGATE
  void propagate(std::map<Op *, ParallelConfig> const &current,
                 std::map<Op *, ParallelConfig> &next) const;
#endif
  void rewrite(std::map<Op const *, ParallelConfig> const &current,
               std::map<Op const *, ParallelConfig> &next,
               bool use_propagation) const;
  void recompile_on_condition(RecompileState &r);
  void zero_gradients();
  void print_layers(int id);

  std::unordered_map<Op *, std::vector<std::pair<Op *, int>>>
      get_bwd_edge_map() const;

  // Internal funcitons
  Legion::IndexSpace get_or_create_task_is(ParallelConfig const &pc);
  Legion::IndexSpace get_or_create_task_is(MachineView const &view);
  Legion::IndexSpace get_or_create_task_is(Legion::Domain const &domain);
  Legion::IndexSpace get_or_create_task_is(const ParallelTensor);
  Legion::IndexSpace get_task_is(Legion::Domain const &domain) const;
  Legion::IndexSpace get_task_is(ParallelConfig const &pc) const;
  Legion::IndexSpace get_task_is(MachineView const &view) const;
  void create_operators_from_layers();
  Op *create_operator_from_layer(Layer *layer,
                                 std::vector<ParallelTensor> const &inputs);
  // APIs for setting iteration configs
public:
  void set_iteration_config_sequence_length(int seq_length);

public:
  size_t op_global_guid, layer_global_guid;
  size_t tensor_global_guid, parallel_tensor_global_guid, node_global_guid;
  FFConfig config;
  FFIterationConfig iter_config;
  Optimizer *optimizer;
  PCG::SearchHelper *search;
  PCG::GraphSearchHelper *graph_search;
  Loss *loss_op;
  Metrics *metrics_op;
  Simulator *simulator;
  int metrics_input;
  ParallelTensor parallel_label_tensor;
  Tensor label_tensor;

  std::vector<Layer *> layers;
  std::vector<Op *> operators;
  std::vector<ParallelTensor> parameters;
  FFHandler handlers[MAX_NUM_WORKERS];
  Legion::Future current_metrics;
  // Cached operators: key: operator hash, value: operator pointer
  std::tuple<
      std::unordered_map<
          std::pair<std::vector<ParallelTensorShape>, ConcatParams>,
          Concat *>,
      std::unordered_map<std::pair<ParallelTensorShape, Conv2DParams>,
                         Conv2D *>,
      std::unordered_map<
          std::pair<std::pair<ParallelTensorShape, ParallelTensorShape>,
                    ElementBinaryParams>,
          ElementBinary *>,
      std::unordered_map<std::pair<ParallelTensorShape, LinearParams>,
                         Linear *>,
      std::unordered_map<std::pair<ParallelTensorShape, ElementUnaryParams>,
                         ElementUnary *>,
      std::unordered_map<std::pair<ParallelTensorShape, Pool2DParams>,
                         Pool2D *>>
      cached_ops;
  std::unordered_map<size_t, NoOp *> cached_noop_ops;
  std::unordered_map<size_t, NoOp *> cached_input_ops;
  std::unordered_map<size_t, Cast *> cached_cast_ops;
  std::unordered_map<size_t, Dropout *> cached_dropout_ops;
  std::unordered_map<size_t, Embedding *> cached_embedding_ops;
  std::unordered_map<size_t, Flat *> cached_flat_ops;
  std::unordered_map<size_t, MultiHeadAttention *> cached_multihead_attn_ops;
  std::unordered_map<size_t, Reshape *> cached_reshape_ops;
  std::unordered_map<size_t, Softmax *> cached_softmax_ops;
  std::unordered_map<size_t, Split *> cached_split_ops;
  std::unordered_map<size_t, Repartition *> cached_repartition_ops;
  std::unordered_map<size_t, Replicate *> cached_replicate_ops;
  std::unordered_map<size_t, Reduction *> cached_reduction_ops;
  std::unordered_map<size_t, Combine *> cached_combine_ops;
  std::unordered_map<size_t, FusedParallelOp *> cached_fused_parallel_ops;
  std::vector<MachineView> all_valid_views;
#ifdef FF_USE_NCCL
  std::unordered_map<size_t, ncclComm_t *> view_hash_to_nccl_comms;
#endif
private:
  bool debug;
  // ParallelTensor label_tensor_with_final_part;//FIXME: to be removed
  std::map<MachineView, Legion::IndexSpace, MachineViewDimCompare> all_task_is;

  template <int NDIM>
  void map_tensor_with_dim(ParallelTensor tensor, Op const *parallel_op);
  template <int NDIM, int TDIM>
  void map_tensor_with_dim2(ParallelTensor tensor, Op const *parallel_op);
  template <int NDIM>
  void map_weight_with_dim(ParallelTensor weight, Op const *parallel_op);

  Tensor binary(OperatorType op,
                Tensor const x,
                Tensor const y,
                bool inplace_a = false,
                char const *name = NULL);
  ElementBinary *binary(OperatorType op, char const *name = NULL);
  Tensor unary(OperatorType op,
               Tensor const x,
               bool inplace = true,
               char const *name = NULL,
               float scalar = 0.0);
  ElementUnary *
      unary(OperatorType op, char const *name = NULL, float scalar = 0.0);
  PCG::Node new_node(Op *);
};

class UtilityTasks {
public:
  static FFHandler
      init_cuda_task(Legion::Task const *task,
                     std::vector<Legion::PhysicalRegion> const &regions,
                     Legion::Context ctx,
                     Legion::Runtime *runtime);
  static void dummy_task(Legion::Task const *task,
                         std::vector<Legion::PhysicalRegion> const &regions,
                         Legion::Context ctx,
                         Legion::Runtime *runtime);
  static void
      init_images_task(Legion::Task const *task,
                       std::vector<Legion::PhysicalRegion> const &regions,
                       Legion::Context ctx,
                       Legion::Runtime *runtime);
  static void
      init_labels_task(Legion::Task const *task,
                       std::vector<Legion::PhysicalRegion> const &regions,
                       Legion::Context ctx,
                       Legion::Runtime *runtime);
  static void
      load_images_task(Legion::Task const *task,
                       std::vector<Legion::PhysicalRegion> const &regions,
                       Legion::Context ctx,
                       Legion::Runtime *runtime);
  static void
      normalize_images_task(Legion::Task const *task,
                            std::vector<Legion::PhysicalRegion> const &regions,
                            Legion::Context ctx,
                            Legion::Runtime *runtime);
};

void top_level_task(Legion::Task const *task,
                    std::vector<Legion::PhysicalRegion> const &regions,
                    Legion::Context ctx,
                    Legion::Runtime *runtime);

void data_load_task(Legion::Task const *task,
                    std::vector<Legion::PhysicalRegion> const &regions,
                    Legion::Context ctx,
                    Legion::Runtime *runtime);

void register_flexflow_internal_tasks();

void register_custom_tasks();

}; // namespace FlexFlow

#endif //_FLEXFLOW_MODEL_H_
