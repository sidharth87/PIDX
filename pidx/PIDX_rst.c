/*****************************************************
 **  PIDX Parallel I/O Library                      **
 **  Copyright (c) 2010-2014 University of Utah     **
 **  Scientific Computing and Imaging Institute     **
 **  72 S Central Campus Drive, Room 3750           **
 **  Salt Lake City, UT 84112                       **
 **                                                 **
 **  PIDX is licensed under the Creative Commons    **
 **  Attribution-NonCommercial-NoDerivatives 4.0    **
 **  International License. See LICENSE.md.         **
 **                                                 **
 **  For information about this project see:        **
 **  http://www.cedmav.com/pidx                     **
 **  or contact: pascucci@sci.utah.edu              **
 **  For support: PIDX-support@visus.net            **
 **                                                 **
 *****************************************************/

/**
 * \file PIDX_rst.c
 *
 * \author Sidharth Kumar
 * \date   10/09/14
 *
 * Implementation of all the functions 
 * declared in PIDX_rst.h
 *
 */

#include "PIDX_inc.h"



int maximum_neighbor_count = 128;

//Struct for restructuring ID
struct PIDX_rst_struct 
{
  //Passed by PIDX API
#if PIDX_HAVE_MPI
  MPI_Comm comm; //Communicator
#endif

  //Contains all relevant IDX file info
  //Blocks per file, samples per block, bitmask, patch, file name template and more
  idx_dataset idx;
  
  //Contains all derieved IDX file info
  //number of files, files that are ging to be populated
  idx_dataset_derived_metadata idx_derived;
  
  int init_index;
  int first_index;
  int last_index;
  
  //int if_perform_rst;

  //dimension of the power-two volume imposed patch
  int64_t reg_patch_size[PIDX_MAX_DIMENSIONS];
  int reg_patch_grp_count;
  Ndim_patch_group* reg_patch_grp;
};

#if PIDX_HAVE_MPI
static int intersectNDChunk(Ndim_patch A, Ndim_patch B);
static void set_default_patch_size(PIDX_rst_id rst_id, int64_t* process_bounds, int nprocs);
static int getPowerOftwo(int x);
#endif


#if PIDX_HAVE_MPI
/// Function to check if NDimensional data chunks A and B intersects
static int intersectNDChunk(Ndim_patch A, Ndim_patch B)
{
  int d = 0, check_bit = 0;
  for (d = 0; d < PIDX_MAX_DIMENSIONS; d++) 
    check_bit = check_bit || (A->offset[d] + A->size[d] - 1) < B->offset[d] || (B->offset[d] + B->size[d] - 1) < A->offset[d];
  
  return !(check_bit);
}


/// Function to find the power of 2 of an integer value (example 5->8)
static int getPowerOftwo(int x)
{
  int n = 1;
  while (n < x)
    n <<= 1;
  return n;
}


/// Function to find the dimension of the imposing regular patch
static void set_default_patch_size(PIDX_rst_id rst_id, int64_t* process_bounds, int nprocs)
{
  int i = 0, j = 0;
  int64_t average_count = 0;
  int check_bit = 0;
  int64_t max_dim_length[PIDX_MAX_DIMENSIONS] = {0, 0, 0, 0, 0};
  int equal_partiton = 1;

  for (i = 0; i < PIDX_MAX_DIMENSIONS; i++) 
  {
    max_dim_length[i] = process_bounds[PIDX_MAX_DIMENSIONS * 0 + i];
    for (j = 0; j < nprocs; j++) 
    {
      if (max_dim_length[i] <= process_bounds[PIDX_MAX_DIMENSIONS * j + i])
        max_dim_length[i] = process_bounds[PIDX_MAX_DIMENSIONS * j + i];
    }
  }

  for (i = 0; i < PIDX_MAX_DIMENSIONS; i++)
  {
    average_count = average_count + max_dim_length[i];
  }
  average_count = average_count / PIDX_MAX_DIMENSIONS;
  average_count = getPowerOftwo(average_count);
  
  for (i = 0; i < PIDX_MAX_DIMENSIONS; i++)
    check_bit = check_bit || ((double) rst_id->idx->bounds[i] / average_count > (double) rst_id->idx->bounds[i] / max_dim_length[i]);

  while (check_bit) 
  {
    average_count = average_count * 2;
    check_bit = 0;
    for (i = 0; i < PIDX_MAX_DIMENSIONS; i++)
      check_bit = check_bit || ((double) rst_id->idx->bounds[i] / average_count > (double) rst_id->idx->bounds[i] / max_dim_length[i]);
  }
  //reg_patch_size =  average_count;
  if (equal_partiton == 1) 
  {
    rst_id->reg_patch_size[0] = average_count / 1;
    rst_id->reg_patch_size[1] = average_count / 1;
    rst_id->reg_patch_size[2] = average_count / 1;
    rst_id->reg_patch_size[3] = 1;
    rst_id->reg_patch_size[4] = 1;
  } 
  else 
  {
    rst_id->reg_patch_size[0] = getPowerOftwo(process_bounds[0]) * 1;
    rst_id->reg_patch_size[1] = getPowerOftwo(process_bounds[1]) * 1;
    rst_id->reg_patch_size[2] = getPowerOftwo(process_bounds[2]) * 1;
    rst_id->reg_patch_size[3] = getPowerOftwo(process_bounds[3]) * 1;
    rst_id->reg_patch_size[4] = getPowerOftwo(process_bounds[4]) * 1;
  }

  memcpy(rst_id->idx->reg_patch_size, rst_id->reg_patch_size, sizeof(uint64_t) * PIDX_MAX_DIMENSIONS);
  //reg_patch_size = reg_patch_size * 4;
}
#endif


PIDX_rst_id PIDX_rst_init(idx_dataset idx_meta_data, idx_dataset_derived_metadata idx_derived, int first_index, int var_start_index, int var_end_index)
{
  //Creating the restructuring ID
  PIDX_rst_id rst_id;
  rst_id = (PIDX_rst_id)malloc(sizeof (*rst_id));
  memset(rst_id, 0, sizeof (*rst_id));

  rst_id->idx = idx_meta_data;
  rst_id->idx_derived = idx_derived;

  rst_id->init_index = first_index;
  rst_id->first_index = var_start_index;
  rst_id->last_index = var_end_index;

  return (rst_id);
}


