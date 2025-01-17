#ifndef _FLEXFLOW_PARALLEL_OP_H
#define _FLEXFLOW_PARALLEL_OP_H

#include "flexflow/model.h"
#include "tl/optional.hpp"

namespace FlexFlow {

struct ParallelOpInfo {
  friend void swap(ParallelOpInfo &, ParallelOpInfo &);

  OperatorType op_type;
  int parallel_dim;
  int parallel_degree;
};

struct ParallelOpJoinResult {
  tl::optional<ParallelOpInfo> op = tl::nullopt;
  bool join_did_succeed = false;
};

ParallelOpJoinResult try_join_parallel_ops(ParallelOpInfo const &,
                                           ParallelOpInfo const &);

class ParallelOp : public Op {
public:
  ParallelOp(FFModel &model,
             OperatorType type,
             char const *_name,
             const ParallelTensor input);
  virtual void init(FFModel const &) = 0;
  virtual void forward(FFModel const &) = 0;
  virtual void backward(FFModel const &) = 0;
  virtual void create_input_partition(FFModel &model) = 0;
  void print_layer(FFModel const &model){};
  virtual bool measure_operator_cost(Simulator *sim,
                                     MachineView const &pc,
                                     CostMetrics &cost_metrics) const = 0;
  virtual bool append_parallel_op_info(
      std::vector<ParallelOpInfo> &parallel_ops) const = 0;
  virtual bool is_parallel_op() const;

public:
  Legion::LogicalPartition input_lp, output_grad_lp;
};

}; // namespace FlexFlow

#endif // _FLEXFLOW_PARALLEL_OP_H
