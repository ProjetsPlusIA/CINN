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

#include "cinn/hlir/pe/nn.h"

#include <functional>

#include "cinn/hlir/framework/node.h"
#include "cinn/hlir/framework/op.h"
#include "cinn/hlir/framework/op_strategy.h"
#include "cinn/hlir/pe/broadcast.h"
#include "cinn/hlir/pe/elementwise.h"
#include "cinn/hlir/pe/ir_schedule_pe.h"
#include "cinn/hlir/pe/schedule.h"
#include "cinn/ir/ir_base.h"
#include "cinn/ir/layout.h"
#include "cinn/poly/stage.h"

DECLARE_bool(cinn_ir_schedule);

namespace cinn {
namespace hlir {
namespace op {
using common::_CINNValuePack_;
using common::CINNValue;
using common::CINNValuePack;
using framework::OpStrategy;
using framework::shape_t;
using framework::StrategyFunction;

std::shared_ptr<OpStrategy> StrategyForRelu(const framework::NodeAttr &attrs,
                                            const std::vector<ir::Tensor> &inputs,
                                            const std::vector<Type> &out_type,
                                            const std::vector<std::vector<int>> &output_shapes,
                                            const Target &target) {
  framework::CINNCompute relu_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of relu compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    CHECK(!a.empty()) << "at least one input tensor for relu compute\n";
    Expr A = a[0];
    CHECK(A.as_tensor());
    auto out    = pe::Relu<float>(A.as_tensor_ref(), 0.0, UniqName("Relu_output"));
    auto stages = CreateStages({out});
    *ret        = CINNValuePack{{CINNValue(Expr(out.get())), CINNValue(stages)}};
  });