#if PIDX_HAVE_MPI
PIDX_return_code PIDX_rst_set_communicator(PIDX_rst_id rst_id, MPI_Comm comm)
{
  if (rst_id == NULL)
    return PIDX_err_id;

  rst_id->comm = comm;

  return PIDX_success;
}
#endif


PIDX_return_code PIDX_rst_meta_data_create(PIDX_rst_id rst_id)
{

#if PIDX_HAVE_MPI
  int r, d, c, nprocs, rank;
  int64_t i, j, k, l, m, v, max_vol, patch_count;
  int reg_patch_count, edge_case = 0;
  
  MPI_Comm_rank(rst_id->comm, &rank);
  MPI_Comm_size(rst_id->comm, &nprocs);

  for (v = rst_id->first_index; v <= rst_id->last_index; v++)
  {
    PIDX_variable var = rst_id->idx->variable[v];

    if (rst_id->idx->enable_rst == 0)
      var->patch_group_count = var->sim_patch_count;
    else
    {
      var->patch_group_count = 0;

      /// STEP 1 : Compute the dimension of the regular patch
      if (rst_id->idx->reg_patch_size[0] == 0)
        set_default_patch_size(rst_id, rst_id->idx_derived->rank_r_count, nprocs);
      else
        memcpy(rst_id->reg_patch_size, rst_id->idx->reg_patch_size, PIDX_MAX_DIMENSIONS * sizeof(int64_t));

      /// extents for the local process(rank)
      Ndim_patch local_proc_patch = (Ndim_patch)malloc(sizeof (*local_proc_patch));
      memset(local_proc_patch, 0, sizeof (*local_proc_patch));
      for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
      {
        local_proc_patch->offset[d] = rst_id->idx_derived->rank_r_offset[PIDX_MAX_DIMENSIONS * rank + d];
        local_proc_patch->size[d] = rst_id->idx_derived->rank_r_count[PIDX_MAX_DIMENSIONS * rank + d];
      }

      rst_id->reg_patch_grp_count = 0;
      for (i = 0; i < rst_id->idx->bounds[0]; i = i + rst_id->reg_patch_size[0])
        for (j = 0; j < rst_id->idx->bounds[1]; j = j + rst_id->reg_patch_size[1])
          for (k = 0; k < rst_id->idx->bounds[2]; k = k + rst_id->reg_patch_size[2])
            for (l = 0; l < rst_id->idx->bounds[3]; l = l + rst_id->reg_patch_size[3])
              for (m = 0; m < rst_id->idx->bounds[4]; m = m + rst_id->reg_patch_size[4])
              {
                Ndim_patch reg_patch = (Ndim_patch)malloc(sizeof (*reg_patch));
                memset(reg_patch, 0, sizeof (*reg_patch));

                //Interior regular patches
                reg_patch->offset[0] = i;
                reg_patch->offset[1] = j;
                reg_patch->offset[2] = k;
                reg_patch->offset[3] = l;
                reg_patch->offset[4] = m;
                reg_patch->size[0] = rst_id->reg_patch_size[0];
                reg_patch->size[1] = rst_id->reg_patch_size[1];
                reg_patch->size[2] = rst_id->reg_patch_size[2];
                reg_patch->size[3] = rst_id->reg_patch_size[3];
                reg_patch->size[4] = rst_id->reg_patch_size[4];

                //Edge regular patches
                if ((i + rst_id->reg_patch_size[0]) > rst_id->idx->bounds[0])
                  reg_patch->size[0] = rst_id->idx->bounds[0] - i;
                if ((j + rst_id->reg_patch_size[1]) > rst_id->idx->bounds[1])
                  reg_patch->size[1] = rst_id->idx->bounds[1] - j;
                if ((k + rst_id->reg_patch_size[2]) > rst_id->idx->bounds[2])
                  reg_patch->size[2] = rst_id->idx->bounds[2] - k;
                if ((l + rst_id->reg_patch_size[3]) > rst_id->idx->bounds[3])
                  reg_patch->size[3] = rst_id->idx->bounds[3] - l;
                if ((m + rst_id->reg_patch_size[4]) > rst_id->idx->bounds[4])
                  reg_patch->size[4] = rst_id->idx->bounds[4] - m;

                if (intersectNDChunk(reg_patch, local_proc_patch))
                  rst_id->reg_patch_grp_count++;

                free(reg_patch);
              }

      rst_id->reg_patch_grp = (Ndim_patch_group*)malloc(sizeof(*rst_id->reg_patch_grp) * rst_id->reg_patch_grp_count);
      memset(rst_id->reg_patch_grp, 0, sizeof(*rst_id->reg_patch_grp) * rst_id->reg_patch_grp_count);

      reg_patch_count = 0;
      /// STEP 3 : iterate through extents of all imposed regular patches, and find all the regular patches a process (local_proc_patch) intersects with
      for (i = 0; i < rst_id->idx->bounds[0]; i = i + rst_id->reg_patch_size[0])
        for (j = 0; j < rst_id->idx->bounds[1]; j = j + rst_id->reg_patch_size[1])
          for (k = 0; k < rst_id->idx->bounds[2]; k = k + rst_id->reg_patch_size[2])
            for (l = 0; l < rst_id->idx->bounds[3]; l = l + rst_id->reg_patch_size[3])
              for (m = 0; m < rst_id->idx->bounds[4]; m = m + rst_id->reg_patch_size[4])
              {
                Ndim_patch reg_patch = (Ndim_patch)malloc(sizeof (*reg_patch));
                memset(reg_patch, 0, sizeof (*reg_patch));

                //Interior regular patches
                reg_patch->offset[0] = i;
                reg_patch->offset[1] = j;
                reg_patch->offset[2] = k;
                reg_patch->offset[3] = l;
                reg_patch->offset[4] = m;
                reg_patch->size[0] = rst_id->reg_patch_size[0];
                reg_patch->size[1] = rst_id->reg_patch_size[1];
                reg_patch->size[2] = rst_id->reg_patch_size[2];
                reg_patch->size[3] = rst_id->reg_patch_size[3];
                reg_patch->size[4] = rst_id->reg_patch_size[4];

                //Edge regular patches
                edge_case = 0;
                if ((i + rst_id->reg_patch_size[0]) > rst_id->idx->bounds[0])
                {
                  reg_patch->size[0] = rst_id->idx->bounds[0] - i;
                  edge_case = 1;
                }
                if ((j + rst_id->reg_patch_size[1]) > rst_id->idx->bounds[1])
                {
                  reg_patch->size[1] = rst_id->idx->bounds[1] - j;
                  edge_case = 1;
                }
                if ((k + rst_id->reg_patch_size[2]) > rst_id->idx->bounds[2])
                {
                  reg_patch->size[2] = rst_id->idx->bounds[2] - k;
                  edge_case = 1;
                }
                if ((l + rst_id->reg_patch_size[3]) > rst_id->idx->bounds[3])
                {
                  reg_patch->size[3] = rst_id->idx->bounds[3] - l;
                  edge_case = 1;
                }
                if ((m + rst_id->reg_patch_size[4]) > rst_id->idx->bounds[4])
                {
                  reg_patch->size[4] = rst_id->idx->bounds[4] - m;
                  edge_case = 1;
                }

                /// STEP 4: If local process intersects with regular patch, then find all other process that intersects with the regular patch.
                if (intersectNDChunk(reg_patch, local_proc_patch))
                {
                  rst_id->reg_patch_grp[reg_patch_count] = malloc(sizeof(*(rst_id->reg_patch_grp[reg_patch_count])));
                  memset(rst_id->reg_patch_grp[reg_patch_count], 0, sizeof(*(rst_id->reg_patch_grp[reg_patch_count])));

                  Ndim_patch_group patch_grp = rst_id->reg_patch_grp[reg_patch_count];

                  patch_grp->source_patch_rank = (int*)malloc(sizeof(int) * maximum_neighbor_count);
                  patch_grp->patch = malloc(sizeof(*patch_grp->patch) * maximum_neighbor_count);
                  memset(patch_grp->source_patch_rank, 0, sizeof(int) * maximum_neighbor_count);
                  memset(patch_grp->patch, 0, sizeof(*patch_grp->patch) * maximum_neighbor_count);

                  patch_count = 0;
                  patch_grp->count = 0;
                  if(edge_case == 0)
                    patch_grp->type = 1;
                  else
                    patch_grp->type = 2;

                  //Iterate through all processes
                  for (r = 0; r < nprocs; r++)
                  {
                    //Extent of process with rank r
                    Ndim_patch rank_r_patch = malloc(sizeof (*rank_r_patch));
                    memset(rank_r_patch, 0, sizeof (*rank_r_patch));

                    for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
                    {
                      rank_r_patch->offset[d] = rst_id->idx_derived->rank_r_offset[PIDX_MAX_DIMENSIONS * r + d];
                      rank_r_patch->size[d] = rst_id->idx_derived->rank_r_count[PIDX_MAX_DIMENSIONS * r + d];
                    }

                    //If process with rank r intersects with the regular patch, then calculate the offset, count and volume of the intersecting volume
                    if (intersectNDChunk(reg_patch, rank_r_patch))
                    {
                      patch_grp->patch[patch_count] = malloc(sizeof(*(patch_grp->patch[patch_count])));
                      memset(patch_grp->patch[patch_count], 0, sizeof(*(patch_grp->patch[patch_count])));

                      for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
                      {
                        //STEP 5 : offset and count of intersecting chunk of process with rank r and regular patch
                        if (rank_r_patch->offset[d] <= reg_patch->offset[d] && (rank_r_patch->offset[d] + rank_r_patch->size[d] - 1) <= (reg_patch->offset[d] + reg_patch->size[d] - 1))
                        {
                          patch_grp->patch[patch_count]->offset[d] = reg_patch->offset[d];
                          patch_grp->patch[patch_count]->size[d] = (rank_r_patch->offset[d] + rank_r_patch->size[d] - 1) - reg_patch->offset[d] + 1;
                        }
                        else if (reg_patch->offset[d] <= rank_r_patch->offset[d] && (rank_r_patch->offset[d] + rank_r_patch->size[d] - 1) >= (reg_patch->offset[d] + reg_patch->size[d] - 1))
                        {
                          patch_grp->patch[patch_count]->offset[d] = rank_r_patch->offset[d];
                          patch_grp->patch[patch_count]->size[d] = (reg_patch->offset[d] + reg_patch->size[d] - 1) - rank_r_patch->offset[d] + 1;
                        }
                        else if (( reg_patch->offset[d] + reg_patch->size[d] - 1) <= (rank_r_patch->offset[d] + rank_r_patch->size[d] - 1) && reg_patch->offset[d] >= rank_r_patch->offset[d])
                        {
                          patch_grp->patch[patch_count]->offset[d] = reg_patch->offset[d];
                          patch_grp->patch[patch_count]->size[d] = reg_patch->size[d];
                        }
                        else if (( rank_r_patch->offset[d] + rank_r_patch->size[d] - 1) <= (reg_patch->offset[d] + reg_patch->size[d] - 1) && rank_r_patch->offset[d] >= reg_patch->offset[d])
                        {
                          patch_grp->patch[patch_count]->offset[d] = rank_r_patch->offset[d];
                          patch_grp->patch[patch_count]->size[d] = rank_r_patch->size[d];
                        }

                        //offset and count of intersecting regular patch
                        patch_grp->reg_patch_offset[d] = reg_patch->offset[d];
                        patch_grp->reg_patch_size[d] = reg_patch->size[d];
                      }

                      patch_grp->source_patch_rank[patch_count] = r;
                      patch_count++;
                      patch_grp->count = patch_count;
                    }
                    free(rank_r_patch);
                  }

                  patch_grp->max_patch_rank = patch_grp->source_patch_rank[0];
                  max_vol = 1;
                  for(d = 0; d < PIDX_MAX_DIMENSIONS; d++)
                    max_vol = max_vol * patch_grp->patch[0]->size[d];
                  int64_t c_vol = 1;
                  for(c = 1; c < patch_grp->count ; c++)
                  {
                    c_vol = 1;
                    for(d = 0; d < PIDX_MAX_DIMENSIONS; d++)
                      c_vol = c_vol * patch_grp->patch[c]->size[d];
                    if(c_vol > max_vol)
                    {
                      max_vol = c_vol;
                      patch_grp->max_patch_rank = patch_grp->source_patch_rank[c];
                    }
                  }

                  if(rank == patch_grp->max_patch_rank)
                    var->patch_group_count = var->patch_group_count + 1;

                  //printf("%d\n", var->patch_group_count);
                  reg_patch_count++;
                }
                free(reg_patch);
              }

      free(local_proc_patch);
      //free(rank_r_offset);
      //free(rank_r_count);

      //return num_output_buffers;
    }
  }
#else
  rst_id->idx->enable_rst = 0;
  var->patch_group_count = var->sim_patch_count;
#endif

  int p = 0;
  for (v = rst_id->first_index; v <= rst_id->last_index; v++)
  {
    PIDX_variable var = rst_id->idx->variable[v];
    var->patch_group_count = rst_id->idx->variable[rst_id->first_index]->patch_group_count;

    var->rst_patch_group = malloc(var->patch_group_count * sizeof(*(var->rst_patch_group)));
    memset(var->rst_patch_group, 0, var->patch_group_count * sizeof(*(var->rst_patch_group)));
    for (p = 0; p < var->patch_group_count; p++)
    {
      var->rst_patch_group[p] = malloc(sizeof(*(var->rst_patch_group[p])));
      memset(var->rst_patch_group[p], 0, sizeof(*(var->rst_patch_group[p])));
    }
  }

  j = 0;
  v = 0;
  p = 0;
  if(rst_id->idx->enable_rst == 1)
  {
#if PIDX_HAVE_MPI
    int rank = 0, cnt = 0, i = 0;
    MPI_Comm_rank(rst_id->comm, &rank);
    for (v = rst_id->first_index; v <= rst_id->last_index; v++)
    {
      PIDX_variable var = rst_id->idx->variable[v];
      cnt = 0;
      for (i = 0; i < rst_id->reg_patch_grp_count; i++)
      {
        if (rank == rst_id->reg_patch_grp[i]->max_patch_rank)
        {
          Ndim_patch_group patch_group = var->rst_patch_group[cnt];
          patch_group->count = rst_id->reg_patch_grp[i]->count;
          patch_group->type = rst_id->reg_patch_grp[i]->type;
          patch_group->patch = malloc(sizeof(*(patch_group->patch)) * rst_id->reg_patch_grp[i]->count);

          for(j = 0; j < rst_id->reg_patch_grp[i]->count; j++)
          {
            patch_group->patch[j] = malloc(sizeof(*(patch_group->patch[j])));
            memcpy(patch_group->patch[j]->offset, rst_id->reg_patch_grp[i]->patch[j]->offset, PIDX_MAX_DIMENSIONS * sizeof(int64_t));
            memcpy(patch_group->patch[j]->size, rst_id->reg_patch_grp[i]->patch[j]->size, PIDX_MAX_DIMENSIONS * sizeof(int64_t));
          }
          memcpy(patch_group->reg_patch_offset, rst_id->reg_patch_grp[i]->reg_patch_offset, sizeof(int64_t) * PIDX_MAX_DIMENSIONS);
          memcpy(patch_group->reg_patch_size, rst_id->reg_patch_grp[i]->reg_patch_size, sizeof(int64_t) * PIDX_MAX_DIMENSIONS);
          cnt++;
        }
      }
      assert(cnt == var->patch_group_count);
    }
#endif
  }
  else
  {
    for (v = rst_id->first_index; v <= rst_id->last_index; v++)
    {
      PIDX_variable var = rst_id->idx->variable[v];
      for (p = 0; p < var->patch_group_count; p++)
      {
        Ndim_patch_group patch_group = var->rst_patch_group[p];
        patch_group->count = 1;
        patch_group->type = 0;
        patch_group->patch = malloc(sizeof(*(patch_group->patch)) * patch_group->count);
        for(j = 0; j < patch_group->count; j++)
        {
          patch_group->patch[j] = malloc(sizeof(*(patch_group->patch[j])));
          memcpy(patch_group->patch[j]->offset, var->sim_patch[p]->offset, PIDX_MAX_DIMENSIONS * sizeof(int64_t));
          memcpy(patch_group->patch[j]->size, var->sim_patch[p]->size, PIDX_MAX_DIMENSIONS * sizeof(int64_t));
        }
        memcpy(patch_group->reg_patch_offset, var->sim_patch[p]->offset, PIDX_MAX_DIMENSIONS * sizeof(int64_t));
        memcpy(patch_group->reg_patch_size, var->sim_patch[p]->size, PIDX_MAX_DIMENSIONS * sizeof(int64_t));
      }
    }
  }

  return PIDX_success;
}


