#pragma once

/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <memory>

#include "src/Check.h"
#include "src/CudaUtils.h"
#include "src/common.h"
#include "nvcomp/snappy.h"
#include "nvcomp_common_deps/hlif_shared_types.h"
#include "src/highlevel/SnappyHlifKernels.h"
#include "BatchManager.hpp"

namespace nvcomp {

struct SnappyFormatSpecHeader {
  // Empty for now
};

struct SnappyBatchManager : BatchManager<SnappyFormatSpecHeader> {
private:
  SnappyFormatSpecHeader* format_spec;

public:
  SnappyBatchManager(size_t uncomp_chunk_size, cudaStream_t user_stream = 0, int device_id = 0)
    : BatchManager(uncomp_chunk_size, user_stream, device_id),      
      format_spec()
  {
    gpuErrchk(cudaHostAlloc(&format_spec, sizeof(SnappyFormatSpecHeader), cudaHostAllocDefault));

    max_comp_chunk_size = compute_max_compressed_chunk_size();    
    
    finish_init();
  }

  virtual ~SnappyBatchManager() 
  {
    gpuErrchk(cudaFreeHost(format_spec));
  }

  SnappyBatchManager& operator=(const SnappyBatchManager&) = delete;     
  SnappyBatchManager(const SnappyBatchManager&) = delete;     

  size_t compute_max_compressed_chunk_size() final override 
  {
    size_t max_comp_chunk_size;
    nvcompBatchedSnappyCompressGetMaxOutputChunkSize(
        uncomp_chunk_size, nvcompBatchedSnappyDefaultOpts, &max_comp_chunk_size);
    return max_comp_chunk_size;
  }

  uint32_t compute_compression_max_block_occupancy() final override 
  {
    return snappyHlifCompMaxBlockOccupancy(device_id);
  }

  uint32_t compute_decompression_max_block_occupancy() final override 
  {
    return snappyHlifDecompMaxBlockOccupancy(device_id); 
  }  

  SnappyFormatSpecHeader* get_format_header() final override 
  {
    return format_spec;
  }

  void do_batch_compress(
      CommonHeader* common_header,
      const uint8_t* decomp_buffer,
      const size_t decomp_buffer_size,
      uint8_t* comp_data_buffer,
      const uint32_t num_chunks,
      size_t* comp_chunk_offsets,
      size_t* comp_chunk_sizes,
      nvcompStatus_t* output_status) final override
  {
    snappyHlifBatchCompress(
        common_header,
        decomp_buffer,
        decomp_buffer_size,
        comp_data_buffer,
        scratch_buffer,
        uncomp_chunk_size,
        &common_header->comp_data_size,
        ix_chunk,
        num_chunks,
        max_comp_chunk_size,
        comp_chunk_offsets,
        comp_chunk_sizes,
        max_comp_ctas,
        user_stream,
        output_status);
  }

  void do_batch_decompress(
      const uint8_t* comp_data_buffer,
      uint8_t* decomp_buffer,
      const uint32_t num_chunks,
      const size_t* comp_chunk_offsets,
      const size_t* comp_chunk_sizes,
      nvcompStatus_t* output_status) final override
  {        
    snappyHlifBatchDecompress(
        comp_data_buffer,
        decomp_buffer,
        uncomp_chunk_size,
        ix_chunk,
        num_chunks,
        comp_chunk_offsets,
        comp_chunk_sizes,
        max_decomp_ctas,
        user_stream,
        output_status);
  }
};

} // namespace nvcomp