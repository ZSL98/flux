// clang-format off
// copyed from cutlass/include/cutlass/gemm/kernel/grouped_problem_visitor.h
/***************************************************************************************************
 * Copyright (c) 2017 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************************************/
#pragma once

#include "cutlass/cutlass.h"
#include "cutlass/gemm/gemm.h"
#include "cutlass/matrix_coord.h"
#include "cutlass/gemm/kernel/grouped_problem_visitor.h"
#include "cutlass/gemm/kernel/gemm_grouped_problem_visitor.h"

namespace cutlass {
namespace gemm {
namespace kernel {
/////////////////////////////////////////////////////////////////////////////////////////////////
// Precomputes schedule on host and prefetches into shared memory
//
template <typename ProblemSizeHelper, typename ThreadblockShape, int PrefetchTileCount, int ThreadCount>
struct AGScatterGroupedProblemVisitor
    : public BaseGroupedProblemVisitor<ProblemSizeHelper, ThreadblockShape>
{
  static_assert(
      PrefetchTileCount > 0,
      "GroupedProblemVisitor with GroupScheduleMode `kHostPrecompute` currently requires prefetching to shared memory");

  using Base = BaseGroupedProblemVisitor<ProblemSizeHelper, ThreadblockShape>;
  using Params = typename Base::Params;
  using ProblemInfo = typename Base::ProblemInfo;
  // skip host_precompute in initialize. host_precompute need extra arguments, so we do this outside ininitialize and
  // just use params.workspace as already computed problem_info_ptr.
  static bool const kRequiresPrecomputation = false;

  static int const kPrefetchTileCount = PrefetchTileCount;
  static int const kThreadCount = ThreadCount;

  struct SharedStorage
  {
    // Sequence of problem IDs and starting tiles to compute
    cutlass::Array<ProblemInfo, kPrefetchTileCount> prefetched_problems;
  };

  int32_t tiles_computed;
  int32_t iterations_per_block;
  int32_t block_load_start;
  SharedStorage &shared_storage;
  ProblemInfo const * problem_info_ptr;
  int problem_tile_idx;

  //
  // Methods
  //
  CUTLASS_DEVICE
  AGScatterGroupedProblemVisitor(Params const &params_, SharedStorage &shared_storage_, int32_t block_idx)
      : Base(params_, block_idx), tiles_computed(0), shared_storage(shared_storage_),
        problem_info_ptr(reinterpret_cast<ProblemInfo const *>(params_.workspace)) {
    iterations_per_block = (params_.tile_count - 1 + gridDim.x) / gridDim.x;
    block_load_start = iterations_per_block * block_idx;
    // Start prefetching the first set of tiles to compute
    prefetch_tiles();
  }

  CUTLASS_HOST_DEVICE
  int32_t threadblock_idx() const {
    return this->problem_tile_idx;
  }

  CUTLASS_DEVICE
  bool next_tile() {
    if (this->tile_idx >= this->params.tile_count) {
      return false;
    }

    int32_t prefetch_idx = (tiles_computed % kPrefetchTileCount);
    if (prefetch_idx == 0) {
      // Ensure all previous stores to shared memory have been completed
      __syncthreads();
    }

    auto problem_info = shared_storage.prefetched_problems[prefetch_idx];
    ++tiles_computed;

    if ((tiles_computed % kPrefetchTileCount) == 0) {
      // Begin prefetching next set of tiles. Synchronize first to ensure that
      // we don't overwrite the current buffer while someone else is using it.
      __syncthreads();
      prefetch_tiles();
    }

    this->problem_idx = problem_info.problem_idx;
    this->problem_tile_idx = problem_info.problem_start;

    return true;
  }

  static size_t get_workspace_size(const cutlass::gemm::GemmCoord * host_problem_sizes_ptr,
                                   int32_t problem_count,
                                   int32_t block_count) {
    return 0;
  }
#if !defined(__CUDACC_RTC__)
  static void host_precompute(const cutlass::gemm::GemmCoord * host_problem_sizes_ptr,
                              int32_t problem_count,
                              int32_t block_count,
                              void * host_workspace_ptr) {}
#endif
private:
  CUTLASS_DEVICE
  void prefetch_tiles() {
    CUTLASS_PRAGMA_UNROLL
    for (int32_t i = 0; i < kPrefetchTileCount; i += kThreadCount) {
      int32_t offset = threadIdx.x + i;
      if (offset < kPrefetchTileCount && (tiles_computed + offset < iterations_per_block)) {
        shared_storage.prefetched_problems[offset] = problem_info_ptr[block_load_start + tiles_computed + offset];
      }
    }
  }
};

/// Visitor class to abstract away the algorithm for iterating over tiles
template <typename ThreadblockShape,
          GroupScheduleMode GroupScheduleMode_,
          int PrefetchTileCount,
          int ThreadCount,
          bool Transposed = false>
struct AGScatterGemmGroupedProblemVisitor
    : public AGScatterGroupedProblemVisitor<detail::GemmGroupedProblemSizeHelper<ThreadblockShape, Transposed>,
                                            ThreadblockShape,
                                            PrefetchTileCount,
                                            ThreadCount>
{
  static bool const kTransposed = Transposed;

  using ProblemSizeHelper = detail::GemmGroupedProblemSizeHelper<ThreadblockShape, Transposed>;
  using Base = AGScatterGroupedProblemVisitor<ProblemSizeHelper, ThreadblockShape, PrefetchTileCount, ThreadCount>;
  using Params = typename Base::Params;
  using SharedStorage = typename Base::SharedStorage;

  //
  // Methods
  //
  CUTLASS_DEVICE
  AGScatterGemmGroupedProblemVisitor(
    Params const &params_,
    SharedStorage &shared_storage_,
    int32_t block_idx
  ): Base (params_, shared_storage_, block_idx)
  {}
};

} // namespace kernel
} // namespace gemm
} // namespace cutlass
// clang-format on