PIDX_return_code PIDX_rst_buf_create(PIDX_rst_id rst_id)
{
  int j = 0, v = 0, p = 0;
  if(rst_id->idx->enable_rst == 1)
  {
#if PIDX_HAVE_MPI
    int rank = 0, cnt = 0, i = 0;
    MPI_Comm_rank(rst_id->comm, &rank);
    for (v = rst_id->first_index; v <= rst_id->last_index; v++)
    {
      PIDX_variable var = rst_id->idx->variable[v];
      cnt = 0;
      for (i = 0; i < rst_id->reg_patch_grp_count; i++)
      {
        if (rank == rst_id->reg_patch_grp[i]->max_patch_rank)
        {
          Ndim_patch_group patch_group = var->rst_patch_group[cnt];
          for(j = 0; j < rst_id->reg_patch_grp[i]->count; j++)
          {
            patch_group->patch[j]->buffer = malloc(patch_group->patch[j]->size[0] * patch_group->patch[j]->size[1] * patch_group->patch[j]->size[2] * patch_group->patch[j]->size[3] * patch_group->patch[j]->size[4] * var->values_per_sample * var->bits_per_value/8);
            memset(patch_group->patch[j]->buffer, 0, (patch_group->patch[j]->size[0] * patch_group->patch[j]->size[1] * patch_group->patch[j]->size[2] * patch_group->patch[j]->size[3] * patch_group->patch[j]->size[4] * var->values_per_sample * var->bits_per_value/8));
          }
          cnt++;
        }
      }
      assert(cnt == var->patch_group_count);
    }
#endif
  }
  else
  {
    for (v = rst_id->first_index; v <= rst_id->last_index; v++)
    {
      PIDX_variable var = rst_id->idx->variable[v];
      for (p = 0; p < var->patch_group_count; p++)
      {
        Ndim_patch_group patch_group = var->rst_patch_group[p];
        for(j = 0; j < patch_group->count; j++)
        {
          patch_group->patch[j]->buffer = malloc(patch_group->patch[j]->size[0] * patch_group->patch[j]->size[1] * patch_group->patch[j]->size[2] * patch_group->patch[j]->size[3] * patch_group->patch[j]->size[4] * var->bits_per_value/8 * var->values_per_sample);

          memcpy(patch_group->patch[j]->buffer, var->sim_patch[p]->buffer, (patch_group->patch[j]->size[0] * patch_group->patch[j]->size[1] * patch_group->patch[j]->size[2] * patch_group->patch[j]->size[3] * patch_group->patch[j]->size[4] * var->bits_per_value/8 * var->values_per_sample));
        }
      }
    }
  }

  //assert(cnt == num_output_buffers);
  return PIDX_success;
}


