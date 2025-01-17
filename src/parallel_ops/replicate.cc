/* Copyright 2021 CMU, Facebook, LANL, MIT, and Stanford (alphabetical)
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

#include "flexflow/parallel_ops/replicate.h"
#include "flexflow/utils/hash_utils.h"

namespace FlexFlow {
// declare Legion names
using Legion::ArgumentMap;
using Legion::Context;
using Legion::coord_t;
using Legion::Domain;
using Legion::FutureMap;
using Legion::IndexLauncher;
using Legion::LogicalPartition;
using Legion::LogicalRegion;
using Legion::Machine;
using Legion::Memory;
using Legion::PhysicalRegion;
using Legion::Predicate;
using Legion::Rect;
using Legion::RegionRequirement;
using Legion::Runtime;
using Legion::Task;
using Legion::TaskArgument;
using Legion::TaskLauncher;

ParallelTensor FFModel::replicate(const ParallelTensor input,
                                  int replicate_legion_dim,
                                  int replicate_degree,
                                  char const *name) {
  assert(false);
#ifdef DEADCODE
  Replicate *repl =
      new Replicate(*this, input, replicate_legion_dim, replicate_degree, name);
  layers.push_back(repl);
  return repl->outputs[0];
#endif
}

Replicate::Replicate(FFModel &model,
                     const ParallelTensor _input,
                     int _replicate_legion_dim,
                     int _replicate_degree,
                     char const *name)
    : ParallelOp(model, OP_REPLICATE, name, _input),
      replicate_dim(_replicate_legion_dim),
      replicate_degree(_replicate_degree) {
  int numdim = _input->num_dims;
  ParallelDim dims[MAX_TENSOR_DIM];
  for (int i = 0; i < numdim; i++) {
    dims[i] = _input->dims[i];
  }
  dims[replicate_dim].size *= replicate_degree;
  dims[replicate_dim].degree *= replicate_degree;
  ParallelTensorBase::update_parallel_ids(numdim, dims);
  outputs[0] = model.create_parallel_tensor_legion_ordering(
      numdim, dims, DT_FLOAT, this);
  // inputs[0]->print("Replicate::input");
  // outputs[0]->print("Replicate::output");
}

void Replicate::create_input_partition(FFModel &ff) {
  assert(outputs[0]->part != LogicalPartition::NO_PART);
  assert(inputs[0]->part != LogicalPartition::NO_PART);
  // input_lp is an aliased partitioning along the replica dim
  ff.create_aliased_partition(outputs[0]->num_dims,
                              outputs[0]->dims,
                              replicate_dim,
                              outputs[0]->parallel_is,
                              inputs[0]->region,
                              input_lp);
  // output_grad_lp is a disjoint partition
  ff.create_disjoint_partition(inputs[0]->num_dims,
                               inputs[0]->dims,
                               inputs[0]->parallel_is,
                               outputs[0]->region_grad,
                               output_grad_lp);
}

void Replicate::init(FFModel const &ff) {
  // Do nothing
  ArgumentMap argmap;
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  assert(numOutputs == 1);
  assert(numInputs == 1);
  IndexLauncher launcher(REPLICATE_FWD_TASK_ID,
                         outputs[0]->parallel_is,
                         TaskArgument(NULL, 0),
                         argmap,
                         Predicate::TRUE_PRED,
                         false /*must*/,
                         0 /*mapper_id*/,
                         outputs[0]->machine_view.hash());
  launcher.add_region_requirement(RegionRequirement(
      input_lp, 0 /*projection id*/, READ_ONLY, EXCLUSIVE, inputs[0]->region));
  launcher.add_field(0, FID_DATA);
  launcher.add_region_requirement(RegionRequirement(outputs[0]->part,
                                                    0 /*projection id*/,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    outputs[0]->region));
  launcher.add_field(1, FID_DATA);
  runtime->execute_index_space(ctx, launcher);
}

void Replicate::forward(FFModel const &ff) {
  ArgumentMap argmap;
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  assert(numOutputs == 1);
  assert(numInputs == 1);
  IndexLauncher launcher(REPLICATE_FWD_TASK_ID,
                         outputs[0]->parallel_is,
                         TaskArgument(NULL, 0),
                         argmap,
                         Predicate::TRUE_PRED,
                         false /*must*/,
                         0 /*mapper_id*/,
                         outputs[0]->machine_view.hash());
  launcher.add_region_requirement(RegionRequirement(
      input_lp, 0 /*projection id*/, READ_ONLY, EXCLUSIVE, inputs[0]->region));
  launcher.add_field(0, FID_DATA);
  launcher.add_region_requirement(RegionRequirement(outputs[0]->part,
                                                    0 /*projection id*/,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    outputs[0]->region));
  launcher.add_field(1, FID_DATA);
  runtime->execute_index_space(ctx, launcher);
}

