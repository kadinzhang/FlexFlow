/* Copyright 2020 Stanford, Los Alamos National Laboratory
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

#include "flexflow_dataloader.h"
#include <fstream>
#include <sstream>
#include <string>

using namespace Legion;
using namespace FlexFlow;

ImgDataLoader::ImgDataLoader() {}

void ImgDataLoader::reset() {
  next_index = 0;
}

ImgDataLoader4D::ImgDataLoader4D(FFModel &ff,
                                 ParallelTensor input,
                                 ParallelTensor label,
                                 ParallelTensor full_input_,
                                 ParallelTensor full_label_,
                                 int num_samples_) {
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  num_samples = num_samples_;
  // Create full input
  {
    batch_input = input;
    ParallelDim dims[4];
    dims[0].size = num_samples;
    dims[1].size = input->dims[2].size;
    dims[2].size = input->dims[1].size;
    dims[3].size = input->dims[0].size;
    full_input = ff.create_parallel_tensor<4>(dims, DT_FLOAT);
    ff.map_tensor(full_input, NULL /*parallel_op*/);
  }
  // Create full label
  {
    batch_label = label;
    ParallelDim dims[2];
    dims[0].size = num_samples;
    dims[1].size = label->dims[0].size;
    full_label = ff.create_parallel_tensor<2>(dims, DT_INT32);
    ff.map_tensor(full_label, NULL /*parallel_op*/);
  }
  // Load entire dataset
  // TODO: Use index launcher instead of task launcher
  TaskLauncher launcher(CUSTOM_CPU_TASK_ID_2, TaskArgument(NULL, 0));
  // regions[0]: full_input
  launcher.add_region_requirement(RegionRequirement(full_input->region,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_input->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(0, FID_DATA);
  // regions[1]: full_label
  launcher.add_region_requirement(RegionRequirement(full_label->region,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_label->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(1, FID_DATA);
  // regions[2]: full_input_
  launcher.add_region_requirement(RegionRequirement(
      full_input_->region, READ_ONLY, EXCLUSIVE, full_input_->region));
  launcher.add_field(2, FID_DATA);
  // regions[3]: full_label_
  launcher.add_region_requirement(RegionRequirement(
      full_label_->region, READ_ONLY, EXCLUSIVE, full_label_->region));
  launcher.add_field(3, FID_DATA);
  Future fu = runtime->execute_task(ctx, launcher);
  fu.wait();
  reset();
  next_batch(ff);
}

ImgDataLoader4D::ImgDataLoader4D(FFModel &ff,
                                 NetConfig const &alexnet,
                                 ParallelTensor input,
                                 ParallelTensor label) {
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  num_samples = 0;
  if (alexnet.dataset_path == "") {
    printf("Use random dataset...");
    num_samples = 256 * 10 * ff.config.workersPerNode * ff.config.numNodes;
    printf("Number of random samples = %d\n", num_samples);
  } else {
    printf("Start loading dataset from %s\n", alexnet.dataset_path.c_str());
    size_t filesize = get_file_size(alexnet.dataset_path);
    assert(filesize % 3073 == 0);
    num_samples = filesize / 3073;
  }
  // Create full input
  {
    batch_input = input;
    ParallelDim dims[4];
    dims[0].size = num_samples;
    dims[1].size = input->dims[2].size;
    dims[2].size = input->dims[1].size;
    dims[3].size = input->dims[0].size;
    full_input = ff.create_parallel_tensor<4>(dims, DT_FLOAT);
    ff.map_tensor(full_input, NULL /*parallel_op*/);
  }
  // Create full label
  {
    batch_label = label;
    ParallelDim dims[2];
    dims[0].size = num_samples;
    dims[1].size = label->dims[0].size;
    full_label = ff.create_parallel_tensor<2>(dims, DT_INT32);
    ff.map_tensor(full_label, NULL /*parallel_op*/);
  }
  // Load entire dataset
  // TODO: Use index launcher instead of task launcher
  NetConfig const *ptr = &alexnet;
  TaskLauncher launcher(CUSTOM_CPU_TASK_ID_1,
                        TaskArgument(&ptr, sizeof(NetConfig *)));
  // regions[0]: full_input
  launcher.add_region_requirement(RegionRequirement(full_input->region,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_input->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(0, FID_DATA);
  // regions[1]: full_label
  launcher.add_region_requirement(RegionRequirement(full_label->region,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_label->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(1, FID_DATA);
  runtime->execute_task(ctx, launcher);
  reset();
  next_batch(ff);
}

void ImgDataLoader4D::load_entire_dataset_from_numpy(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime) {
  assert(regions.size() == 4);
  assert(task->regions.size() == regions.size());
  AccessorWO<float, 4> const acc_input(regions[0], FID_DATA);
  AccessorWO<int, 2> const acc_label(regions[1], FID_DATA);
  AccessorRO<float, 4> const acc_input_(regions[2], FID_DATA);
  AccessorRO<int, 2> const acc_label_(regions[3], FID_DATA);
  Rect<4> rect_input = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  assert(acc_input.accessor.is_dense_arbitrary(rect_input));
  Rect<2> rect_label = runtime->get_index_space_domain(
      ctx, task->regions[1].region.get_index_space());
  Rect<4> rect_input_ = runtime->get_index_space_domain(
      ctx, task->regions[2].region.get_index_space());
  assert(acc_input_.accessor.is_dense_arbitrary(rect_input_));
  Rect<2> rect_label_ = runtime->get_index_space_domain(
      ctx, task->regions[3].region.get_index_space());
  assert(acc_label_.accessor.is_dense_arbitrary(rect_label_));
  float *input_ptr = acc_input.ptr(rect_input.lo);
  int *label_ptr = acc_label.ptr(rect_label.lo);
  float const *input_ptr_ = acc_input_.ptr(rect_input_.lo);
  int const *label_ptr_ = acc_label_.ptr(rect_label_.lo);
  printf("Check ptr input %p %lu %lu, label %p %lu %lu\n",
         input_ptr_,
         (uintptr_t)input_ptr_,
         rect_input.volume(),
         label_ptr_,
         (uintptr_t)label_ptr_,
         rect_label.volume());
  int num_samples = rect_label.hi[1] - rect_label.lo[1] + 1;
  assert(rect_input.hi[3] - rect_input.lo[3] + 1 == num_samples);
  assert(rect_label.volume() == rect_label_.volume());
  assert(rect_input.volume() == rect_input_.volume());
  memcpy(input_ptr, input_ptr_, sizeof(float) * rect_input.volume());
  memcpy(label_ptr, label_ptr_, sizeof(int) * rect_label.volume());
  for (int i = 0; i < 32; i++) {
    printf("%f ", input_ptr[i]);
  }
  printf("\n");
}

__inline__ int calc_offset(int c, int y, int x, int yscale, int xscale) {
  return (c * yscale * xscale + y * xscale + x);
}

void nearest_neigh(unsigned char *image,
                   unsigned char *buffer,
                   int height,
                   int width,
                   int orig_height,
                   int orig_width,
                   float height_scale,
                   float width_scale) {
  for (int y = 0; y < height; y++) {
    int y0 =
        std::min(static_cast<int>(roundf(y * height_scale)), orig_height - 1);
    for (int x = 0; x < width; x++) {
      int x0 =
          std::min(static_cast<int>(roundf(x * width_scale)), orig_width - 1);
      for (int c = 0; c < 3; c++) {
        int origOffset = calc_offset(y0, x0, c, orig_width, 3);
        int offset = calc_offset(c, y, x, height, width);
        image[offset] = buffer[origOffset];
      }
    }
  }
}

void ImgDataLoader4D::load_entire_dataset(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime) {
  NetConfig const *alexnet = *((NetConfig **)task->args);
  assert(regions.size() == 2);
  assert(task->regions.size() == regions.size());
  AccessorWO<float, 4> const acc_input(regions[0], FID_DATA);
  AccessorWO<int, 2> const acc_label(regions[1], FID_DATA);
  Rect<4> rect_input = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  assert(acc_input.accessor.is_dense_arbitrary(rect_input));
  Rect<2> rect_label = runtime->get_index_space_domain(
      ctx, task->regions[1].region.get_index_space());
  assert(acc_label.accessor.is_dense_arbitrary(rect_label));
  float *input_ptr = acc_input.ptr(rect_input.lo);
  int *label_ptr = acc_label.ptr(rect_label.lo);
  int num_samples = rect_label.hi[1] - rect_label.lo[1] + 1;
  assert(rect_input.hi[3] - rect_input.lo[3] + 1 == num_samples);
  if (alexnet->dataset_path.length() == 0) {
    printf("Start generating random input samples\n");
    for (size_t i = 0; i < rect_label.volume(); i++)
      label_ptr[i] = std::rand() % 10;
    return;
  }
  printf("Start loading %d samples from %s\n",
         num_samples,
         alexnet->dataset_path.c_str());
  int height = rect_input.hi[1] - rect_input.lo[1] + 1;
  int width = rect_input.hi[0] - rect_input.lo[0] + 1;
  int origHeight = 32;
  int origWidth = 32;
  float heightScale = static_cast<float>(origHeight) / height;
  float widthScale = static_cast<float>(origWidth) / width;
  FILE *file = fopen(alexnet->dataset_path.c_str(), "rb");
  unsigned char *buffer = (unsigned char *)malloc(3073);
  unsigned char *image = (unsigned char *)malloc(3 * height * width);
  for (off_t i = 0; i < num_samples; i++) {
    size_t ret = fread(buffer, sizeof(unsigned char), 3073, file);
    assert(ret = 3073);
    if (i == 0) {
      for (int i = 0; i < 32; i++) {
        printf("%f ", static_cast<float>(buffer[i]) / 255);
      }
      printf("\n");
    }
    if ((i + 1) % 1000 == 0) {
      printf("Loaded %zd samples\n", i + 1);
    }
    label_ptr[i] = buffer[0];
    nearest_neigh(image,
                  buffer + 1,
                  height,
                  width,
                  origHeight,
                  origWidth,
                  heightScale,
                  widthScale);
    off_t input_offset = i * 3 * height * width;
    off_t image_offset = 0;
    for (off_t h = 0; h < 3 * height * width; h++)
      input_ptr[input_offset++] =
          static_cast<float>(image[image_offset++]) / 255;
  }
  printf("Finish loading %d samples from %s\n",
         num_samples,
         alexnet->dataset_path.c_str());
  fclose(file);
  for (int i = 0; i < 32; i++) {
    printf("%f ", input_ptr[i]);
  }
  printf("\n");
}

void ImgDataLoader4D::next_batch(FFModel &ff) {
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  // Load input
  {
    Domain domain =
        runtime->get_index_space_domain(ctx, batch_input->parallel_is);
    ArgumentMap argmap;
    int idx = next_index;
    for (Domain::DomainPointIterator it(domain); it; it++) {
      SampleIdxs meta;
      assert(ff.config.batchSize == batch_input->dims[3].size);
      meta.num_samples =
          batch_input->dims[3].size / batch_input->dims[3].degree;
      for (int i = 0; i < meta.num_samples; i++)
        meta.idxs[i] = idx++;
      argmap.set_point(*it, TaskArgument(&meta, sizeof(SampleIdxs)));
    }
    IndexLauncher launcher(CUSTOM_GPU_TASK_ID_1,
                           batch_input->parallel_is,
                           TaskArgument(NULL, 0),
                           argmap,
                           Predicate::TRUE_PRED,
                           false /*must*/,
                           0 /*mapper_id*/,
                           batch_input->machine_view.hash());
    launcher.add_region_requirement(RegionRequirement(full_input->region,
                                                      0 /*projection id*/,
                                                      READ_ONLY,
                                                      EXCLUSIVE,
                                                      full_input->region,
                                                      MAP_TO_ZC_MEMORY));
    launcher.add_field(0, FID_DATA);
    launcher.add_region_requirement(RegionRequirement(batch_input->part,
                                                      0 /*projection id*/,
                                                      WRITE_ONLY,
                                                      EXCLUSIVE,
                                                      batch_input->region));
    launcher.add_field(1, FID_DATA);
    runtime->execute_index_space(ctx, launcher);
  }
  // Load label
  {
    Domain domain =
        runtime->get_index_space_domain(ctx, batch_label->parallel_is);
    ArgumentMap argmap;
    int idx = next_index;
    for (Domain::DomainPointIterator it(domain); it; it++) {
      SampleIdxs meta;
      assert(ff.config.batchSize == batch_label->dims[1].size);
      meta.num_samples =
          batch_label->dims[1].size / batch_label->dims[1].degree;
      for (int i = 0; i < meta.num_samples; i++)
        meta.idxs[i] = idx++;
      argmap.set_point(*it, TaskArgument(&meta, sizeof(SampleIdxs)));
    }
    IndexLauncher launcher(CUSTOM_GPU_TASK_ID_2,
                           batch_label->parallel_is,
                           TaskArgument(NULL, 0),
                           argmap,
                           Predicate::TRUE_PRED,
                           false /*must*/,
                           0 /*mapper_id*/,
                           batch_label->machine_view.hash());
    launcher.add_region_requirement(RegionRequirement(full_label->region,
                                                      0 /*projection id*/,
                                                      READ_ONLY,
                                                      EXCLUSIVE,
                                                      full_label->region,
                                                      MAP_TO_ZC_MEMORY));
    launcher.add_field(0, FID_DATA);
    launcher.add_region_requirement(RegionRequirement(batch_label->part,
                                                      0 /*projection id*/,
                                                      WRITE_ONLY,
                                                      EXCLUSIVE,
                                                      batch_label->region));
    launcher.add_field(1, FID_DATA);
    runtime->execute_index_space(ctx, launcher);
  }
  next_index += ff.config.batchSize;
}

size_t ImgDataLoader4D::get_file_size(std::string const &filename) {
  std::streampos begin, end;
  std::ifstream file(filename.c_str(), std::ios::binary);
  begin = file.tellg();
  file.seekg(0, std::ios::end);
  end = file.tellg();
  file.close();
  size_t filesize = end - begin;
  return filesize;
}

ImgDataLoader2D::ImgDataLoader2D(FFModel &ff,
                                 ParallelTensor input,
                                 ParallelTensor label,
                                 ParallelTensor full_input_,
                                 ParallelTensor full_label_,
                                 int num_samples_) {
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  num_samples = num_samples_;
  // Create full input
  {
    batch_input = input;
    ParallelDim dims[2];
    dims[0].size = num_samples;
    dims[1].size = input->dims[0].size;
    full_input = ff.create_parallel_tensor<2>(dims, DT_FLOAT);
    ff.map_tensor(full_input, NULL);
  }
  // Create full label
  {
    batch_label = label;
    ParallelDim dims[2];
    dims[0].size = num_samples;
    dims[1].size = label->dims[0].size;
    full_label = ff.create_parallel_tensor<2>(dims, DT_INT32);
    ff.map_tensor(full_label, NULL);
  }
  // Load entire dataset
  // TODO: Use index launcher instead of task launcher
  TaskLauncher launcher(CUSTOM_CPU_TASK_ID_3, TaskArgument(NULL, 0));
  // regions[0]: full_input
  launcher.add_region_requirement(RegionRequirement(full_input->region,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_input->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(0, FID_DATA);
  // regions[1]: full_label
  launcher.add_region_requirement(RegionRequirement(full_label->region,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_label->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(1, FID_DATA);
  // regions[2]: full_input_
  launcher.add_region_requirement(RegionRequirement(
      full_input_->region, READ_ONLY, EXCLUSIVE, full_input_->region));
  launcher.add_field(2, FID_DATA);
  // regions[3]: full_label_
  launcher.add_region_requirement(RegionRequirement(
      full_label_->region, READ_ONLY, EXCLUSIVE, full_label_->region));
  launcher.add_field(3, FID_DATA);
  Future fu = runtime->execute_task(ctx, launcher);
  fu.wait();
  reset();
  next_batch(ff);
}

void ImgDataLoader2D::load_entire_dataset_from_numpy(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime) {
  assert(regions.size() == 4);
  assert(task->regions.size() == regions.size());
  AccessorWO<float, 2> const acc_input(regions[0], FID_DATA);
  AccessorWO<int, 2> const acc_label(regions[1], FID_DATA);
  AccessorRO<float, 2> const acc_input_(regions[2], FID_DATA);
  AccessorRO<int, 2> const acc_label_(regions[3], FID_DATA);
  Rect<2> rect_input = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  assert(acc_input.accessor.is_dense_arbitrary(rect_input));
  Rect<2> rect_label = runtime->get_index_space_domain(
      ctx, task->regions[1].region.get_index_space());
  Rect<2> rect_input_ = runtime->get_index_space_domain(
      ctx, task->regions[2].region.get_index_space());
  assert(acc_input_.accessor.is_dense_arbitrary(rect_input_));
  Rect<2> rect_label_ = runtime->get_index_space_domain(
      ctx, task->regions[3].region.get_index_space());
  assert(acc_label_.accessor.is_dense_arbitrary(rect_label_));
  float *input_ptr = acc_input.ptr(rect_input.lo);
  int *label_ptr = acc_label.ptr(rect_label.lo);
  float const *input_ptr_ = acc_input_.ptr(rect_input_.lo);
  int const *label_ptr_ = acc_label_.ptr(rect_label_.lo);
  printf("Check ptr input %p %lu %lu, label %p %lu %lu\n",
         input_ptr_,
         (uintptr_t)input_ptr_,
         rect_input.volume(),
         label_ptr_,
         (uintptr_t)label_ptr_,
         rect_label.volume());
  int num_samples = rect_label.hi[1] - rect_label.lo[1] + 1;
  assert(rect_input.hi[1] - rect_input.lo[1] + 1 == num_samples);
  assert(rect_label.volume() == rect_label_.volume());
  assert(rect_input.volume() == rect_input_.volume());
  memcpy(input_ptr, input_ptr_, sizeof(float) * rect_input.volume());
  memcpy(label_ptr, label_ptr_, sizeof(int) * rect_label.volume());
  for (int i = 0; i < 32; i++) {
    printf("%f ", input_ptr[i]);
  }
  printf("\n");
}

void ImgDataLoader2D::next_batch(FFModel &ff) {
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  // Load input
  {
    Domain domain =
        runtime->get_index_space_domain(ctx, batch_input->parallel_is);
    ArgumentMap argmap;
    int idx = next_index;
    for (Domain::DomainPointIterator it(domain); it; it++) {
      SampleIdxs meta;
      assert(ff.config.batchSize == batch_input->dims[1].size);
      meta.num_samples =
          batch_input->dims[1].size / batch_input->dims[1].degree;
      for (int i = 0; i < meta.num_samples; i++)
        meta.idxs[i] = idx++;
      argmap.set_point(*it, TaskArgument(&meta, sizeof(SampleIdxs)));
    }
    IndexLauncher launcher(CUSTOM_GPU_TASK_ID_3,
                           batch_input->parallel_is,
                           TaskArgument(NULL, 0),
                           argmap,
                           Predicate::TRUE_PRED,
                           false /*must*/,
                           0 /*mapper_id*/,
                           batch_input->machine_view.hash());
    launcher.add_region_requirement(RegionRequirement(full_input->region,
                                                      0 /*projection id*/,
                                                      READ_ONLY,
                                                      EXCLUSIVE,
                                                      full_input->region,
                                                      MAP_TO_ZC_MEMORY));
    launcher.add_field(0, FID_DATA);
    launcher.add_region_requirement(RegionRequirement(batch_input->part,
                                                      0 /*projection id*/,
                                                      WRITE_ONLY,
                                                      EXCLUSIVE,
                                                      batch_input->region));
    launcher.add_field(1, FID_DATA);
    runtime->execute_index_space(ctx, launcher);
  }
  // Load label
  {
    Domain domain =
        runtime->get_index_space_domain(ctx, batch_label->parallel_is);
    ArgumentMap argmap;
    int idx = next_index;
    for (Domain::DomainPointIterator it(domain); it; it++) {
      SampleIdxs meta;
      assert(ff.config.batchSize == batch_label->dims[1].size);
      meta.num_samples =
          batch_label->dims[1].size / batch_label->dims[1].degree;
      for (int i = 0; i < meta.num_samples; i++)
        meta.idxs[i] = idx++;
      argmap.set_point(*it, TaskArgument(&meta, sizeof(SampleIdxs)));
    }
    IndexLauncher launcher(CUSTOM_GPU_TASK_ID_2,
                           batch_label->parallel_is,
                           TaskArgument(NULL, 0),
                           argmap,
                           Predicate::TRUE_PRED,
                           false /*must*/,
                           0 /*mapper_id*/,
                           batch_label->machine_view.hash());
    launcher.add_region_requirement(RegionRequirement(full_label->region,
                                                      0 /*projection id*/,
                                                      READ_ONLY,
                                                      EXCLUSIVE,
                                                      full_label->region,
                                                      MAP_TO_ZC_MEMORY));
    launcher.add_field(0, FID_DATA);
    launcher.add_region_requirement(RegionRequirement(batch_label->part,
                                                      0 /*projection id*/,
                                                      WRITE_ONLY,
                                                      EXCLUSIVE,
                                                      batch_label->region));
    launcher.add_field(1, FID_DATA);
    runtime->execute_index_space(ctx, launcher);
  }
  next_index += ff.config.batchSize;
}

SingleDataLoader::SingleDataLoader(FFModel &ff,
                                   ParallelTensor input,
                                   ParallelTensor full_input_,
                                   int num_samples_,
                                   DataType datatype_) {
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  num_samples = num_samples_;
  datatype = datatype_;
  // Create full input
  assert(input->num_dims == full_input_->num_dims);
  for (int i = 0; i < input->num_dims - 1; i++)
    assert(full_input_->dims[i].size == input->dims[i].size);
  batch_input = input;
  // Currently assume that the leading dim of input is a replica dim of degree 1
  assert(input->dims[input->num_dims - 1].is_replica_dim);
  assert(input->dims[input->num_dims - 1].size == 1);
  ParallelDim dims[MAX_TENSOR_DIM];
  for (int i = 1; i < input->num_dims - 1; i++) {
    dims[i - 1].size = input->dims[input->num_dims - 2 - i].size;
    dims[i - 1].degree = 1;
    dims[i - 1].parallel_idx = -1;
  }
  dims[0].size = num_samples;
  switch (input->num_dims - 1) {
#define DIMFUNC(DIM)                                                           \
  case DIM: {                                                                  \
    full_input = ff.create_parallel_tensor<DIM>(dims, datatype);               \
    ff.map_tensor(full_input, NULL);                                           \
    break;                                                                     \
  }
    LEGION_FOREACH_N(DIMFUNC)
#undef DIMFUNC
    default:
      assert(false);
  }
  int task_id = -1;
  if (datatype == DT_FLOAT) {
    task_id = PY_DL_FLOAT_LOAD_ENTIRE_CPU_TASK_ID;
  } else if (datatype == DT_INT32) {
    task_id = PY_DL_INT32_LOAD_ENTIRE_CPU_TASK_ID;
  } else if (datatype == DT_INT64) {
    task_id = PY_DL_INT64_LOAD_ENTIRE_CPU_TASK_ID;
  } else {
    assert(0);
  }
  // Load entire dataset
  // TODO: Use index launcher instead of task launcher
  TaskLauncher launcher(task_id, TaskArgument(NULL, 0));
  // regions[0]: full_input
  launcher.add_region_requirement(RegionRequirement(full_input->region,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_input->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(0, FID_DATA);
  // regions[2]: full_input_
  launcher.add_region_requirement(RegionRequirement(
      full_input_->region, READ_ONLY, EXCLUSIVE, full_input_->region));
  launcher.add_field(1, FID_DATA);
  Future fu = runtime->execute_task(ctx, launcher);
  fu.wait();
  reset();
  next_batch(ff);
}

SingleDataLoader::SingleDataLoader(FFModel &ff,
                                   ParallelTensor input,
                                   void *full_input_ptr,
                                   int num_samples_,
                                   DataType datatype_) {
  num_samples = num_samples_;
  datatype = datatype_;
  // Currently assume that the leading dim of input is a replica dim of degree 1
  assert(input->dims[input->num_dims - 1].is_replica_dim);
  assert(input->dims[input->num_dims - 1].size == 1);

  batch_input = input;
  ParallelDim dims[MAX_TENSOR_DIM];
  for (int i = 1; i < input->num_dims; i++) {
    dims[i - 1].size = input->dims[input->num_dims - 1 - i].size;
    dims[i - 1].parallel_idx = -1;
    dims[i - 1].degree = 1;
  }
  dims[0].size = num_samples;

  int task_id = -1;
  if (datatype == DT_FLOAT) {
    task_id = PY_DL_FLOAT_INDEX_LOAD_ENTIRE_CPU_TASK_ID;
  } else if (datatype == DT_INT32) {
    task_id = PY_DL_INT32_INDEX_LOAD_ENTIRE_CPU_TASK_ID;
  } else if (datatype == DT_INT64) {
    task_id = PY_DL_INT64_INDEX_LOAD_ENTIRE_CPU_TASK_ID;
  } else {
    assert(0);
  }

  size_t size_per_sample = 1;
  for (int i = 1; i < input->num_dims - 1; i++) {
    assert(dims[i].size != 0);
    size_per_sample *= dims[i].size;
  }
  switch (input->num_dims - 1) {
#define DIMFUNC(DIM)                                                           \
  case DIM: {                                                                  \
    full_input = ff.create_parallel_tensor<DIM>(dims, datatype);               \
    ff.map_tensor(full_input, NULL);                                           \
    index_loader_xd_launcher<DIM>(                                             \
        ff, task_id, full_input_ptr, size_per_sample);                         \
    break;                                                                     \
  }
    LEGION_FOREACH_N(DIMFUNC)
#undef DIMFUNC
    default:
      assert(false);
  }
  reset();
  next_batch(ff);
}

template <int NDIM>
void SingleDataLoader::index_loader_xd_launcher(FFModel &ff,
                                                int task_id,
                                                void *full_input_ptr,
                                                size_t size_per_sample) {
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;

#ifdef FF_PYTHON_USE_INDEX_LOADER
  IndexSpaceT<NDIM> task_is =
      IndexSpaceT<NDIM>(ff.get_or_create_task_is(NDIM, ""));
  Rect<NDIM> rect = runtime->get_index_space_domain(ctx, task_is);
  ArgumentMap argmap;
  int total_shards = rect.volume();
  int idx = 0;
  for (PointInRectIterator<NDIM> it(rect); it(); it++) {
    IndexLoadArg meta;
    assert(num_samples % total_shards == 0);
    meta.num_samples = num_samples / total_shards;
    meta.size_per_sample = size_per_sample;
    meta.ptr = full_input_ptr;
    meta.idx = idx;
    argmap.set_point(*it, TaskArgument(&meta, sizeof(IndexLoadArg)));
    idx++;
  }
  // Load entire dataset
  IndexLauncher launcher(task_id, task_is, TaskArgument(NULL, 0), argmap);
  // regions[0]: full_input
  launcher.add_region_requirement(RegionRequirement(full_input->part,
                                                    0,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_input->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(0, FID_DATA);
  FutureMap fu = runtime->execute_index_space(ctx, launcher);
  fu.wait_all_results();
#else
  IndexLoadArg meta;
  int total_shards = 1;
  assert(num_samples % total_shards == 0);
  meta.num_samples = num_samples / total_shards;
  meta.size_per_sample = size_per_sample;
  meta.ptr = full_input_ptr;
  meta.idx = 0;
  // Load entire dataset
  TaskLauncher launcher(task_id, TaskArgument(&meta, sizeof(IndexLoadArg)));
  // regions[0]: full_input
  launcher.add_region_requirement(RegionRequirement(full_input->region,
                                                    WRITE_ONLY,
                                                    EXCLUSIVE,
                                                    full_input->region,
                                                    MAP_TO_ZC_MEMORY));
  launcher.add_field(0, FID_DATA);
  Future fu = runtime->execute_task(ctx, launcher);
  fu.wait();
#endif
}

void SingleDataLoader::reset() {
  next_index = 0;
}

void SingleDataLoader::next_batch(FFModel &ff) {
  int task_id = -1;
  if (datatype == DT_FLOAT)
    task_id = PY_DL_FLOAT_LOAD_BATCH_GPU_TASK_ID;
  else if (datatype == DT_INT32)
    task_id = PY_DL_INT32_LOAD_BATCH_GPU_TASK_ID;
  else if (datatype == DT_INT64)
    task_id = PY_DL_INT64_LOAD_BATCH_GPU_TASK_ID;
  else
    assert(0);
  switch (full_input->num_dims) {
#define DIMFUNC(DIM)                                                           \
  case DIM:                                                                    \
    next_batch_xd_launcher<DIM>(ff, task_id);                                  \
    break;
    LEGION_FOREACH_N(DIMFUNC)
#undef DIMFUNC
    default:
      assert(false);
  }
}

template <int NDIM>
void SingleDataLoader::next_batch_xd_launcher(FFModel &ff, int task_id) {
  Context ctx = ff.config.lg_ctx;
  Runtime *runtime = ff.config.lg_hlr;
  // Load input
#if 1
  {
    Domain domain =
        runtime->get_index_space_domain(ctx, full_input->parallel_is);
    ArgumentMap argmap;
    int idx = next_index;
    for (Domain::DomainPointIterator it(domain); it; it++) {
      SampleIdxs meta;
      assert(ff.config.batchSize == batch_input->dims[NDIM - 1].size);
      meta.num_samples =
          batch_input->dims[NDIM - 1].size / batch_input->dims[NDIM - 1].degree;
      for (int i = 0; i < meta.num_samples; i++)
        meta.idxs[i] = idx++;
      argmap.set_point(*it, TaskArgument(&meta, sizeof(SampleIdxs)));
    }
    IndexLauncher launcher(task_id,
                           full_input->parallel_is,
                           TaskArgument(NULL, 0),
                           argmap,
                           Predicate::TRUE_PRED,
                           false /*must*/,
                           0 /*mapper_id*/,
                           full_input->machine_view.hash());
    launcher.add_region_requirement(RegionRequirement(full_input->region,
                                                      0 /*projection id*/,
                                                      READ_ONLY,
                                                      EXCLUSIVE,
                                                      full_input->region,
                                                      MAP_TO_ZC_MEMORY));
    launcher.add_field(0, FID_DATA);
    launcher.add_region_requirement(RegionRequirement(batch_input->part,
                                                      0 /*projection id*/,
                                                      WRITE_ONLY,
                                                      EXCLUSIVE,
                                                      batch_input->region));
    launcher.add_field(1, FID_DATA);
    runtime->execute_index_space(ctx, launcher);
  }
  next_index += ff.config.batchSize;
#else
  {
    IndexSpaceT<NDIM> task_is =
        IndexSpaceT<NDIM>(ff.get_or_create_task_is(NDIM, ""));
    Rect<NDIM> rect = runtime->get_index_space_domain(ctx, task_is);
    ArgumentMap argmap;
    int idx = next_index;
    SampleIdxs meta;
    assert(ff.config.batchSize % (rect.hi[NDIM - 1] - rect.lo[NDIM - 1] + 1) ==
           0);
    meta.num_samples =
        ff.config.batchSize / (rect.hi[NDIM - 1] - rect.lo[NDIM - 1] + 1);
    for (int i = 0; i < meta.num_samples; i++)
      meta.idxs[i] = idx++;
    for (PointInRectIterator<NDIM> it(rect); it(); it++) {
      argmap.set_point(*it, TaskArgument(&meta, sizeof(SampleIdxs)));
    }
    IndexLauncher launcher(task_id,
                           task_is,
                           TaskArgument(NULL, 0),
                           argmap,
                           Predicate::TRUE_PRED,
                           false /*must*/,
                           0 /*mapper_id*/,
                           FFConfig::get_hash_id(""));
    launcher.add_region_requirement(RegionRequirement(full_input->part,
                                                      0 /*projection id*/,
                                                      READ_ONLY,
                                                      EXCLUSIVE,
                                                      full_input->region,
                                                      MAP_TO_ZC_MEMORY));
    launcher.add_field(0, FID_DATA);
    launcher.add_region_requirement(RegionRequirement(batch_input->part,
                                                      0 /*projection id*/,
                                                      WRITE_ONLY,
                                                      EXCLUSIVE,
                                                      batch_input->region));
    launcher.add_field(1, FID_DATA);
    runtime->execute_index_space(ctx, launcher);
    next_index +=
        ff.config.batchSize / (rect.hi[NDIM - 1] - rect.lo[NDIM - 1] + 1);
  }
#endif
}

// Task body
template <typename DT>
void SingleDataLoader::load_entire_dataset_from_numpy(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime) {
  assert(regions.size() == 2);
  assert(task->regions.size() == regions.size());
  Domain domain = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  switch (domain.get_dim()) {
#define DIMFUNC(DIM)                                                           \
  case DIM:                                                                    \
    return load_entire_dataset_from_numpy_with_dim<DT, DIM>(                   \
        task, regions, ctx, runtime);
    LEGION_FOREACH_N(DIMFUNC)
#undef DIMFUNC
    default:
      assert(false);
  }
}

template <typename DT, int NDIM>
void SingleDataLoader::load_entire_dataset_from_numpy_with_dim(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime) {
  assert(regions.size() == 2);
  assert(task->regions.size() == regions.size());
  AccessorWO<DT, NDIM> const acc_input(regions[0], FID_DATA);
  AccessorRO<DT, NDIM> const acc_input_(regions[1], FID_DATA);
  Rect<NDIM> rect_input = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  assert(acc_input.accessor.is_dense_arbitrary(rect_input));
  Rect<NDIM> rect_input_ = runtime->get_index_space_domain(
      ctx, task->regions[1].region.get_index_space());
  assert(acc_input_.accessor.is_dense_arbitrary(rect_input_));
  assert(rect_input_.volume() == rect_input.volume());

  DT *input_ptr = acc_input.ptr(rect_input.lo);
  const DT *input_ptr_ = acc_input_.ptr(rect_input_.lo);
  printf("Load entire dataset: ptr input_ %p %lu %lu, input %p %lu %lu\n",
         input_ptr_,
         (uintptr_t)input_ptr_,
         rect_input_.volume(),
         input_ptr,
         (uintptr_t)input_ptr,
         rect_input.volume());
  assert(rect_input.volume() == rect_input_.volume());
  memcpy(input_ptr, input_ptr_, sizeof(DT) * rect_input.volume());
  for (int i = 0; i < 32; i++) {
    std::cout << input_ptr[i] << " ";
  }
  std::cout << std::endl;
}

// Task body
template <typename DT>
void SingleDataLoader::index_load_entire_dataset_from_numpy(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime) {
  assert(regions.size() == 1);
  assert(task->regions.size() == regions.size());
  Domain domain = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  switch (domain.get_dim()) {
#define DIMFUNC(DIM)                                                           \
  case DIM:                                                                    \
    return index_load_entire_dataset_from_numpy_with_dim<DT, DIM>(             \
        task, regions, ctx, runtime);
    LEGION_FOREACH_N(DIMFUNC)
#undef DIMFUNC
    default:
      assert(false);
  }
}

template <typename DT, int NDIM>
void SingleDataLoader::index_load_entire_dataset_from_numpy_with_dim(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime) {
  assert(regions.size() == 1);
  assert(task->regions.size() == regions.size());
#ifdef FF_PYTHON_USE_INDEX_LOADER
  IndexLoadArg *meta = (IndexLoadArg *)task->local_args;
#else
  IndexLoadArg *meta = (IndexLoadArg *)task->args;
#endif
  AccessorWO<DT, NDIM> const acc_input(regions[0], FID_DATA);
  Rect<NDIM> rect_input = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  assert(acc_input.accessor.is_dense_arbitrary(rect_input));

  DT *input_ptr = acc_input.ptr(rect_input.lo);
  size_t volume = meta->size_per_sample * meta->num_samples;
  DT *input_ptr_head_ = static_cast<DT *>(meta->ptr);
  DT *input_ptr_ = input_ptr_head_ + volume * meta->idx;

  printf("Index load entire dataset: ptr input_head_ %p, idx %d, input_ %p %lu "
         "%lu, input %p %lu %lu\n",
         input_ptr_head_,
         meta->idx,
         input_ptr_,
         (uintptr_t)input_ptr_,
         volume,
         input_ptr,
         (uintptr_t)input_ptr,
         rect_input.volume());
  assert(rect_input.volume() == volume);
  memcpy(input_ptr, input_ptr_, sizeof(DT) * volume);
  for (int i = 0; i < 32; i++) {
    std::cout << input_ptr[i] << " ";
  }
  std::cout << std::endl;
}

void SingleDataLoader::register_cpu_tasks(void) {
  // float Load entire dataset from numpy
  {
    TaskVariantRegistrar registrar(PY_DL_FLOAT_LOAD_ENTIRE_CPU_TASK_ID,
                                   "Float Load Entire Dataset Numpy");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<
        SingleDataLoader::load_entire_dataset_from_numpy<float>>(
        registrar, "Float Load Entire Dataset Task Numpy");
  }
  // int32 Load entire dataset from numpy
  {
    TaskVariantRegistrar registrar(PY_DL_INT32_LOAD_ENTIRE_CPU_TASK_ID,
                                   "Int32 Load Entire Dataset Numpy");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<
        SingleDataLoader::load_entire_dataset_from_numpy<int32_t>>(
        registrar, "Int32 Load Entire Dataset Task Numpy");
  }
  // int64 Load entire dataset from numpy
  {
    TaskVariantRegistrar registrar(PY_DL_INT64_LOAD_ENTIRE_CPU_TASK_ID,
                                   "Int64 Load Entire Dataset Numpy");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<
        SingleDataLoader::load_entire_dataset_from_numpy<int64_t>>(
        registrar, "Int64 Load Entire Dataset Task Numpy");
  }
  // float Index load entire dataset from numpy
  {
    TaskVariantRegistrar registrar(PY_DL_FLOAT_INDEX_LOAD_ENTIRE_CPU_TASK_ID,
                                   "Float Index Load Entire Dataset Numpy");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<
        SingleDataLoader::index_load_entire_dataset_from_numpy<float>>(
        registrar, "Float Index Load Entire Dataset Task Numpy");
  }
  // int32 Index load entire dataset from numpy
  {
    TaskVariantRegistrar registrar(PY_DL_INT32_INDEX_LOAD_ENTIRE_CPU_TASK_ID,
                                   "Int32 Index Load Entire Dataset Numpy");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<
        SingleDataLoader::index_load_entire_dataset_from_numpy<int32_t>>(
        registrar, "Int32 Index Load Entire Dataset Task Numpy");
  }
  // int64 Index load entire dataset from numpy
  {
    TaskVariantRegistrar registrar(PY_DL_INT64_INDEX_LOAD_ENTIRE_CPU_TASK_ID,
                                   "Int64 Index Load Entire Dataset Numpy");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<
        SingleDataLoader::index_load_entire_dataset_from_numpy<int64_t>>(
        registrar, "Int64 Index Load Entire Dataset Task Numpy");
  }
}

void SingleDataLoader::register_gpu_tasks(void) {
  // float load input
  {
    TaskVariantRegistrar registrar(PY_DL_FLOAT_LOAD_BATCH_GPU_TASK_ID,
                                   "Float Load Inputs");
    registrar.add_constraint(ProcessorConstraint(Processor::TOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<SingleDataLoader::load_input<float>>(
        registrar, "Float Load Input Task");
  }
  // int32 load input
  {
    TaskVariantRegistrar registrar(PY_DL_INT32_LOAD_BATCH_GPU_TASK_ID,
                                   "Int32 Load Inputs");
    registrar.add_constraint(ProcessorConstraint(Processor::TOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<SingleDataLoader::load_input<int32_t>>(
        registrar, "Int32 Load Input Task");
  }
  // int64 load input
  {
    TaskVariantRegistrar registrar(PY_DL_INT64_LOAD_BATCH_GPU_TASK_ID,
                                   "Int64 Load Inputs");
    registrar.add_constraint(ProcessorConstraint(Processor::TOC_PROC));
    registrar.set_leaf();
    Runtime::preregister_task_variant<SingleDataLoader::load_input<int64_t>>(
        registrar, "Int64 Load Input Task");
  }
}

template void SingleDataLoader::next_batch_xd_launcher<2>(FFModel &ff,
                                                          int task_id);
template void SingleDataLoader::next_batch_xd_launcher<4>(FFModel &ff,
                                                          int task_id);
template void SingleDataLoader::index_loader_xd_launcher<2>(
    FFModel &ff, int task_id, void *full_input_ptr, size_t size_per_sample);
template void SingleDataLoader::index_loader_xd_launcher<4>(
    FFModel &ff, int task_id, void *full_input_ptr, size_t size_per_sample);
template void SingleDataLoader::load_entire_dataset_from_numpy<float>(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime);
template void SingleDataLoader::load_entire_dataset_from_numpy<int32_t>(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime);
template void SingleDataLoader::load_entire_dataset_from_numpy<int64_t>(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime);
template void SingleDataLoader::index_load_entire_dataset_from_numpy<float>(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime);
template void SingleDataLoader::index_load_entire_dataset_from_numpy<int32_t>(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime);
template void SingleDataLoader::index_load_entire_dataset_from_numpy<int64_t>(
    Task const *task,
    std::vector<PhysicalRegion> const &regions,
    Context ctx,
    Runtime *runtime);