  framework::CINNSchedule relu_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of relu schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK_EQ(arg_pack.size(), 2UL);
    if (target.arch == Target::Arch::NVGPU) {
      Expr out              = arg_pack[0];
      poly::StageMap stages = arg_pack[1];
      CHECK(out.as_tensor());
      pe::CudaScheduleInjective(stages[out.as_tensor_ref()], output_shapes.front(), target);
    } else if (target.arch == Target::Arch::X86) {
      Expr out              = arg_pack[0];
      poly::StageMap stages = arg_pack[1];
      CHECK(out.as_tensor());
      pe::ScheduleInjectiveCPU(stages[out.as_tensor_ref()], output_shapes.front(), target);
    }
    *ret = arg_pack;
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  CHECK(out_type.size()) << "Out_type of relu op is empty! Please check.";
  if (out_type[0] == Float(32)) {
    strategy->AddImpl(relu_compute, relu_schedule, "strategy.relu.x86", 1);
  } else {
    LOG(FATAL) << "Relu op with dtype != float32 is not implemented yet!";
  }
  return strategy;
}

std::vector<framework::shape_t> InferShapeForRelu(const std::vector<framework::shape_t> &inputs_shape,
                                                  const framework::AttrMapType &attrs) {
  CHECK(!inputs_shape.empty() && !inputs_shape[0].empty()) << "The input's shape size is 0! Please check again.";
  std::vector<framework::shape_t> res{inputs_shape[0]};
  return res;
}

std::vector<Type> InferDtypeForRelu(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  std::vector<Type> res{inputs_type[0]};
  return res;
}

std::shared_ptr<OpStrategy> StrategyForRelu6(const framework::NodeAttr &attrs,
                                             const std::vector<ir::Tensor> &inputs,
                                             const std::vector<Type> &out_type,
                                             const std::vector<std::vector<int>> &output_shapes,
                                             const Target &target) {
  framework::CINNCompute relu_compute([](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of relu6 compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    CHECK(!a.empty()) << "at least one input tensor for relu6 compute\n";
    Expr A = a[0];
    CHECK(A.as_tensor());
    auto out    = pe::Relu6<float>(A.as_tensor_ref(), 0.0, UniqName("Relu6_output"));
    auto stages = CreateStages({out});
    *ret        = CINNValuePack{{CINNValue(Expr(out.get())), CINNValue(stages)}};
  });

  framework::CINNSchedule relu_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of relu6 schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK_EQ(arg_pack.size(), 2UL);
    if (target.arch == Target::Arch::NVGPU) {
      Expr out              = arg_pack[0];
      poly::StageMap stages = arg_pack[1];
      CHECK(out.as_tensor());
      pe::CudaScheduleInjective(stages[out.as_tensor_ref()], output_shapes.front(), target);
    } else if (target.arch == Target::Arch::X86) {
      Expr out              = arg_pack[0];
      poly::StageMap stages = arg_pack[1];
      CHECK(out.as_tensor());
      pe::ScheduleInjectiveCPU(stages[out.as_tensor_ref()], output_shapes.front(), target);
    }
    *ret = arg_pack;
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  CHECK(out_type.size()) << "Out_type of relu6 op is empty! Please check.";
  if (out_type[0] == Float(32)) {
    strategy->AddImpl(relu_compute, relu_schedule, "strategy.relu6.x86", 1);
  } else {
    LOG(FATAL) << "Relu6 op with dtype != float32 is not implemented yet!";
  }
  return strategy;
}

std::shared_ptr<OpStrategy> StrategyForConv2d(const framework::NodeAttr &attrs,
                                              const std::vector<ir::Tensor> &inputs,
                                              const std::vector<Type> &out_type,
                                              const std::vector<std::vector<int>> &output_shapes,
                                              const Target &target) {
  std::vector<int> padding({0, 0});
  std::vector<int> stride({1, 1});
  std::vector<int> dilation({1, 1});
  std::string data_format = "NCHW";
  int groups              = 1;
  std::string key         = "";
  std::string conv_type   = "";
  bool use_mkldnn         = false;
  if (attrs.attr_store.find("padding") != attrs.attr_store.end()) {
    padding = absl::get<std::vector<int>>(attrs.attr_store.at("padding"));
  }
  if (attrs.attr_store.find("stride") != attrs.attr_store.end()) {
    stride = absl::get<std::vector<int>>(attrs.attr_store.at("stride"));
  }
  if (attrs.attr_store.find("dilation") != attrs.attr_store.end()) {
    dilation = absl::get<std::vector<int>>(attrs.attr_store.at("dilation"));
  }
  if (attrs.attr_store.find("data_format") != attrs.attr_store.end()) {
    data_format = absl::get<std::string>(attrs.attr_store.at("data_format"));
  }
  if (attrs.attr_store.find("groups") != attrs.attr_store.end()) {
    groups = absl::get<int>(attrs.attr_store.at("groups"));
  }
  if (attrs.attr_store.find("use_mkldnn") != attrs.attr_store.end()) {
    use_mkldnn = absl::get<bool>(attrs.attr_store.at("use_mkldnn"));
  }
  if (attrs.attr_store.find("key") != attrs.attr_store.end()) {
    key = absl::get<std::string>(attrs.attr_store.at("key"));
  }
  // get conv type
  if (attrs.attr_store.find("conv_type") != attrs.attr_store.end()) {
    conv_type = absl::get<std::string>(attrs.attr_store.at("conv_type"));
  } else {
    conv_type = "forward";
  }

#ifndef CINN_WITH_CUDNN
  CHECK_EQ(conv_type, "forward") << "cudnn is not found, backward_data/backward_filter is not supported!";
#endif

  framework::CINNCompute conv2d_compute([=](lang::Args args, lang::RetValue *ret) {
    std::vector<CINNValue> res;
    CHECK(!args.empty()) << "The input argument of conv2d compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    CHECK_GE(a.size(), 2U) << "at least 2 input tensors for conv2d compute\n";
    Expr A = a[0];
    Expr B = a[1];
    CHECK(A.as_tensor());
    CHECK(B.as_tensor());
    CHECK_EQ(padding.size(), 2) << "The size of padding in conv2d op is not 2! Please check.";
    CHECK_EQ(stride.size(), 2) << "The size of stride in conv2d op is not 2! Please check.";
    CHECK_EQ(dilation.size(), 2) << "The size of stride in conv2d op is not 2! Please check.";
    std::vector<ir::Tensor> out;
    VLOG(3) << "input shape: " << utils::Join(A.as_tensor_ref()->shape, ", ");
    VLOG(3) << "weight shape: " << utils::Join(B.as_tensor_ref()->shape, ", ");
    if (data_format == "NCHW") {
      // A is input: [N, C, H, W], B is filter: [C_out, C_in/group, filter_h, filter_w]
      if (target.arch == Target::Arch::X86) {
        if (groups == 1 && !use_mkldnn) {
          out = pe::Conv2d_NCHW_5D(A.as_tensor_ref(),
                                   B.as_tensor_ref(),
                                   padding[0],
                                   padding[1],
                                   stride[0],
                                   stride[1],
                                   dilation[0],
                                   dilation[1],
                                   key,
                                   UniqName("Conv2d_nchw_5d_out"),
                                   target);
        } else {
#ifdef CINN_WITH_MKLDNN
          out = pe::Conv2d_NCHW_MKLDNN(A.as_tensor_ref(),
                                       B.as_tensor_ref(),
                                       padding[0],
                                       padding[1],
                                       stride[0],
                                       stride[1],
                                       dilation[0],
                                       dilation[1],
                                       UniqName("Conv2d_nchw_mkldnn_out"));
#else
          out = pe::Conv2d_NCHW_5D(A.as_tensor_ref(),
                                   B.as_tensor_ref(),
                                   padding[0],
                                   padding[1],
                                   stride[0],
                                   stride[1],
                                   dilation[0],
                                   dilation[1],
                                   key,
                                   UniqName("Conv2d_nchw_5D_out"));
#endif
        }
      } else {
        if (conv_type == "forward") {
          out = pe::Conv2d_NCHW(A.as_tensor_ref(),
                                B.as_tensor_ref(),
                                padding[0],
                                padding[1],
                                stride[0],
                                stride[1],
                                dilation[0],
                                dilation[1],
                                UniqName("Conv2d_nchw_out"));
          out.push_back(B.as_tensor_ref());
        } else {
#ifdef CINN_WITH_CUDNN
          // as backward_data and backward_filter is not support now, we built a fake op to instead.
          // as the runtime use cudnn to compute the conv2d, so this fake op is not been called.
          // When cinn support backward_filter/backward_data code gen, this code is to be removed.
          out = pe::Identity(A.as_tensor_ref());
          out.push_back(A.as_tensor_ref());
          out.push_back(B.as_tensor_ref());
#endif
        }
      }
    } else if (data_format == "NHWC") {
      // A is input: [N, H, W, C], B is filter: [C_out, C_in/group, filter_h, filter_w]
      out = pe::Conv2d_NHWC(A.as_tensor_ref(),
                            B.as_tensor_ref(),
                            padding[0],
                            padding[1],
                            stride[0],
                            stride[1],
                            dilation[0],
                            dilation[1],
                            UniqName("Conv2d_nhwc_out"));
    } else {
      LOG(FATAL) << "Only support NCHW and NHWC data layout\n";
    }
    auto stages = CreateStages({A.as_tensor_ref(), B.as_tensor_ref()});

    for (auto &t : out) {
      stages->InsertLazily(t);
      res.push_back(CINNValue(t));
    }
    CHECK(out.size() == 3U || out.size() == 2U || out.size() == 5U || out.size() == 12U)
        << "The output tensor sizes of conv2d op in conv2d op should be 2 or 3 or 5\n";

    res.push_back(CINNValue(stages));
    *ret = CINNValuePack{res};
  });

  framework::CINNSchedule conv2d_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of conv2d schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK(arg_pack.size() == 4UL || arg_pack.size() == 3UL || arg_pack.size() == 6UL || arg_pack.size() == 13UL);
    poly::StageMap stages = arg_pack.back();
    if (target.arch == Target::Arch::NVGPU) {
#ifdef CINN_WITH_CUDNN
      // If conv_type is backward_filter or backward_data, we built a fake op.
      // As runtime use cudnn to compute conv2d, this fake op is not to be called.
      // When cinn support backward_filter/backward_data code gen, this code is to be removed.
      if (conv_type != "forward") {
        Expr out = arg_pack[0];
        pe::CudaScheduleInjective(stages[out.as_tensor_ref()], output_shapes.front(), target);
        *ret = CINNValuePack{{CINNValue(out), CINNValue(stages)}};
        return;
      }
#endif
      if (arg_pack.size() == 4UL) {
        Expr Out             = arg_pack[0];
        Expr input_pad       = arg_pack[1];
        Expr weights         = arg_pack[2];
        ir::Tensor out_t     = Out.as_tensor_ref();
        ir::Tensor input_t   = input_pad.as_tensor_ref();
        ir::Tensor weights_t = weights.as_tensor_ref();
        CHECK(Out.as_tensor());
        pe::CudaScheduleConv(stages, input_t, weights_t, out_t, target);
        arg_pack[0] = Expr(out_t);
        arg_pack[1] = Expr(input_t);
        arg_pack[2] = Expr(weights_t);
        *ret        = CINNValuePack{{arg_pack[0], CINNValue(stages)}};
        return;
      } else if (arg_pack.size() == 13UL) {
        Expr wino_weights_dilation          = arg_pack[0];
        Expr wino_input_pad                 = arg_pack[1];
        Expr wino_A                         = arg_pack[2];
        Expr wino_B                         = arg_pack[3];
        Expr wino_G                         = arg_pack[4];
        Expr kernel_pack                    = arg_pack[5];
        Expr input_tile                     = arg_pack[6];
        Expr data_pack                      = arg_pack[7];
        Expr bgemm                          = arg_pack[8];
        Expr inverse                        = arg_pack[9];
        Expr wino_conv                      = arg_pack[10];
        ir::Tensor wino_weights_dilation_t  = wino_weights_dilation.as_tensor_ref();
        ir::Tensor wino_input_pad_t         = wino_input_pad.as_tensor_ref();
        ir::Tensor wino_A_t                 = wino_A.as_tensor_ref();
        ir::Tensor wino_B_t                 = wino_B.as_tensor_ref();
        ir::Tensor wino_G_t                 = wino_G.as_tensor_ref();
        ir::Tensor kernel_pack_t            = kernel_pack.as_tensor_ref();
        ir::Tensor input_tile_t             = input_tile.as_tensor_ref();
        ir::Tensor data_pack_t              = data_pack.as_tensor_ref();
        ir::Tensor bgemm_t                  = bgemm.as_tensor_ref();
        ir::Tensor inverse_t                = inverse.as_tensor_ref();
        ir::Tensor wino_conv_t              = wino_conv.as_tensor_ref();
        std::vector<ir::Tensor> all_tensors = {wino_weights_dilation_t,
                                               wino_input_pad_t,
                                               wino_A_t,
                                               wino_B_t,
                                               wino_G_t,
                                               kernel_pack_t,
                                               input_tile_t,
                                               data_pack_t,
                                               bgemm_t,
                                               inverse_t,
                                               wino_conv_t};
        hlir::pe::CudaScheduleWinogradConv(stages, all_tensors, target);
        arg_pack[0]  = Expr(all_tensors[0]);
        arg_pack[1]  = Expr(all_tensors[1]);
        arg_pack[2]  = Expr(all_tensors[2]);
        arg_pack[3]  = Expr(all_tensors[3]);
        arg_pack[4]  = Expr(all_tensors[4]);
        arg_pack[5]  = Expr(all_tensors[5]);
        arg_pack[6]  = Expr(all_tensors[6]);
        arg_pack[7]  = Expr(all_tensors[7]);
        arg_pack[8]  = Expr(all_tensors[8]);
        arg_pack[9]  = Expr(all_tensors[9]);
        arg_pack[10] = Expr(all_tensors[10]);
        *ret         = CINNValuePack{{arg_pack[10], arg_pack[5], arg_pack[7], arg_pack[8], CINNValue(stages)}};
        return;
      }
    } else if (target.arch == Target::Arch::X86) {
      if (arg_pack.size() == 6UL) {
        Expr res              = arg_pack[0];
        Expr packed_out       = arg_pack[1];
        Expr weights_dilation = arg_pack[2];
        Expr input_pad        = arg_pack[3];
        Expr data             = arg_pack[4];
        CHECK(res.as_tensor());
        CHECK(packed_out.as_tensor());
        CHECK(input_pad.as_tensor());
        CHECK(weights_dilation.as_tensor());
        CHECK(data.as_tensor());
        std::vector<Expr> kernel_shape = weights_dilation.as_tensor_ref()->shape;
        // kernel_h == 1 && kernel_w == 1
        CHECK_EQ(kernel_shape.size(), 6U) << "kernel_dialtion shape size should be 6";
        bool is_1x1                  = (is_zero(kernel_shape[2] - 1)) && (is_zero(kernel_shape[3] - 1));
        ir::Tensor packed_out_tensor = packed_out.as_tensor_ref();
        bool do_padding              = (padding[0] == 0 && padding[1] == 0) ? false : true;

        if (groups == 1) {
          if (is_1x1) {
            pe::Conv2d_NCHWc_1X1_Schedule_CPU(stages,
                                              res.as_tensor_ref(),
                                              packed_out_tensor,
                                              input_pad.as_tensor_ref(),
                                              weights_dilation.as_tensor_ref(),
                                              data.as_tensor_ref(),
                                              target,
                                              key,
                                              do_padding);
          } else {
            pe::Conv2d_NCHWc_Schedule_CPU(stages,
                                          res.as_tensor_ref(),
                                          packed_out_tensor,
                                          input_pad.as_tensor_ref(),
                                          weights_dilation.as_tensor_ref(),
                                          data.as_tensor_ref(),
                                          target,
                                          key,
                                          do_padding);
          }
          if (do_padding) {
            *ret = CINNValuePack{
                {CINNValue(res), CINNValue(packed_out_tensor), arg_pack[2], arg_pack[3], CINNValue(stages)}};
          } else {
            *ret = CINNValuePack{{CINNValue(res), CINNValue(packed_out_tensor), arg_pack[2], CINNValue(stages)}};
          }
          return;
        } else {
          // todo: opt group_conv schedule
          VLOG(3) << "use simple group convolution schedule";
          stages[input_pad.as_tensor_ref()]->ComputeInline();
          stages[weights_dilation.as_tensor_ref()]->ComputeInline();
          stages[data.as_tensor_ref()]->ComputeInline();
          *ret = CINNValuePack{{arg_pack[0], CINNValue(packed_out_tensor), CINNValue(stages)}};
        }
        return;
      } else if (arg_pack.size() == 4UL) {
        Expr input_pad = arg_pack[1];
        CHECK(input_pad.as_tensor());
        stages[input_pad.as_tensor_ref()]->ComputeInline();
        Expr weights_dilation = arg_pack[2];
        CHECK(weights_dilation.as_tensor());
        stages[weights_dilation.as_tensor_ref()]->ComputeInline();
        *ret = CINNValuePack{{arg_pack[0], CINNValue(stages)}};
        return;
      }
    }
    *ret = arg_pack;
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  CHECK(out_type.size()) << "Out_type of conv2d op is empty! Please check.";
  if (out_type[0] == Float(32)) {
    strategy->AddImpl(conv2d_compute, conv2d_schedule, "strategy.conv2d.x86", 1);
  } else {
    LOG(FATAL) << "Conv2d op with dtype != float32 is not implemented yet!";
  }
  return strategy;
}

std::vector<shape_t> InferShapeForConv2d(const std::vector<shape_t> &inputs_shape,
                                         const framework::AttrMapType &attrs) {
  CHECK(!inputs_shape.empty() && !inputs_shape[0].empty()) << "The input's shape size is 0! Please check again.";
  std::vector<int> padding({0, 0});
  std::vector<int> stride({1, 1});
  std::vector<int> dilation({1, 1});
  int groups              = 1;
  std::string data_format = "NCHW";
  std::string conv_type   = "";
  if (attrs.find("padding") != attrs.end()) {
    padding = absl::get<std::vector<int>>(attrs.at("padding"));
  }
  if (attrs.find("stride") != attrs.end()) {
    stride = absl::get<std::vector<int>>(attrs.at("stride"));
  }
  if (attrs.find("dilation") != attrs.end()) {
    dilation = absl::get<std::vector<int>>(attrs.at("dilation"));
  }
  if (attrs.find("groups") != attrs.end()) {
    groups = absl::get<int>(attrs.at("groups"));
  }
  if (attrs.find("data_format") != attrs.end()) {
    data_format = absl::get<std::string>(attrs.at("data_format"));
  }
  if (attrs.find("conv_type") != attrs.end()) {
    conv_type = absl::get<std::string>(attrs.at("conv_type"));
  } else {
    conv_type = "forward";
  }

  CHECK_EQ(padding.size(), 2) << "The size of padding in conv2d op is not 2! Please check.";
  CHECK_EQ(stride.size(), 2) << "The size of stride in conv2d op is not 2! Please check.";
  CHECK_GE(inputs_shape[0].size(), 3) << "The first input tensor's shape size of conv2d op is < 3! Please check.";
  CHECK(conv_type == "forward" || conv_type == "backward_data" || conv_type == "backward_filter")
      << "The conv type should be one of {forward, backward_data, backward_filter}.";

  std::vector<shape_t> res;
  if (data_format == "NCHW") {
    // A is input: [N, C, H, W], B is filter: [C_out, C_in/group, filter_h, filter_w]
    int out_shape_h = 0, out_shape_w = 0;
    if (conv_type == "forward") {
      out_shape_h =
          (inputs_shape[0][2] - ((inputs_shape[1][2] - 1) * dilation[0] + 1) + 2 * padding[0]) / stride[0] + 1;
      out_shape_w =
          (inputs_shape[0][3] - ((inputs_shape[1][3] - 1) * dilation[1] + 1) + 2 * padding[1]) / stride[1] + 1;
    } else if (conv_type == "backward_data") {
      CHECK(attrs.find("output_shape") != attrs.end()) << "The shape of filter is not found! Please check.";
      auto data_shape = absl::get<std::vector<int>>(attrs.at("output_shape"));
      CHECK_EQ(data_shape.size(), 4) << "The size of filter shape is not 4(oc, ic, fh, fw)!Please check";
      out_shape_h = data_shape[2];
      out_shape_w = data_shape[3];
    } else if (conv_type == "backward_filter") {
      CHECK(attrs.find("output_shape") != attrs.end()) << "The shape of filter is not found! Please check.";
      auto filter_shape = absl::get<std::vector<int>>(attrs.at("output_shape"));
      CHECK_EQ(filter_shape.size(), 4) << "The size of filter shape is not 4(oc, ic, fh, fw)!Please check";
      out_shape_h = filter_shape[2];
      out_shape_w = filter_shape[3];
    }

    res = {{inputs_shape[0][0], inputs_shape[1][0], out_shape_h, out_shape_w}};

    absl::flat_hash_map<std::string, int> conv2d_factors;
    int batch       = inputs_shape[0][0];
    int oc          = inputs_shape[1][0];
    int ic          = inputs_shape[0][1];
    int fc          = inputs_shape[1][1];
    int h_in        = inputs_shape[0][2];
    int w_in        = inputs_shape[0][3];
    int h_f         = inputs_shape[1][2];
    int w_f         = inputs_shape[1][3];
    int pad_h       = padding[0];
    int pad_w       = padding[1];
    std::string key = pe::GenerateX86ConvKey(inputs_shape[0], inputs_shape[1], stride, padding, dilation);
    VLOG(3) << "key: " << key;
    pe::GetConv2dFactors(&conv2d_factors, oc, ic, fc, -1, -1, Float(32), common::DefaultHostTarget(), key);
    int ic_bn = conv2d_factors["ic_bn"];
    int oc_bn = conv2d_factors["oc_bn"];
    int fc_bn = conv2d_factors["fc_bn"];
    VLOG(3) << "ic_bn: " << ic_bn;
    VLOG(3) << "oc_bn: " << oc_bn;
    VLOG(3) << "fc_bn: " << fc_bn;
    int oc_chunk                            = oc / oc_bn;
    int ic_chunk                            = ic / ic_bn;
    int fc_chunk                            = fc / fc_bn;
    std::vector<int> packed_out_shape       = {batch, oc_chunk, out_shape_h, out_shape_w, oc_bn};
    std::vector<int> input_pad_shape        = {batch, ic_chunk, h_in + 2 * pad_h, w_in + 2 * pad_w, ic_bn};
    std::vector<int> weights_dilation_shape = {
        oc_chunk, fc_chunk, dilation[0] * (h_f - 1) + 1, dilation[1] * (w_f - 1) + 1, fc_bn, oc_bn};
    std::vector<int> data_shape = {batch, ic_chunk, h_in, w_in, ic_bn};

    // output shape
    std::vector<int> res_shape = {};
    if (conv_type == "forward") {
      // x w y
      res_shape = {batch, oc, out_shape_h, out_shape_w};
    } else if (conv_type == "backward_data") {
      // w(C_out, C_in/group, h, w) dy(Batch, C_out, h, w) dx(batch, C_in, h, w)
      res_shape = {inputs_shape[1][0], inputs_shape[0][1] * groups, out_shape_h, out_shape_w};
    } else if (conv_type == "backward_filter") {
      // x(batch, C_in, h, w) dy(batch, C_out, h, w) dw (C_out, C_in/group, h, w)
      res_shape = {inputs_shape[1][1], inputs_shape[0][1] / groups, out_shape_h, out_shape_w};
    }
    return {res_shape, packed_out_shape, weights_dilation_shape, input_pad_shape};
  } else if (data_format == "NHWC") {
    // A is input: [N, H, W, C], B is filter: [C_out, C_in/group, filter_h, filter_w]
    int out_shape_h =
        (inputs_shape[0][1] - ((inputs_shape[1][2] - 1) * dilation[0] + 1) + 2 * padding[0]) / stride[0] + 1;
    int out_shape_w =
        (inputs_shape[0][2] - ((inputs_shape[1][3] - 1) * dilation[1] + 1) + 2 * padding[1]) / stride[1] + 1;
    res = {{inputs_shape[0][0], out_shape_h, out_shape_w, inputs_shape[1][0]}};
  } else {
    LOG(FATAL) << "Only support NCHW and NHWC data layout\n";
  }
  return res;
}

std::vector<Type> InferDtypeForConv2d(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  std::vector<Type> res{inputs_type[0], inputs_type[0], inputs_type[0], inputs_type[0]};
  return res;
}

std::vector<std::vector<std::string>> InferLayoutForConv2d(const std::vector<framework::shape_t> &input_shapes,
                                                           const std::vector<std::string> &input_layouts,
                                                           const framework::NodeAttr &attrs,
                                                           const Target &target) {
  CHECK_EQ(input_layouts.size(), 2U) << "The input's layouts size is not 2! Please check again.";
  ir::Layout weight_layout(input_layouts[1]);
  return {{input_layouts[0], input_layouts[0], input_layouts[0], input_layouts[0]}, input_layouts};
}

std::shared_ptr<OpStrategy> StrategyForConv2dNCHWc(const framework::NodeAttr &attrs,
                                                   const std::vector<ir::Tensor> &inputs,
                                                   const std::vector<Type> &out_type,
                                                   const std::vector<std::vector<int>> &output_shapes,
                                                   const Target &target) {
  std::vector<int> padding({0, 0});
  std::vector<int> stride({1, 1});
  std::vector<int> dilation({1, 1});
  std::string data_format = "NCHWc";
  int groups              = 1;
  if (attrs.attr_store.find("padding") != attrs.attr_store.end()) {
    padding = absl::get<std::vector<int>>(attrs.attr_store.at("padding"));
  }
  if (attrs.attr_store.find("stride") != attrs.attr_store.end()) {
    stride = absl::get<std::vector<int>>(attrs.attr_store.at("stride"));
  }
  if (attrs.attr_store.find("dilation") != attrs.attr_store.end()) {
    dilation = absl::get<std::vector<int>>(attrs.attr_store.at("dilation"));
  }
  if (attrs.attr_store.find("data_format") != attrs.attr_store.end()) {
    data_format = absl::get<std::string>(attrs.attr_store.at("data_format"));
  }
  if (attrs.attr_store.find("groups") != attrs.attr_store.end()) {
    groups = absl::get<int>(attrs.attr_store.at("groups"));
  }
  CHECK(data_format == "NCHWc") << "conv2d_NCHWc op's data_format should be NCHWc";
  framework::CINNCompute conv2d_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of conv2d_NCHWc compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    CHECK_GE(a.size(), 2U) << "at least 2 input tensors for conv2d_NCHWc compute\n";
    Expr A = a[0];
    Expr B = a[1];
    CHECK(A.as_tensor());
    CHECK(B.as_tensor());
    auto tensor_a = A.as_tensor_ref();
    auto tensor_b = B.as_tensor_ref();
    CHECK_EQ(tensor_a->shape.size(), 5) << "input's shape should be 5";
    CHECK_EQ(tensor_b->shape.size(), 6) << "weight's shape should be 6";
    CHECK_EQ(padding.size(), 2) << "The size of padding in conv2d_NCHWc op is not 2! Please check.";
    CHECK_EQ(stride.size(), 2) << "The size of stride in conv2d_NCHWc op is not 2! Please check.";
    CHECK_EQ(dilation.size(), 2) << "The size of stride in conv2d_NCHWc op is not 2! Please check.";
    std::vector<ir::Tensor> out;
    CHECK(target.arch == Target::Arch::X86) << "conv2d_NCHWc op is only used in x86";
    // A is input: [N, C_in_outer, H, W, C_in_inner], B is filter: [C_out, C_in_group_outer, filter_h, filter_w,
    // C_in_group_inner]
    std::string key;
    VLOG(3) << "input[" << utils::Join(tensor_a->shape, ", ") << "], weight shape["
            << utils::Join(tensor_b->shape, ", ") << "]";
    out = pe::Conv2d_NCHWc(tensor_a,
                           tensor_b,
                           padding[0],
                           padding[1],
                           stride[0],
                           stride[1],
                           dilation[0],
                           dilation[1],
                           UniqName("T_conv2d_NCHWc_out"),
                           target);

    auto stages = CreateStages({tensor_a, tensor_b});

    std::vector<CINNValue> res;
    CHECK(out.size() == 2U) << "The output tensor sizes of conv2d_NCHWc op should be 2\n";
    for (auto &t : out) {
      stages->InsertLazily(t);
      res.push_back(CINNValue(t));
    }
    res.push_back(CINNValue(stages));
    *ret = CINNValuePack{res};
  });

  framework::CINNSchedule conv2d_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of conv2d_NCHWc schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK_EQ(arg_pack.size(), 3UL);
    poly::StageMap stages = arg_pack.back();
    Expr packed_out       = arg_pack[0];
    Expr input_pad        = arg_pack[1];
    CHECK(packed_out.as_tensor());
    CHECK(input_pad.as_tensor());
    std::vector<Expr> kernel_shape = inputs[1]->shape;
    // kernel_h == 1 && kernel_w == 1
    CHECK_EQ(kernel_shape.size(), 6U) << "kernel_dialtion shape size should be 6";
    bool is_1x1 = (is_zero(kernel_shape[2] - 1)) && (is_zero(kernel_shape[3] - 1));
    ir::Tensor res;
    ir::Tensor data;
    ir::Tensor weights;
    ir::Tensor packed_out_tensor = packed_out.as_tensor_ref();
    std::string key;
    bool do_padding = (padding[0] == 0 && padding[1] == 0) ? false : true;
    if (attrs.attr_store.find("key") != attrs.attr_store.end()) {
      key = absl::get<std::string>(attrs.attr_store.at("key"));
    }
    if (is_1x1) {
      pe::Conv2d_NCHWc_1X1_Schedule_CPU(
          stages, res, packed_out_tensor, input_pad.as_tensor_ref(), weights, data, target, key, do_padding);
    } else {
      pe::Conv2d_NCHWc_Schedule_CPU(
          stages, res, packed_out_tensor, input_pad.as_tensor_ref(), weights, data, target, key, do_padding);
    }
    if (do_padding) {
      *ret = CINNValuePack{{CINNValue(packed_out_tensor), arg_pack[0], arg_pack[1], CINNValue(stages)}};
    } else {
      *ret = CINNValuePack{{CINNValue(packed_out_tensor), arg_pack[0], CINNValue(stages)}};
    }
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  CHECK(out_type.size()) << "Out_type of conv2d_NCHWc op is empty! Please check.";
  if (out_type[0] == Float(32)) {
    strategy->AddImpl(conv2d_compute, conv2d_schedule, "strategy.conv2d_NCHWc.x86", 1);
  } else {
    LOG(FATAL) << "conv2d_NCHWc op with dtype != float32 is not implemented yet!";
  }
  return strategy;
}

