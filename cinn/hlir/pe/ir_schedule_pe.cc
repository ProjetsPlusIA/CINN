// Copyright (c) 2022 CINN Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cinn/hlir/pe/ir_schedule_pe.h"

#include <absl/container/flat_hash_map.h>
#include <isl/cpp.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <utility>

#include "cinn/common/cas.h"
#include "cinn/hlir/pe/load_x86_params.h"
#include "cinn/hlir/pe/schedule.h"
#include "cinn/optim/ir_simplify.h"
#include "cinn/poly/isl_utils.h"

namespace cinn {
namespace hlir {
namespace pe {

void IRCudaScheduleBlockShuffleReduce(
    ir::IRSchedule &ir_sch, ir::Tensor reshape, ir::Tensor internal, ir::Tensor out, const common::Target &target) {
  int fuse_times = internal->shape.size() - 2;
  for (int idx = 0; idx < fuse_times; ++idx) {
    auto internal_block = ir_sch.GetBlock(internal->name);
    auto internal_loops = ir_sch.GetLoops(internal_block);
    CHECK_GE(internal_loops.size(), 2U);
    ir_sch.Fuse({internal_loops[0], internal_loops[1]});
    auto out_block = ir_sch.GetBlock(out->name);
    auto out_loops = ir_sch.GetLoops(out_block);
    CHECK_GE(out_loops.size(), 2U);
    ir_sch.Fuse({out_loops[0], out_loops[1]});
  }

  fuse_times = out->shape.size() - internal->shape.size();
  for (int idx = 0; idx < fuse_times; ++idx) {
    if (internal->shape.size() == 1) {
      auto out_block = ir_sch.GetBlock(out->name);
      auto out_loops = ir_sch.GetLoops(out_block);
      CHECK_GE(out_loops.size(), 2U);
      ir_sch.Fuse({out_loops[0], out_loops[1]});
    } else {
      auto out_block = ir_sch.GetBlock(out->name);
      auto out_loops = ir_sch.GetLoops(out_block);
      CHECK_GE(out_loops.size(), 3U);
      ir_sch.Fuse({out_loops[1], out_loops[2]});
    }
  }

  auto out_block = ir_sch.GetBlock(out->name);
  auto out_loops = ir_sch.GetLoops(out_block);

  if (out_loops.size() == 1) {
    auto internal_block = ir_sch.GetBlock(internal->name);
    auto internal_loops = ir_sch.GetLoops(internal_block);
    ir_sch.Split(internal_loops[0], {-1, ir::GetLoopExtent(internal_loops[0])});
    ir_sch.Split(out_loops[0], {-1, ir::GetLoopExtent(out_loops[0])});
  }

  auto reshape_block = ir_sch.GetBlock(reshape->name);
  ir_sch.ComputeInline(reshape_block);

  auto internal_block = ir_sch.GetBlock(internal->name);
  ir_sch.SetBuffer(internal_block, "shared");
  internal_block      = ir_sch.GetBlock(internal->name);
  auto internal_loops = ir_sch.GetLoops(internal_block);
  CHECK_GE(internal_loops.size(), 2U);
  ir_sch.Bind(internal_loops[0], "blockIdx.x");
  ir_sch.Bind(internal_loops[1], "threadIdx.x");

  out_block = ir_sch.GetBlock(out->name);
  out_loops = ir_sch.GetLoops(out_block);
  CHECK_GE(out_loops.size(), 2U);
  ir_sch.Bind(out_loops[0], "blockIdx.x");
  ir_sch.Bind(out_loops[1], "threadIdx.x");

  internal_block = ir_sch.GetBlock(internal->name);
  out_block      = ir_sch.GetBlock(out->name);
  out_loops      = ir_sch.GetLoops(out_block);
  ir_sch.SimpleComputeAt(internal_block, out_loops[0]);

  // stages[out]->SyncThreads(0, {internal}, stages);
  VLOG(3) << "IRCudaScheduleBlockShuffleReduce result expr is: " << ir_sch.GetModule().GetExprs().at(0);
}

void IRScheduleInjectiveCPU(ir::IRSchedule &ir_sch,
                            const std::vector<int> &output_shape,
                            const common::Target &target,
                            bool vectorizable) {
  auto all_blocks = ir_sch.GetAllBlocks();
  auto loops      = ir_sch.GetLoops(all_blocks[0]);
  int dims        = output_shape.size();
  int factor      = GetBasicFactor(GetTensor(all_blocks[0])->type(), target);
  auto fused      = loops[0];
  if (dims >= 5) {
    fused = ir_sch.Fuse({loops[0], loops[1], loops[2]});
    dims  = dims - 2;
  } else if (dims >= 3) {
    fused = ir_sch.Fuse({loops[0], loops[1]});
    dims  = dims - 1;
  }
  ir_sch.Parallel(fused);

  if (vectorizable) {
    auto all_blocks = ir_sch.GetAllBlocks();
    auto loops      = ir_sch.GetLoops(all_blocks[0]);
    int last_shape  = ir::GetLoopExtent(loops[dims - 1]);
    factor          = GetVectorizeFactor(last_shape, factor);
    auto splited    = ir_sch.Split(loops[dims - 1], {-1, factor});
    ir_sch.Vectorize(splited[1], factor);
    if (dims == 1) {
      ir_sch.Parallel(splited[0]);
    }
  }
}

void IRCudaScheduleInjective(ir::IRSchedule &ir_sch,
                             const std::vector<int> &output_shape,
                             const common::Target &target) {
  auto all_blocks = ir_sch.GetAllBlocks();
  auto loops      = ir_sch.GetLoops(all_blocks[0]);
  auto fused      = ir_sch.Fuse(loops);

  int num_thread        = target.max_num_threads();
  int num_block         = 1024;
  int vector_width      = 1;
  int prod_size         = std::accumulate(output_shape.begin(), output_shape.end(), 1, std::multiplies<int>());
  bool need_block_split = prod_size > num_thread * num_block * vector_width ? true : false;
  if (need_block_split) {
    auto splited = ir_sch.Split(fused, {num_block, num_thread, -1});
    ir_sch.Bind(splited[0], "blockIdx.x");
    ir_sch.Bind(splited[1], "threadIdx.x");
  } else {
    if (prod_size > num_thread) {
      auto splited = ir_sch.Split(fused, {-1, num_thread});
      ir_sch.Bind(splited[0], "blockIdx.x");
      ir_sch.Bind(splited[1], "threadIdx.x");
    } else {
      ir_sch.Bind(fused, "threadIdx.x");
    }
  }
  LOG(INFO) << "After IRCudaScheduleInjective, new ir is : " << ir_sch.GetModule().GetExprs().at(0);
}

void IRCudaScheduleMul(ir::IRSchedule &ir_sch, const std::vector<int> &output_shape, const common::Target &target) {
  auto all_blocks = ir_sch.GetAllBlocks();
  auto loops      = ir_sch.GetLoops(all_blocks.back());
  auto splited    = ir_sch.Split(loops[1], {-1, 2});
  all_blocks      = ir_sch.GetAllBlocks();
  loops           = ir_sch.GetLoops(all_blocks.back());
  ir_sch.Bind(loops[0], "blockIdx.x");
  ir_sch.Bind(loops[1], "threadIdx.x");
}

void IRMulScheduleCPU(ir::IRSchedule &ir_sch,
                      const std::vector<int> &reduce_first_shape,
                      const common::Target &target) {
  ir_sch.MergeExprs();
  auto all_blocks = ir_sch.GetAllBlocks();
  CHECK_EQ(all_blocks.size(), 4U);
  auto loops    = ir_sch.GetLoops(all_blocks[1]);
  int loop_size = loops.size();
  // ir_sch.Reorder({loops[loop_size-1], loops[loop_size-2]});

  if (reduce_first_shape.back() > 1) {
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks[3]);
    ir_sch.Unroll(loops.back());
  }
}

void IRCudaSplitSchedule(ir::IRSchedule &ir_sch,
                         const std::vector<std::vector<int>> &output_shapes,
                         int axis,
                         const common::Target &target) {
  ir_sch.MergeExprs();
  int dims = output_shapes[0].size();
  std::vector<int> reorders;
  for (int i = 0; i < dims; ++i) {
    reorders.push_back(i);
  }
  reorders.erase(reorders.begin() + axis);
  reorders.push_back(axis);
  auto all_blocks = ir_sch.GetAllBlocks();
  for (auto &block : all_blocks) {
    ir_sch.Reorder(block, reorders);
  }
  std::vector<int> fuse_index;
  for (int i = 0; i < dims - 1; ++i) fuse_index.push_back(i);
  all_blocks = ir_sch.GetAllBlocks();
  for (auto &block : all_blocks) {
    ir_sch.Fuse(block, fuse_index);
  }
  int fused_shape = 1;
  for (int i = 0; i < dims; ++i) {
    if (i != axis) fused_shape = fused_shape * output_shapes[0][i];
  }

  all_blocks           = ir_sch.GetAllBlocks();
  auto loops           = ir_sch.GetLoops(all_blocks.back());
  int compute_at_level = 0;
  if (target.arch == Target::Arch::NVGPU) {
    if (fused_shape > target.max_num_threads()) {
      ir_sch.Split(loops[0], {-1, target.max_num_threads()});
      all_blocks = ir_sch.GetAllBlocks();
      loops      = ir_sch.GetLoops(all_blocks.back());
      ir_sch.Bind(loops[0], "blockIdx.x");
      ir_sch.Bind(loops[1], "threadIdx.x");
      compute_at_level++;
    } else {
      ir_sch.Bind(loops[0], "threadIdx.x");
    }
    int all_blocks_num = all_blocks.size();
    for (int i = 0; i < all_blocks_num - 1; ++i) {
      all_blocks = ir_sch.GetAllBlocks();
      loops      = ir_sch.GetLoops(all_blocks[i]);
      if (fused_shape > target.max_num_threads()) {
        ir_sch.Split(loops[0], {-1, target.max_num_threads()});
        all_blocks = ir_sch.GetAllBlocks();
        loops      = ir_sch.GetLoops(all_blocks.back());
        ir_sch.SimpleComputeAt(all_blocks[i], loops[compute_at_level]);
      }
    }
  } else {
    int all_blocks_num = all_blocks.size();
    for (int i = 0; i < all_blocks_num - 1; ++i) {
      all_blocks = ir_sch.GetAllBlocks();
      loops      = ir_sch.GetLoops(all_blocks.back());
      ir_sch.SimpleComputeAt(all_blocks[i], loops[0]);
    }
  }
}

void IRCudaScheduleReduce(ir::IRSchedule &ir_sch,
                          const std::vector<int> &output_shape,
                          int last_dimension_num,
                          const common::Target &target) {
  int parallel_thread_num = 1;
  for (int idx = output_shape.size() - 1; idx >= static_cast<int>(output_shape.size()) - last_dimension_num; --idx) {
    parallel_thread_num *= output_shape[idx];
  }

  int index = output_shape.size() - last_dimension_num;

  auto all_blocks = ir_sch.GetAllBlocks();
  auto loops      = ir_sch.GetLoops(all_blocks.back());

  for (int idx = output_shape.size() - last_dimension_num; idx < static_cast<int>(output_shape.size()) - 1; ++idx) {
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks.back());
    ir_sch.Fuse({loops[index], loops[index + 1]});
  }

