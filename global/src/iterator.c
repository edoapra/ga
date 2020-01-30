/* $Id: iterator.c,v 1.80.2.18 2007/12/18 22:22:27 d3g293 Exp $ */
/* 
 * module: iterator.c
 * author: Jarek Nieplocha
 * description: implements an iterator that can be used for looping over blocks
 *              in a GA operation. This functionality is designed to hide
 *              the details of the data layout from the operation
 * 
 * DISCLAIMER
 *
 * This material was prepared as an account of work sponsored by an
 * agency of the United States Government.  Neither the United States
 * Government nor the United States Department of Energy, nor Battelle,
 * nor any of their employees, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * SOFTWARE, OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT
 * INFRINGE PRIVATELY OWNED RIGHTS.
 *
 *
 * ACKNOWLEDGMENT
 *
 * This software and its documentation were produced with United States
 * Government support under Contract Number DE-AC06-76RLO-1830 awarded by
 * the United States Department of Energy.  The United States Government
 * retains a paid-up non-exclusive, irrevocable worldwide license to
 * reproduce, prepare derivative works, perform publicly and display
 * publicly by or for the US Government, including the right to
 * distribute to other US Government contractors.
 */
#if HAVE_CONFIG_H
#   include "config.h"
#endif

#define MAX_INT_VALUE 2147483648

#define LARGE_BLOCK_REQ
 
#if HAVE_STDIO_H
#   include <stdio.h>
#endif
#if HAVE_STRING_H
#   include <string.h>
#endif
#if HAVE_STDLIB_H
#   include <stdlib.h>
#endif
#if HAVE_STDINT_H
#   include <stdint.h>
#endif
#if HAVE_MATH_H
#   include <math.h>
#endif
#if HAVE_ASSERT_H
#   include <assert.h>
#endif
#if HAVE_STDDEF_H
#include <stddef.h>
#endif

#include "global.h"
#include "globalp.h"
#include "base.h"
#include "ga_iterator.h"
#include "armci.h"
#include "macdecls.h"
#include "ga-papi.h"
#include "ga-wapi.h"
#include "thread-safe.h"

#define DEBUG 0
#ifdef PROFILE_OLD
#include "ga_profile.h"
#endif

/*\ prepare permuted list of processes for remote ops
\*/
#define gaPermuteProcList(nproc,ProcListPerm)                         \
{                                                                     \
  if((nproc) ==1) ProcListPerm[0]=0;                                  \
  else{                                                               \
    int _i, iswap, temp;                                              \
    if((nproc) > GAnproc) pnga_error("permute_proc: error ", (nproc));\
                                                                      \
    /* every process generates different random sequence */           \
    (void)srand((unsigned)GAme);                                      \
                                                                      \
    /* initialize list */                                             \
    for(_i=0; _i< (nproc); _i++) ProcListPerm[_i]=_i;                 \
                                                                      \
    /* list permutation generated by random swapping */               \
    for(_i=0; _i< (nproc); _i++){                                     \
      iswap = (int)(rand() % (nproc));                                \
      temp = ProcListPerm[iswap];                                     \
      ProcListPerm[iswap] = ProcListPerm[_i];                         \
      ProcListPerm[_i] = temp;                                        \
    }                                                                 \
  }                                                                   \
}

#define gam_GetRangeFromMap(p, ndim, plo, phi){  \
  Integer   _mloc = p* ndim *2;                  \
            *plo  = hdl->map + _mloc;            \
            *phi  = *plo + ndim;                 \
}

/*\ Return pointer (ptr_loc) to location in memory of element with subscripts
 *  (subscript). Also return physical dimensions of array in memory in ld.
\*/
#define gam_Location(proc, g_handle,  subscript, ptr_loc, ld)              \
{                                                                          \
  Integer _offset=0, _d, _w, _factor=1, _last=GA[g_handle].ndim-1;         \
  Integer _lo[MAXDIM], _hi[MAXDIM], _pinv, _p_handle;                      \
                                                                           \
  ga_ownsM(g_handle, proc, _lo, _hi);                                      \
  gaCheckSubscriptM(subscript, _lo, _hi, GA[g_handle].ndim);               \
  if(_last==0) ld[0]=_hi[0]- _lo[0]+1+2*(Integer)GA[g_handle].width[0];    \
  __CRAYX1_PRAGMA("_CRI shortloop");                                       \
  for(_d=0; _d < _last; _d++)            {                                 \
    _w = (Integer)GA[g_handle].width[_d];                                  \
    _offset += (subscript[_d]-_lo[_d]+_w) * _factor;                       \
    ld[_d] = _hi[_d] - _lo[_d] + 1 + 2*_w;                                 \
    _factor *= ld[_d];                                                     \
  }                                                                        \
  _offset += (subscript[_last]-_lo[_last]                                  \
      + (Integer)GA[g_handle].width[_last])                                \
  * _factor;                                                               \
  _p_handle = GA[g_handle].p_handle;                                       \
  if (_p_handle != 0) {                                                    \
    if (GA[g_handle].num_rstrctd == 0) {                                   \
      _pinv = proc;                                                        \
    } else {                                                               \
      _pinv = GA[g_handle].rstrctd_list[proc];                             \
    }                                                                      \
  } else {                                                                 \
    _pinv = PGRP_LIST[_p_handle].inv_map_proc_list[proc];                  \
  }                                                                        \
  *(ptr_loc) =  GA[g_handle].ptr[_pinv]+_offset*GA[g_handle].elemsize;     \
}