void Replicate::backward(FFModel const &ff) {
  ArgumentMap argmap;
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  assert(numOutputs == 1);
  assert(numInputs == 1);
  IndexLauncher launcher(REPLICATE_BWD_TASK_ID,
                         inputs[0]->parallel_is,
                         TaskArgument(NULL, 0),
                         argmap,
                         Predicate::TRUE_PRED,
                         false /*must*/,
                         0 /*mapper_id*/,
                         inputs[0]->machine_view.hash());
  launcher.add_region_requirement(RegionRequirement(output_grad_lp,
                                                    0 /*projection id*/,
                                                    READ_ONLY,
                                                    EXCLUSIVE,
                                                    outputs[0]->region_grad));
  launcher.add_field(0, FID_DATA);
  launcher.add_region_requirement(RegionRequirement(inputs[0]->part_grad,
                                                    0 /*projection id*/,
                                                    READ_WRITE,
                                                    EXCLUSIVE,
                                                    inputs[0]->region_grad));
  launcher.add_field(1, FID_DATA);
  runtime->execute_index_space(ctx, launcher);
}

bool Replicate::measure_operator_cost(Simulator *sim,
                                      MachineView const &pc,
                                      CostMetrics &cost_metrics) const {
  cost_metrics.forward_time = 0.0f;
  cost_metrics.backward_time = 0.0f;
  return true;
}

bool Replicate::get_int_parameter(PMParameter para, int *value) const {
  switch (para) {
    case PM_REPLICATE_DIM:
      *value = replicate_dim;
      return true;
    case PM_REPLICATE_DEGREE:
      *value = replicate_degree;
      return true;
    default:
      return Op::get_int_parameter(para, value);
  }
}

bool Replicate::append_parallel_op_info(
    std::vector<ParallelOpInfo> &parallel_ops) const {
  ParallelOpInfo ret;
  ret.op_type = op_type;
  ret.parallel_dim = replicate_dim;
  ret.parallel_degree = replicate_degree;
  parallel_ops.push_back(ret);
  return true;
}

size_t Replicate::get_params_hash() const {
  size_t hash = this->inputs[0]->get_owner_independent_hash();
  hash_combine(hash, this->replicate_dim);
  hash_combine(hash, this->replicate_degree);

  return hash;
}

using PCG::Node;
Node FFModel::get_or_create_replicate_node(const ParallelTensor input,
                                           int replicate_dim,
                                           int replicate_degree) {
  // replica degree cannot be larger than workersPerNode
  if (input->dims[replicate_dim].degree * replicate_degree >
      config.workersPerNode)
    return Node::INVALID_NODE;
  // check that degree is not larger than total available devices
  int degree = input->get_total_num_parts() * replicate_degree;
  if (degree > config.workersPerNode * config.numNodes &&
      (degree > config.cpusPerNode * config.numNodes))
    return Node::INVALID_NODE;
  size_t hash = input->get_owner_independent_hash();
  hash = hash * 31 + std::hash<int>()(replicate_dim);
  hash = hash * 31 + std::hash<int>()(replicate_degree);
  auto const &it = cached_replicate_ops.find(hash);
  Replicate *replicate = NULL;
  if (it != cached_replicate_ops.end()) {
    replicate = it->second;
  } else {
    replicate =
        new Replicate(*this, input, replicate_dim, replicate_degree, NULL);
    cached_replicate_ops[hash] = replicate;
  }
  Node ret;
  ret.ptr = replicate;
  ret.guid = node_global_guid++;
  return ret;
}

void Replicate::forward_task(Task const *task,
                             std::vector<PhysicalRegion> const &regions,
                             Context ctx,
                             Runtime *runtime) {
  assert(regions.size() == 2);
  assert(task->regions.size() == 2);
  Domain input_domain = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  Domain output_domain = runtime->get_index_space_domain(
      ctx, task->regions[1].region.get_index_space());
  // Currently only support the outter most dimension
  for (int i = 0; i < output_domain.get_dim() - 1; i++) {
    assert(output_domain.lo()[i] == input_domain.lo()[i]);
    assert(output_domain.hi()[i] == input_domain.hi()[i]);
  }
  assert(input_domain.get_volume() == output_domain.get_volume());
  float const *input_ptr = helperGetTensorPointerRO<float>(
      regions[0], task->regions[0], FID_DATA, ctx, runtime);
  float *output_ptr = helperGetTensorPointerRW<float>(
      regions[1], task->regions[1], FID_DATA, ctx, runtime);

  forward_kernel<float>(input_ptr, output_ptr, input_domain.get_volume());
}

void Replicate::backward_task(Task const *task,
                              std::vector<PhysicalRegion> const &regions,
                              Context ctx,
                              Runtime *runtime) {
  assert(regions.size() == 2);
  assert(task->regions.size() == 2);
  Domain output_grad_domain = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  Domain input_grad_domain = runtime->get_index_space_domain(
      ctx, task->regions[1].region.get_index_space());
  // Currently only support the outter most dimension
  for (int i = 0; i < output_grad_domain.get_dim() - 1; i++) {
    assert(output_grad_domain.lo()[i] == input_grad_domain.lo()[i]);
    assert(output_grad_domain.hi()[i] == input_grad_domain.hi()[i]);
  }
  size_t num_elements = input_grad_domain.get_volume();
  size_t num_replicas = output_grad_domain.get_volume() / num_elements;
  float const *output_grad_ptr = helperGetTensorPointerRO<float>(
      regions[0], task->regions[0], FID_DATA, ctx, runtime);
  float *input_grad_ptr = helperGetTensorPointerRW<float>(
      regions[1], task->regions[1], FID_DATA, ctx, runtime);

  backward_kernel<float>(
      output_grad_ptr, input_grad_ptr, num_elements, num_replicas);
}

}; // namespace FlexFlow
