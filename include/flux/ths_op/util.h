//===- util.h ----------------------------------------------------- C++ ---===//
//
// Copyright 2025 ByteDance Ltd. and/or its affiliates. All rights reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "flux/flux.h"

#define CHECK_TYPE(x, st) FLUX_CHECK_EQ(x.scalar_type(), st) << "Inconsistency type of Tensor " #x
#define CHECK_CPU(x) FLUX_CHECK(x.is_cpu()) << #x << " must be a cpu tensor"
#define CHECK_CUDA(x) FLUX_CHECK(x.is_cuda()) << #x << " must be a CUDA tensor"
#define CHECK_CONTIGUOUS(x) FLUX_CHECK(x.is_contiguous()) << #x << " must be contiguous"
#define CHECK_INPUT(x, st) \
  CHECK_CUDA(x);           \
  CHECK_CONTIGUOUS(x);     \
  CHECK_TYPE(x, st)
#define CHECK_INPUT_LOOSE(x) \
  CHECK_CUDA(x);             \
  CHECK_CONTIGUOUS(x)
#define CHECK_NDIM(x, ndim) FLUX_CHECK_EQ((x).dim(), ndim) << "ndim check failed"
#define CHECK_DIM(x, dim, sz) FLUX_CHECK_EQ(x.size(dim), sz)
#define CHECK_1D(x, dim0) \
  CHECK_NDIM(x, 1);       \
  CHECK_DIM(x, 0, (dim0))
#define CHECK_2D(x, dim0, dim1) \
  CHECK_NDIM(x, 2);             \
  CHECK_DIM(x, 0, (dim0));      \
  CHECK_DIM(x, 1, (dim1))
#define CHECK_3D(x, dim0, dim1, dim2) \
  CHECK_NDIM(x, 3);                   \
  CHECK_DIM(x, 0, (dim0));            \
  CHECK_DIM(x, 1, (dim1));            \
  CHECK_DIM(x, 2, (dim2))
#define CHECK_DIV(x, y) TORCH_CHECK(x % y == 0, #x, " % ", #y, " != 0")