void gam_LocationF(int proc, Integer g_handle,  Integer subscript[],
    char **ptr_loc, Integer ld[])
{                                                                          
  Integer _offset=0, _d, _w, _factor=1, _last=GA[g_handle].ndim-1;         
  Integer _lo[MAXDIM], _hi[MAXDIM], _pinv, _p_handle;                     
                                                                           
  ga_ownsM(g_handle, proc, _lo, _hi);                                      
  gaCheckSubscriptM(subscript, _lo, _hi, GA[g_handle].ndim);               
  if(_last==0) ld[0]=_hi[0]- _lo[0]+1+2*(Integer)GA[g_handle].width[0];   
  __CRAYX1_PRAGMA("_CRI shortloop");                                      
  for(_d=0; _d < _last; _d++)            {                                 
    _w = (Integer)GA[g_handle].width[_d];                                  
    _offset += (subscript[_d]-_lo[_d]+_w) * _factor;                       
    ld[_d] = _hi[_d] - _lo[_d] + 1 + 2*_w;                                 
    _factor *= ld[_d];                                                     
  }                                                                        
  _offset += (subscript[_last]-_lo[_last]                                  
      + (Integer)GA[g_handle].width[_last])                                
  * _factor;                                                               
  _p_handle = GA[g_handle].p_handle;                                       
  if (_p_handle != 0) {                                                    
    if (GA[g_handle].num_rstrctd == 0) {                                   
      _pinv = proc;                                                        
    } else {                                                               
      _pinv = GA[g_handle].rstrctd_list[proc];                             
    }                                                                      
  } else {                                                                 
    _pinv = PGRP_LIST[_p_handle].inv_map_proc_list[proc];                  
  }                                                                       
  *(ptr_loc) =  GA[g_handle].ptr[_pinv]+_offset*GA[g_handle].elemsize;    
}

#define gam_GetBlockPatch(plo,phi,lo,hi,blo,bhi,ndim) {                    \
  Integer _d;                                                              \
  for (_d=0; _d<ndim; _d++) {                                              \
    if (lo[_d] <= phi[_d] && lo[_d] >= plo[_d]) blo[_d] = lo[_d];          \
    else blo[_d] = plo[_d];                                                \
    if (hi[_d] <= phi[_d] && hi[_d] >= plo[_d]) bhi[_d] = hi[_d];          \
    else bhi[_d] = phi[_d];                                                \
  }                                                                        \
}

/**
 * Initialize an iterator handle
 * @param g_a global array handle
 * @param lo indices for lower corner of block in global array
 * @param hi indices for upper corner of block in global array
 * @param hdl handle for iterator
 */