std::vector<shape_t> InferShapeForConv2dNCHWc(const std::vector<shape_t> &inputs_shape,
                                              const framework::AttrMapType &attrs) {
  CHECK(!inputs_shape.empty() && !inputs_shape[0].empty()) << "The input's shape size is 0! Please check again.";
  std::vector<int> padding({0, 0});
  std::vector<int> stride({1, 1});
  std::vector<int> dilation({1, 1});
  std::string data_format = "NCHWc";
  if (attrs.find("padding") != attrs.end()) {
    padding = absl::get<std::vector<int>>(attrs.at("padding"));
  }
  if (attrs.find("stride") != attrs.end()) {
    stride = absl::get<std::vector<int>>(attrs.at("stride"));
  }
  if (attrs.find("dilation") != attrs.end()) {
    dilation = absl::get<std::vector<int>>(attrs.at("dilation"));
  }
  if (attrs.find("data_format") != attrs.end()) {
    data_format = absl::get<std::string>(attrs.at("data_format"));
  }
  CHECK_EQ(padding.size(), 2) << "The size of padding in conv2d_NCHWc op is not 2! Please check.";
  CHECK_EQ(stride.size(), 2) << "The size of stride in conv2d_NCHWc op is not 2! Please check.";
  CHECK_EQ(inputs_shape[0].size(), 5)
      << "The first input tensor's shape size of conv2d_NCHWc op should be 5! Please check.";
  CHECK_EQ(inputs_shape[1].size(), 6)
      << "The second input tensor's shape size of conv2d_NCHWc op should be 6! Please check.";

  std::vector<shape_t> res;
  CHECK(data_format == "NCHWc") << "NCHWc op's data_format should be NCHWc";
  int out_shape_h =
      (inputs_shape[0][2] - ((inputs_shape[1][2] - 1) * dilation[0] + 1) + 2 * padding[0]) / stride[0] + 1;
  int out_shape_w =
      (inputs_shape[0][3] - ((inputs_shape[1][3] - 1) * dilation[1] + 1) + 2 * padding[1]) / stride[1] + 1;

  // A: NCHWc, B: OIHWio
  int batch                         = inputs_shape[0][0];
  int h_in                          = inputs_shape[0][2];
  int w_in                          = inputs_shape[0][3];
  int oc                            = inputs_shape[1][0];
  int h_f                           = inputs_shape[1][2];
  int w_f                           = inputs_shape[1][3];
  int pad_h                         = padding[0];
  int pad_w                         = padding[1];
  int ic_bn                         = inputs_shape[0][4];
  int ic_chunk                      = inputs_shape[0][1];
  int oc_bn                         = inputs_shape[1][5];
  int oc_chunk                      = inputs_shape[1][0];
  std::vector<int> packed_out_shape = {batch, oc_chunk, out_shape_h, out_shape_w, oc_bn};
  auto pad_h_bound                  = (out_shape_h - 1) * stride[0] + (h_f - 1) * dilation[0] + 1;
  auto pad_w_bound                  = (out_shape_w - 1) * stride[1] + (w_f - 1) * dilation[1] + 1;
  auto input_pad_h                  = std::min(pad_h_bound, h_in + 2 * pad_h);
  auto input_pad_w                  = std::min(pad_w_bound, w_in + 2 * pad_w);
  std::vector<int> input_pad_shape  = {batch, ic_chunk, input_pad_h, input_pad_w, ic_bn};
  VLOG(3) << "packed_out_shape: " << utils::Join(packed_out_shape, ", ");
  return {packed_out_shape, packed_out_shape, input_pad_shape};
}

std::vector<std::vector<std::string>> InferLayoutForConv2dNCHWc(const std::vector<framework::shape_t> &input_shapes,
                                                                const std::vector<std::string> &input_layouts,
                                                                const framework::NodeAttr &attrs,
                                                                const Target &target) {
  CHECK_EQ(input_layouts.size(), 2U) << "The input's layouts size is not 2! Please check again.";
  ir::Layout weight_layout(input_layouts[1]);
  CHECK_EQ(weight_layout.ndims(), 6U);
  auto var   = weight_layout.axes().back();
  int factor = var->upper_bound.as_int32();
  CHECK_GE(factor, 1) << "factor should be larger than 1";
  std::string outlayout = "NCHW" + std::to_string(factor) + "c";
  return {{outlayout, outlayout, input_layouts[0]}, input_layouts};
}

std::vector<Type> InferDtypeForConv2dNCHWc(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  std::vector<Type> res{inputs_type[0], inputs_type[0], inputs_type[0]};
  return res;
}

