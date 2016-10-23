#ifndef __PIDX_INC_H
#define __PIDX_INC_H

#include "PIDX_define.h"

#include <byteswap.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>

#if PIDX_HAVE_MPI
  #include <mpi.h>
#else
  #include <sys/time.h>
#endif

#if PIDX_HAVE_ZFP
  #include <zfp.h>  
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "./utils/PIDX_error_codes.h"
#include "./utils/PIDX_point.h"
#include "./utils/PIDX_utils.h"
#include "./utils/PIDX_file_name.h"
#include "./utils/PIDX_file_access_modes.h"

#include "./comm/PIDX_comm.h"

#include "./data_handle/PIDX_data_layout.h"
#include "./data_handle/PIDX_blocks.h"
#include "./data_handle/PIDX_idx_data_structs.h"

#include "./core/PIDX_header/PIDX_header_io.h"
#include "./core/PIDX_rst/PIDX_rst.h"
#include "./core/PIDX_multi_patch_rst/PIDX_multi_patch_rst.h"
#include "./core/PIDX_hz/PIDX_hz_encode.h"
#include "./core/PIDX_block_rst/PIDX_block_restructure.h"
#include "./core/PIDX_cmp/PIDX_compression.h"
#include "./core/PIDX_agg/PIDX_agg.h"
#include "./core/PIDX_file_io/PIDX_file_io.h"

#include "./io/PIDX_io.h"

#ifdef __cplusplus
}
#endif

#endif