  int max_block_size = 1024;
  if (parallel_thread_num > max_block_size) {
    all_blocks   = ir_sch.GetAllBlocks();
    loops        = ir_sch.GetLoops(all_blocks.back());
    auto splited = ir_sch.Split(loops[index], {-1, max_block_size});
    ir_sch.Bind(splited[1], "threadIdx.x");
  } else {
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks.back());
    ir_sch.Bind(loops[index], "threadIdx.x");
  }

  for (int idx = 0; idx < index - 1; ++idx) {
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks.back());
    ir_sch.Fuse({loops[0], loops[1]});
  }

  if (index > 0) {
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks.back());
    ir_sch.Bind(loops[0], "blockIdx.x");
  }
}

void IRCudaScheduleBlockReduceInternal(ir::IRSchedule &ir_sch,
                                       ir::Tensor tmp_out,
                                       ir::Tensor out,
                                       const common::Target &target) {
  ir_sch.MergeExprs();
  auto all_blocks = ir_sch.GetAllBlocks();
  auto loops      = ir_sch.GetLoops(all_blocks[0]);
  CHECK_EQ(all_blocks.size(), 2U);

  for (int idx = 0; idx < static_cast<int>(tmp_out->shape.size()) - 2; ++idx) {
    all_blocks = ir_sch.GetAllBlocks();
    ir_sch.Fuse(all_blocks[0], {0, 1});
    ir_sch.Fuse(all_blocks[1], {0, 1});
  }

  if (tmp_out->shape.size() == 1) {
    all_blocks      = ir_sch.GetAllBlocks();
    auto all_block0 = ir_sch.GetBlock(tmp_out->name);
    loops           = ir_sch.GetLoops(all_block0);
    ir_sch.Bind(loops[0], "threadIdx.x");
    all_block0 = ir_sch.GetBlock(tmp_out->name);
    ir_sch.SetBuffer(all_block0, "local");
    auto all_block1 = ir_sch.GetBlock(out->name);
    loops           = ir_sch.GetLoops(all_block1);
    ir_sch.Bind(loops[0], "threadIdx.x");
  } else {
    all_blocks      = ir_sch.GetAllBlocks();
    auto all_block0 = ir_sch.GetBlock(tmp_out->name);
    loops           = ir_sch.GetLoops(all_block0);
    all_block0      = ir_sch.GetBlock(tmp_out->name);
    ir_sch.SetBuffer(all_block0, "local");
    all_blocks = ir_sch.GetAllBlocks();
    all_block0 = ir_sch.GetBlock(tmp_out->name);
    loops      = ir_sch.GetLoops(all_block0);
    ir_sch.Bind(loops[0], "blockIdx.x");
    all_blocks      = ir_sch.GetAllBlocks();
    auto all_block1 = ir_sch.GetBlock(out->name);
    loops           = ir_sch.GetLoops(all_block1);
    all_block0      = ir_sch.GetBlock(tmp_out->name);
    ir_sch.SimpleComputeAt(all_block0, loops[0]);
    all_blocks = ir_sch.GetAllBlocks();
    all_block0 = ir_sch.GetBlock(tmp_out->name);
    loops      = ir_sch.GetLoops(all_block0);
    ir_sch.Bind(loops[1], "threadIdx.x");
  }
  VLOG(3) << "IRCudaScheduleBlockReduceInternal result expr is: " << ir_sch.GetModule().GetExprs().at(0);
}