PIDX_return_code PIDX_rst_write(PIDX_rst_id rst_id)
{ 
  if (rst_id->idx->enable_rst != 1)
    return PIDX_success;

#if PIDX_HAVE_MPI
  uint64_t a1 = 0, b1 = 0, k1 = 0, i1 = 0, j1 = 0;
  uint64_t i, j, v, index, count1 = 0, req_count = 0;
  int *send_count, *send_offset;
  uint64_t send_c = 0, send_o = 0, counter = 0, req_counter = 0;
  int rank = 0, ret = 0;


  MPI_Request *req;
  MPI_Status *status;

  //rank and nprocs
  MPI_Comm_rank(rst_id->comm, &rank);
  
  //printf("rst_id->reg_patch_grp_count = %d\n", rst_id->reg_patch_grp_count);
  for (i = 0; i < rst_id->reg_patch_grp_count; i++)
    for(j = 0; j < rst_id->reg_patch_grp[i]->count; j++)
      req_count++;
    
  //creating ample requests and statuses
  req = (MPI_Request*) malloc(sizeof (*req) * req_count * 2 * (rst_id->last_index - rst_id->first_index + 1));
  if (!req)
  {
    fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
    return (-1);
  }

  status = (MPI_Status*) malloc(sizeof (*status) * req_count * 2 * (rst_id->last_index - rst_id->first_index + 1));
  if (!status) 
  {
    fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
    return (-1);
  }

  for (i = 0; i < rst_id->reg_patch_grp_count; i++)
  {
    if (rank == rst_id->reg_patch_grp[i]->max_patch_rank)
    {
      for(j = 0; j < rst_id->reg_patch_grp[i]->count; j++)
      {
        int64_t *reg_patch_offset = rst_id->reg_patch_grp[i]->patch[j]->offset;
        int64_t *reg_patch_count  = rst_id->reg_patch_grp[i]->patch[j]->size;
        
        if(rank == rst_id->reg_patch_grp[i]->source_patch_rank[j])
        {
          count1 = 0;
          for (a1 = reg_patch_offset[4]; a1 < reg_patch_offset[4] + reg_patch_count[4]; a1++)
            for (b1 = reg_patch_offset[3]; b1 < reg_patch_offset[3] + reg_patch_count[3]; b1++)
              for (k1 = reg_patch_offset[2]; k1 < reg_patch_offset[2] + reg_patch_count[2]; k1++)
                for (j1 = reg_patch_offset[1]; j1 < reg_patch_offset[1] + reg_patch_count[1]; j1++)
                  for (i1 = reg_patch_offset[0]; i1 < reg_patch_offset[0] + reg_patch_count[0]; i1 = i1 + reg_patch_count[0])
                  {
                    int64_t *sim_patch_offset = rst_id->idx->variable[rst_id->first_index]->sim_patch[0]->offset;
                    int64_t *sim_patch_count = rst_id->idx->variable[rst_id->first_index]->sim_patch[0]->size;
                    
                    index = (sim_patch_count[0] * sim_patch_count[1] * sim_patch_count[2] * sim_patch_count[3] * (a1 - sim_patch_offset[4])) +
                            (sim_patch_count[0] * sim_patch_count[1] * sim_patch_count[2] * (b1 - sim_patch_offset[3])) +
                            (sim_patch_count[0] * sim_patch_count[1] * (k1 - sim_patch_offset[2])) +
                            (sim_patch_count[0] * (j1 - sim_patch_offset[1])) +
                            (i1 - sim_patch_offset[0]);


                    for(v = rst_id->first_index; v <= rst_id->last_index; v++)
                    {
                      PIDX_variable var = rst_id->idx->variable[v];
                      send_o = index * var->values_per_sample;
                      send_c = reg_patch_count[0] * var->values_per_sample;

                      memcpy(var->rst_patch_group[counter]->patch[j]->buffer + (count1 * send_c * var->bits_per_value/8), var->sim_patch[0]->buffer + send_o * var->bits_per_value/8, send_c * var->bits_per_value/8);
                    }

                    count1++;
                  }
        }
        else
        {
          for(v = rst_id->first_index; v <= rst_id->last_index; v++)
          {
            PIDX_variable var = rst_id->idx->variable[v];


            int length = (reg_patch_count[0] * reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]) * var->values_per_sample * var->bits_per_value/8;

            ret = MPI_Irecv(var->rst_patch_group[counter]->patch[j]->buffer, length, MPI_BYTE, rst_id->reg_patch_grp[i]->source_patch_rank[j], 123, rst_id->comm, &req[req_counter]);
            if (ret != MPI_SUCCESS)
            {
              fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
              return PIDX_err_mpi;
            }

            req_counter++;
          }
        }
      }
      counter++;
    }
    else
    {
      for(j = 0; j < rst_id->reg_patch_grp[i]->count; j++)
      {
        if(rank == rst_id->reg_patch_grp[i]->source_patch_rank[j])
        {
          for(v = rst_id->first_index; v <= rst_id->last_index; v++)
          {
            PIDX_variable var = rst_id->idx->variable[v];

            int64_t *reg_patch_count = rst_id->reg_patch_grp[i]->patch[j]->size;
            int64_t *reg_patch_offset = rst_id->reg_patch_grp[i]->patch[j]->offset;
            

            send_offset = malloc(sizeof (int) * (reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]));
            if (!send_offset) 
            {
              fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
              return PIDX_err_mpi;
            }
            memset(send_offset, 0, sizeof (int) * (reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]));

            send_count = malloc(sizeof (int) * (reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]));
            if (!send_count)
            {
              fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
              return PIDX_err_mpi;
            }
            memset(send_count, 0, sizeof (int) * (reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]));

            count1 = 0;
            for (a1 = reg_patch_offset[4]; a1 < reg_patch_offset[4] + reg_patch_count[4]; a1++)
              for (b1 = reg_patch_offset[3]; b1 < reg_patch_offset[3] + reg_patch_count[3]; b1++)
                for (k1 = reg_patch_offset[2]; k1 < reg_patch_offset[2] + reg_patch_count[2]; k1++)
                  for (j1 = reg_patch_offset[1]; j1 < reg_patch_offset[1] + reg_patch_count[1]; j1++)
                    for (i1 = reg_patch_offset[0]; i1 < reg_patch_offset[0] + reg_patch_count[0]; i1 = i1 + reg_patch_count[0])
                    {
                      int64_t *sim_patch_count  = rst_id->idx->variable[rst_id->first_index]->sim_patch[0]->size;
                      int64_t *sim_patch_offset = rst_id->idx->variable[rst_id->first_index]->sim_patch[0]->offset;
                      
                      index = (sim_patch_count[0] * sim_patch_count[1] * sim_patch_count[2] * sim_patch_count[3] * (a1 - sim_patch_offset[4])) +
                              (sim_patch_count[0] * sim_patch_count[1] * sim_patch_count[2] * (b1 - sim_patch_offset[3])) +
                              (sim_patch_count[0] * sim_patch_count[1] * (k1 - sim_patch_offset[2])) +
                              (sim_patch_count[0] * (j1 - sim_patch_offset[1])) +
                              (i1 - sim_patch_offset[0]);
                      send_offset[count1] = index * var->values_per_sample * var->bits_per_value/8;
                      send_count[count1] = reg_patch_count[0] * var->values_per_sample * var->bits_per_value/8;

                      count1++;
                    }


            MPI_Datatype chunk_data_type;
            MPI_Type_indexed(count1, send_count, send_offset, MPI_BYTE, &chunk_data_type);
            MPI_Type_commit(&chunk_data_type);

            ret = MPI_Isend(var->sim_patch[0]->buffer, 1, chunk_data_type, rst_id->reg_patch_grp[i]->max_patch_rank, 123, rst_id->comm, &req[req_counter]);
            if (ret != MPI_SUCCESS) 
            {
              fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
              return PIDX_err_mpi;
            }

            req_counter++;

            MPI_Type_free(&chunk_data_type);
            free(send_offset);
            free(send_count);

          }
        }
      }
    }
  }

  ret = MPI_Waitall(req_counter, req, status);
  if (ret != MPI_SUCCESS)
  {
    fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
    return (-1);
  }


  free(req);
  req = 0;
  free(status);
  status = 0;
  
  return PIDX_success;