void gai_iterator_init(Integer g_a, Integer lo[], Integer hi[],
                       _iterator_hdl *hdl)
{
  Integer handle = GA_OFFSET + g_a;
  Integer ndim = GA[handle].ndim;
  int grp = GA[handle].p_handle;
  int nproc = pnga_pgroup_nnodes(grp);
  Integer i;
  hdl->g_a = g_a;
  hdl->count = 0;
  hdl->oversize = 0;
  hdl->map = malloc((size_t)(nproc*2*MAXDIM+1)*sizeof(Integer));
  hdl->proclist = malloc(nproc*sizeof(Integer));;
  hdl->proclistperm = malloc(nproc*sizeof(int));
  for (i=0; i<ndim; i++) {
    hdl->lo[i] = lo[i];
    hdl->hi[i] = hi[i];
  }
  /* Standard GA distribution */
  if (GA[handle].distr_type == REGULAR) {
    /* Locate the processors containing some portion of the patch
     * specified by lo and hi and return the results in hdl->map,
     * hdl->proclist, and np. hdl->proclist contains a list of processors
     * containing some portion of the patch, hdl->map contains
     * the lower and upper indices of the portion of the patch held
     * by a given processor, and np contains the total number of
     * processors that contain some portion of the patch.
     */
    if(!pnga_locate_region(g_a, lo, hi, hdl->map, hdl->proclist, &hdl->nproc ))
      ga_RegionError(pnga_ndim(g_a), lo, hi, g_a);

    gaPermuteProcList(hdl->nproc, hdl->proclistperm);

    /* Block-cyclic distribution */
  } else if (GA[handle].distr_type == BLOCK_CYCLIC) {
    /* GA uses simple block cyclic data distribution */
    hdl->iproc = 0;
    hdl->iblock = pnga_nodeid();
  } else if (GA[handle].distr_type == SCALAPACK)  {
    /* GA uses ScaLAPACK block cyclic data distribution */
    int j;
    C_Integer *block_dims;
    /* Scalapack-type block-cyclic data distribution */
    /* Calculate some properties associated with data distribution */
    int *proc_grid = GA[handle].nblock;
    /*num_blocks = GA[handle].num_blocks;*/
    block_dims = GA[handle].block_dims;
    for (j=0; j<ndim; j++)  {
      hdl->blk_size[j] = block_dims[j];
      hdl->blk_dim[j] = block_dims[j]*proc_grid[j];
      hdl->blk_num[j] = GA[handle].dims[j]/hdl->blk_dim[j];
      hdl->blk_inc[j] = GA[handle].dims[j]-hdl->blk_num[j]*hdl->blk_dim[j];
      hdl->blk_ld[j] = hdl->blk_num[j]*block_dims[j];
      hdl->hlf_blk[j] = hdl->blk_inc[j]/block_dims[j];
    }
    hdl->iproc = 0;
    hdl->offset = 0;
    /* Initialize proc_index and index arrays */
    gam_find_proc_indices(handle, hdl->iproc, hdl->proc_index);
    gam_find_proc_indices(handle, hdl->iproc, hdl->index);
  } else if (GA[handle].distr_type == TILED)  {
    int j;
    C_Integer *block_dims;
    hdl->iproc = 0;
    hdl->offset = 0;
    block_dims = GA[handle].block_dims;
    for (j=0; j<ndim; j++)  {
      hdl->blk_size[j] = block_dims[j];
    }
    /* Initialize proc_index and index arrays */
    gam_find_tile_proc_indices(handle, hdl->iproc, hdl->proc_index);
    gam_find_tile_proc_indices(handle, hdl->iproc, hdl->index);
  } else if (GA[handle].distr_type == TILED_IRREG)  {
    int j;
    hdl->iproc = 0;
    hdl->offset = 0;
    hdl->mapc =  GA[handle].mapc;
    /* Initialize proc_index and index arrays */
    gam_find_tile_proc_indices(handle, hdl->iproc, hdl->proc_index);
    gam_find_tile_proc_indices(handle, hdl->iproc, hdl->index);
  }
}

/**
 * Reset an iterator back to the start
 * @param hdl handle for iterator
 */
void gai_iterator_reset(_iterator_hdl *hdl)
{
  Integer handle = GA_OFFSET + hdl->g_a;
  if (GA[handle].distr_type == REGULAR) {
    /* Regular data distribution */
    hdl->count = 0;
  } else if (GA[handle].distr_type == BLOCK_CYCLIC) {
    /* simple block cyclic data distribution */
    hdl->iproc = 0;
    hdl->iblock = 0;
    hdl->offset = 0;
  } else if (GA[handle].distr_type == SCALAPACK) {
    hdl->iproc = 0;
    hdl->offset = 0;
    /* Initialize proc_index and index arrays */
    gam_find_proc_indices(handle, hdl->iproc, hdl->proc_index);
    gam_find_proc_indices(handle, hdl->iproc, hdl->index);
  } else if (GA[handle].distr_type == TILED ||
      GA[handle].distr_type == TILED_IRREG)  {
    hdl->iproc = 0;
    hdl->offset = 0;
    /* Initialize proc_index and index arrays */
    gam_find_tile_proc_indices(handle, hdl->iproc, hdl->proc_index);
    gam_find_tile_proc_indices(handle, hdl->iproc, hdl->index);
  }
}

/**
 * Get the next sub-block from the larger block defined when the iterator was
 * initialized
 * @param hdl handle for iterator
 * @param proc processor on which the next block resides
 * @param plo indices for lower corner of remote block
 * @param phi indices for upper corner of remote block
 * @param prem pointer to remote buffer
 * @return returns false if there is no new block, true otherwise
 */