void IRCudaScheduleBlockReduce(ir::IRSchedule &ir_sch,
                               ir::Tensor reduce_tmp_out,
                               ir::Tensor tmp_out,
                               ir::Tensor out,
                               const common::Target &target) {
  ir_sch.MergeExprs();
  auto all_blocks = ir_sch.GetAllBlocks();
  auto loops      = ir_sch.GetLoops(all_blocks[0]);
  CHECK_EQ(all_blocks.size(), 3U);

  int output_shape_size_without_reduce = tmp_out->shape.size() - 1;
  // fuse last parallel dimension
  for (int idx = 0; idx < reduce_tmp_out->shape.size() - tmp_out->shape.size(); ++idx) {
    auto all_blocks = ir_sch.GetAllBlocks();
    ir_sch.Fuse(all_blocks[0], {output_shape_size_without_reduce, output_shape_size_without_reduce + 1});
  }

  // fuse parallel dimension
  for (int idx = 0; idx < output_shape_size_without_reduce - 1; ++idx) {
    auto all_blocks = ir_sch.GetAllBlocks();
    ir_sch.Fuse(all_blocks[0], {0, 1});
    ir_sch.Fuse(all_blocks[1], {0, 1});
    ir_sch.Fuse(all_blocks[2], {0, 1});
  }

  if (tmp_out->shape.size() == 1) {
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks[0]);
    ir_sch.Bind(loops[0], "threadIdx.x");
    ir_sch.SetBuffer(all_blocks[0], "local");
    loops = ir_sch.GetLoops(all_blocks[1]);
    ir_sch.Bind(loops[0], "threadIdx.x");
    ir_sch.SetBuffer(all_blocks[1], "local");
    loops = ir_sch.GetLoops(all_blocks[2]);
    ir_sch.Bind(loops[0], "threadIdx.x");

  } else {
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks[0]);
    ir_sch.Bind(loops[1], "threadIdx.x");
    ir_sch.SetBuffer(all_blocks[0], "local");
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks[1]);
    ir_sch.Bind(loops[1], "threadIdx.x");
    ir_sch.SetBuffer(all_blocks[1], "local");
    loops = ir_sch.GetLoops(all_blocks[1]);
    ir_sch.SimpleComputeAt(all_blocks[0], loops[0]);
    all_blocks = ir_sch.GetAllBlocks();
    loops      = ir_sch.GetLoops(all_blocks[2]);
    ir_sch.Bind(loops[0], "blockIdx.x");
    ir_sch.Bind(loops[1], "threadIdx.x");
    loops = ir_sch.GetLoops(all_blocks[2]);
    ir_sch.SimpleComputeAt(all_blocks[1], loops[0]);
  }
  VLOG(3) << "IRCudaScheduleBlockReduce result expr is: " << ir_sch.GetModule().GetExprs().at(0);
}

