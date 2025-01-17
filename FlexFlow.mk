# Copyright 2021 Stanford, Facebook, LANL
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

ifndef FF_HOME
$(error FF_HOME variable is not defined, aborting build)
endif

ifndef LG_RT_DIR
LG_RT_DIR	?= $(FF_HOME)/deps/legion/runtime
endif

ifndef CUDA_HOME
CUDA_HOME = $(patsubst %/bin/nvcc,%,$(shell which nvcc | head -1))
endif

ifndef CUDNN_HOME
CUDNN_HOME = $(CUDA_HOME)
endif

ifndef NCCL_HOME
NCCL_HOME = $(CUDA_HOME)
endif

ifndef HIPLIB_HOME
HIPLIB_HOME = /opt/rocm-4.3.1
endif

#ifndef MPI_HOME
#MPI_HOME = $(patsubst %/bin/mpicc,%,$(shell which mpicc | head -1))
#endif

ifeq ($(strip $(USE_GASNET)),1)
  ifndef GASNET
  $(error USE_GASNET is enabled, but GASNET variable is not defined, aborting build)
  endif
endif

GEN_SRC += ${FF_HOME}/src/runtime/accessor.cc\
		${FF_HOME}/src/runtime/graph.cc\
		${FF_HOME}/src/runtime/initializer.cc\
		${FF_HOME}/src/runtime/layer.cc\
		${FF_HOME}/src/runtime/machine_model.cc\
		${FF_HOME}/src/runtime/machine_view.cc\
		${FF_HOME}/src/runtime/model.cc\
		${FF_HOME}/src/runtime/network.cc\
		${FF_HOME}/src/runtime/optimizer.cc\
		${FF_HOME}/src/runtime/parallel_op.cc\
		${FF_HOME}/src/runtime/recursive_logger.cc\
		${FF_HOME}/src/runtime/simulator.cc\
		${FF_HOME}/src/runtime/strategy.cc\
		${FF_HOME}/src/runtime/substitution.cc\
		${FF_HOME}/src/runtime/tensor.cc\
		${FF_HOME}/src/mapper/mapper.cc\
		${FF_HOME}/src/ops/aggregate.cc\
		${FF_HOME}/src/ops/aggregate_spec.cc\
		${FF_HOME}/src/ops/attention.cc\
		${FF_HOME}/src/ops/batch_matmul.cc\
		${FF_HOME}/src/ops/batch_norm.cc\
		${FF_HOME}/src/ops/cast.cc\
		${FF_HOME}/src/ops/cache.cc\
		${FF_HOME}/src/ops/concat.cc\
		${FF_HOME}/src/ops/conv_2d.cc\
		${FF_HOME}/src/ops/dropout.cc\
		${FF_HOME}/src/ops/element_binary.cc\
		${FF_HOME}/src/ops/element_unary.cc\
		${FF_HOME}/src/ops/embedding.cc\
		${FF_HOME}/src/ops/flat.cc\
		${FF_HOME}/src/ops/fused.cc\
		${FF_HOME}/src/ops/group_by.cc\
		${FF_HOME}/src/ops/layer_norm.cc\
		${FF_HOME}/src/ops/linear.cc\
		${FF_HOME}/src/ops/mean.cc\
		${FF_HOME}/src/ops/noop.cc\
		${FF_HOME}/src/ops/pool_2d.cc\
		${FF_HOME}/src/ops/reshape.cc\
		${FF_HOME}/src/ops/reverse.cc\
		${FF_HOME}/src/ops/softmax.cc\
		${FF_HOME}/src/ops/split.cc\
		${FF_HOME}/src/ops/topk.cc\
		${FF_HOME}/src/ops/transpose.cc\
		${FF_HOME}/src/parallel_ops/partition.cc\
		${FF_HOME}/src/parallel_ops/combine.cc\
		${FF_HOME}/src/parallel_ops/replicate.cc\
		${FF_HOME}/src/parallel_ops/reduction.cc\
		${FF_HOME}/src/parallel_ops/fused_parallel_op.cc\
		${FF_HOME}/src/loss_functions/loss_functions.cc\
		${FF_HOME}/src/metrics_functions/metrics_functions.cc\
		${FF_HOME}/src/recompile/recompile_state.cc