#else
  if (rst_id->idx->enable_rst == 1)
    return PIDX_err_rst;
  else
    return PIDX_success;
#endif
}


PIDX_return_code PIDX_rst_read(PIDX_rst_id rst_id)
{  
  if (rst_id->idx->enable_rst != 1)
    return PIDX_success;

#if PIDX_HAVE_MPI
  int64_t a1 = 0, b1 = 0, k1 = 0, i1 = 0, j1 = 0;
  int i, j, v, index, count1 = 0, ret = 0, req_count = 0;
  int *send_count, *send_offset;
  int rank, send_c = 0, send_o = 0, counter = 0, req_counter = 0;

  MPI_Request *req;
  MPI_Status *status;

  //rank and nprocs
  MPI_Comm_rank(rst_id->comm, &rank);
  
  for (i = 0; i < rst_id->reg_patch_grp_count; i++)
    for(j = 0; j < rst_id->reg_patch_grp[i]->count; j++)
      req_count++;
    
  //creating ample requests and statuses
  req = (MPI_Request*) malloc(sizeof (*req) * req_count * 2 * (rst_id->last_index - rst_id->first_index + 1));
  if (!req)
  {
    fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
    return PIDX_err_rst;
  }

  status = (MPI_Status*) malloc(sizeof (*status) * req_count * 2 * (rst_id->last_index - rst_id->first_index + 1));
  if (!status) 
  {
    fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
    return PIDX_err_rst;
  }

  for (i = 0; i < rst_id->reg_patch_grp_count; i++)
  {
    if (rank == rst_id->reg_patch_grp[i]->max_patch_rank)
    {
      for (j = 0; j < rst_id->reg_patch_grp[i]->count; j++)
      {
        int64_t *reg_patch_offset = rst_id->reg_patch_grp[i]->patch[j]->offset;
        int64_t *reg_patch_count  = rst_id->reg_patch_grp[i]->patch[j]->size;
        
        if (rank == rst_id->reg_patch_grp[i]->source_patch_rank[j])
        {
          count1 = 0;
          for (a1 = reg_patch_offset[4]; a1 < reg_patch_offset[4] + reg_patch_count[4]; a1++)
            for (b1 = reg_patch_offset[3]; b1 < reg_patch_offset[3] + reg_patch_count[3]; b1++)
              for (k1 = reg_patch_offset[2]; k1 < reg_patch_offset[2] + reg_patch_count[2]; k1++)
                for (j1 = reg_patch_offset[1]; j1 < reg_patch_offset[1] + reg_patch_count[1]; j1++)
                  for (i1 = reg_patch_offset[0]; i1 < reg_patch_offset[0] + reg_patch_count[0]; i1 = i1 + reg_patch_count[0])
                  {
                    int64_t *sim_patch_offset = rst_id->idx->variable[rst_id->first_index]->sim_patch[0]->offset;
                    int64_t *sim_patch_count = rst_id->idx->variable[rst_id->first_index]->sim_patch[0]->size;
                    
                    index = (sim_patch_count[0] * sim_patch_count[1] * sim_patch_count[2] * sim_patch_count[3] * (a1 - sim_patch_offset[4])) +
                            (sim_patch_count[0] * sim_patch_count[1] * sim_patch_count[2] * (b1 - sim_patch_offset[3])) +
                            (sim_patch_count[0] * sim_patch_count[1] * (k1 - sim_patch_offset[2])) +
                            (sim_patch_count[0] * (j1 - sim_patch_offset[1])) +
                            (i1 - sim_patch_offset[0]);
                            
                    for(v = rst_id->first_index; v <= rst_id->last_index; v++)
                    {
                      PIDX_variable var = rst_id->idx->variable[v];

                      send_o = index * var->values_per_sample;
                      send_c = reg_patch_count[0] * var->values_per_sample;
                      memcpy(var->sim_patch[0]->buffer + send_o * var->bits_per_value/8,
                             var->rst_patch_group[counter]->patch[j]->buffer + (count1 * send_c * var->bits_per_value/8),
                             send_c * var->bits_per_value/8);
                    }                
                    count1++;
                  }
        }
        else
        {
          for(v = rst_id->first_index; v <= rst_id->last_index; v++)
          {
            PIDX_variable var = rst_id->idx->variable[v];

            ret = MPI_Isend(var->rst_patch_group[counter]->patch[j]->buffer, (reg_patch_count[0] * reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]) * var->values_per_sample * var->bits_per_value/8, MPI_BYTE, rst_id->reg_patch_grp[i]->source_patch_rank[j], 123, rst_id->comm, &req[req_counter]);
            if (ret != MPI_SUCCESS) 
            {
              fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
              return PIDX_err_rst;
            }
            req_counter++; 
          }
        }
      }
      counter++;
    }
    else
    {
      for(j = 0; j < rst_id->reg_patch_grp[i]->count; j++)
      {
        if(rank == rst_id->reg_patch_grp[i]->source_patch_rank[j])
        {
          for(v = rst_id->first_index; v <= rst_id->last_index; v++)
          {
            PIDX_variable var = rst_id->idx->variable[v];

            int64_t *reg_patch_count = rst_id->reg_patch_grp[i]->patch[j]->size;
            int64_t *reg_patch_offset = rst_id->reg_patch_grp[i]->patch[j]->offset;
            
            send_offset = (int*) malloc(sizeof (int) * (reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]));
            if (!send_offset) 
            {
              fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
              return PIDX_err_rst;
            }
            memset(send_offset, 0, sizeof (int) * (reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]));

            send_count = (int*) malloc(sizeof (int) * (reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]));
            if (!send_count) 
            {
              fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
              return PIDX_err_rst;
            }
            memset(send_count, 0, sizeof (int) * (reg_patch_count[1] * reg_patch_count[2] * reg_patch_count[3] * reg_patch_count[4]));
            
            count1 = 0;
            for (a1 = reg_patch_offset[4]; a1 < reg_patch_offset[4] + reg_patch_count[4]; a1++)
              for (b1 = reg_patch_offset[3]; b1 < reg_patch_offset[3] + reg_patch_count[3]; b1++)
                for (k1 = reg_patch_offset[2]; k1 < reg_patch_offset[2] + reg_patch_count[2]; k1++)
                  for (j1 = reg_patch_offset[1]; j1 < reg_patch_offset[1] + reg_patch_count[1]; j1++)
                    for (i1 = reg_patch_offset[0]; i1 < reg_patch_offset[0] + reg_patch_count[0]; i1 = i1 + reg_patch_count[0])
                    {
                      int64_t *sim_patch_count  = rst_id->idx->variable[rst_id->first_index]->sim_patch[0]->size;
                      int64_t *sim_patch_offset = rst_id->idx->variable[rst_id->first_index]->sim_patch[0]->offset;
                      
                      index = (sim_patch_count[0] * sim_patch_count[1] * sim_patch_count[2] * sim_patch_count[3] * (a1 - sim_patch_offset[4])) +
                              (sim_patch_count[0] * sim_patch_count[1] * sim_patch_count[2] * (b1 - sim_patch_offset[3])) +
                              (sim_patch_count[0] * sim_patch_count[1] * (k1 - sim_patch_offset[2])) +
                              (sim_patch_count[0] * (j1 - sim_patch_offset[1])) +
                              (i1 - sim_patch_offset[0]);
                      send_offset[count1] = index * var->values_per_sample * var->bits_per_value/8;
                      send_count[count1] = reg_patch_count[0] * var->values_per_sample * var->bits_per_value/8;
                      count1++;
                    }

            MPI_Datatype chunk_data_type;
            MPI_Type_indexed(count1, send_count, send_offset, MPI_BYTE, &chunk_data_type);
            MPI_Type_commit(&chunk_data_type);

            ret = MPI_Irecv(rst_id->idx->variable[v]->sim_patch[0]->buffer, 1, chunk_data_type, rst_id->reg_patch_grp[i]->max_patch_rank, 123, rst_id->comm, &req[req_counter]);
            if (ret != MPI_SUCCESS)
            {
              fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
              return PIDX_err_rst;
            }
            req_counter++;
             
            MPI_Type_free(&chunk_data_type);
            free(send_offset);
            free(send_count);
          }
        }
      }
    }
  }

  ret = MPI_Waitall(req_counter, req, status);
  if (ret != MPI_SUCCESS)
  {
    fprintf(stderr, "Error: File [%s] Line [%d]\n", __FILE__, __LINE__);
    return PIDX_err_rst;
  }

  free(req);
  req = 0;
  free(status);
  status = 0;
  
  return PIDX_success;
