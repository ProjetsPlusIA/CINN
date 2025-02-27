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

#pragma once

#include <absl/container/flat_hash_map.h>

#include <string>
#include <vector>

#include "cinn/hlir/framework/node.h"
#include "cinn/hlir/pe/schedule_param.pb.h"
#include "cinn/ir/ir.h"
#include "cinn/ir/ir_schedule.h"
#include "cinn/lang/compute.h"
#include "cinn/poly/stage.h"

namespace cinn {
namespace hlir {
namespace pe {
void IRCudaScheduleBlockShuffleReduce(
    ir::IRSchedule &ir_sch, ir::Tensor reshape, ir::Tensor internal, ir::Tensor out, const common::Target &target);

void IRScheduleInjectiveCPU(ir::IRSchedule &ir_sch,
                            const std::vector<int> &output_shape,
                            const common::Target &target,
                            bool vectorizable = true);

void IRCudaScheduleInjective(ir::IRSchedule &ir_sch,
                             const std::vector<int> &output_shape,
                             const common::Target &target);

void IRCudaScheduleMul(ir::IRSchedule &ir_sch, const std::vector<int> &output_shape, const common::Target &target);

void IRMulScheduleCPU(ir::IRSchedule &ir_sch, const std::vector<int> &reduce_first_shape, const common::Target &target);

void IRCudaSplitSchedule(ir::IRSchedule &ir_sch,
                         const std::vector<std::vector<int>> &output_shapes,
                         int axis,
                         const common::Target &target);

void IRCudaScheduleReduce(ir::IRSchedule &ir_sch,
                          const std::vector<int> &output_shape,
                          int last_dimension_num,
                          const common::Target &target);

void IRCudaScheduleBlockReduce(ir::IRSchedule &ir_sch,
                               ir::Tensor reduce_tmp_out,
                               ir::Tensor tmp_out,
                               ir::Tensor out,
                               const common::Target &target);

void IRCudaScheduleBlockReduceInternal(ir::IRSchedule &ir_sch,
                                       ir::Tensor tmp_out,
                                       ir::Tensor out,
                                       const common::Target &target);

void IRSoftmaxScheduleCPU(ir::IRSchedule &ir_sch, int axis = -1);

void IRPoolScheduleGPU(ir::IRSchedule &ir_sch, const common::Target &target);

void IRGlobalPoolScheduleGPU(ir::IRSchedule &ir_sch, const common::Target &target);

void IRCudaScheduleConv2(ir::IRSchedule &ir_sch,
                         ir::Tensor &input_pad,
                         ir::Tensor &weights,
                         ir::Tensor &output,
                         const common::Target &target,
                         const std::string &key);

void IRCudaScheduleConv(ir::IRSchedule &ir_sch, const common::Target &target);

}  // namespace pe
}  // namespace hlir
}  // namespace cinn