int gai_iterator_next(_iterator_hdl *hdl, int *proc, Integer *plo[],
    Integer *phi[], char **prem, Integer ldrem[])
{
  Integer idx, i, p;
  Integer handle = GA_OFFSET + hdl->g_a;
  Integer p_handle = GA[handle].p_handle;
  Integer n_rstrctd = GA[handle].num_rstrctd;
  Integer *rank_rstrctd = GA[handle].rank_rstrctd;
  Integer elemsize = GA[handle].elemsize;
  int ndim;
  int grp = GA[handle].p_handle;
  int nproc = pnga_pgroup_nnodes(grp);
  int ok;
  ndim = GA[handle].ndim;
  if (GA[handle].distr_type == REGULAR) {
    Integer *blo, *bhi;
    Integer nelems;
    idx = hdl->count;

    /* Check to see if range is valid (it may not be valid if user has
     * created and irregular distribution in which some processors do not have
     * data). If invalid, skip this block and go to the next one
     */
    ok = 0;
    while(!ok) {
      /* no blocks left, so return */
      if (idx>=hdl->nproc) return 0;
      p = (Integer)hdl->proclistperm[idx];
      *proc = (int)hdl->proclist[p];
      if (p_handle >= 0) {
        *proc = (int)PGRP_LIST[p_handle].inv_map_proc_list[*proc];
      }
      /* Find  visible portion of patch held by processor p and
       * return the result in plo and phi. Also get actual processor
       * index corresponding to p and store the result in proc.
       */
      gam_GetRangeFromMap(p, ndim, plo, phi);
      ok = 1;
      for (i=0; i<ndim; i++) {
        if (phi[i]<plo[i]) {
          ok = 0;
          break;
        }
      }
      if (ok) {
        *proc = (int)hdl->proclist[p];
        blo = *plo;
        bhi = *phi;
#ifdef LARGE_BLOCK_REQ
        /* Check to see if block size will overflow int values and initialize
         * counter over sub-blocks if the block is too big*/
        if (!hdl->oversize) {
          nelems = 1; 
          for (i=0; i<ndim; i++) nelems *= (bhi[i]-blo[i]+1);
          if (elemsize*nelems > MAX_INT_VALUE) {
            Integer maxint = 0;
            int maxidx;
            hdl->oversize = 1;
            /* Figure out block dimensions that correspond to block sizes
             * that are beneath MAX_INT_VALUE */
            for (i=0; i<ndim; i++) {
              hdl->blk_size[i] = (bhi[i]-blo[i]+1);
            }
            while (elemsize*nelems > MAX_INT_VALUE) {
              for (i=0; i<ndim; i++) {
                if (hdl->blk_size[i] > maxint) {
                  maxidx = i;
                  maxint = hdl->blk_size[i];
                }
              }
              hdl->blk_size[maxidx] /= 2;
              nelems = 1;
              for (i=0; i<ndim; i++) nelems *= hdl->blk_size[i];
            }
            /* Calculate the number of blocks along each dimension */
            for (i=0; i<ndim; i++) {
              hdl->blk_dim[i] = (bhi[i]-blo[i]+1)/hdl->blk_size[i];
              if (hdl->blk_dim[i]*hdl->blk_size[i] < (bhi[i]-blo[i]+1))
                hdl->blk_dim[i]++;
            }
            /* initialize block counting */
            for (i=0; i<ndim; i++) hdl->blk_inc[i] = 0;
          }
        }

        /* Get sub-block bounding dimensions */
        if (hdl->oversize) {
          Integer tmp;
          for (i=0; i<ndim; i++) {
            hdl->lobuf[i] = blo[i];
            hdl->hibuf[i] = bhi[i];
          }
          *plo = hdl->lobuf;
          *phi = hdl->hibuf;
          blo = *plo;
          bhi = *phi;
          for (i=0; i<ndim; i++) {
            hdl->lobuf[i] += hdl->blk_inc[i]*hdl->blk_size[i];
            tmp = hdl->lobuf[i] + hdl->blk_size[i]-1;
            if (tmp < hdl->hibuf[i]) hdl->hibuf[i] = tmp;
          }
        }
#endif

        if (n_rstrctd == 0) {
          gam_Location(*proc, handle, blo, prem, ldrem);
        } else {
          gam_Location(rank_rstrctd[*proc], handle, blo, prem, ldrem);
        }
        if (p_handle >= 0) {
          *proc = (int)hdl->proclist[p];
          /* BJP */
          *proc = PGRP_LIST[p_handle].inv_map_proc_list[*proc];
        }
      }
#ifdef LARGE_BLOCK_REQ
      if (!hdl->oversize) {
#endif
        hdl->count++;
#ifdef LARGE_BLOCK_REQ
      } else {
        /* update blk_inc array */
        hdl->blk_inc[0]++; 
        for (i=0; i<ndim-1; i++) {
          if (hdl->blk_inc[i] >= hdl->blk_dim[i]) {
            hdl->blk_inc[i] = 0;
            hdl->blk_inc[i+1]++;
          }
        }
        if (hdl->blk_inc[ndim-1] >= hdl->blk_dim[ndim-1]) {
          hdl->count++;
          hdl->oversize = 0;
        }
      }
#endif
    }
    return 1;
  } else {
    Integer offset, l_offset, last, pinv;
    Integer blk_tot = GA[handle].block_total;
    Integer blo[MAXDIM], bhi[MAXDIM];
    Integer idx, j, jtot, chk, iproc;
    int check1, check2;
    if (GA[handle].distr_type == BLOCK_CYCLIC) {
      /* Simple block-cyclic distribution */
      if (hdl->iproc >= nproc) return 0;
      /*if (hdl->iproc == nproc-1 && hdl->iblock >= blk_tot) return 0;*/
      if (hdl->iblock == hdl->iproc) hdl->offset = 0;
      chk = 0;
      /* loop over blocks until a block with data is found */
      while (!chk) {
        /* get the block corresponding to the current value of iblock */
        idx = hdl->iblock;
        ga_ownsM(handle,idx,blo,bhi);
        /* check to see if this block overlaps with requested block
         * defined by lo and hi */
        chk = 1;
        for (j=0; j<ndim; j++) {
          /* check to see if at least one end point of the interval
           * represented by blo and bhi falls in the interval
           * represented by lo and hi */
          check1 = ((blo[j] >= hdl->lo[j] && blo[j] <= hdl->hi[j]) ||
              (bhi[j] >= hdl->lo[j] && bhi[j] <= hdl->hi[j]));
          /* check to see if interval represented by lo and hi
           * falls entirely within interval represented by blo and bhi */
          check2 = ((hdl->lo[j] >= blo[j] && hdl->lo[j] <= bhi[j]) &&
              (hdl->hi[j] >= blo[j] && hdl->hi[j] <= bhi[j]));
          /* If there is some data, move to the next section of code,
           * otherwise, check next block */
          if (!check1 && !check2) {
            chk = 0;
          }
        }
        
        if (!chk) {
          /* evaluate new offset for block idx */
          jtot = 1;
          for (j=0; j<ndim; j++) {
            jtot *= bhi[j]-blo[j]+1;
          }
          hdl->offset += jtot;
          /* increment to next block */
          hdl->iblock += pnga_nnodes();
          if (hdl->iblock >= blk_tot) {
            hdl->offset = 0;
            hdl->iproc++;
            hdl->iblock = hdl->iproc;
            if (hdl->iproc >= nproc) return 0;
          }
        }
      }

      /* The block overlaps some data in lo,hi */
      if (chk) {
        Integer *clo, *chi;
        *plo = hdl->lobuf;
        *phi = hdl->hibuf;
        clo = *plo;
        chi = *phi;
        /* get the patch of block that overlaps requested region */
        gam_GetBlockPatch(blo,bhi,hdl->lo,hdl->hi,clo,chi,ndim);

        /* evaluate offset within block */
        last = ndim - 1;
        jtot = 1;
        if (last == 0) ldrem[0] = bhi[0] - blo[0] + 1;
        l_offset = 0;
        for (j=0; j<last; j++) {
          l_offset += (clo[j]-blo[j])*jtot;
          ldrem[j] = bhi[j]-blo[j]+1;
          jtot *= ldrem[j];
        }
        l_offset += (clo[last]-blo[last])*jtot;
        l_offset += hdl->offset;

        /* get pointer to data on remote block */
        pinv = idx%nproc;
        if (p_handle > 0) {
          pinv = PGRP_LIST[p_handle].inv_map_proc_list[pinv];
        }
        *prem =  GA[handle].ptr[pinv]+l_offset*GA[handle].elemsize;
        *proc = pinv;

        /* evaluate new offset for block idx */
        jtot = 1;
        for (j=0; j<ndim; j++) {
          jtot *= bhi[j]-blo[j]+1;
        }
        hdl->offset += jtot;

        hdl->iblock += pnga_nnodes();
        if (hdl->iblock >= blk_tot) {
          hdl->iproc++;
          hdl->iblock = hdl->iproc;
          hdl->offset = 0;
        }
      }
      return 1;
    } else if (GA[handle].distr_type == SCALAPACK ||
        GA[handle].distr_type == TILED ||
        GA[handle].distr_type == TILED_IRREG) {
      /* Scalapack-type data distribution */
      Integer proc_index[MAXDIM], index[MAXDIM];
      Integer itmp;
      Integer blk_jinc;
      /* Return false at the end of the iteration */
      if (hdl->iproc >= nproc) return 0;
      chk = 0;
      /* loop over blocks until a block with data is found */
      while (!chk) {
        /* get bounds for current block */
        if (GA[handle].distr_type == SCALAPACK ||
            GA[handle].distr_type == TILED) {
          for (j = 0; j < ndim; j++) {
            blo[j] = hdl->blk_size[j]*(hdl->index[j])+1;
            bhi[j] = hdl->blk_size[j]*(hdl->index[j]+1);
            if (bhi[j] > GA[handle].dims[j]) bhi[j] = GA[handle].dims[j];
          }
        } else {
          offset = 0;
          for (j = 0; j < ndim; j++) {
            blo[j] = GA[handle].mapc[offset+hdl->index[j]];
            if (hdl->index[j] == GA[handle].num_blocks[j]-1) {
              bhi[j] = GA[handle].dims[j];
            } else {
              bhi[j] = GA[handle].mapc[offset+hdl->index[j]+1]-1;
            }
            offset += GA[handle].num_blocks[j];
          }
        }
        /* check to see if this block overlaps with requested block
         * defined by lo and hi */
        chk = 1;
        for (j=0; j<ndim; j++) {
          /* check to see if at least one end point of the interval
           * represented by blo and bhi falls in the interval
           * represented by lo and hi */
          check1 = ((blo[j] >= hdl->lo[j] && blo[j] <= hdl->hi[j]) ||
              (bhi[j] >= hdl->lo[j] && bhi[j] <= hdl->hi[j]));
          /* check to see if interval represented by lo and hi
           * falls entirely within interval represented by blo and bhi */
          check2 = ((hdl->lo[j] >= blo[j] && hdl->lo[j] <= bhi[j]) &&
              (hdl->hi[j] >= blo[j] && hdl->hi[j] <= bhi[j]));
          /* If there is some data, move to the next section of code,
           * otherwise, check next block */
          if (!check1 && !check2) {
            chk = 0;
          }
        }
        
        if (!chk) {
          /* update offset for block */
          itmp = 1;
          for (j=0; j<ndim; j++) {
            itmp *= bhi[j]-blo[j]+1;
          }
          hdl->offset += itmp;

          /* increment to next block */
          hdl->index[0] += GA[handle].nblock[0];
          for (j=0; j<ndim; j++) {
            if (hdl->index[j] >= GA[handle].num_blocks[j] && j < ndim-1) {
              hdl->index[j] = hdl->proc_index[j];
              hdl->index[j+1] += GA[handle].nblock[j+1];
            }
          }
          if (hdl->index[ndim-1] >= GA[handle].num_blocks[ndim-1]) {
            /* last iteration has been completed on current processor. Go
             * to next processor */
            hdl->iproc++;
            if (hdl->iproc >= nproc) return 0;
            hdl->offset = 0;
            if (GA[handle].distr_type == TILED ||
                GA[handle].distr_type == TILED_IRREG) {
              gam_find_tile_proc_indices(handle, hdl->iproc, hdl->proc_index);
              gam_find_tile_proc_indices(handle, hdl->iproc, hdl->index);
            } else if (GA[handle].distr_type == SCALAPACK) {
              gam_find_proc_indices(handle, hdl->iproc, hdl->proc_index);
              gam_find_proc_indices(handle, hdl->iproc, hdl->index);
            }
          }
        }
      }
      if (chk) {
        Integer *clo, *chi;
        *plo = hdl->lobuf;
        *phi = hdl->hibuf;
        clo = *plo;
        chi = *phi;
        /* get the patch of block that overlaps requested region */
        gam_GetBlockPatch(blo,bhi,hdl->lo,hdl->hi,clo,chi,ndim);

        /* evaluate offset within block */
        last = ndim - 1;
        if (GA[handle].distr_type == TILED ||
            GA[handle].distr_type == TILED_IRREG) {
          jtot = 1;
          if (last == 0) ldrem[0] = bhi[0] - blo[0] + 1;
          l_offset = 0;
          for (j=0; j<last; j++) {
            l_offset += (clo[j]-blo[j])*jtot;
            ldrem[j] = bhi[j]-blo[j]+1;
            jtot *= ldrem[j];
          }
          l_offset += (clo[last]-blo[last])*jtot;
          l_offset += hdl->offset;
        } else if (GA[handle].distr_type == SCALAPACK) {
          l_offset = 0;
          jtot = 1;
          for (j=0; j<last; j++)  {
            ldrem[j] = hdl->blk_ld[j];
            blk_jinc = GA[handle].dims[j]%hdl->blk_size[j];
            if (hdl->blk_inc[j] > 0) {
              if (hdl->proc_index[j]<hdl->hlf_blk[j]) {
                blk_jinc = hdl->blk_size[j];
              } else if (hdl->proc_index[j] == hdl->hlf_blk[j]) {
                blk_jinc = hdl->blk_inc[j]%hdl->blk_size[j];
              } else {
                blk_jinc = 0;
              }
            }
            ldrem[j] += blk_jinc;
            l_offset += (clo[j]-blo[j]
                + ((blo[j]-1)/hdl->blk_dim[j])*hdl->blk_size[j])*jtot;
            jtot *= ldrem[j];
          }
          l_offset += (clo[last]-blo[last]
              + ((blo[last]-1)/hdl->blk_dim[j])*hdl->blk_size[last])*jtot;
        }
        /* get pointer to data on remote block */
        pinv = (hdl->iproc)%nproc;
        if (p_handle > 0) {
          pinv = PGRP_LIST[p_handle].inv_map_proc_list[pinv];
        }
        *prem =  GA[handle].ptr[pinv]+l_offset*GA[handle].elemsize;
        *proc = pinv;

        /* evaluate new offset for block */
        itmp = 1;
        for (j=0; j<ndim; j++) {
          itmp *= bhi[j]-blo[j]+1;
        }
        hdl->offset += itmp;
        /* increment to next block */
        hdl->index[0] += GA[handle].nblock[0];
        for (j=0; j<ndim; j++) {
          if (hdl->index[j] >= GA[handle].num_blocks[j] && j < ndim-1) {
            hdl->index[j] = hdl->proc_index[j];
            hdl->index[j+1] += GA[handle].nblock[j+1];
          }
        }
        if (hdl->index[ndim-1] >= GA[handle].num_blocks[ndim-1]) {
          hdl->iproc++;
          hdl->offset = 0;
          if (GA[handle].distr_type == TILED ||
              GA[handle].distr_type == TILED_IRREG) {
            gam_find_tile_proc_indices(handle, hdl->iproc, hdl->proc_index);
            gam_find_tile_proc_indices(handle, hdl->iproc, hdl->index);
          } else if (GA[handle].distr_type == SCALAPACK) {
            gam_find_proc_indices(handle, hdl->iproc, hdl->proc_index);
            gam_find_proc_indices(handle, hdl->iproc, hdl->index);
          }
        }
      }
    }
    return 1;
  }
  return 0;
}