#else
  if (rst_id->idx->enable_rst == 1)
    return PIDX_err_rst;
  else
    return PIDX_success;

#endif
}


PIDX_return_code PIDX_rst_buf_destroy(PIDX_rst_id rst_id)
{
  int i, j, v;

  for(v = rst_id->first_index; v <= rst_id->last_index; v++)
  {
    PIDX_variable var = rst_id->idx->variable[v];
    for(i = 0; i < rst_id->idx->variable[v]->patch_group_count; i++)
    {
      for(j = 0; j < rst_id->idx->variable[v]->rst_patch_group[i]->count; j++)
      {
        free(var->rst_patch_group[i]->patch[j]->buffer);
        var->rst_patch_group[i]->patch[j]->buffer = 0;
      }
    }
  }
  
  return PIDX_success;
}

PIDX_return_code PIDX_rst_meta_data_destroy(PIDX_rst_id rst_id)
{
  int i, j, v;

  for(v = rst_id->first_index; v <= rst_id->last_index; v++)
  {
    PIDX_variable var = rst_id->idx->variable[v];
    for(i = 0; i < rst_id->idx->variable[v]->patch_group_count; i++)
    {
      for(j = 0; j < rst_id->idx->variable[v]->rst_patch_group[i]->count; j++)
      {
        free(var->rst_patch_group[i]->patch[j]);
        var->rst_patch_group[i]->patch[j] = 0;
      }

      free(var->rst_patch_group[i]->patch);
      var->rst_patch_group[i]->patch = 0;

      free(var->rst_patch_group[i]);
      var->rst_patch_group[i] = 0;
    }

    free(var->rst_patch_group);
    var->rst_patch_group = 0;
  }

  return PIDX_success;
}


