#include <gtest/gtest.h>

#include "cinn/backends/compiler.h"
#include "cinn/backends/extern_func_jit_register.h"
#include "cinn/backends/llvm/execution_engine.h"
#include "cinn/backends/llvm/simple_jit.h"
#include "cinn/cinn.h"
#include "cinn/common/ir_util.h"
#include "cinn/common/target.h"
#include "cinn/common/test_helper.h"
#include "cinn/runtime/cpu/host_intrinsics.h"
#include "cinn/runtime/cpu/use_extern_funcs.h"

namespace cinn {
namespace runtime {
namespace cpu {

cinn_buffer_t *CreateBuffer(const std::vector<int> shape, bool random = true, int set_value = 0) {
  if (random) {
    return common::BufferBuilder(Float(32), shape).set_random().Build();
  } else if (set_value != 0) {
    return common::BufferBuilder(Float(32), shape).set_val(set_value).Build();
  }
  return common::BufferBuilder(Float(32), shape).set_zero().Build();
}

void TestCallElementwise(const std::string &fn_name,
                         float (*fn_runtime)(float),
                         bool is_elementwise,
                         int set_value = 0) {
  Expr M(10);
  Expr N(10);
  Placeholder<float> x("x", {M, N});

  ir::Tensor out;

  std::vector<ir::Tensor> lower_args({x});
  if (is_elementwise) {
    out = Compute(
        {M, N}, [&](Var i, Var j) -> Expr { return lang::CallExtern(fn_name, {x(i, j)}); }, fn_name + "_out");
    lower_args.push_back(out);
  } else {
    auto comp_out = Compute(
        {Expr(1)}, [&]() -> Expr { return lang::CallExtern(fn_name, {x}); }, fn_name + "_out");
    out = comp_out->TupleGet(0);
    out->WithBuffer(Float(32));
    lower_args.push_back(out);
    lower_args.push_back(comp_out);
  }

  auto stages = CreateStages(lower_args);

  auto target = common::DefaultHostTarget();
  target.arch = Target::Arch::X86;
  ir::Module::Builder builder("module0", target);
  auto func = Lower("fn", stages, lower_args);
  builder.AddFunction(func);

  LOG(INFO) << "func:\n" << func;

  auto jit    = backends::ExecutionEngine::Create({});
  auto module = builder.Build();

  jit->Link(module);
  auto fn = jit->Lookup("fn");
  CHECK(fn);
  auto fn_ = reinterpret_cast<void (*)(void *, int32_t)>(fn);

  cinn_buffer_t *A_buf;
  if (set_value != 0) {
    A_buf = CreateBuffer({10, 10}, false, set_value);
  } else {
    A_buf = CreateBuffer({10, 10});
  }
  auto *B_buf = CreateBuffer({10, 10}, false);

  cinn_pod_value_t a_arg(A_buf), b_arg(B_buf);
  cinn_pod_value_t args[] = {a_arg, b_arg};
  fn_(args, 2);

  auto *ad = reinterpret_cast<float *>(A_buf->memory);
  auto *bd = reinterpret_cast<float *>(B_buf->memory);
  for (int i = 0; i < A_buf->num_elements(); i++) {
    ASSERT_NEAR(bd[i], fn_runtime(ad[i]), 1e-5);
  }
}

#define TEST_MKL_MATH_FP32(test_name__, is_elementwise)                                                   \
  TEST(mkl_math, test_name__) {                                                                           \
    TestCallElementwise("cinn_cpu_" #test_name__ "_fp32", cinn_cpu_##test_name__##_fp32, is_elementwise); \
  }
#define TEST_MKL_MATH_FP32_SET(test_name__, is_elementwise, value)                                               \
  TEST(mkl_math, test_name__) {                                                                                  \
    TestCallElementwise("cinn_cpu_" #test_name__ "_fp32", cinn_cpu_##test_name__##_fp32, is_elementwise, value); \
  }

TEST_MKL_MATH_FP32(exp, true)
TEST_MKL_MATH_FP32(erf, true)
TEST_MKL_MATH_FP32(sqrt, true)
TEST_MKL_MATH_FP32(log, true)
TEST_MKL_MATH_FP32(log2, true)
TEST_MKL_MATH_FP32(log10, true)
TEST_MKL_MATH_FP32(floor, true)
TEST_MKL_MATH_FP32(ceil, true)
TEST_MKL_MATH_FP32(round, true)
TEST_MKL_MATH_FP32(trunc, true)
TEST_MKL_MATH_FP32(cos, true)
TEST_MKL_MATH_FP32(cosh, true)
TEST_MKL_MATH_FP32(tan, true)
TEST_MKL_MATH_FP32(sin, true)
TEST_MKL_MATH_FP32(sinh, true)
TEST_MKL_MATH_FP32(acos, true)
TEST_MKL_MATH_FP32_SET(acosh, true, 1.5)
TEST_MKL_MATH_FP32(asin, true)
TEST_MKL_MATH_FP32(asinh, true)
TEST_MKL_MATH_FP32(atan, true)
TEST_MKL_MATH_FP32(atanh, true)
TEST_MKL_MATH_FP32(isnan, true)
TEST_MKL_MATH_FP32(tanh, true)
TEST_MKL_MATH_FP32(isfinite, true)
TEST_MKL_MATH_FP32(isinf, true)

TEST(mkl_math, tanh_v_fp32) { TestCallElementwise("cinn_mkl_tanh_v_fp32", cinn_cpu_tanh_fp32, false); }

TEST(cinn_cpu_mkl_gemm_fp32, test) {
  Expr M(30);
  Expr N(20);
  Expr K(40);

  Placeholder<float> A("A", {M, K});
  Placeholder<float> B("B", {K, N});

  auto call = Compute(
      {Expr(1)},
      [=]() -> Expr {
        return lang::CallExtern("cinn_cpu_mkl_gemm_fp32",
                                {
                                    common::make_one<float>(),   // alpha
                                    M,                           // M
                                    N,                           // N
                                    K,                           // K
                                    common::make_bool(false),    // ta
                                    common::make_bool(false),    // tb
                                    M,                           // lda
                                    K,                           // ldb
                                    M,                           // ldc
                                    common::make_zero<float>(),  // beta
                                    A.tensor(),                  // A
                                    B.tensor(),                  // B
                                });
      },
      "extern_call");

  auto out = call->TupleGet(0);
  out->WithBuffer(Float(32));

  auto stages = CreateStages({call, out});

  auto target = common::DefaultHostTarget();
  target.arch = Target::Arch::X86;
  ir::Module::Builder builder("module0", target);

  auto func = Lower("fn", stages, {A, B, out, call});
  builder.AddFunction(func);

  LOG(INFO) << "func:\n" << func;

  auto jit    = backends::SimpleJIT::Create();
  auto module = builder.Build();

  jit->Link(module, /*optimize=*/true);
  auto fn     = jit->Lookup("fn");
  auto fn_ptr = reinterpret_cast<void (*)(void *, int32_t)>(fn);

  // test with real data
  auto *A_buf = common::BufferBuilder(Float(32), {M.as_int32(), K.as_int32()}).set_random().Build();
  auto *B_buf = common::BufferBuilder(Float(32), {K.as_int32(), N.as_int32()}).set_random().Build();
  auto *C_buf = common::BufferBuilder(Float(32), {M.as_int32(), N.as_int32()}).set_zero().Build();

  auto args = common::ArgsBuilder().Add(A_buf).Add(B_buf).Add(C_buf).Build();

  fn_ptr(args.data(), args.size());

  cinn_buffer_free(nullptr, A_buf);
  cinn_buffer_free(nullptr, B_buf);
  cinn_buffer_free(nullptr, C_buf);
}

}  // namespace cpu
}  // namespace runtime
}  // namespace cinn

CINN_USE_REGISTER(mkl_math)
CINN_USE_REGISTER(cinn_cpu_mkl_gemm_fp32)