std::shared_ptr<OpStrategy> StrategyForDepthwiseConv2d(const framework::NodeAttr &attrs,
                                                       const std::vector<ir::Tensor> &inputs,
                                                       const std::vector<Type> &out_type,
                                                       const std::vector<std::vector<int>> &output_shapes,
                                                       const Target &target) {
  std::vector<int> padding  = {0, 0};
  std::vector<int> stride   = {1, 1};
  std::vector<int> dilation = {1, 1};
  std::string data_format   = "NCHW";
  std::string key;
  if (attrs.attr_store.find("padding") != attrs.attr_store.end()) {
    padding = absl::get<std::vector<int>>(attrs.attr_store.at("padding"));
  }
  if (attrs.attr_store.find("stride") != attrs.attr_store.end()) {
    stride = absl::get<std::vector<int>>(attrs.attr_store.at("stride"));
  }
  if (attrs.attr_store.find("data_format") != attrs.attr_store.end()) {
    data_format = absl::get<std::string>(attrs.attr_store.at("data_format"));
  }
  if (attrs.attr_store.find("dilation") != attrs.attr_store.end()) {
    dilation = absl::get<std::vector<int>>(attrs.attr_store.at("dilation"));
  }
  if (attrs.attr_store.find("key") != attrs.attr_store.end()) {
    key = absl::get<std::string>(attrs.attr_store.at("key"));
  }

  framework::CINNCompute depthwise_conv2d_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of depthwise_conv compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    CHECK_GE(a.size(), 2U) << "at least 2 input tensors for depthwise_conv compute\n";
    Expr A = a[0];
    Expr B = a[1];
    CHECK(A.as_tensor());
    CHECK(B.as_tensor());
    CHECK_EQ(padding.size(), 2) << "The size of padding in depthwise_conv op is not 2! Please check.\n";
    CHECK_EQ(stride.size(), 2) << "The size of stride in depthwise_conv op is not 2! Please check.\n";
    CHECK(data_format == "NCHW" || data_format == "NHWC") << "only support NCHW/NHWC data_format.\n";
    std::vector<ir::Tensor> out;
    if (data_format == "NCHW") {
      if (target.arch == Target::Arch::X86) {
        out = pe::Conv2d_NCHW_5D(A.as_tensor_ref(),
                                 B.as_tensor_ref(),
                                 padding[0],
                                 padding[1],
                                 stride[0],
                                 stride[1],
                                 dilation[0],
                                 dilation[1],
                                 key,
                                 UniqName("T_depthwise_conv2d_nchw_5d_out"),
                                 target);
      } else {
        out = pe::Depthwise_Conv2d_NCHW(A.as_tensor_ref(),
                                        B.as_tensor_ref(),
                                        padding[0],
                                        padding[1],
                                        stride[0],
                                        stride[1],
                                        UniqName("T_depthwise_conv2d_nchw_out"));
      }
    } else if (data_format == "NHWC") {
      out = pe::Depthwise_Conv2d_NHWC(A.as_tensor_ref(),
                                      B.as_tensor_ref(),
                                      padding[0],
                                      padding[1],
                                      stride[0],
                                      stride[1],
                                      UniqName("T_depthwise_conv2d_nhwc_out"));
    } else {
      LOG(FATAL) << "Only support NCHW and NHWC data layout\n";
    }

    auto stages = CreateStages({A.as_tensor_ref(), B.as_tensor_ref()});
    std::vector<CINNValue> res;
    for (auto &t : out) {
      stages->InsertLazily(t);
      res.push_back(CINNValue(t));
    }
    CHECK(out.size() == 2U || out.size() == 1U || out.size() == 5U)
        << "The output tensor sizes of depthwise_conv op in depthwise_conv op should be 1 or 2 or 5\n";
    res.push_back(CINNValue(stages));
    *ret = CINNValuePack{res};
  });

  framework::CINNSchedule depthwise_conv2d_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of depthwise_conv schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK(arg_pack.size() == 2UL || arg_pack.size() == 3UL || arg_pack.size() == 6UL);
    poly::StageMap stages = arg_pack[arg_pack.size() - 1];
    Expr Out              = arg_pack[0];
    CHECK(Out.as_tensor());
    if (arg_pack.size() == 3UL) {
      Expr input_pad = arg_pack[1];
      CHECK(input_pad.as_tensor());
      stages[input_pad.as_tensor_ref()]->ComputeInline();
    }
    if (target.arch == Target::Arch::NVGPU) {
      ir::Tensor output = Out.as_tensor_ref();
      CHECK(Out.as_tensor());
      pe::CudaScheduleDepthwiseConv(stages, output, target);
      arg_pack[0] = Expr(output);
    } else if (target.arch == Target::Arch::X86) {
      if (arg_pack.size() == 6UL) {
        Expr res              = arg_pack[0];
        Expr packed_out       = arg_pack[1];
        Expr weights_dilation = arg_pack[2];
        Expr input_pad        = arg_pack[3];
        Expr data             = arg_pack[4];
        CHECK(res.as_tensor());
        CHECK(packed_out.as_tensor());
        CHECK(input_pad.as_tensor());
        CHECK(weights_dilation.as_tensor());
        CHECK(data.as_tensor());
        ir::Tensor packed_out_tensor = packed_out.as_tensor_ref();
        bool do_padding              = (padding[0] == 0 && padding[1] == 0) ? false : true;
        pe::Depthwise_Conv2d_NCHWc_Schedule_CPU_Nofuse(stages,
                                                       res.as_tensor_ref(),
                                                       packed_out_tensor,
                                                       input_pad.as_tensor_ref(),
                                                       weights_dilation.as_tensor_ref(),
                                                       data.as_tensor_ref(),
                                                       target,
                                                       do_padding);
        if (do_padding) {
          *ret = CINNValuePack{
              {CINNValue(res), CINNValue(packed_out_tensor), arg_pack[2], arg_pack[3], CINNValue(stages)}};
        } else {
          *ret = CINNValuePack{{CINNValue(res), CINNValue(packed_out_tensor), arg_pack[2], CINNValue(stages)}};
        }
        return;
      }
    }

    *ret = CINNValuePack{{arg_pack[0], CINNValue(stages)}};
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  CHECK(out_type.size()) << "Out_type of depthwise_conv op is empty! Please check.";
  if (out_type[0] == Float(32)) {
    strategy->AddImpl(depthwise_conv2d_compute, depthwise_conv2d_schedule, "strategy.depthwise_conv.x86", 1);
  } else {
    VLOG(3) << "depthwise_conv op with dtype != float32 is not implemented yet!";
  }
  return strategy;
}

std::vector<shape_t> InferShapeForDepthwiseConv2d(const std::vector<shape_t> &inputs_shape,
                                                  const framework::AttrMapType &attrs) {
  CHECK_EQ(inputs_shape.size(), 2U) << "at least 2 input tensors for depthwise_conv2d op\n";
  CHECK_EQ(inputs_shape[0].size(), 4U) << "The input tensor's shape should be 4! Please check again.";
  CHECK_EQ(inputs_shape[1].size(), 4U) << "The input tensor's shape should be 4! Please check again.";
  std::vector<int> padding = {0, 0};
  std::vector<int> stride  = {1, 1};
  std::string data_format  = "NCHW";
  if (attrs.find("padding") != attrs.end()) {
    padding = absl::get<std::vector<int>>(attrs.at("padding"));
  }
  if (attrs.find("stride") != attrs.end()) {
    stride = absl::get<std::vector<int>>(attrs.at("stride"));
  }
  if (attrs.find("data_format") != attrs.end()) {
    data_format = absl::get<std::string>(attrs.at("data_format"));
  }
  std::vector<shape_t> res;
  CHECK_EQ(padding.size(), 2U) << "The size of padding in depthwise_conv2d op is not 2! Please check.";
  CHECK_EQ(stride.size(), 2U) << "The size of stride in depthwise_conv2d op is not 2! Please check.";
  if (data_format == "NCHW") {
    // A is input: [N, C, H, W], and B is filter: [C_in, channel_multiplier, f_h, f_w]
    int out_shape_h = (inputs_shape[0][2] - inputs_shape[1][2] + 2 * padding[0]) / stride[0] + 1;
    int out_shape_w = (inputs_shape[0][3] - inputs_shape[1][3] + 2 * padding[1]) / stride[1] + 1;
    res             = {{inputs_shape[0][0], inputs_shape[1][1] * inputs_shape[0][1], out_shape_h, out_shape_w}};
  } else if (data_format == "NHWC") {
    // A is input: [N, H, W, C], and B is filter: [C_in, channel_multiplier, f_h, f_w]
    int out_shape_h = (inputs_shape[0][1] - inputs_shape[1][1] + 2 * padding[0]) / stride[0] + 1;
    int out_shape_w = (inputs_shape[0][2] - inputs_shape[1][2] + 2 * padding[1]) / stride[1] + 1;
    res             = {{inputs_shape[0][0], out_shape_h, out_shape_w, inputs_shape[1][1] * inputs_shape[0][3]}};
  } else {
    LOG(FATAL) << "Only support NCHW and NHWC data layout\n";
  }
  return res;
}

std::vector<Type> InferDtypeForDepthwiseConv2d(const std::vector<Type> &inputs_type,
                                               const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  std::vector<Type> res{inputs_type[0]};
  return res;
}

