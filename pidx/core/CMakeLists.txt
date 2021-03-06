##
## BSD 3-Clause License
## 
## Copyright (c) 2010-2019 ViSUS L.L.C., 
## Scientific Computing and Imaging Institute of the University of Utah
## 
## ViSUS L.L.C., 50 W. Broadway, Ste. 300, 84101-2044 Salt Lake City, UT
## University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT
##  
## All rights reserved.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
## 
## * Redistributions of source code must retain the above copyright notice, this
## list of conditions and the following disclaimer.
## 
## * Redistributions in binary form must reproduce the above copyright notice,
## this list of conditions and the following disclaimer in the documentation
## and/or other materials provided with the distribution.
## 
## * Neither the name of the copyright holder nor the names of its
## contributors may be used to endorse or promote products derived from
## this software without specific prior written permission.
## 
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
## DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
## DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
## SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
## CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
## OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
## 
## For additional information about this project contact: pascucci@acm.org
## For support: support@visus.net
## 
##

ADD_SUBDIRECTORY(PIDX_raw_rst)
ADD_SUBDIRECTORY(PIDX_brick_res_precision_rst)
ADD_SUBDIRECTORY(PIDX_particles_rst)
ADD_SUBDIRECTORY(PIDX_idx_rst)
ADD_SUBDIRECTORY(PIDX_block_rst)
ADD_SUBDIRECTORY(PIDX_cmp)
ADD_SUBDIRECTORY(PIDX_hz)
ADD_SUBDIRECTORY(PIDX_agg)
ADD_SUBDIRECTORY(PIDX_file_io)
ADD_SUBDIRECTORY(PIDX_header)


FILE(GLOB PIDX_CORE_SOURCES *.h *.c)


# ////////////////////////////////////////
# includes
# ////////////////////////////////////////

IF (MPI_C_FOUND)
  #MESSAGE("Configuring pidx with MPI support")
  INCLUDE_DIRECTORIES(${MPI_C_INCLUDE_PATH})
  SET(PIDX_CORE_LINK_LIBS ${PIDX_CORE_LINK_LIBS} ${MPI_C_LIBRARIES})
ENDIF ()

IF (ZFP_FOUND)
  #MESSAGE("Configuring pidx with ZFP support")
  INCLUDE_DIRECTORIES(${ZFP_INCLUDE_DIR})
  SET(PIDX_CORE_LINK_LIBS ${PIDX_CORE_LINK_LIBS} ${ZFP_LIBRARIES})
ENDIF ()


# ////////////////////////////////////////
# library
# ////////////////////////////////////////

PIDX_ADD_LIBRARY(pidx_core "${PIDX_CORE_SOURCES}")
ADD_DEPENDENCIES(pidx_core zfp)
ADD_DEPENDENCIES(pidx_utils zfp)
ADD_DEPENDENCIES(pidx_data_handle zfp)
ADD_DEPENDENCIES(pidx_header zfp)
ADD_DEPENDENCIES(pidx_idx_rst zfp)
ADD_DEPENDENCIES(pidx_raw_rst zfp)
ADD_DEPENDENCIES(pidx_brick_res_precision_rst zfp)
ADD_DEPENDENCIES(pidx_cmp zfp)
ADD_DEPENDENCIES(pidx_hz zfp)
ADD_DEPENDENCIES(pidx_agg zfp)
ADD_DEPENDENCIES(pidx_file_io zfp)

TARGET_LINK_LIBRARIES(pidx_core ${OS_SPECIFIC_LIBS} pidx_utils pidx_data_handle pidx_header pidx_idx_rst pidx_particles_rst pidx_raw_rst pidx_brick_res_precision_rst pidx_cmp pidx_hz pidx_agg pidx_file_io ${PIDX_CORE_LINK_LIBS})
SET_TARGET_PROPERTIES(pidx_core PROPERTIES LINKER_LANGUAGE CXX)