void IRSoftmaxScheduleCPU(ir::IRSchedule &ir_sch, int axis) {
  ir_sch.MergeExprs();
  auto all_blocks = ir_sch.GetAllBlocks();
  CHECK_EQ(all_blocks.size(), 3U);
  auto output = GetTensor(all_blocks[2]);
  if (axis == -1) {
    axis += output->shape.size();
  }
  auto loops = ir_sch.GetLoops(all_blocks[2]);
  // ir_sch.Parallel(loops[0]);
  all_blocks = ir_sch.GetAllBlocks();
  for (int i = 1; i < axis; ++i) {
    ir_sch.Fuse(all_blocks[2], {0, 1});
  }
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[2]);
  ir_sch.ComputeAt(all_blocks[1], loops[0]);
}

void IRPoolScheduleGPU(ir::IRSchedule &ir_sch, const common::Target &target) {
  auto all_blocks = ir_sch.GetAllBlocks();
  CHECK_EQ(all_blocks.size(), 1U);
  ir_sch.Fuse(all_blocks[0], {0, 1, 2, 3});
  auto loops   = ir_sch.GetLoops(all_blocks[0]);
  auto splited = ir_sch.Split(loops[0], {-1, 1024});
  ir_sch.Bind(splited[0], "blockIdx.x");
  ir_sch.Bind(splited[1], "threadIdx.x");
}

