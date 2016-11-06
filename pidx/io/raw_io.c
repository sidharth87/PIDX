#include "../PIDX_inc.h"
#include "restructure.h"
#include "partition.h"
#include "local_buffer.h"
#include "headers.h"
#include "blocks.h"
#include "hz_buffer.h"
#include "agg_io.h"
#include "initialize.h"
#include "timming.h"

static PIDX_return_code group_meta_data_init(PIDX_io file, int gi, int svi, int evi, int mode);

/// Raw Write Steps
/********************************************************
*  Step 0: Setup Group related meta-data                *
*                                                       *
*  Step 1: Setup Restructuring Phase                    *
*  Step 2: Perform data Restructuring                   *
*  Step 3: Perform actual file IO                       *
*  Step 4: cleanup for Steps 1                          *
*********************************************************/

PIDX_return_code PIDX_raw_write(PIDX_io file, int gi, int svi, int evi)
{  
  int si = 0, ei = 0;
  PIDX_return_code ret;

  // Step 0
  ret = group_meta_data_init(file, gi, svi, evi, PIDX_WRITE);
  if (ret != PIDX_success)
  {
    fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

  file->idx->variable_pipe_length = file->idx->variable_count;
  for (si = svi; si < evi; si = si + (file->idx->variable_pipe_length + 1))
  {
    ei = ((si + file->idx->variable_pipe_length) >= (evi)) ? (evi - 1) : (si + file->idx->variable_pipe_length);
    file->idx->variable_grp[gi]->variable_tracker[si] = 1;

    // Step 1: Setup restructuring buffers
    ret = restructure_setup(file, gi, si, ei);
    if (ret != PIDX_success)
    {
      fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }

    // Step 2: Perform data restructuring
    ret = restructure(file, PIDX_WRITE);
    if (ret != PIDX_success)
    {
      fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }

    // Step 3: Write out restructured data
    ret = restructure_io(file, PIDX_WRITE);
    if (ret != PIDX_success)
    {
      fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }

    // Step 4: Cleanup all buffers and ids
    ret = restructure_cleanup(file, gi);
    if (ret != PIDX_success)
    {
      fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }
  }

  return PIDX_success;
}



/// Raw Read Steps
/********************************************************
*  Step 0: Setup Group related meta-data                *
*                                                       *
*  Step 1: Setup Restructuring Phase                    *
*  Step 2: Perform actual file IO                       *
*  Step 3: Perform data Restructuring                   *
*  Step 4: cleanup for Steps 1                          *
*********************************************************/

PIDX_return_code PIDX_raw_read(PIDX_io file, int gi, int svi, int evi)
{
  int si = 0, ei = 0;
  PIDX_return_code ret;

  // Step 0
  ret = group_meta_data_init(file, gi, svi, evi, PIDX_READ);
  if (ret != PIDX_success)
  {
    fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

  int nprocs = 1;
  int rst_case_type = 0;
  PIDX_variable_group var_grp = file->idx->variable_grp[gi];
  PIDX_variable var0 = var_grp->variable[svi];
  int patch_count = var0->sim_patch_count;
  int max_patch_count = 0;
  int pipe_length = file->idx->variable_count;

  MPI_Allreduce(&patch_count, &max_patch_count, 1, MPI_INT, MPI_MAX, file->comm);
  if (max_patch_count > 1)
    rst_case_type = 1;
  MPI_Comm_size(file->comm, &nprocs);

  if (file->idx_d->data_core_count == nprocs && rst_case_type == 0)
  {
    for (si = svi; si < evi; si = si + (pipe_length + 1))
    {
      ei = ((si + pipe_length) >= (evi)) ? (evi - 1) : (si + pipe_length);

      // Step 1: Setup restructuring buffers
      ret = restructure_setup(file, gi, si, ei);
      if (ret != PIDX_success)
      {
        fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }

      // Step 2: Write out restructured data
      ret = restructure_io(file, PIDX_READ);
      if (ret != PIDX_success)
      {
        fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }

      // Step 3: Perform data restructuring
      ret = restructure(file, PIDX_READ);
      if (ret != PIDX_success)
      {
        fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }

      // Step 4: Cleanup all buffers and ids
      ret = restructure_cleanup(file, gi);
      if (ret != PIDX_success)
      {
        fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }
    }
  }
  else
  {
    for (si = svi; si < evi; si = si + (pipe_length + 1))
    {
      ei = ((si + pipe_length) >= (evi)) ? (evi - 1) : (si + pipe_length);
      ret = restructure_forced_read(file, svi, evi);
      if (ret != PIDX_success)
      {
        fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }
    }
  }

  return PIDX_success;
}


static PIDX_return_code group_meta_data_init(PIDX_io file, int gi, int svi, int evi, int mode)
{
  int ret;
  PIDX_time time = file->idx_d->time;

  time->idx_init_start = MPI_Wtime();
  ret = raw_init(file);
  if (ret != PIDX_success)
  {
    fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }
  time->idx_init_end = MPI_Wtime();


  time->idx_set_reg_box_start = MPI_Wtime();
  ret = set_rst_box_size(file, gi, svi);
  if (ret != PIDX_success)
  {
    fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

  if (file->one_time_initializations == 0)
  {
    PIDX_init_timming_buffers2(file->idx_d->time, file->idx->variable_count, file->idx_d->perm_layout_count);
    file->one_time_initializations = 1;
  }
  time->idx_set_reg_box_end = MPI_Wtime();


  if (mode == PIDX_WRITE)
  {
    time->idx_header_io_start = PIDX_get_time();
    // Creates the file heirarchy and writes the header info for all binary files
    ret = init_raw_headers_layout(file, gi, svi, evi, file->idx->filename);
    if (ret != PIDX_success)
    {
      fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }
    time->idx_header_io_end = PIDX_get_time();
  }

  return PIDX_success;
}
