// Copyright (c) 2021 CINN Authors. All Rights Reserved.
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

#include <utility>
#include <vector>

#include "cinn/ir/ir.h"
#include "cinn/ir/ir_schedule.h"
#include "cinn/ir/lowered_func.h"

namespace cinn {
namespace optim {

//! Shallow copy an expression.
Expr IRCopy(Expr x);

std::vector<Expr> IRCopy(const std::vector<Expr>& x);

ir::ModuleExpr IRCopy(const ir::ModuleExpr& x);

ir::LoweredFunc IRCopy(const ir::LoweredFunc& x);

std::vector<ir::LoweredFunc> IRCopy(const std::vector<ir::LoweredFunc>& x);

}  // namespace optim
}  // namespace cinn
