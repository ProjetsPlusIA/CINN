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

#include "cinn/backends/codegen_cuda_dev.h"

#include <cinn/utils/string.h>

#include <fstream>
#include <set>
#include <unordered_set>

#include "cinn/ir/ir_operators.h"
#include "cinn/ir/ir_verify.h"
#include "cinn/optim/ir_simplify.h"
#include "cinn/optim/remove_nested_block.h"

namespace cinn {
namespace backends {

CodeGenCUDA_Dev::CodeGenCUDA_Dev(Target target) : CodeGenC(target) {}

std::string CodeGenCUDA_Dev::Compile(const ir::Module &module, bool for_nvrtc) {
  for_nvrtc_  = for_nvrtc;
  auto source = Compile(module, OutputKind::CImpl);

  return source;
}

void CodeGenCUDA_Dev::Compile(const ir::Module &module, const Outputs &outputs) {
  ir::IrVerify(Expr(module));

  CodeGenC::inline_builtin_codes_ = false;
  if (!outputs.c_header_name.empty()) {
    auto source = Compile(module, OutputKind::CHeader);
    std::ofstream file(outputs.c_header_name);
    CHECK(file.is_open()) << "failed to open file " << outputs.c_header_name;
    file << source;
    file.close();
    LOG(WARNING) << "Output C header to file " << outputs.c_header_name;
  }

  if (!outputs.cuda_source_name.empty()) {
    auto source = Compile(module, OutputKind::CImpl);
    std::ofstream file(outputs.cuda_source_name);
    CHECK(file.is_open()) << "failed to open file " << outputs.cuda_source_name;
    file << source;
    file.close();
    LOG(WARNING) << "Output C source to file " << outputs.cuda_source_name;
  }
}

std::string CodeGenCUDA_Dev::Compile(const ir::LoweredFunc &func) {
  Print(Expr(func));
  return ss_.str();
}

std::vector<Expr> CodeGenCUDA_Dev::GenerateBufferAliasExprs(const ir::_LoweredFunc_ *op,
                                                            const std::vector<ir::Buffer> &temp_buffers) {
  std::set<ir::Buffer> temp_buffer_set(temp_buffers.begin(), temp_buffers.end());
  // prepare temp buffer alias
  std::vector<Expr> buffer_alias;
  auto tensors = ir::CollectIRNodes(op->body, [&](const Expr *x) {
    return x->as_tensor() && x->as_tensor()->buffer.defined() && temp_buffer_set.count(x->as_tensor()->buffer);
  });

  // unique tensors
  std::set<ir::Tensor> unique_tensors;
  for (auto &e : tensors) {
    unique_tensors.insert(e.as_tensor_ref());
  }

  for (auto &t : unique_tensors) {
    auto data_type     = t->type();
    auto data_ptr_type = data_type;
    data_ptr_type.set_cpp_handle();

    Var t_var(t->name, data_ptr_type);
    Var buf_var(t->buffer->name, data_ptr_type);
    buffer_alias.push_back(ir::Let::Make(t_var, buf_var));
  }

  return buffer_alias;
}

void CodeGenCUDA_Dev::Visit(const ir::_LoweredFunc_ *op) {
  // clear names valid within scope when enter a new function
  vectorized_tensor_names_.clear();
  os() << "__global__\n";

  PrintFunctionDeclaration(op);
  os() << "\n";

  DoIndent();

  std::vector<Expr> new_body;

  auto alloca_temp_buffers = op->PrepareAllocTempBufferExprs();
  auto temp_buffer_alias   = GenerateBufferAliasExprs(op, op->temp_bufs);
  auto alis_var_exprs      = op->CudaAliasVarExprs();

#define APPEND_TO_NEW_BODY(field__) new_body.insert(std::end(new_body), std::begin(field__), std::end(field__));
  APPEND_TO_NEW_BODY(alloca_temp_buffers)
  APPEND_TO_NEW_BODY(temp_buffer_alias)
  APPEND_TO_NEW_BODY(alis_var_exprs)

  new_body.push_back(op->body);

  Expr func_body = ir::Block::Make(new_body);

  optim::RemoveNestedBlock(&func_body);
  // Make sure that the function's body is wrapped by a block
  if (!func_body.As<ir::Block>()) {
    func_body = ir::Block::Make({func_body});
  }

  Print(func_body);
}

void CodeGenCUDA_Dev::Visit(const ir::_Var_ *op) {
  if (utils::Startswith(op->name, "threadIdx") || utils::Startswith(op->name, "blockIdx")) {
    os() << "(int)" + op->name;
  } else {
    os() << op->name;
  }
}

void CodeGenCUDA_Dev::Visit(const ir::Alloc *op) {
  CHECK(op->destination.as_buffer());
  PrintTempBufferCreation(op->destination.as_buffer_ref());
}

void CodeGenCUDA_Dev::Visit(const ir::Min *op) {
  os() << "cinn_nvgpu_min_fp32(";
  Print(op->a());
  os() << ", ";
  Print(op->b());
  os() << ")";
}

void CodeGenCUDA_Dev::Visit(const ir::Max *op) {
  os() << "cinn_nvgpu_max_fp32(";
  Print(op->a());
  os() << ", ";
  Print(op->b());
  os() << ")";
}

void CodeGenCUDA_Dev::PrintFunctionDeclaration(const ir::_LoweredFunc_ *op) {
  os() << "void ";
  if (op->cuda_axis_info.valid()) {
    int thread_num = 1;
    for (int i = 0; i < 3; i++) {
      thread_num *= op->cuda_axis_info.block_dim(i);
    }
    os() << "__launch_bounds__(" << thread_num << ") ";
  }

  os() << op->name << "(";
  for (int i = 0; i < op->args.size() - 1; i++) {
    auto &arg = op->args[i];
    PrintFuncArg(arg);
    os() << ", ";
  }
  if (!op->args.empty()) {
    PrintFuncArg(op->args.back());
  }
  os() << ")";
}

void CodeGenCUDA_Dev::PrintFuncArg(const ir::Argument &arg) {
  if (arg.is_buffer()) {
    // In CUDA kernel, only primitive type is supported, so we replace the buffer with T*j
    if (arg.is_input()) os() << "const ";
    os() << GetTypeRepr(arg.buffer_arg()->dtype);
    os() << "* ";
    os() << kCKeywordRestrict << " ";
    os() << ir::BufferGetTensorName(arg.buffer_arg().As<ir::_Buffer_>());
  } else if (arg.is_var()) {
    if (arg.var_arg()->type().is_cpp_handle()) {
      os() << kCKeywordRestrict;
    }
    os() << GetTypeRepr(arg.type()) << " ";
    os() << arg.name();
  } else {
    CINN_NOT_IMPLEMENTED
  }
}

void CodeGenCUDA_Dev::PrintBuiltinCodes() {
  os() << R"ROC(
)ROC";
}

std::string CodeGenCUDA_Dev::Compile(const ir::Module &module, CodeGenC::OutputKind output_kind) {
  ss_.str("");

  if (for_nvrtc_) {
    os() << "extern \"C\" {\n\n";
  }

  if (output_kind == OutputKind::CHeader) {
    GenerateHeaderFile(module);
  } else if (output_kind == OutputKind::CImpl) {
    PrintIncludes();

    PrintBuiltinCodes();

    for (auto &func : module.functions()) {
      Compile(func);
    }
  } else {
    LOG(FATAL) << "Not supported OutputKind";
  }

  if (for_nvrtc_) {
    os() << "\n\n}";
  }

  return ss_.str();
}

void CodeGenCUDA_Dev::PrintIncludes() {
  os() << "#include \"cinn_cuda_runtime_source.cuh\"\n\n";
  os() << "#ifdef __CUDACC_RTC__\n";
  os() << "typedef int int32_t;\n";
  os() << "typedef char int8_t;\n";
  os() << "#endif\n";
  os() << "\n";
  os() << "\n";
}

void CodeGenCUDA_Dev::PrintTempBufferCreation(const ir::Buffer &buffer) {
  CHECK_NE(buffer->type(), Void());
  auto print_gpu_memory = [&](const std::string &mark) {
    os() << mark << GetTypeRepr(buffer->dtype) << " " << buffer->name << " ";

    os() << "[ ";
    Expr buffer_size(1);
    for (int i = 0; i < buffer->shape.size(); i++) {
      buffer_size = buffer_size * buffer->shape[i];
    }
    optim::Simplify(&buffer_size);
    Print(buffer_size);
    os() << " ]";
  };
  switch (buffer->memory_type) {
    case ir::MemoryType::GPUShared:
      print_gpu_memory("__shared__ ");
      break;

    case ir::MemoryType::GPULocal:
      print_gpu_memory("");
      break;

    default:
      LOG(FATAL) << "CUDA device codegen not support memory " << buffer->name << ", type " << buffer->memory_type;
  }
}

void CodeGenCUDA_Dev::Visit(const ir::Call *op) {
  os() << op->name + "(";

  if (!op->read_args.empty()) {
    for (int i = 0; i < op->read_args.size() - 1; i++) {
      auto &arg = op->read_args[i];
      if (arg.as_tensor()) {
        os() << arg.as_tensor()->name;
        os() << ", ";
      } else {
        Print(arg);
        os() << ", ";
      }
    }
    if (op->read_args.back().as_tensor()) {
      os() << op->read_args.back().as_tensor()->name;
    } else {
      Print(op->read_args.back());
    }
  }

  if (!op->write_args.empty()) {
    os() << ", ";
    for (int i = 0; i < op->write_args.size() - 1; i++) {
      auto &arg = op->write_args[i];
      if (arg.as_tensor()) {
        os() << arg.as_tensor()->name;
        os() << ", ";
      } else {
        Print(arg);
        os() << ", ";
      }
    }
    if (op->write_args.back().as_tensor()) {
      os() << op->write_args.back().as_tensor()->name;
    } else {
      Print(op->write_args.back());
    }
  }

  os() << ")";
}

void CodeGenCUDA_Dev::Visit(const ir::Let *op) {
  CHECK(op->type().valid());

  // identify vectorized tensors by checking their dtypes are customized_type
  // with customized_type::kcuda_builtin_vector_t prefix, and save their names
  if (op->type().is_customized() &&
      utils::Startswith(op->type().customized_type(), common::customized_type::kcuda_builtin_vector_t)) {
    os() << GetTypeRepr(op->type());
    os() << " ";
    Print(op->symbol);
    vectorized_tensor_names_.insert(utils::GetStreamCnt(op->symbol));
    os() << " = ";
    Print(op->body);
  } else {
    CodeGenC::Visit(op);
  }
}

bool CodeGenCUDA_Dev::PrintBuiltinVectorAccess(const ir::LoadStoreAddrMnger *op, ir::Expr index_expr) {
  static constexpr char index2suffix[4] = {'x', 'y', 'z', 'w'};

  // addr of op should be a place of tensor and the index is simple int number
  if (!op->is_addr_tensor() || !index_expr.As<ir::IntImm>()) {
    return false;
  }
  auto *tensor = op->tensor.As<ir::_Tensor_>();
  CHECK(tensor);

  // identify vectorized tensors by their names
  if (!vectorized_tensor_names_.count(tensor->name)) {
    return false;
  }

  // the index can't exceed the range of cuda built-in vector type
  int index = index_expr.As<ir::IntImm>()->value;
  if (index < 0 || index >= 4) {
    return false;
  }

  os() << tensor->name << (tensor->type().is_cpp_handle() ? "->" : ".") << index2suffix[index];
  return true;
}

void CodeGenCUDA_Dev::Visit(const ir::Load *op) {
  // overload this visit function to especially deal with the case when it accesses
  // element at a cuda built-in vector, others still resolve to CodeGenC
  if (!PrintBuiltinVectorAccess(op, op->index())) {
    CodeGenC::Visit(op);
  }
}

void CodeGenCUDA_Dev::Visit(const ir::Store *op) {
  // overload this visit function to especially deal with the case when it accesses
  // element at a cuda built-in vector, others still resolve to CodeGenC
  if (PrintBuiltinVectorAccess(op, op->index())) {
    os() << " = ";
    Print(op->value);
  } else {
    CodeGenC::Visit(op);
  }
}

}  // namespace backends
}  // namespace cinn