std::shared_ptr<OpStrategy> StrategyForBatchNorm(const framework::NodeAttr &attrs,
                                                 const std::vector<ir::Tensor> &inputs,
                                                 const std::vector<Type> &out_type,
                                                 const std::vector<std::vector<int>> &output_shapes,
                                                 const Target &target) {
  float epsilon = 0.00001f;
  std::vector<std::string> input_layouts;
  if (attrs.attr_store.find("epsilon") != attrs.attr_store.end()) {
    epsilon = absl::get<float>(attrs.attr_store.at("epsilon"));
  }
  if (attrs.attr_store.find("input_layouts") != attrs.attr_store.end()) {
    input_layouts = absl::get<std::vector<std::string>>(attrs.attr_store.at("input_layouts"));
  }
  framework::CINNCompute batchnorm_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of batchnorm compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    CHECK_GE(a.size(), 5U) << "at least 5 input tensors for batchnorm compute\n";
    Expr A               = a[0];
    Expr Scale           = a[1];
    Expr Bias            = a[2];
    Expr Mean            = a[3];
    Expr Variance        = a[4];
    std::string out_name = UniqName("BatchNorm_output");
    if (FLAGS_cinn_ir_schedule) {
      CHECK_EQ(a.size(), 6U);
      const char *out_name_char = a[5];
      out_name                  = out_name_char;
    }
    CHECK(A.as_tensor());
    CHECK(Scale.as_tensor());
    CHECK(Bias.as_tensor());
    CHECK(Mean.as_tensor());
    CHECK(Variance.as_tensor());
    ir::Tensor out;
    auto tensor_input = A.as_tensor_ref();
    if (tensor_input->shape.size() != 4 && target.arch == Target::Arch::X86) {
      CHECK_EQ(input_layouts.size(), 5U) << "batch_norm_NCHWc's input layout should be 5";
      std::string input_layout = input_layouts[0];
      CHECK_GE(input_layout.size(), 5U);
      CHECK_EQ(input_layout.substr(0, 4), "NCHW");
      CHECK_EQ(tensor_input->shape.size(), 5U);
      out = pe::BatchNorm_NCHWc(tensor_input,
                                Scale.as_tensor_ref(),
                                Bias.as_tensor_ref(),
                                Mean.as_tensor_ref(),
                                Variance.as_tensor_ref(),
                                epsilon,
                                out_name);
    } else {
      out = pe::BatchNorm_NCHW(tensor_input,
                               Scale.as_tensor_ref(),
                               Bias.as_tensor_ref(),
                               Mean.as_tensor_ref(),
                               Variance.as_tensor_ref(),
                               epsilon,
                               out_name);
    }
    auto stages = CreateStages({out});
    *ret        = CINNValuePack{{CINNValue(out), CINNValue(stages)}};
  });

  framework::CINNSchedule batchnorm_schedule([=](lang::Args args, lang::RetValue *ret) {
    if (FLAGS_cinn_ir_schedule) {
      CHECK(!args.empty()) << "The input argument of batchnorm schedule is empty! Please check.\n";
      CINNValuePack arg_pack = args[0];
      CHECK_EQ(arg_pack.size(), 2UL);
      Expr ast_expr = arg_pack[0];
      std::vector<Expr> vec_ast{ast_expr};
      ir::ModuleExpr mod_expr(vec_ast);
      ir::IRSchedule ir_sch(mod_expr);
      if (target.arch == Target::Arch::NVGPU) {
        pe::IRCudaScheduleInjective(ir_sch, output_shapes.front(), target);
      } else if (target.arch == Target::Arch::X86) {
        pe::IRScheduleInjectiveCPU(ir_sch, output_shapes.front(), target);
      }
      std::vector<CINNValue> res;
      res.push_back(arg_pack[0]);
      *ret = CINNValuePack{res};
    } else {
      CHECK(!args.empty()) << "The input argument of batchnorm schedule is empty! Please check.\n";
      CINNValuePack arg_pack = args[0];
      CHECK_EQ(arg_pack.size(), 2UL);
      Expr Out              = arg_pack[0];
      poly::StageMap stages = arg_pack[1];
      CHECK(Out.as_tensor());
      if (target.arch == Target::Arch::NVGPU) {
        pe::CudaScheduleInjective(stages[Out.as_tensor_ref()], output_shapes.front(), target);
      } else if (target.arch == Target::Arch::X86) {
        pe::ScheduleInjectiveCPU(stages[Out.as_tensor_ref()], output_shapes.front(), target);
      }
      *ret = arg_pack;
    }
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  CHECK(out_type.size()) << "Out_type of batchnorm op is empty! Please check.";
  if (out_type[0] == Float(32)) {
    strategy->AddImpl(batchnorm_compute, batchnorm_schedule, "strategy.batchnorm.x86", 1);
  } else {
    LOG(FATAL) << "BatchNorm op with dtype != float32 is not implemented yet!";
  }
  return strategy;
}

std::vector<shape_t> InferShapeForBatchNorm(const std::vector<shape_t> &inputs_shape,
                                            const framework::AttrMapType &attrs) {
  CHECK(!inputs_shape.empty() && !inputs_shape[0].empty()) << "The input's shape size is 0! Please check again.";
  std::vector<shape_t> res{inputs_shape[0]};
  return res;
}

std::vector<Type> InferDtypeForBatchNorm(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  std::vector<Type> res{inputs_type[0]};
  return res;
}

std::vector<std::vector<std::string>> InferLayoutForBatchNorm(const std::vector<framework::shape_t> &input_shapes,
                                                              const std::vector<std::string> &input_layouts,
                                                              const framework::NodeAttr &attrs,
                                                              const Target &target) {
  CHECK_EQ(input_layouts.size(), 5U) << "The input's layouts size is not 5! Please check again.";
  std::string input_layout = input_layouts[0];
  CHECK_GE(input_layout.size(), 4) << "batchnorm's first input layout size should be >= 4";
  return {{input_layout}, input_layouts};
}

std::shared_ptr<OpStrategy> StrategyForPool1d(const framework::NodeAttr &attrs,
                                              const std::vector<ir::Tensor> &inputs,
                                              const std::vector<Type> &out_type,
                                              const std::vector<std::vector<int>> &output_shapes,
                                              const Target &target) {
  framework::CINNCompute pool1d_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of pool1d compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    CHECK(!a.empty()) << "The input tensor of pool1d compute is empty! Please check.\n";
    Expr A = a[0];
    CHECK(A.as_tensor());
    auto attr_store = attrs.attr_store;
    std::vector<int> kernel_size;   // [kernel_w]
    std::vector<int> stride_size;   // [stride_w]
    std::vector<int> padding_size;  // [padding_left, padding_right]
    std::string pool_type   = "max";
    bool ceil_mode          = false;
    bool exclusive          = true;
    std::string data_format = "NCW";
    for (auto &iter : attrs.attr_store) {
      if (iter.first == "kernel_size") {
        kernel_size = absl::get<std::vector<int>>(iter.second);
      } else if (iter.first == "stride_size") {
        stride_size = absl::get<std::vector<int>>(iter.second);
      } else if (iter.first == "padding_size") {
        padding_size = absl::get<std::vector<int>>(iter.second);
      } else if (iter.first == "pool_type") {
        pool_type = absl::get<std::string>(iter.second);
      } else if (iter.first == "ceil_mode") {
        ceil_mode = absl::get<bool>(iter.second);
      } else if (iter.first == "exclusive") {
        exclusive = absl::get<bool>(iter.second);
      } else if (iter.first == "data_format") {
        data_format = absl::get<std::string>(iter.second);
      } else {
        LOG(ERROR) << "Unsupported attr: " << iter.first << std::endl;
      }
    }
    CHECK(!kernel_size.empty()) << "kernel_size for pool1d is empty. Please check.\n";
    CHECK(!stride_size.empty()) << "stride_size for pool1d is empty. Please check.\n";
    CHECK(!padding_size.empty()) << "padding_size for pool1d is empty. Please check.\n";

    auto out = pe::Pool1d(A.as_tensor_ref(),
                          kernel_size,
                          stride_size,
                          padding_size,
                          pool_type,
                          ceil_mode,
                          exclusive,
                          data_format,
                          UniqName("T_Pool1d_out"));

    auto stages = CreateStages(out);
    CHECK(out.size() == 1U || out.size() == 2U) << "The size of pe::Pool1d's output should be 1 or 2.";
    CHECK(!out_type.empty()) << "Output type of Pool1d is empty! Please check.\n";
    std::vector<CINNValue> res;
    for (auto &t : out) {
      res.push_back(CINNValue(Expr(t.get())));
    }
    res.push_back(CINNValue(stages));
    *ret = CINNValuePack{res};
  });

  framework::CINNSchedule pool1d_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of pool1d schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK(arg_pack.size() == 2UL || arg_pack.size() == 3UL);
    Expr Out              = arg_pack[0];
    poly::StageMap stages = arg_pack[arg_pack.size() - 1];
    if (arg_pack.size() == 3UL) {
      Expr input_pad = arg_pack[1];
      CHECK(input_pad.as_tensor());
      stages[input_pad.as_tensor_ref()]->ComputeInline();
    }

    if (target.arch == Target::Arch::NVGPU) {
      CHECK(Out.as_tensor());
      stages[Out.as_tensor_ref()]->Split(1, 2);
      stages[Out.as_tensor_ref()]->Bind(0, "blockIdx.x");
      stages[Out.as_tensor_ref()]->Bind(1, "threadIdx.x");
    }
    *ret = CINNValuePack{{CINNValue(Out), CINNValue(stages)}};
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  strategy->AddImpl(pool1d_compute, pool1d_schedule, "strategy.pool1d.x86", 1);

  return strategy;
}

std::vector<std::vector<int>> InferShapeForPool1d(const std::vector<std::vector<int>> &inputs_shape,
                                                  const framework::AttrMapType &attrs) {
  CHECK(!inputs_shape.empty() && !inputs_shape[0].empty()) << "The input's shape size is 0! Please check again.";
  std::vector<int> kernel_size;   // [kernel_w]
  std::vector<int> stride_size;   // [stride_w]
  std::vector<int> padding_size;  // [padding_left, padding_right]
  std::string pool_type   = "max";
  bool ceil_mode          = false;
  bool exclusive          = true;
  std::string data_format = "NCW";
  for (auto &iter : attrs) {
    if (iter.first == "kernel_size") {
      kernel_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "stride_size") {
      stride_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "padding_size") {
      padding_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "ceil_mode") {
      ceil_mode = absl::get<bool>(iter.second);
    } else if (iter.first == "exclusive") {
      exclusive = absl::get<bool>(iter.second);
    } else if (iter.first == "data_format") {
      data_format = absl::get<std::string>(iter.second);
    }
  }
  CHECK_EQ(kernel_size.size(), 1U) << "kernel size for pool1d should be 1.\n";
  CHECK_EQ(stride_size.size(), 1U) << "stride_size size for pool1d should be 1.\n";
  CHECK_EQ(padding_size.size(), 2U) << "padding_size size for pool1d should be 2.\n";

  std::vector<int> output_shape1 = inputs_shape[0];
  CHECK_EQ(output_shape1.size(), 3U);
  int width_axis = -1;
  if (data_format == "NCW") {
    width_axis = 2;
  } else if (data_format == "NWC") {
    width_axis = 1;
  } else {
    LOG(FATAL) << "unsupported data_format: " << data_format << std::endl;
  }

  if (ceil_mode) {
    output_shape1[width_axis] =
        (inputs_shape[0][width_axis] - kernel_size[0] + padding_size[0] + padding_size[1] + stride_size[0] - 1) /
            stride_size[0] +
        1;
  } else {
    output_shape1[width_axis] =
        (inputs_shape[0][width_axis] - kernel_size[0] + padding_size[0] + padding_size[1]) / stride_size[0] + 1;
  }

  std::vector<std::vector<int>> res{output_shape1};
  return res;
}

std::shared_ptr<OpStrategy> StrategyForPool2d(const framework::NodeAttr &attrs,
                                              const std::vector<ir::Tensor> &inputs,
                                              const std::vector<Type> &out_type,
                                              const std::vector<std::vector<int>> &output_shapes,
                                              const Target &target) {
  auto attr_store = attrs.attr_store;
  std::vector<int> kernel_size;   // [kernel_h, kernel_w]
  std::vector<int> stride_size;   // [stride_h, stride_w]
  std::vector<int> padding_size;  // [padding_top, padding_left, padding_bottom, padding_right]
  std::string pool_type   = "max";
  bool ceil_mode          = false;
  bool exclusive          = true;
  bool global_pooling     = false;
  bool adaptive           = false;
  std::string data_format = "NCHW";
  for (auto &iter : attrs.attr_store) {
    if (iter.first == "kernel_size") {
      kernel_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "stride_size") {
      stride_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "padding_size") {
      padding_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "pool_type") {
      pool_type = absl::get<std::string>(iter.second);
    } else if (iter.first == "ceil_mode") {
      ceil_mode = absl::get<bool>(iter.second);
    } else if (iter.first == "exclusive") {
      exclusive = absl::get<bool>(iter.second);
    } else if (iter.first == "data_format") {
      data_format = absl::get<std::string>(iter.second);
    } else if (iter.first == "global_pooling") {
      global_pooling = absl::get<bool>(iter.second);
    } else if (iter.first == "adaptive") {
      adaptive = absl::get<bool>(iter.second);
    }
  }
  CHECK(!kernel_size.empty()) << "kernel_size for pool2d is empty. Please check.\n";
  CHECK(!stride_size.empty()) << "stride_size for pool2d is empty. Please check.\n";
  CHECK(!padding_size.empty()) << "padding_size for pool2d is empty. Please check.\n";

  CHECK(!inputs.empty()) << "The input tensor of pool2d compute is empty! Please check.\n";
  const ir::Tensor &A_tensor = inputs[0];
  CHECK(A_tensor->shape.size() == 4U || A_tensor->shape.size() == 5U)
      << "pool2d requires tensor's shape_size to be 4 or 5\n";

  if (global_pooling) {
    int height_index = -1;
    int width_index  = -1;
    if (data_format == "NCHW") {
      height_index = 2;
      width_index  = 3;
    } else if (data_format == "NHWC") {
      height_index = 1;
      width_index  = 2;
    } else if (data_format == "AnyLayout") {
      height_index = 2;
      width_index  = 3;
      data_format  = "NCHW";
    } else {
      LOG(FATAL) << "Only support 'NCHW' or 'NHWC' or 'AnyLayout' data_format.\n";
    }
    kernel_size  = {A_tensor->shape[height_index].as_int32(), A_tensor->shape[width_index].as_int32()};
    padding_size = {0, 0, 0, 0};
  }
  if (kernel_size.size() == padding_size.size()) {
    padding_size.insert(padding_size.end(), padding_size.begin(), padding_size.end());
  }

  framework::CINNCompute global_pool2d_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of pool2d compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    Expr A          = a[0];
    CHECK(A.as_tensor());
    ir::Tensor A_tensor = A.as_tensor_ref();
    auto out            = pe::GlobalPool2d(A_tensor, pool_type, UniqName("T_GlobalPool2d_out"));
    CHECK(out.size() == 2U) << "The size of pe::GlobalPool2d's output should be 2.";
    auto stages = CreateStages({A_tensor, out[0], out[1]});
    *ret        = CINNValuePack{{CINNValue(out[0]), CINNValue(out[1]), CINNValue(stages)}};
  });

  framework::CINNSchedule global_pool2d_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of pool2d schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK(arg_pack.size() == 3UL);
    Expr out    = arg_pack[0];
    Expr reduce = arg_pack[1];
    CHECK(out.as_tensor() && reduce.as_tensor());
    poly::StageMap stages = arg_pack[arg_pack.size() - 1];
    pe::GlobalPoolScheduleGPU(stages, {out.as_tensor_ref(), reduce.as_tensor_ref()}, target);
    *ret = CINNValuePack{{CINNValue(out), CINNValue(stages)}};
  });

  framework::CINNCompute pool2d_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of pool2d compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    Expr A          = a[0];
    CHECK(A.as_tensor());
    ir::Tensor A_tensor = A.as_tensor_ref();

    auto out = pe::Pool2d(A_tensor,
                          kernel_size,
                          stride_size,
                          padding_size,
                          pool_type,
                          ceil_mode,
                          exclusive,
                          data_format,
                          adaptive,
                          UniqName("T_Pool2d_out"));

    auto stages = CreateStages({A_tensor});
    CHECK(out.size() == 1U || out.size() == 2U) << "The size of pe::Pool2d's output should be 1 or 2.";
    std::vector<CINNValue> res;
    for (auto &t : out) {
      stages->InsertLazily(t);
      res.push_back(CINNValue(t));
    }
    CHECK(!out_type.empty()) << "Output type of Pool2d is empty! Please check.\n";
    res.push_back(CINNValue(stages));
    *ret = CINNValuePack{res};
  });

  framework::CINNSchedule pool2d_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of pool2d schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK(arg_pack.size() == 2UL || arg_pack.size() == 3UL);
    Expr Out = arg_pack[0];
    CHECK(Out.as_tensor());
    poly::StageMap stages = arg_pack[arg_pack.size() - 1];
    if (arg_pack.size() == 3UL) {
      Expr input_pad = arg_pack[1];
      CHECK(input_pad.as_tensor());
      stages[input_pad.as_tensor_ref()]->ComputeInline();
    }
    ir::Tensor temp_out = Out.as_tensor_ref();
    if (target.arch == Target::Arch::NVGPU) {
      pe::PoolScheduleGPU(stages, temp_out, target);
      arg_pack[arg_pack.size() - 2] = Expr(temp_out);
    }
    *ret = CINNValuePack{{CINNValue(Out), CINNValue(stages)}};
  });

  auto strategy = std::make_shared<framework::OpStrategy>();

  bool use_warp_reduce = false;
  if (global_pooling && data_format == "NCHW" && target.arch == Target::Arch::NVGPU) {
    // TODO 32 may not be the exact number, try also 16 or 8 or other number
    //      we choose 32 to make sure all the threads in a warp has work to do,
    if ((A_tensor->shape[2].as_int32() * A_tensor->shape[3].as_int32()) >= 32) {
      use_warp_reduce = true;
    }
  }
  strategy->AddImpl(pool2d_compute, pool2d_schedule, "strategy.pool2d.x86", 1);
  if (use_warp_reduce) {
    strategy->AddImpl(global_pool2d_compute, global_pool2d_schedule, "strategy.pool2d.gpu.global", 2);
  }

  return strategy;
}