FF_CUDA_SRC	+= ${FF_HOME}/src/ops/conv_2d.cu\
		${FF_HOME}/src/ops/aggregate.cu\
		${FF_HOME}/src/ops/aggregate_spec.cu\
		${FF_HOME}/src/ops/attention.cu\
		${FF_HOME}/src/ops/batch_matmul.cu\
		${FF_HOME}/src/ops/batch_norm.cu\
		${FF_HOME}/src/ops/cache.cu\
		${FF_HOME}/src/ops/cast.cu\
		${FF_HOME}/src/ops/concat.cu\
		${FF_HOME}/src/ops/dropout.cu\
		${FF_HOME}/src/ops/element_binary.cu\
		${FF_HOME}/src/ops/element_unary.cu\
		${FF_HOME}/src/ops/embedding.cu\
		${FF_HOME}/src/ops/flat.cu\
		${FF_HOME}/src/ops/fused.cu\
		${FF_HOME}/src/ops/group_by.cu\
		${FF_HOME}/src/ops/layer_norm.cu\
		${FF_HOME}/src/ops/linear.cu\
		${FF_HOME}/src/ops/mean.cu\
		${FF_HOME}/src/ops/pool_2d.cu\
		${FF_HOME}/src/ops/reshape.cu\
		${FF_HOME}/src/ops/reverse.cu\
		${FF_HOME}/src/ops/softmax.cu\
		${FF_HOME}/src/ops/split.cu\
		${FF_HOME}/src/ops/topk.cu\
		${FF_HOME}/src/ops/transpose.cu\
		${FF_HOME}/src/parallel_ops/combine.cu\
		${FF_HOME}/src/parallel_ops/partition.cu\
		${FF_HOME}/src/parallel_ops/replicate.cu\
		${FF_HOME}/src/parallel_ops/reduction.cu\
		${FF_HOME}/src/parallel_ops/fused_parallel_op.cu\
		${FF_HOME}/src/loss_functions/loss_functions.cu\
		${FF_HOME}/src/metrics_functions/metrics_functions.cu\
		${FF_HOME}/src/runtime/accessor_kernel.cu\
		${FF_HOME}/src/runtime/cuda_helper.cu\
		${FF_HOME}/src/runtime/initializer_kernel.cu\
		${FF_HOME}/src/runtime/model.cu\
		${FF_HOME}/src/runtime/optimizer_kernel.cu\
		${FF_HOME}/src/runtime/simulator.cu\
		${FF_HOME}/src/runtime/tensor.cu
		
FF_HIP_SRC	+= ${FF_HOME}/src/ops/conv_2d.cpp\
		${FF_HOME}/src/ops/aggregate.cpp\
		${FF_HOME}/src/ops/aggregate_spec.cpp\
		${FF_HOME}/src/ops/attention.cpp\
		${FF_HOME}/src/ops/batch_matmul.cpp\
		${FF_HOME}/src/ops/batch_norm.cpp\
		${FF_HOME}/src/ops/cache.cpp\
		${FF_HOME}/src/ops/cast.cpp\
		${FF_HOME}/src/ops/concat.cpp\
		${FF_HOME}/src/ops/dropout.cpp\
		${FF_HOME}/src/ops/element_binary.cpp\
		${FF_HOME}/src/ops/element_unary.cpp\
		${FF_HOME}/src/ops/embedding.cpp\
		${FF_HOME}/src/ops/flat.cpp\
		${FF_HOME}/src/ops/fused.cpp\
		${FF_HOME}/src/ops/group_by.cpp\
		${FF_HOME}/src/ops/layer_norm.cpp\
		${FF_HOME}/src/ops/linear.cpp\
		${FF_HOME}/src/ops/mean.cpp\
		${FF_HOME}/src/ops/pool_2d.cpp\
		${FF_HOME}/src/ops/reshape.cpp\
		${FF_HOME}/src/ops/reverse.cpp\
		${FF_HOME}/src/ops/softmax.cpp\
		${FF_HOME}/src/ops/split.cpp\
		${FF_HOME}/src/ops/topk.cpp\
		${FF_HOME}/src/ops/transpose.cpp\
		${FF_HOME}/src/parallel_ops/combine.cpp\
		${FF_HOME}/src/parallel_ops/partition.cpp\
		${FF_HOME}/src/parallel_ops/replicate.cpp\
		${FF_HOME}/src/parallel_ops/reduction.cpp\
		${FF_HOME}/src/parallel_ops/fused_parallel_op.cpp\
		${FF_HOME}/src/loss_functions/loss_functions.cpp\
		${FF_HOME}/src/metrics_functions/metrics_functions.cpp\
		${FF_HOME}/src/runtime/accessor_kernel.cpp\
    ${FF_HOME}/src/runtime/hip_helper.cpp\
		${FF_HOME}/src/runtime/initializer_kernel.cpp\
		${FF_HOME}/src/runtime/model.cpp\
		${FF_HOME}/src/runtime/optimizer_kernel.cpp\
		${FF_HOME}/src/runtime/simulator.cpp\
		${FF_HOME}/src/runtime/tensor.cpp
		