void IRGlobalPoolScheduleGPU(ir::IRSchedule &ir_sch, const common::Target &target) {
  auto all_blocks = ir_sch.GetAllBlocks();
  CHECK_EQ(all_blocks.size(), 2U);
  auto fused   = ir_sch.Fuse(all_blocks[0], {0, 1});
  auto splited = ir_sch.Split(fused, {-1, 32});
  all_blocks   = ir_sch.GetAllBlocks();
  fused        = ir_sch.Fuse(all_blocks[1], {0, 1});
  splited      = ir_sch.Split(fused, {-1, 32});
  ir_sch.Bind(splited[0], "blockIdx.x");
  ir_sch.Bind(splited[1], "threadIdx.y");
  all_blocks = ir_sch.GetAllBlocks();
  ir_sch.SimpleComputeAt(all_blocks[0], splited[1]);
  all_blocks = ir_sch.GetAllBlocks();
  ir_sch.SetBuffer(all_blocks[0], "local");
  auto loops = ir_sch.GetLoops(all_blocks[0]);
  ir_sch.Bind(loops[2], "threadIdx.x");
}

void IRCudaScheduleConv(ir::IRSchedule &ir_sch, const common::Target &target) {
  LOG(INFO) << "After Merge, expr is : " << ir_sch.GetModule().GetExprs().at(0);
  auto &res = ScheduleParam::get_cuda_instance().GetParam();
  if (res.empty()) {
    /*     CreateCudaSerialData();
        LoadSerialData(&res); */
  }

  auto all_blocks = ir_sch.GetAllBlocks();
  CHECK_EQ(all_blocks.size(), 3U);
  auto input_pad = GetTensor(all_blocks[0]);
  auto output    = GetTensor(all_blocks[2]);
  all_blocks     = ir_sch.GetAllBlocks();
  CHECK_EQ(all_blocks.size(), 3U);
  auto weights = GetReadTensor(all_blocks[2], 2);

  int n = output->shape[0].as_int32();
  int c = output->shape[1].as_int32();
  optim::Simplify(&(output->shape[2]));
  int h = output->shape[2].as_int32();
  optim::Simplify(&(output->shape[3]));
  int w  = output->shape[3].as_int32();
  int rc = input_pad->shape[1].as_int32();

  std::string key =
      "CudaDirectConvSchedule " + std::to_string(input_pad->shape[0].as_int32()) + " " +
      std::to_string(input_pad->shape[1].as_int32()) + " " + std::to_string(input_pad->shape[2].as_int32()) + " " +
      std::to_string(input_pad->shape[3].as_int32()) + " " + std::to_string(weights->shape[0].as_int32()) + " " +
      std::to_string(weights->shape[1].as_int32()) + " " + std::to_string(weights->shape[2].as_int32()) + " " +
      std::to_string(weights->shape[3].as_int32()) + " " + std::to_string(output->shape[0].as_int32()) + " " +
      std::to_string(output->shape[1].as_int32()) + " " + std::to_string(output->shape[2].as_int32()) + " " +
      std::to_string(output->shape[3].as_int32());
  if (res.count(key) == 0) {
    VLOG(3) << "Didn't find saved param, key is: " << key;
  } else {
    VLOG(3) << "Find saved param! key is: " << key;
    IRCudaScheduleConv2(ir_sch, input_pad, weights, output, target, key);
    return;
  }
  ir_sch.ComputeInline(all_blocks[0]);
  int f_inner  = GetInnerSplitter(c, h);
  int block_z  = SplitEven(c / f_inner);
  int thread_z = c / f_inner / block_z;

  int rc_factor = SplitEven(rc);
  while (w * thread_z > 1024 && thread_z % 2 == 0) {
    thread_z = thread_z / 2;
    f_inner  = f_inner * 2;
  }
  CHECK_LE(w * thread_z, 1024) << "Wrong Param of Conv2d!";
  all_blocks = ir_sch.GetAllBlocks();
  auto OL    = ir_sch.CacheWrite(all_blocks[1], 0, "local");
  all_blocks = ir_sch.GetAllBlocks();
  auto loops = ir_sch.GetLoops(all_blocks[2]);
  ir_sch.Split(loops[1], {-1, thread_z, f_inner});
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[2]);
  ir_sch.Reorder({loops[1], loops[4], loops[2], loops[5], loops[3]});
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[2]);
  ir_sch.Bind(loops[1], "blockIdx.z");
  ir_sch.Bind(loops[2], "blockIdx.y");
  ir_sch.Bind(loops[3], "threadIdx.z");
  ir_sch.Bind(loops[4], "threadIdx.x");
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[2]);
  ir_sch.ComputeAt(all_blocks[1], loops[4]);
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[1]);
  ir_sch.Split(loops[6], {-1, rc_factor});
  LOG(INFO) << "In the end, expr is : " << ir_sch.GetModule().GetExprs().at(0);
}