std::vector<std::vector<int>> InferShapeForPool2d(const std::vector<std::vector<int>> &inputs_shape,
                                                  const framework::AttrMapType &attrs) {
  CHECK(inputs_shape[0].size() == 4 || inputs_shape[0].size() == 5)
      << "The input's shape size of pool2d should be 4 or 5! Please check again.";
  std::vector<int> kernel_size;
  std::vector<int> stride_size;
  std::vector<int> padding_size;
  std::string pool_type   = "max";
  bool ceil_mode          = false;
  bool exclusive          = true;
  std::string data_format = "NCHW";
  bool global_pooling     = false;
  bool adaptive           = false;
  for (auto &iter : attrs) {
    if (iter.first == "kernel_size") {
      kernel_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "stride_size") {
      stride_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "padding_size") {
      padding_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "ceil_mode") {
      ceil_mode = absl::get<bool>(iter.second);
    } else if (iter.first == "exclusive") {
      exclusive = absl::get<bool>(iter.second);
    } else if (iter.first == "global_pooling") {
      global_pooling = absl::get<bool>(iter.second);
    } else if (iter.first == "data_format") {
      data_format = absl::get<std::string>(iter.second);
    } else if (iter.first == "adaptive") {
      adaptive = absl::get<bool>(iter.second);
    }
  }
  CHECK_EQ(kernel_size.size(), 2U) << "kernel size for pool2d should be 2.\n";
  CHECK_EQ(stride_size.size(), 2U) << "stride_size size for pool2d should be 2.\n";

  std::vector<int> output_shape1 = inputs_shape[0];
  int height_axis                = -1;
  int width_axis                 = -1;
  if (data_format == "NCHW") {
    height_axis = 2;
    width_axis  = 3;
  } else if (data_format == "NHWC") {
    height_axis = 1;
    width_axis  = 2;
  } else if (data_format == "AnyLayout") {
    height_axis = 2;
    width_axis  = 3;
    data_format = "NCHW";
  } else {
    LOG(ERROR) << "unsupported data_format: " << data_format << std::endl;
  }

  if (global_pooling) {
    kernel_size  = {inputs_shape[0][height_axis], inputs_shape[0][width_axis]};
    padding_size = {0, 0, 0, 0};
  }

  if (ceil_mode) {
    output_shape1[height_axis] =
        (inputs_shape[0][height_axis] - kernel_size[0] + padding_size[0] + padding_size[2] + stride_size[0] - 1) /
            stride_size[0] +
        1;
    output_shape1[width_axis] =
        (inputs_shape[0][width_axis] - kernel_size[1] + padding_size[1] + padding_size[3] + stride_size[1] - 1) /
            stride_size[1] +
        1;
  } else {
    output_shape1[height_axis] =
        (inputs_shape[0][height_axis] - kernel_size[0] + padding_size[0] + padding_size[2]) / stride_size[0] + 1;
    output_shape1[width_axis] =
        (inputs_shape[0][width_axis] - kernel_size[1] + padding_size[1] + padding_size[3]) / stride_size[1] + 1;
  }

  if (adaptive) {
    kernel_size = absl::get<std::vector<int>>(attrs.at("kernel_size"));
    if (kernel_size.size() == 1UL) kernel_size.push_back(kernel_size[0]);
    CHECK(kernel_size.size() >= 2UL) << "In pool2d, kernel_size's size should be >= 2, please check!";
    output_shape1[height_axis] = kernel_size[0];
    output_shape1[width_axis]  = kernel_size[1];
  }
  std::vector<std::vector<int>> res{output_shape1};
  return res;
}

std::shared_ptr<OpStrategy> StrategyForPool3d(const framework::NodeAttr &attrs,
                                              const std::vector<ir::Tensor> &inputs,
                                              const std::vector<Type> &out_type,
                                              const std::vector<std::vector<int>> &output_shapes,
                                              const Target &target) {
  framework::CINNCompute pool3d_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of pool3d compute is empty! Please check.\n";
    CINNValuePack a = args[0];
    CHECK(!a.empty()) << "The input tensor of pool3d compute is empty! Please check.\n";
    Expr A = a[0];
    CHECK(A.as_tensor());
    auto attr_store = attrs.attr_store;
    std::vector<int> kernel_size;  // [kernel_d, kernel_h, kernel_w]
    std::vector<int> stride_size;  // [stride_d, stride_h, stride_w]
    std::vector<int>
        padding_size;  // [padding_front, padding_top, padding_left, padding_back, padding_bottom, padding_right]
    std::string pool_type   = "max";
    bool ceil_mode          = false;
    bool exclusive          = true;
    std::string data_format = "NCDHW";
    for (auto &iter : attrs.attr_store) {
      if (iter.first == "kernel_size") {
        kernel_size = absl::get<std::vector<int>>(iter.second);
      } else if (iter.first == "stride_size") {
        stride_size = absl::get<std::vector<int>>(iter.second);
      } else if (iter.first == "padding_size") {
        padding_size = absl::get<std::vector<int>>(iter.second);
      } else if (iter.first == "pool_type") {
        pool_type = absl::get<std::string>(iter.second);
      } else if (iter.first == "ceil_mode") {
        ceil_mode = absl::get<bool>(iter.second);
      } else if (iter.first == "exclusive") {
        exclusive = absl::get<bool>(iter.second);
      } else if (iter.first == "data_format") {
        data_format = absl::get<std::string>(iter.second);
      } else {
        LOG(ERROR) << "Unsupported attr: " << iter.first << std::endl;
      }
    }
    CHECK(!kernel_size.empty()) << "kernel_size for pool3d is empty. Please check.\n";
    CHECK(!stride_size.empty()) << "stride_size for pool3d is empty. Please check.\n";
    CHECK(!padding_size.empty()) << "padding_size for pool3d is empty. Please check.\n";

    auto out = pe::Pool3d(A.as_tensor_ref(),
                          kernel_size,
                          stride_size,
                          padding_size,
                          pool_type,
                          ceil_mode,
                          exclusive,
                          data_format,
                          UniqName("T_Pool3d_out"));

    auto stages = CreateStages(out);
    CHECK(out.size() == 1U || out.size() == 2U) << "The size of pe::Pool3d's output should be 1 or 2.";
    CHECK(!out_type.empty()) << "Output type of Pool3d is empty! Please check.\n";

    std::vector<CINNValue> res;
    for (auto &t : out) {
      res.push_back(CINNValue(Expr(t.get())));
    }
    res.push_back(CINNValue(stages));
    *ret = CINNValuePack{res};
  });

  framework::CINNSchedule pool3d_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of pool3d schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK(arg_pack.size() == 2UL || arg_pack.size() == 3UL);
    Expr Out              = arg_pack[0];
    poly::StageMap stages = arg_pack[arg_pack.size() - 1];
    if (arg_pack.size() == 3UL) {
      Expr input_pad = arg_pack[1];
      CHECK(input_pad.as_tensor());
      stages[input_pad.as_tensor_ref()]->ComputeInline();
    }
    if (target.arch == Target::Arch::NVGPU) {
      CHECK(Out.as_tensor());
      stages[Out.as_tensor_ref()]->Split(1, 2);
      stages[Out.as_tensor_ref()]->Bind(0, "blockIdx.x");
      stages[Out.as_tensor_ref()]->Bind(1, "threadIdx.x");
    }
    *ret = CINNValuePack{{CINNValue(Out), CINNValue(stages)}};
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  strategy->AddImpl(pool3d_compute, pool3d_schedule, "strategy.pool3d.x86", 1);

  return strategy;
}

std::vector<std::vector<int>> InferShapeForPool3d(const std::vector<std::vector<int>> &inputs_shape,
                                                  const framework::AttrMapType &attrs) {
  CHECK(!inputs_shape.empty() && !inputs_shape[0].empty()) << "The input's shape size is 0! Please check again.";
  std::vector<int> kernel_size;  // [kernel_d, kernel_h, kernel_w]
  std::vector<int> stride_size;  // [stride_d, stride_h, stride_w]
  std::vector<int>
      padding_size;  // [padding_front, padding_top, padding_left, padding_bottom, padding_right, padding_back]
  std::string pool_type   = "max";
  bool ceil_mode          = false;
  bool exclusive          = true;
  std::string data_format = "NCDHW";
  for (auto &iter : attrs) {
    if (iter.first == "kernel_size") {
      kernel_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "stride_size") {
      stride_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "padding_size") {
      padding_size = absl::get<std::vector<int>>(iter.second);
    } else if (iter.first == "ceil_mode") {
      ceil_mode = absl::get<bool>(iter.second);
    } else if (iter.first == "exclusive") {
      exclusive = absl::get<bool>(iter.second);
    } else if (iter.first == "data_format") {
      data_format = absl::get<std::string>(iter.second);
    }
  }

  CHECK_EQ(kernel_size.size(), 3U) << "kernel_size for pool3d should be 3.\n";
  CHECK_EQ(stride_size.size(), 3U) << "stride_size for pool3d should be 3.\n";

  std::vector<int> output_shape1 = inputs_shape[0];
  CHECK_EQ(inputs_shape[0].size(), 5U) << "input_shape size for pool3d should be 5.\n";
  int depth_axis  = -1;
  int height_axis = -1;
  int width_axis  = -1;
  if (data_format == "NCDHW") {
    depth_axis  = 2;
    height_axis = 3;
    width_axis  = 4;
  } else if (data_format == "NDHWC") {
    depth_axis  = 1;
    height_axis = 2;
    width_axis  = 3;
  } else {
    LOG(ERROR) << "unsupported data_format: " << data_format << std::endl;
  }

  if (ceil_mode) {
    output_shape1[depth_axis] =
        (inputs_shape[0][depth_axis] - kernel_size[0] + padding_size[0] + padding_size[3] + stride_size[0] - 1) /
            stride_size[0] +
        1;
    output_shape1[height_axis] =
        (inputs_shape[0][height_axis] - kernel_size[1] + padding_size[1] + padding_size[4] + stride_size[1] - 1) /
            stride_size[1] +
        1;
    output_shape1[width_axis] =
        (inputs_shape[0][width_axis] - kernel_size[2] + padding_size[2] + padding_size[5] + stride_size[2] - 1) /
            stride_size[2] +
        1;
  } else {
    output_shape1[depth_axis] =
        (inputs_shape[0][depth_axis] - kernel_size[0] + padding_size[0] + padding_size[3]) / stride_size[0] + 1;
    output_shape1[height_axis] =
        (inputs_shape[0][height_axis] - kernel_size[1] + padding_size[1] + padding_size[4]) / stride_size[1] + 1;
    output_shape1[width_axis] =
        (inputs_shape[0][width_axis] - kernel_size[2] + padding_size[2] + padding_size[5]) / stride_size[2] + 1;
  }

  std::vector<std::vector<int>> res{output_shape1};
  return res;
}

