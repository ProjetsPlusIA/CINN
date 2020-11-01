#include "cinn/backends/compiler.h"

#include <gtest/gtest.h>

#include <vector>

#include "cinn/cinn.h"
#include "cinn/common/test_helper.h"
#include "cinn/hlir/pe/elementwise.h"
#include "cinn/hlir/pe/nn.h"
#include "cinn/runtime/use_extern_funcs.h"

namespace cinn {
namespace backends {

TEST(Compiler, x86) {
  Expr M(10), N(20);

  auto create_module = [&]() {
    Placeholder<float> A("A", {M, N});
    Placeholder<float> B("B", {M, N});

    auto C = Compute(
        {M, N}, [=](Expr i, Expr j) { return A(i, j) + B(i, j); }, "C");
    return std::make_tuple(A, B, C);
  };

  {                                    // test x86
    auto [A, B, C] = create_module();  // NOLINT

    auto stages = CreateStages({C});

    auto fn = Lower("fn", stages, {A, B, C});

    ir::Module::Builder builder("some_module", common::DefaultHostTarget());
    builder.AddFunction(fn);

    auto compiler = Compiler::Create(common::DefaultHostTarget());
    compiler->Build(builder.Build());

    auto* fnp = compiler->Lookup("fn");
    ASSERT_TRUE(fnp);

    auto* Ab = common::BufferBuilder(Float(32), {M.as_int32(), N.as_int32()}).set_random().Build();
    auto* Bb = common::BufferBuilder(Float(32), {M.as_int32(), N.as_int32()}).set_random().Build();
    auto* Cb = common::BufferBuilder(Float(32), {M.as_int32(), N.as_int32()}).set_zero().Build();

    auto args = common::ArgsBuilder().Add(Ab).Add(Bb).Add(Cb).Build();
    fnp(args.data(), args.size());

    // test result
    auto* Ad = reinterpret_cast<float*>(Ab->memory);
    auto* Bd = reinterpret_cast<float*>(Bb->memory);
    auto* Cd = reinterpret_cast<float*>(Cb->memory);
    for (int i = 0; i < Ab->num_elements(); i++) {
      ASSERT_NEAR(Ad[i] + Bd[i], Cd[i], 1e-5);
    }
  }

#ifdef CINN_WITH_CUDA
  {                                    // cuda
    auto [A, B, C] = create_module();  // NOLINT
    auto stages    = CreateStages({C});

    stages[C]->Bind(0, "blockIdx.x");
    stages[C]->Bind(1, "threadIdx.x");

    auto fn = Lower("fn", stages, {A, B, C});

    ir::Module::Builder builder("some_module", common::DefaultHostTarget());
    builder.AddFunction(fn);

    auto compiler = Compiler::Create(common::DefaultNVGPUTarget());
    compiler->Build(builder.Build());

    auto* fnp = compiler->Lookup("fn");
    ASSERT_TRUE(fnp);

    auto* Ab = common::BufferBuilder(Float(32), {M.as_int32(), N.as_int32()}).set_random().Build();
    auto* Bb = common::BufferBuilder(Float(32), {M.as_int32(), N.as_int32()}).set_random().Build();
    auto* Cb = common::BufferBuilder(Float(32), {M.as_int32(), N.as_int32()}).set_zero().Build();

    // allocate CUDA buffer
    void *Ag, *Bg, *Cg;
    const int num_bytes = Ab->num_elements() * sizeof(float);
    cudaMalloc(&Ag, num_bytes);
    cudaMalloc(&Bg, num_bytes);
    cudaMalloc(&Cg, num_bytes);

    CUDA_CALL(cudaMemcpy(Ag, Ab->memory, num_bytes, cudaMemcpyHostToDevice));
    CUDA_CALL(cudaMemcpy(Bg, Bb->memory, num_bytes, cudaMemcpyHostToDevice));
    CUDA_CALL(cudaMemcpy(Cg, Cb->memory, num_bytes, cudaMemcpyHostToDevice));

    cinn_buffer_t Abb;
    Abb.memory = reinterpret_cast<uint8_t*>(Ag);
    cinn_buffer_t Bbb;
    Bbb.memory = reinterpret_cast<uint8_t*>(Bg);
    cinn_buffer_t Cbb;
    Cbb.memory = reinterpret_cast<uint8_t*>(Cg);

    auto args = common::ArgsBuilder().Add(&Abb).Add(&Bbb).Add(&Cbb).Build();
    fnp(args.data(), args.size());

    std::vector<float> ch(M.as_int32() * N.as_int32(), 0.f);
    CUDA_CALL(cudaMemcpy(ch.data(), Cg, ch.size() * sizeof(float), cudaMemcpyDeviceToHost));

    auto* Ad = reinterpret_cast<float*>(Ab->memory);
    auto* Bd = reinterpret_cast<float*>(Bb->memory);
    for (int i = 0; i < Ab->num_elements(); i++) {
      ASSERT_NEAR(Ad[i] + Bd[i], ch[i], 1e-5);
    }
  }
#endif
}

TEST(Compiler, sqrt) {
  Expr N(100);
  Expr C(10);
  Expr H(10);
  Expr W(10);

  Placeholder<float> input("input", {N, C, H, W});
  Placeholder<float> mean("mean", {C});
  Placeholder<float> scale("scale", {C});
  Placeholder<float> variance("variance", {C});
  Placeholder<float> bias("bias", {C});
  float epsilon = 0.1f;

  auto A = Compute({N, C, H, W}, [=](Expr n, Expr c, Expr h, Expr w) {
    return (input(n, c, h, w) - mean(c)) * scale(c) / lang::Sqrt(variance(c) + Expr(epsilon)) + bias(c);
  });

  auto B = hlir::pe::Pool2d(input, {3, 3}, {1, 1}, {1, 1, 1, 1}, "max", false, false);

  auto BB = hlir::pe::BatchNorm_NCHW(input, scale, bias, mean, variance, epsilon, "batchnorm");

  auto stages = CreateStages({input, mean, scale, variance, A, bias, B[0], BB});

  auto fn = Lower("fn", stages, {input, mean, scale, bias, variance, A, B[0], BB});

  Module::Builder builder("some", common::DefaultHostTarget());
  builder.AddFunction(fn);

  auto compiler = Compiler::Create(common::DefaultHostTarget());
  compiler->Build(builder.Build());
}

}  // namespace backends
}  // namespace cinn