/**
 * Return true if this is the last data packet for this iterator
 */
int gai_iterator_last(_iterator_hdl *hdl)
{
  Integer idx;
  Integer handle = GA_OFFSET + hdl->g_a;
  Integer p_handle = GA[handle].p_handle;
  Integer n_rstrctd = GA[handle].num_rstrctd;
  Integer *rank_rstrctd = GA[handle].rank_rstrctd;
  Integer elemsize = GA[handle].elemsize;
  int ndim;
  int grp = GA[handle].p_handle;
  int nproc = pnga_pgroup_nnodes(grp);
  ndim = GA[handle].ndim;
  if (GA[handle].distr_type == REGULAR) {
    idx = hdl->count;
    /* no blocks left after this iteration */
    if (idx>=hdl->nproc) return 1;
  } else {
    if (GA[handle].distr_type == BLOCK_CYCLIC) {
      /* Simple block-cyclic distribution */
      if (hdl->iproc >= nproc) return 1;
    } else if (GA[handle].distr_type == SCALAPACK ||
        GA[handle].distr_type == TILED ||
        GA[handle].distr_type == TILED_IRREG) {
      if (hdl->iproc >= nproc) return 1;
    }
  }
  return 0;
}

/**
 * Clean up iterator
 */
void gai_iterator_destroy(_iterator_hdl *hdl)
{
    free(hdl->map);
    free(hdl->proclist);
    free(hdl->proclistperm);
}

