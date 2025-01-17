#ifndef _FLEXFLOW_BATCH_MATMUL_H
#define _FLEXFLOW_BATCH_MATMUL_H

#include "flexflow/model.h"

namespace FlexFlow {

class BatchMatmulMeta : public OpMeta {
public:
  BatchMatmulMeta(FFHandler handler);
  int a_seq_length_dim, b_seq_length_dim;
};

class BatchMatmul : public Op {
public:
  BatchMatmul(FFModel &model,
              const ParallelTensor A,
              const ParallelTensor B,
              int a_seq_length_dim,
              int b_seq_length_dim,
              char const *name);
  static Op *
      create_operator_from_layer(FFModel &model,
                                 Layer const *layer,
                                 std::vector<ParallelTensor> const &inputs);

  void init(FFModel const &) override;
  void forward(FFModel const &) override;
  void backward(FFModel const &) override;
  void print_layer(FFModel const &model) override;
  static OpMeta *init_task(Legion::Task const *task,
                           std::vector<Legion::PhysicalRegion> const &regions,
                           Legion::Context ctx,
                           Legion::Runtime *runtime);
  static void forward_task(Legion::Task const *task,
                           std::vector<Legion::PhysicalRegion> const &regions,
                           Legion::Context ctx,
                           Legion::Runtime *runtime);
  static void backward_task(Legion::Task const *task,
                            std::vector<Legion::PhysicalRegion> const &regions,
                            Legion::Context ctx,
                            Legion::Runtime *runtime);
  static void forward_kernel(BatchMatmulMeta const *meta,
                             float *o_ptr,
                             float const *a_ptr,
                             float const *b_ptr,
                             float const *c_ptr,
                             int m,
                             int n,
                             int k,
                             int batch,
                             ffStream_t stream,
                             int a_seq_length_dim = -1,
                             int b_seq_length_dim = -1,
                             int seq_length = -1);
  static void forward_kernel_wrapper(BatchMatmulMeta const *meta,
                                     float *o_ptr,
                                     float const *a_ptr,
                                     float const *b_ptr,
                                     float const *c_ptr,
                                     int m,
                                     int n,
                                     int k,
                                     int batch,
                                     int a_seq_length_dim = -1,
                                     int b_seq_length_dim = -1,
                                     int seq_length = -1);
  static void backward_kernel(BatchMatmulMeta const *meta,
                              float const *o_ptr,
                              float const *o_grad_ptr,
                              float const *a_ptr,
                              float *a_grad_ptr,
                              float const *b_ptr,
                              float *b_grad_ptr,
                              float *c_grad_ptr,
                              int m,
                              int n,
                              int k,
                              int batch,
                              ffStream_t stream);
  static void backward_kernel_wrapper(BatchMatmulMeta const *meta,
                                      float const *o_ptr,
                                      float const *o_grad_ptr,
                                      float const *a_ptr,
                                      float *a_grad_ptr,
                                      float const *b_ptr,
                                      float *b_grad_ptr,
                                      float *c_grad_ptr,
                                      int m,
                                      int n,
                                      int k,
                                      int batch);
  bool measure_operator_cost(Simulator *sim,
                             MachineView const &pc,
                             CostMetrics &cost_metrics) const override;

private:
  template <int NDIM>
  void init_with_dim(FFModel const &ff);
  template <int NDIM>
  void forward_with_dim(FFModel const &ff);
  template <int NDIM>
  void backward_with_dim(FFModel const &ff);

public:
  int a_seq_length_dim, b_seq_length_dim;
};

}; // namespace FlexFlow

#endif