PIDX_return_code PIDX_rst_finalize(PIDX_rst_id rst_id)
{
  if (rst_id->idx->enable_rst == 1)
  {
    int i, j;
    for (i = 0; i < rst_id->reg_patch_grp_count; i++)
    {
      for (j = 0; j < rst_id->reg_patch_grp[i]->count ; j++ )
      {
        free(rst_id->reg_patch_grp[i]->patch[j]);
        rst_id->reg_patch_grp[i]->patch[j] = 0;
      }

      free(rst_id->reg_patch_grp[i]->source_patch_rank);
      rst_id->reg_patch_grp[i]->source_patch_rank = 0;
      free(rst_id->reg_patch_grp[i]->patch);
      rst_id->reg_patch_grp[i]->patch = 0;
    
      free(rst_id->reg_patch_grp[i]);
      rst_id->reg_patch_grp[i] = 0;
    }
    free(rst_id->reg_patch_grp);
    rst_id->reg_patch_grp = 0;
  }
  
  free(rst_id);
  rst_id = 0;
  
  return PIDX_success;
}


PIDX_return_code HELPER_rst(PIDX_rst_id rst_id)
{
  int i, j, k, rank = 0, v = 0, u = 0, s = 0, a, m, n, bytes_for_datatype;
  int64_t element_count = 0;
  int64_t lost_element_count = 0;
  
#if PIDX_HAVE_MPI
  MPI_Comm_rank(rst_id->comm, &rank);
#endif

#if long_buffer
  uint64_t dvalue_1, dvalue_2;
#else
  double dvalue_1, dvalue_2;
#endif

  int64_t *bounds = rst_id->idx->bounds;


  for(v = rst_id->first_index; v <= rst_id->last_index; v++)
  {
    PIDX_variable var = rst_id->idx->variable[v];
    bytes_for_datatype = var->bits_per_value / 8;

    for (m = 0; m < var->patch_group_count; m++)
    {

      for(n = 0; n < var->rst_patch_group[m]->count; n++)
      {
        int64_t *count_ptr = var->rst_patch_group[m]->patch[n]->size;
        int64_t *offset_ptr = var->rst_patch_group[m]->patch[n]->offset;
        
        for (a = 0; a < count_ptr[4]; a++)
          for (u = 0; u < count_ptr[3]; u++)
            for (k = 0; k < count_ptr[2]; k++) 
              for (j = 0; j < count_ptr[1]; j++) 
                for (i = 0; i < count_ptr[0]; i++) 
                {
                  int64_t index = (count_ptr[0] * count_ptr[1] * count_ptr[2] * count_ptr[3] * a) + (count_ptr[0] * count_ptr[1] * count_ptr[2] * u) + (count_ptr[0] * count_ptr[1] * k) + (count_ptr[0] * j) + i;
                  
                  int check_bit = 1;
                  for (s = 0; s < var->values_per_sample; s++)
                  {
                    dvalue_1 = 100 + v + s + (bounds[0] * bounds[1] * bounds[2] * bounds[3] * (offset_ptr[4] + a)) + (bounds[0] * bounds[1] * bounds[2] * (offset_ptr[3] + u)) + (bounds[0] * bounds[1] * (offset_ptr[2] + k)) + (bounds[0] * (offset_ptr[1] + j)) + offset_ptr[0] + i + ( rst_id->idx_derived->color * bounds[0] * bounds[1] * bounds[2]);
                    
                    memcpy(&dvalue_2, var->rst_patch_group[m]->patch[n]->buffer + ((index * var->values_per_sample) + s) * bytes_for_datatype, bytes_for_datatype);

                    //printf("[%d (%d) %d] :: %f %f \n", v, var->values_per_sample, s, dvalue_1, dvalue_2);
                    
                    check_bit = check_bit && (dvalue_1 == dvalue_2);
                  }

                  if (check_bit == 0)
                  {
                    lost_element_count++;
                    printf("[%d] [RST] LOST Element : %f %f\n", rank, dvalue_1, dvalue_2);
                  } 
                  else 
                  {
                    //printf("[RST] %d %f %f\n", rank, dvalue_1, dvalue_2);
                    element_count++;
                  }
                }
      }
    }
  }
  
  int64_t global_volume;
#if PIDX_HAVE_MPI
  MPI_Allreduce(&element_count, &global_volume, 1, MPI_LONG_LONG, MPI_SUM, rst_id->comm);
#else
  global_volume = element_count;
#endif
    
  if (global_volume != (int64_t) bounds[0] * bounds[1] * bounds[2] * bounds[3] * bounds[4] * (rst_id->last_index - rst_id->first_index + 1))
  {
    if (rank == 0)
      fprintf(stderr, "[RST Debug FAILED!!!!]  [Color %d] [Recorded Volume %lld] [Actual Volume %lld]\n", rst_id->idx_derived->color, (long long) global_volume, (long long) bounds[0] * bounds[1] * bounds[2]  * (rst_id->last_index - rst_id->first_index + 1));
    
    printf("[RST]  Rank %d Color %d [LOST ELEMENT COUNT %lld] [FOUND ELEMENT COUNT %lld] [TOTAL ELEMNTS %lld] \n", rank,  rst_id->idx_derived->color, (long long) lost_element_count, (long long) element_count, (long long) (bounds[0] * bounds[1] * bounds[2] * bounds[3] * bounds[4]) * (rst_id->last_index - rst_id->first_index + 1));
      
    return PIDX_err_rst;
  }
  else
    if (rank == 0)
      fprintf(stderr, "[RST Debug PASSED!!!!]  [Color %d] [Recorded Volume %lld] [Actual Volume %lld]\n", rst_id->idx_derived->color, (long long) global_volume, (long long) bounds[0] * bounds[1] * bounds[2]  * (rst_id->last_index - rst_id->first_index + 1));
    
  return PIDX_success;
}