/**
 * Functions that iterate over locally held blocks in a global array. These
 * are used in some routines such as copy
 */

/**
 * Initialize a local iterator
 * @param g_a global array handle
 * @param hdl handle for iterator
 */
void pnga_local_iterator_init(Integer g_a, _iterator_hdl *hdl)
{
  Integer handle = GA_OFFSET + g_a;
  Integer ndim = GA[handle].ndim;
  Integer grp = (Integer)GA[handle].p_handle;
  hdl->g_a = g_a;
  hdl->count = 0;
  hdl->oversize = 0;
  /* If standard GA distribution then no additional action needs to be taken */
  if (GA[handle].distr_type == BLOCK_CYCLIC) {
    /* GA uses simple block cyclic data distribution */
    hdl->count = pnga_pgroup_nodeid(grp);
  } else if (GA[handle].distr_type == SCALAPACK) {
    /* GA uses ScaLAPACK block cyclic data distribution */
    int j;
    Integer me = pnga_pgroup_nodeid(grp);
    /* Calculate some properties associated with data distribution */
    for (j=0; j<ndim; j++)  {
      hdl->blk_size[j] = GA[handle].block_dims[j];
      hdl->blk_num[j] = GA[handle].num_blocks[j];
      hdl->blk_inc[j] = GA[handle].nblock[j];
      hdl->blk_dim[j] = GA[handle].dims[j];
    }
    /* Initialize proc_index and index arrays */
    gam_find_proc_indices(handle, me, hdl->proc_index);
    gam_find_proc_indices(handle, me, hdl->index);
  } else if (GA[handle].distr_type == TILED) {
    /* GA uses tiled distribution */
    int j;
    Integer me = pnga_pgroup_nodeid(grp);
    /* Calculate some properties associated with data distribution */
    for (j=0; j<ndim; j++)  {
      hdl->blk_size[j] = GA[handle].block_dims[j];
      hdl->blk_num[j] = GA[handle].num_blocks[j];
      hdl->blk_inc[j] = GA[handle].nblock[j];
      hdl->blk_dim[j] = GA[handle].dims[j];
    }
    /* Initialize proc_index and index arrays */
    gam_find_tile_proc_indices(handle, me, hdl->proc_index);
    gam_find_tile_proc_indices(handle, me, hdl->index);
  } else if (GA[handle].distr_type == TILED_IRREG) {
    /* GA uses irregular tiled distribution */
    int j;
    Integer me = pnga_pgroup_nodeid(grp);
    hdl->mapc = GA[handle].mapc;
    /* Calculate some properties associated with data distribution */
    for (j=0; j<ndim; j++)  {
      hdl->blk_num[j] = GA[handle].num_blocks[j];
      hdl->blk_inc[j] = GA[handle].nblock[j];
      hdl->blk_dim[j] = GA[handle].dims[j];
    }
    /* Initialize proc_index and index arrays */
    gam_find_tile_proc_indices(handle, me, hdl->proc_index);
    gam_find_tile_proc_indices(handle, me, hdl->index);
  }
}

