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

#include <memory>
#include <string>
#include <vector>

#include "cinn/auto_schedule/measure/schedule_measurer.h"
#include "cinn/auto_schedule/task/task_optimizer.h"
#include "cinn/auto_schedule/task/tune_task.h"
#include "cinn/auto_schedule/task_scheduler/task_scheduler.h"
#include "cinn/auto_schedule/tuning.h"
#include "cinn/common/target.h"
#include "cinn/hlir/framework/graph.h"
#include "cinn/hlir/framework/graph_compiler.h"

namespace cinn {
namespace auto_schedule {

// This class is entrance of auto-tune, users can use it
// to tune graph (not supported yet) and search a series of schedules
// that maybe more likely to obtain better performance.
// Internally, it creates necessary components and use them to finish tuning.
class AutoTuner {
 public:
  // configure how to perform auto-tune, such as
  // the way to create tasks, the strategy of scheduling tasks and so on.
  struct Config {
    std::string task_schedule_strategy = "round_robin";
    TaskScheduler::Config task_schedule_config;
    int runner_repeat_times = 1;
  };

  AutoTuner(const common::Target& target, hlir::framework::Graph* graph);

  // Initialize tuner with specific config and auxiliary objects.
  void Initialize(const Config& config, hlir::framework::GraphCompiler* graph_compiler);

  // Perform the tuning process and return the final result
  TuningResult Tune(const TuningOptions& options);

 private:
  const common::Target& target_;
  hlir::framework::Graph* graph_;

  // Tasks to tune
  std::vector<TuneTask> tasks_;
  // Scheduler that select a task to tune at every turn.
  std::unique_ptr<TaskScheduler> task_scheduler_;
  // The actor to perform auto-tune, each optimizer take a task.
  std::vector<std::unique_ptr<TaskOptimizer>> task_optimizers_;

  // Classes used to measure AutoTune samples
  std::unique_ptr<ScheduleBuilder> builder_;
  std::unique_ptr<ScheduleRunner> runner_;
  std::unique_ptr<ScheduleMeasurer> schedule_measurer_;
};

}  // namespace auto_schedule
}  // namespace cinn