GEN_GPU_SRC += $(FF_CUDA_SRC)
ifeq ($(strip $(HIP_TARGET)),CUDA)
  GEN_HIP_SRC += $(FF_CUDA_SRC)
else
  GEN_HIP_SRC += $(FF_HIP_SRC)
endif

ifneq ($(strip $(FF_USE_PYTHON)), 1)
  GEN_SRC		+= ${FF_HOME}/src/runtime/cpp_driver.cc
endif


INC_FLAGS	+= -I${FF_HOME}/include
CC_FLAGS	+= -DMAX_TENSOR_DIM=$(MAX_DIM) -DLEGION_MAX_RETURN_SIZE=32768
NVCC_FLAGS	+= -DMAX_TENSOR_DIM=$(MAX_DIM) -DLEGION_MAX_RETURN_SIZE=32768
HIPCC_FLAGS     += -DMAX_TENSOR_DIM=$(MAX_DIM) -DLEGION_MAX_RETURN_SIZE=32768
GASNET_FLAGS	+=
# For Point and Rect typedefs
CC_FLAGS	+= -std=c++11
NVCC_FLAGS	+= -std=c++11
HIPCC_FLAGS     += -std=c++11

ifeq ($(strip $(FF_USE_NCCL)), 1)
INC_FLAGS	+= -I$(MPI_HOME)/include -I$(NCCL_HOME)/include
CC_FLAGS	+= -DFF_USE_NCCL
NVCC_FLAGS	+= -DFF_USE_NCCL
HIPCC_FLAGS     += -DFF_USE_NCCL
LD_FLAGS	+= -L$(NCCL_HOME)/lib -lnccl
endif

ifeq ($(strip $(FF_USE_AVX2)), 1)
CC_FLAGS	+= -DFF_USE_AVX2 -mavx2
endif

ifeq ($(strip $(USE_CUDA)),1)
CC_FLAGS	+= -DFF_USE_CUDA
NVCC_FLAGS	+= -DFF_USE_CUDA
INC_FLAGS	+= -I$(CUDNN_HOME)/include -I$(CUDA_HOME)/include
LD_FLAGS	+= -lcudnn -lcublas -lcurand -L$(CUDNN_HOME)/lib64 -L$(CUDA_HOME)/lib64
endif

ifeq ($(strip $(USE_HIP)),1)
ifeq ($(strip $(HIP_TARGET)),CUDA)
CC_FLAGS	+= -DFF_USE_HIP_CUDA
HIPCC_FLAGS	+= -DFF_USE_HIP_CUDA
INC_FLAGS	+= -I$(CUDNN_HOME)/include -I$(CUDA_HOME)/include
LD_FLAGS	+= -lcudnn -lcublas -lcurand -L$(CUDNN_HOME)/lib64 -L$(CUDA_HOME)/lib64
else
CC_FLAGS	+= -DFF_USE_HIP_ROCM
HIPCC_FLAGS	+= -DFF_USE_HIP_ROCM
INC_FLAGS	+= -I$(HIPLIB_HOME)/include -I$(HIPLIB_HOME)/include/miopen -I$(HIPLIB_HOME)/include/rocrand -I$(HIPLIB_HOME)/include/hiprand 
LD_FLAGS	+= -lMIOpen -lhipblas -lhiprand -L$(HIPLIB_HOME)/lib
endif
endif

#ifndef HDF5
#HDF5_inc	?= /usr/include/hdf5/serial
#HDF5_lib	?= /usr/lib/x86_64-linux-gnu/hdf5/serial
#INC_FLAGS	+= -I${HDF5}/
#LD_FLAGS	+= -L${HDF5_lib} -lhdf5
#endif

###########################################################################
#
#   Don't change anything below here
#
###########################################################################

include $(LG_RT_DIR)/runtime.mk