/**
 * Get the next sub-block from local portion of global array
 * @param hdl handle for iterator
 * @param plo indices for lower corner of block
 * @param phi indices for upper corner of block
 * @param ptr pointer to local buffer
 * @param ld array of strides for local block
 * @return returns false if there is no new block, true otherwise
 */
int pnga_local_iterator_next(_iterator_hdl *hdl, Integer plo[],
    Integer phi[], char **ptr, Integer ld[])
{
  Integer i;
  Integer handle = GA_OFFSET + hdl->g_a;
  Integer grp = GA[handle].p_handle;
  Integer elemsize = GA[handle].elemsize;
  int ndim;
  int me = pnga_pgroup_nodeid(grp);
  ndim = GA[handle].ndim;
  if (GA[handle].distr_type == REGULAR) {
    Integer nelems;
    /* no blocks left, so return */
    if (hdl->count>0) return 0;

    /* Find  visible portion of patch held by this processor and
     * return the result in plo and phi. Return pointer to local
     * data as well
     */
    pnga_distribution(hdl->g_a, me, plo, phi);
    /* Check to see if this process has any data. Return 0 if
     * it does not */
    for (i=0; i<ndim; i++) {
      if (phi[i]<plo[i]) return 0;
    }
    pnga_access_ptr(hdl->g_a,plo,phi,ptr,ld);
    hdl->count++;
  } else if (GA[handle].distr_type == BLOCK_CYCLIC) {
    /* Simple block-cyclic distribution */
    if (hdl->count >= pnga_total_blocks(hdl->g_a)) return 0;
    pnga_distribution(hdl->g_a,hdl->count,plo,phi);
    pnga_access_block_ptr(hdl->g_a,hdl->count,ptr,ld);
    hdl->count += pnga_pgroup_nnodes(grp);
  } else if (GA[handle].distr_type == SCALAPACK ||
      GA[handle].distr_type == TILED) {
    /* Scalapack-type data distribution */
    if (hdl->index[ndim-1] >= hdl->blk_num[ndim-1]) return 0;
    /* Find coordinates of bounding block */
    for (i=0; i<ndim; i++) {
      plo[i] = hdl->index[i]*hdl->blk_size[i]+1;
      phi[i] = (hdl->index[i]+1)*hdl->blk_size[i];
      if (phi[i] > hdl->blk_dim[i]) phi[i] = hdl->blk_dim[i];
    }
    pnga_access_block_grid_ptr(hdl->g_a,hdl->index,ptr,ld);
    hdl->index[0] += hdl->blk_inc[0];
    for (i=0; i<ndim; i++) {
      if (hdl->index[i] >= hdl->blk_num[i] && i<ndim-1) {
        hdl->index[i] = hdl->proc_index[i];
        hdl->index[i+1] += hdl->blk_inc[i+1];
      }
    }
  } else if (GA[handle].distr_type == TILED_IRREG) {
    /* Irregular tiled data distribution */
    Integer offset = 0;
    if (hdl->index[ndim-1] >= hdl->blk_num[ndim-1]) return 0;
    /* Find coordinates of bounding block */
    for (i=0; i<ndim; i++) {
      plo[i] = hdl->mapc[offset+hdl->index[i]];
      if (hdl->index[i] < hdl->blk_num[i]-1) {
        phi[i] = hdl->mapc[offset+hdl->index[i]+1]-1;
      } else {
        phi[i] = GA[handle].dims[i];
      }
      offset += GA[handle].num_blocks[i];
    }
    pnga_access_block_grid_ptr(hdl->g_a,hdl->index,ptr,ld);
    hdl->index[0] += hdl->blk_inc[0];
    for (i=0; i<ndim; i++) {
      if (hdl->index[i] >= hdl->blk_num[i] && i<ndim-1) {
        hdl->index[i] = hdl->proc_index[i];
        hdl->index[i+1] += hdl->blk_inc[i+1];
      }
    }
  }
  return 1;
}