void IRCudaScheduleConv2(ir::IRSchedule &ir_sch,
                         ir::Tensor &input_pad,
                         ir::Tensor &weights,
                         ir::Tensor &output,
                         const common::Target &target,
                         const std::string &key) {
  auto &res = ScheduleParam::get_cuda_instance().GetParam();

  auto all_blocks = ir_sch.GetAllBlocks();

  // stages[input_pad]->ComputeInline();

  optim::Simplify(&(output->shape[2]));
  optim::Simplify(&(output->shape[3]));

  LOG(INFO) << "All cache 1, expr is : " << ir_sch.GetModule().GetExprs().at(0);
  auto input_cache = ir_sch.CacheRead(all_blocks[2], 1, "shared");
  all_blocks       = ir_sch.GetAllBlocks();
  LOG(INFO) << "All cache 2, expr is : " << ir_sch.GetModule().GetExprs().at(0);
  auto weights_cache = ir_sch.CacheRead(all_blocks[3], 2, "shared");
  all_blocks         = ir_sch.GetAllBlocks();
  LOG(INFO) << "All cache 3, expr is : " << ir_sch.GetModule().GetExprs().at(0);
  auto output_cache = ir_sch.CacheWrite(all_blocks[4], 0, "local");
  LOG(INFO) << "After all cache, expr is : " << ir_sch.GetModule().GetExprs().at(0);
  all_blocks = ir_sch.GetAllBlocks();
  ir_sch.ComputeInline(all_blocks[1]);

  auto &x_param  = res[key]["x"];
  auto &y_param  = res[key]["y"];
  auto &f_param  = res[key]["f"];
  auto &rx_param = res[key]["rx"];
  auto &ry_param = res[key]["ry"];
  auto &rc_param = res[key]["rc"];

  all_blocks = ir_sch.GetAllBlocks();
  auto loops = ir_sch.GetLoops(all_blocks[4]);
  ir_sch.Split(loops[3], {-1, x_param[1], x_param[2], x_param[3]});

  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[4]);
  ir_sch.Split(loops[2], {-1, y_param[1], y_param[2], y_param[3]});

  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[4]);
  ir_sch.Split(loops[1], {-1, f_param[1], f_param[2], f_param[3]});

  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[4]);
  ir_sch.Reorder({loops[0],
                  loops[1],
                  loops[5],
                  loops[9],
                  loops[2],
                  loops[6],
                  loops[10],
                  loops[3],
                  loops[7],
                  loops[11],
                  loops[4],
                  loops[8],
                  loops[12]});

  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[4]);
  ir_sch.Bind(loops[1], "blockIdx.z");
  ir_sch.Bind(loops[2], "blockIdx.y");
  ir_sch.Bind(loops[3], "blockIdx.x");
  ir_sch.Bind(loops[7], "threadIdx.z");
  ir_sch.Bind(loops[8], "threadIdx.y");
  ir_sch.Bind(loops[9], "threadIdx.x");
  ir_sch.Unroll(loops[10]);
  ir_sch.Unroll(loops[11]);
  ir_sch.Unroll(loops[12]);

  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[4]);
  ir_sch.ComputeAt(all_blocks[3], loops[9]);

  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[3]);
  ir_sch.Split(loops[15], {-1, rx_param[1]});
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[3]);
  ir_sch.Split(loops[14], {-1, ry_param[1]});
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[3]);
  ir_sch.Split(loops[13], {-1, rc_param[1]});
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[3]);
  ir_sch.Reorder({loops[13], loops[15], loops[17], loops[14], loops[16], loops[18], loops[10], loops[11], loops[12]});

  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[3]);
  ir_sch.ComputeAt(all_blocks[0], loops[12]);
  all_blocks = ir_sch.GetAllBlocks();
  loops      = ir_sch.GetLoops(all_blocks[3]);
  ir_sch.ComputeAt(all_blocks[1], loops[12]);
  // Work In Progress
}

}  // namespace pe
}  // namespace hlir
}  // namespace cinn