std::vector<Type> InferDtypeForPool(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  std::vector<Type> res{inputs_type[0]};
  return res;
}

std::vector<std::vector<std::string>> InferLayoutForPool(const std::vector<framework::shape_t> &input_shapes,
                                                         const std::vector<std::string> &input_layouts,
                                                         const framework::NodeAttr &attrs,
                                                         const Target &target) {
  CHECK_EQ(input_layouts.size(), 1U) << "The input's layout size is not 1! Please check again.";
  return {input_layouts, input_layouts};
}

std::shared_ptr<OpStrategy> StrategyForSoftmax(const framework::NodeAttr &attrs,
                                               const std::vector<ir::Tensor> &inputs,
                                               const std::vector<Type> &out_type,
                                               const std::vector<std::vector<int>> &output_shapes,
                                               const Target &target) {
  int axis        = -1;
  bool use_mkldnn = false;
  if (attrs.attr_store.count("axis")) {
    axis = absl::get<int>(attrs.attr_store.at("axis"));
  }
  if (attrs.attr_store.count("use_mkldnn")) {
    use_mkldnn = absl::get<bool>(attrs.attr_store.at("use_mkldnn"));
  }
  framework::CINNCompute softmax_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input arguments of softmax compute is empty! Please check.";
    CINNValuePack a = args[0];
    CHECK(!a.empty()) << "The input tensors of softmax compute is empty! Please check.";
    Expr A_expr = a[0];
    CHECK(A_expr.as_tensor());
    ir::Tensor A = A_expr.as_tensor_ref();
    auto stages  = CreateStages({A});
    int new_axis = axis;
    if (axis == -1) {
      new_axis = A->shape.size() - 1;
    }
    std::vector<ir::Tensor> out;
#ifdef CINN_WITH_MKLDNN
    if (use_mkldnn) {
      out = pe::SoftmaxMKLDNN(A, new_axis, UniqName("Softmax_mkldnn_output"));
    } else {
      out = pe::Softmax(A, new_axis, UniqName("Softmax_output"));
    }
#else
    out = pe::Softmax(A, new_axis, UniqName("Softmax_output"));
#endif
    std::vector<CINNValue> res;
    for (auto &t : out) {
      stages->InsertLazily(t);
      res.push_back(CINNValue(t));
    }
    CHECK_EQ(out.size(), 2U) << "The size of pe::Softmax's output should be 2.";
    CHECK(!out_type.empty()) << "Output type of Softmax is empty! Please check.\n";
    res.push_back(CINNValue(stages));
    *ret = CINNValuePack{res};
  });

  framework::CINNSchedule softmax_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input arguments of softmax schedule is empty! Please check.";
    CINNValuePack arg_pack = args[0];
    CHECK_EQ(arg_pack.size(), 3UL) << "The input tensor's size of softmax schedule is " << arg_pack.size()
                                   << "and it should be equal to 3! Please check.";
    Expr out1             = arg_pack[0];
    Expr out2             = arg_pack[1];
    poly::StageMap stages = arg_pack[2];
    CHECK(out1.as_tensor());
    CHECK(out2.as_tensor());
    ir::Tensor tensor_a = out1.as_tensor_ref();
    ir::Tensor tensor_b = out2.as_tensor_ref();
    if (target.arch == Target::Arch::NVGPU) {
      if (tensor_a->shape.size() > 1) {
        stages[tensor_a]->Split(1, 5);
        stages[tensor_a]->Bind(0, "blockIdx.x");
        stages[tensor_a]->Bind(1, "threadIdx.x");
        int shape_size = tensor_a->shape.size();
        stages[tensor_b]->ComputeAt(stages[tensor_a], shape_size);
      }
    } else if (target.arch == Target::Arch::X86) {
      pe::SoftmaxScheduleCPU(stages, tensor_a, tensor_b, axis);
    }
    *ret = arg_pack;
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  strategy->AddImpl(softmax_compute, softmax_schedule, "strategy.softmax.x86", 1);

  return strategy;
}

std::vector<std::vector<int>> InferShapeForSoftmax(const std::vector<std::vector<int>> &inputs_shape,
                                                   const framework::AttrMapType &attrs) {
  CHECK(!inputs_shape.empty() && !inputs_shape[0].empty()) << "The input's shape size is 0! Please check again.";
  std::vector<std::vector<int>> res{inputs_shape[0], inputs_shape[0]};
  return res;
}

std::vector<Type> InferDtypeForSoftmax(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  std::vector<Type> res{inputs_type[0], inputs_type[0]};
  return res;
}

std::vector<std::vector<std::string>> InferLayoutForSoftmax(const std::vector<framework::shape_t> &input_shapes,
                                                            const std::vector<std::string> &input_layouts,
                                                            const framework::NodeAttr &attrs,
                                                            const Target &target) {
  CHECK_EQ(input_layouts.size(), 1U) << "The input's layout size is not 1! Please check again.";
  if (input_shapes[0].size() > 4) {
    // input tensor needs to be transformed back to NCHW for mkldnn
    return {{"NCHW", "NCHW"}, {"NCHW"}};
  }
  return {{input_layouts[0], input_layouts[0]}, input_layouts};
}

std::shared_ptr<OpStrategy> StrategyForDropoutInfer(const framework::NodeAttr &attrs,
                                                    const std::vector<ir::Tensor> &inputs,
                                                    const std::vector<Type> &out_type,
                                                    const std::vector<std::vector<int>> &output_shapes,
                                                    const Target &target) {
  float dropout_prob                 = 0;
  std::string dropout_implementation = "downgrade_in_infer";
  if (attrs.attr_store.find("dropout_prob") != attrs.attr_store.end()) {
    dropout_prob = absl::get<float>(attrs.attr_store.at("dropout_prob"));
  }
  if (attrs.attr_store.find("dropout_implementation") != attrs.attr_store.end()) {
    dropout_implementation = absl::get<std::string>(attrs.attr_store.at("dropout_implementation"));
  }

  framework::CINNCompute dropout_infer_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input arguments of dropout_infer compute is empty! Please check.";
    CINNValuePack a = args[0];
    CHECK(!a.empty()) << "The input tensors of dropout_infer compute is empty! Please check.";
    Expr A_expr = a[0];
    CHECK(A_expr.as_tensor());
    ir::Tensor A = A_expr.as_tensor_ref();

    auto out    = pe::DropoutInfer(A, dropout_prob, dropout_implementation, UniqName("T_dropout_infer_out"));
    auto stages = CreateStages({A, out});
    *ret        = CINNValuePack{{CINNValue(out), CINNValue(stages)}};
  });

  framework::CINNSchedule dropout_infer_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input arguments of dropout_infer schedule is empty! Please check.";
    CINNValuePack arg_pack = args[0];
    CHECK_EQ(arg_pack.size(), 2UL) << "The input tensor's size of dropout_infer schedule is " << arg_pack.size()
                                   << "and it should be equal to 2! Please check.";
    Expr Out              = arg_pack[0];
    poly::StageMap stages = arg_pack[1];
    CHECK(Out.as_tensor());
    if (target.arch == Target::Arch::NVGPU) {
      pe::CudaScheduleInjective(stages[Out.as_tensor_ref()], output_shapes.front(), target);
    } else {
      pe::ScheduleInjectiveCPU(stages[Out.as_tensor_ref()], output_shapes.front(), target);
    }
    *ret = arg_pack;
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  strategy->AddImpl(dropout_infer_compute, dropout_infer_schedule, "strategy.dropout_infer.x86", 1);

  return strategy;
}

std::vector<std::vector<int>> InferShapeForDropoutInfer(const std::vector<std::vector<int>> &inputs_shape,
                                                        const framework::AttrMapType &attrs) {
  CHECK(!inputs_shape.empty() && !inputs_shape[0].empty()) << "The input's shape size is 0! Please check again.";
  float dropout_prob                 = 0;
  std::string dropout_implementation = "downgrade_in_infer";
  for (auto &iter : attrs) {
    if (iter.first == "dropout_prob") {
      dropout_prob = absl::get<float>(iter.second);
    } else if (iter.first == "dropout_implementation") {
      dropout_implementation = absl::get<std::string>(iter.second);
    } else {
      LOG(ERROR) << "Unsupported attr: " << iter.first << std::endl;
    }
  }

  std::vector<std::vector<int>> res{inputs_shape[0]};
  return res;
}

std::vector<Type> InferDtypeForDropoutInfer(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  std::vector<Type> res{inputs_type[0]};
  return res;
}

std::shared_ptr<OpStrategy> StrategyForSelect(const framework::NodeAttr &attrs,
                                              const std::vector<ir::Tensor> &inputs,
                                              const std::vector<Type> &out_type,
                                              const std::vector<std::vector<int>> &output_shapes,
                                              const Target &target) {
  framework::CINNCompute select_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of select compute is empty! Please check.\n";
    CINNValuePack arg = args[0];
    CHECK(arg.size() >= 3) << "at least three input tensor for select compute\n";
    Expr condition   = arg[0];
    Expr true_value  = arg[1];
    Expr false_value = arg[2];
    CHECK(condition.as_tensor());
    CHECK(true_value.as_tensor());
    CHECK(false_value.as_tensor());
    auto out = pe::Select(
        condition.as_tensor_ref(), true_value.as_tensor_ref(), false_value.as_tensor_ref(), UniqName("Select_output"));
    auto stages =
        CreateStages({condition.as_tensor_ref(), true_value.as_tensor_ref(), false_value.as_tensor_ref(), out});
    *ret = CINNValuePack{{CINNValue(out), CINNValue(stages)}};
  });

  framework::CINNSchedule select_schedule([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input argument of select schedule is empty! Please check.\n";
    CINNValuePack arg_pack = args[0];
    CHECK_EQ(arg_pack.size(), 2UL);
    Expr out              = arg_pack[0];
    poly::StageMap stages = arg_pack[1];
    CHECK(out.as_tensor());
    CHECK_GE(output_shapes.size(), 1);
    if (target.arch == Target::Arch::NVGPU) {
      pe::CudaScheduleInjective(stages[out.as_tensor_ref()], output_shapes[0], target);
    } else if (target.arch == Target::Arch::X86) {
      pe::ScheduleInjectiveCPU(stages[out.as_tensor_ref()], output_shapes[0], target, false);
    }
    *ret = arg_pack;
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  CHECK(out_type.size()) << "Out_type of select op is empty! Please check.";
  if (out_type[0] == Float(32)) {
    strategy->AddImpl(select_compute, select_schedule, "strategy.select.x86", 1);
  } else {
    LOG(FATAL) << "Select op with dtype != float32 is not implemented yet!";
  }
  return strategy;
}

std::vector<framework::shape_t> InferShapeForSelect(const std::vector<framework::shape_t> &inputs_shape,
                                                    const framework::AttrMapType &attrs) {
  CHECK_GE(inputs_shape.size(), 3) << "The input's shape size is 0! Please check again.";
  CHECK(inputs_shape[0].size() == inputs_shape[1].size() && inputs_shape[1].size() == inputs_shape[2].size())
      << "input tensors n_dim is not equal!";
  CHECK(inputs_shape[0] == inputs_shape[1] && inputs_shape[1] == inputs_shape[2])
      << "input tensor shapes is not equal!";
  std::vector<framework::shape_t> res{inputs_shape[0]};
  return res;
}

std::vector<Type> InferDtypeForSelect(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK_GE(inputs_type.size(), 3) << "The input's type size is less than three! Please check again.";
  CHECK(inputs_type[0].is_bool()) << "The condition tensor type should be bool";
  std::vector<Type> res{inputs_type[1]};
  return res;
}

std::vector<std::vector<std::string>> InferLayoutForUnary(const std::vector<framework::shape_t> &input_shapes,
                                                          const std::vector<std::string> &input_layouts,
                                                          const framework::NodeAttr &attrs,
                                                          const Target &target) {
  CHECK_EQ(input_layouts.size(), 1U) << "The input's layout size is not 1! Please check again.";
  return {input_layouts, input_layouts};
}

// batch norm train
std::vector<framework::shape_t> InferShapeForBatchNormTrain(const std::vector<framework::shape_t> &inputs_shape,
                                                            const framework::AttrMapType &attrs) {
  CHECK_EQ(inputs_shape.size(), 5U) << "The input's layout size is not 5! Please check again.";
  std::string data_layout = "";
  if (attrs.find("data_layout") != attrs.end()) {
    data_layout = absl::get<std::string>(attrs.at("data_layout"));
  } else {
    LOG(FATAL) << "data_layout is not found, please check!";
  }

  CHECK_EQ(inputs_shape[0].size(), 4) << "x dimension size is not required!";
  CHECK_EQ(inputs_shape[1].size(), 1) << "scale dimension size is not required!";
  CHECK_EQ(inputs_shape[2].size(), 1) << "bias dimension size is not required!";
  CHECK_EQ(inputs_shape[3].size(), 1) << "moving_mean dimension size is not required!";
  CHECK_EQ(inputs_shape[4].size(), 1) << "moving_variance dimension size is not required!";

  if (data_layout == "NCHW") {
    CHECK_EQ(inputs_shape[0][1], inputs_shape[1][0]) << "x and scale dimension is not equal!";
    CHECK_EQ(inputs_shape[0][1], inputs_shape[2][0]) << "x and bias dimension size is not equal!";
    CHECK_EQ(inputs_shape[0][1], inputs_shape[3][0]) << "x and moveing_mean dimension size is not equal!";
    CHECK_EQ(inputs_shape[0][1], inputs_shape[4][0]) << "x and moveing_variance dimension size is not equal!";
  } else if (data_layout == "NHWC") {
    CHECK_EQ(inputs_shape[0][3], inputs_shape[1][0]) << "x and scale dimension is not equal!";
    CHECK_EQ(inputs_shape[0][3], inputs_shape[2][0]) << "x and bias dimension size is not equal!";
    CHECK_EQ(inputs_shape[0][3], inputs_shape[3][0]) << "x and moveing_mean dimension size is not equal!";
    CHECK_EQ(inputs_shape[0][3], inputs_shape[4][0]) << "x and moveing_variance dimension size is not equal!";
  } else {
    LOG(FATAL) << "data_layout " << data_layout << " is not support!";
  }

  return {inputs_shape[0], inputs_shape[1], inputs_shape[1], inputs_shape[1], inputs_shape[1]};
}

std::vector<Type> InferDtypeForBatchNormTrain(const std::vector<Type> &inputs_type,
                                              const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  return {inputs_type[0], inputs_type[0], inputs_type[0], inputs_type[0], inputs_type[0]};
}

std::shared_ptr<OpStrategy> StrategyForGradOp(const framework::NodeAttr &attrs,
                                              const std::vector<ir::Tensor> &inputs,
                                              const std::vector<Type> &out_type,
                                              const std::vector<std::vector<int>> &output_shapes,
                                              const Target &target) {
  LOG(FATAL)
      << "Gradient operator will be decomposed into several primitive operators. Please Use Decomposer Program Pass.";
}

// batch norm grad
std::vector<framework::shape_t> InferShapeForBatchNormGrad(const std::vector<framework::shape_t> &inputs_shape,
                                                           const framework::AttrMapType &attrs) {
  CHECK_EQ(inputs_shape.size(), 5U) << "The input's layout size is not 5! Please check again.";
  std::string data_layout = "";
  if (attrs.find("data_layout") != attrs.end()) {
    data_layout = absl::get<std::string>(attrs.at("data_layout"));
  } else {
    LOG(FATAL) << "data_layout is not found, please check!";
  }

  CHECK_EQ(inputs_shape[0].size(), 4) << "dy dimension size is not required!";
  CHECK_EQ(inputs_shape[1].size(), 4) << "x dimension size is not required!";
  CHECK_EQ(inputs_shape[2].size(), 1) << "scale dimension size is not required!";
  CHECK_EQ(inputs_shape[3].size(), 1) << "save_mean dimension size is not required!";
  CHECK_EQ(inputs_shape[4].size(), 1) << "save_variance dimension size is not required!";

  CHECK(inputs_shape[0] == inputs_shape[1]) << "dy and x shape is not equal!";
  if (data_layout == "NCHW") {
    CHECK_EQ(inputs_shape[0][1], inputs_shape[2][0]) << "dy and bias dimension size is not equal!";
    CHECK_EQ(inputs_shape[0][1], inputs_shape[3][0]) << "dy and moveing_mean dimension size is not equal!";
    CHECK_EQ(inputs_shape[0][1], inputs_shape[4][0]) << "dy and moveing_variance dimension size is not equal!";
  } else if (data_layout == "NHWC") {
    CHECK_EQ(inputs_shape[0][3], inputs_shape[2][0]) << "dy and bias dimension size is not equal!";
    CHECK_EQ(inputs_shape[0][3], inputs_shape[3][0]) << "dy and moveing_mean dimension size is not equal!";
    CHECK_EQ(inputs_shape[0][3], inputs_shape[4][0]) << "dy and moveing_variance dimension size is not equal!";
  } else {
    LOG(FATAL) << "data_layout " << data_layout << " is not support!";
  }

  return {inputs_shape[0], inputs_shape[2], inputs_shape[2]};
}

std::vector<Type> InferDtypeForBatchNormGrad(const std::vector<Type> &inputs_type,
                                             const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  return {inputs_type[0], inputs_type[0], inputs_type[0]};
}

// conv2d grad
std::vector<framework::shape_t> InferShapeForConv2dGrad(const std::vector<framework::shape_t> &inputs_shape,
                                                        const framework::AttrMapType &attrs) {
  CHECK_EQ(inputs_shape.size(), 3U) << "The input's layout size is not 3! Please check again.";
  CHECK_EQ(inputs_shape[0].size(), 4U) << "Dy shape is not 4, Please check again.";
  CHECK_EQ(inputs_shape[1].size(), 4U) << "Dy shape is not 4, Please check again.";
  CHECK_EQ(inputs_shape[2].size(), 4U) << "Dy shape is not 4, Please check again.";
  return {inputs_shape[1], inputs_shape[2]};
}

std::vector<Type> InferDtypeForConv2dGrad(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK(!inputs_type.empty()) << "The input's type size is 0! Please check again.";
  return {inputs_type[0], inputs_type[0]};
}

}  // namespace op
}  // namespace hlir
}  // namespace cinn

CINN_REGISTER_HELPER(nn_ops) {
  CINN_REGISTER_OP(relu)
      .describe("Output 0 for each input element < 0. Output itself for each input element >= 0.")
      .set_num_inputs(1)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForRelu)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForRelu))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForRelu))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForUnary))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kElemWise)
      .set_support_level(4);

  CINN_REGISTER_OP(relu6)
      .describe("Output 0 for each input element < 0. Output itself for each input element >= 0 and <=6.")
      .set_num_inputs(1)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForRelu6)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForRelu))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForRelu))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForUnary))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kElemWise)
      .set_support_level(4);

  CINN_REGISTER_OP(conv2d)
      .describe("Do a 2-D convolution with an NCHW/NHWC layout.")
      .set_num_inputs(2)  // here we consider filter as another input
      .set_num_outputs(4)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForConv2d)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForConv2d))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForConv2d))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForConv2d))
#endif
#ifdef CINN_WITH_CUDNN
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kOpaque)
#else
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern",
                                                      cinn::hlir::framework::OpPatternKind::kOutEWiseFusable)
#endif
      .set_support_level(4);

  CINN_REGISTER_OP(conv2d_NCHWc)
      .describe("Do a 2-D convolution with an NCHWc layout. Input is 5D tensor and weight is 6D tensor.")
      .set_num_inputs(2)  // here we consider filter as another input
      .set_num_outputs(3)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForConv2dNCHWc)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForConv2dNCHWc))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForConv2dNCHWc))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForConv2dNCHWc))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern",
                                                      cinn::hlir::framework::OpPatternKind::kOutEWiseFusable)
      .set_support_level(4);

  CINN_REGISTER_OP(depthwise_conv2d)
      .describe("Do a 2-D depthwise convolution with an NCHW/NHWC layout.")
      .set_num_inputs(2)  // here we consider filter as another input
      .set_num_outputs(4)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForDepthwiseConv2d)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForConv2d))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForConv2d))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForConv2d))
#endif
#ifdef CINN_WITH_CUDNN
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kOpaque)
#else
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern",
                                                      cinn::hlir::framework::OpPatternKind::kOutEWiseFusable)
#endif
      .set_support_level(4);

  CINN_REGISTER_OP(batchnorm)
      .describe("Can be used as a normalizer function for convolution or fully_connected operations.")
      .set_num_inputs(5)  // here we consider batchnorm's 4 attrs(mean, variance, scale, bias) as other 4 inputs
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForBatchNorm)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForBatchNorm))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForBatchNorm))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForBatchNorm))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kElemWise)
      .set_support_level(4);

  CINN_REGISTER_OP(pool1d)
      .describe("Do pooling on the width dimension of the input tensor.")
      .set_num_inputs(1)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForPool1d)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForPool1d))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForPool))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForPool))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kOpaque)
      .set_support_level(4);

  CINN_REGISTER_OP(pool2d)
      .describe("Do pooling on the height and width dimension of the input tensor.")
      .set_num_inputs(1)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForPool2d)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForPool2d))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForPool))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForPool))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kOpaque)
      .set_support_level(4);

  CINN_REGISTER_OP(pool3d)
      .describe("Do pooling on the depth, height and width dimension of the input tensor.")
      .set_num_inputs(1)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForPool3d)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForPool3d))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForPool))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForPool))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kOpaque)
      .set_support_level(4);

  CINN_REGISTER_OP(softmax)
      .describe("This operator implements the softmax layer")
      .set_num_inputs(1)
      .set_num_outputs(2)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForSoftmax)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForSoftmax))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForSoftmax))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForSoftmax))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kOpaque)
      .set_support_level(4);

  CINN_REGISTER_OP(dropout_infer)
      .describe("Downgrade the outcome at inference or keep the same.")
      .set_num_inputs(1)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForDropoutInfer)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForDropoutInfer))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForDropoutInfer))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForUnary))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kOpaque)
      .set_support_level(4);

  CINN_REGISTER_OP(select)
      .describe("This operator implements the meta op 'Select'.")
      .set_num_inputs(3)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForSelect)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForSelect))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForSelect))
#ifndef CINN_WITH_CUDA
      .set_attr("inferlayout", MakeOpFunction(cinn::hlir::op::InferLayoutForUnary))
#endif
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kElemWise)
      .set_support_level(4);

  return true;
}

CINN_REGISTER_HELPER(nn_grad_ops) {
  CINN_REGISTER_OP(relu_grad)
      .describe("The gradient of relu.")
      .set_num_inputs(2)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForGradOp)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForRelu))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForRelu))
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kElemWise);

  CINN_REGISTER_OP(batch_norm_train)
      .describe("This operator implements the batch normalization training forward.")
      .set_num_inputs(5)
      .set_num_outputs(5)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForBatchNormTrain))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForBatchNormTrain))
      .set_support_level(4);

  CINN_REGISTER_OP(batch_norm_grad)
      .describe("This operator implements the batch normalization backward.")
      .set_num_inputs(5)
      .set_num_outputs(3)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForBatchNormGrad))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForBatchNormGrad))
      .set_support_level(4);

  CINN_REGISTER_OP(conv2d_grad)
      .describe("This operator implements the convolution backward.")
      .set_num_inputs(3)
      .set_num_outputs(2)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForConv2dGrad))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForConv2dGrad))
      .set_support_level(4);

  return true;
}
