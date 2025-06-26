#if HAVE_CONFIG_H
#   include "config.fh"
#endif

#define NDIM 1024
#define ISEED 228103

c
c  Test individual functions for sparse arrays using the fortran interface
c  These tests only test double precision fortran arrays and are primarily
c  focused on the correctness of the fortran interface. They assume that
c  correctness of the underlying operations has already been verified by the
c  corresponding tests using the c-interface
c
      program main
      implicit none
#include "mafdecls.fh"
#include "global.fh"
#include "mpif.h"
      integer ierr
c
c   Initialize GA
c
      call mpi_init(ierr)
      call ga_initialize

      call matrix_test
      call ga_terminate
c
c  Tidy up after message-passing library
c
      call mpi_finalize(ierr)
      stop
      end

c
c  subroutine to set up a sparse matrix for testing purposes
c  @param s_a sparse matrix handle
c  @param dim dimension of sparse matrix
c  @param type data type used by sparse matrix
c
      subroutine setup_matrix(s_a, dim)
      implicit none
#include "mafdecls.fh"
#include "global.fh"
      integer s_a
      integer dim
      integer jlo, jhi, idx
      integer me, nprocs
      integer i, j
      integer skip_len
      double precision d_val, o_val
      integer size
      integer nskip

      me = ga_nodeid()
      nprocs = ga_nnodes()

      nskip = 5
c
c    Create sparse matrix
c
      s_a = nga_sprs_array_create(dim, dim, MT_DBL)
c
c    Determine column block set by me
c
      jlo = dim*me/nprocs
      jhi = dim*(me+1)/nprocs-1
      if (me.eq.nprocs-1) jhi = dim-1

      d_val = 2.0d00
      o_val = -1.0d00
c
c    loop over all columns in column block and add elements for each column.
c    Currently assume that each column has 5 elements, one on the diagonal 
c    and 4 others off the diagonl. Final matrix is partitioned into row blocks
c    so this guarantees that sorting routines for elements are tested */
c
      skip_len = dim/nskip
      if (skip_len.lt.2) then
        nskip = dim/2
        skip_len = dim/nskip
      endif
      do j = jlo, jhi
        call nga_sprs_array_add_element(s_a,j,j,d_val)
        do i = 0, nskip-2
          idx = mod((j+(i+1)*skip_len),dim)
          call nga_sprs_array_add_element(s_a,idx,j,o_val)
        end do
      end do

      if (nga_sprs_array_assemble(s_a).and.me.eq.0) then
        write(6,'(a)') " "
        write(6,'(a)') "  Sparse array assembly completed"
        write(6,'(a)') " "
      endif
      end subroutine

c
c  subroutine to set up a dense matrix for testing purposes
c  @param g_a dense matrix handle
c  @param a pointer to a regular matrix that is equivalent to g_a
c  @param dim dimension of sparse matrix
c  @param type data type used by sparse matrix
c
      subroutine setup_dense_matrix(g_a, dim)
      implicit none
#include "mafdecls.fh"
#include "global.fh"
      integer g_a, dim
      double precision a(dim,dim)
      integer jlo, jhi, idx
      integer me, nprocs
      integer i, j;
      integer skip_len;
      double precision d_val, o_val
      integer size;
      integer nskip
      integer two
      integer dims(2),lo(2),hi(2),ld(2)
      logical status

      me = ga_nodeid()
      nprocs = ga_nnodes()
      nskip = 5
      two = 2

c      if (me.eq.0) then
c        write(6,'(a)') " "
c        write(6,'(a,i8,a,i8)')"  Create dense matrix of size ",dim," x ",dim
c        write(6,'(a)') " "
c      endif
c
c    Create dense matrix
c
      g_a = nga_create_handle()
      dims(1) = dim
      dims(2) = dim
      call nga_set_data(g_a,two,dims,MT_DBL)
      status = nga_allocate(g_a)

      d_val = 2.0d00
      o_val = -1.0d00

      skip_len = dim/nskip
      if (skip_len.lt.2) then
        nskip = dim/2
        skip_len = dim/nskip
      endif
c
c  Set all elements of a to zero
c
      do j = 1, dim
        do i = 1, dim
          a(i,j) = 0.0d00
        end do
      end do
c
c  Set non-zero value in local buffer
c
      do j = 1, dim
        a(j,j) = d_val
        do i = 0, nskip-1
          idx = mod((j-1)+i*skip_len,dim) + 1
            
          a(idx,j) =o_val
        end do
      end do
c
c  copy values in local buffer to global array */
c
      if (me.eq.0) then
        lo(1) = 1;
        hi(1) = dim;
        ld(1) = dim;
        lo(2) = 1;
        hi(2) = dim;
        ld(2) = dim;
        call nga_put(g_a,lo,hi,a,ld);
      endif
      call ga_sync();
      end subroutine

c
c  subroutine to set up a diagonal matrix for testing purposes
c  @param g_d handle to 1D array representing diagonal matrix
c  @param a pointer to a local array that is equivalent to g_d
c  @param dim dimension of sparse matrix
c  @param type data type used by sparse matrix
c
      subroutine setup_diag_matrix(g_d, dim)
      implicit none
#include "mafdecls.fh"
#include "global.fh"
      integer g_d, dim
      integer me, nprocs
      integer ilo, ihi, ld
      integer i, j
      integer size
      integer dims(1)
      integer one
      logical status
      GA_ACCESS_INDEX_TYPE idx

      me = ga_nodeid()
      nprocs = ga_nnodes()
      dims(1) = dim

c      if (me.eq.0) then
c        write(6,'(a)') " "
c        write(6,'(a,i8,a,i8)')"  Create diagonal matrix of size ",dim," x ",dim
c        write(6,'(a)') " "
c      endif

c
c   Create a 1D global array
c
      g_d = nga_create_handle()
      one = 1
      call nga_set_data(g_d,one,dims,MT_DBL)
      status = ga_allocate(g_d)
c
c   Determine row block set by me
c
      ilo = dim*me/nprocs+1
      ihi = dim*(me+1)/nprocs
      if (me.eq.nprocs-1) ihi = dim
c
c    get pointers to local data
c
      call nga_distribution(g_d,me,ilo,ihi)
      call nga_access(g_d,ilo,ihi,idx,ld)
c
c    set diagonal values
c
      do i = ilo, ihi
        dbl_mb(idx+i-ilo) = dble(i)
      end do
      call nga_release(g_d,ilo,ihi)
      call nga_sync();
c
c    make copy of g_d in local array */
c
      if (me.eq.0) then
c        write(6,'(a)') " "
c        write(6,'(a)') "  Diagonal array completed"
c        write(6,'(a)') " "
      endif
      end subroutine

      subroutine matrix_test
      implicit none
#include "mafdecls.fh"
#include "global.fh"
      integer s_a, s_b, s_c, g_a, g_b, g_c, g_d, g_sk, g_k, g_w
      integer dim
      integer size_k
      integer me, nprocs
      integer one
      integer ilo, ihi, jlo, jhi
      integer i, j, k, l, iproc
      integer ld
      integer g_v
      character(1) plus
      integer lo(2), hi(2), tld(2)
      double precision tbeg, time
      logical status
      double precision x
      double precision shift_val
c
      me = ga_nodeid()
      nprocs = ga_nnodes()
      dim = NDIM
      size_k = NDIM/2
      one = 1
c  
c    create sparse matrix
c  
      call setup_matrix(s_a, dim)
c
c    Test get column functionality
c
      do i = 1, dim
        tbeg = ga_wtime()
        g_v = nga_sprs_array_get_column(s_a, i)
        time = ga_wtime()-tbeg
        status = ga_destroy(g_v);
      end do
      plus(1:1) = '+'
      call ga_dgop(one,time,one,plus)
      time = time/dble(dim*nprocs)
      if (me.eq.0) then
        write(6,'(a,f16.8)')
     +     "    Time for matrix get column operation: ",time
      endif
c
c    extract diagonal of s_a to g_d
c
      tbeg = ga_wtime()
      call nga_sprs_array_get_diag(s_a, g_d)
      time = ga_wtime()-tbeg

      call ga_dgop(one,time,one,plus)
      time = time /dble(nprocs)
      if (me.eq.0) then
        write(6,'(a)') ' '
        write(6,'(a,f16.8)')
     +      "    Time for matrix get diagonal operation: ",time
      endif
      status = nga_destroy(g_d)

      shift_val = 1.0d00
      tbeg = ga_wtime()
      call nga_sprs_array_shift_diag(s_a, shift_val)
      time = ga_wtime()-tbeg
c
c   extract diagonal of s_a to g_d
c
      call nga_sprs_array_get_diag(s_a, g_d)

      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a)') ' '
        write(6,'(a,f16.8)')
     +     "    Time for matrix get diagonal operation: ",time
      endif
      status = nga_destroy(g_d)
      status = nga_sprs_array_destroy(s_a)

c
c    Create a fresh copy of sparse matrix
c
      call setup_matrix(s_a, dim)
c
c    Create diagonal matrix
c
      call setup_diag_matrix(g_d, dim);
c
c    Do a right hand multiply
c
      tbeg = ga_wtime()
      call nga_sprs_array_diag_right_multiply(s_a, g_d)
      time = ga_wtime()-tbeg
      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a,f16.8)')
     +      "    Time for matrix right diagonal multiply operation: ",
     +      time
      endif

      status = nga_sprs_array_destroy(s_a)
c
c    Create a fresh copy of sparse matrix
c
      call setup_matrix(s_a, dim)
c
c    Create diagonal matrix
c
      call setup_diag_matrix(g_d, dim);
c
c    Do a left hand multiply
c
      tbeg = ga_wtime()
      call nga_sprs_array_diag_left_multiply(s_a, g_d)
      time = ga_wtime()-tbeg
      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a,f16.8)')
     +      "    Time for matrix left diagonal multiply operation: ",
     +      time
      endif

      status = nga_sprs_array_destroy(s_a)
c
c   Skip over test for get_block for now
c

c
c    Create a fresh copy of sparse matrix
c
      call setup_matrix(s_a, dim)
      call setup_matrix(s_b, dim)
c
c    multiply sparse matrix A times sparse matrix B 
c
      tbeg = ga_wtime()
      s_c = nga_sprs_array_matmat_multiply(s_a, s_b)
      time = ga_wtime()-tbeg
      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a,f16.8)')
     +      "    Time for matrix-matrix multiply operation: ",time
      endif

      status = nga_sprs_array_destroy(s_a)
      status = nga_sprs_array_destroy(s_b)

c
c     create sparse matrix A
c
      call setup_matrix(s_a, dim)
c
c    Initialize random number generator */
c
      x = nga_rand(ISEED+me)

      tbeg = ga_wtime()
      g_sk = nga_sprs_array_count_sketch(s_a, size_k, g_k, g_w)
      time = ga_wtime()-tbeg
      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a,f16.8)')
     +      "    Time for count sketch operation: ",time
      endif
      status = nga_sprs_array_destroy(s_a)
      status = nga_destroy(g_k)
      status = nga_destroy(g_w)
c
c     create sparse matrix A
c
      call setup_matrix(s_a, dim)
c
c     create dense matrix B
c
      call setup_dense_matrix(g_b, dim)
c
c    multiply sparse matrix A times dense matrix B
c
      tbeg = ga_wtime()
      g_c = nga_sprs_array_sprsdns_multiply(s_a, g_b)
      time = ga_wtime()-tbeg

      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a,f16.8)')
     + "    Time for sparse-dense matrix-matrix multiply operation: ",
     +      time
      endif
      status = nga_sprs_array_destroy(s_a)
      status = nga_destroy(g_b)

c
c     create dense matrix A
c
      call setup_dense_matrix(g_a, dim)
c
c     create sparse matrix B
c
      call setup_matrix(s_b, dim)
c
c     multiply dense matrix A times sparse matrix B */
c
      tbeg = ga_wtime()
      g_c = nga_sprs_array_dnssprs_multiply(g_a, s_b)
      time = ga_wtime()-tbeg;

      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a,f16.8)')
     + "    Time for dense-sparse matrix-matrix multiply operation: ",
     +      time
      endif
      status = nga_destroy(g_a)
      status = nga_sprs_array_destroy(s_b)

c
c    create an ordinary global array with sparse non-zeros */
c
      call setup_dense_matrix(g_a, dim)
c
c    copy dense matrix g_a to sparse matrix s_a */
c
      tbeg = ga_wtime()
      s_a = nga_sprs_array_create_from_dense(g_a)
      time = ga_wtime()-tbeg

      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a)') ' '
        write(6,'(a,f16.8)')
     +   "    Time for create from dense array operation: ",time
      endif
      status = nga_destroy(g_a)
      status = nga_sprs_array_destroy(s_a)

c
c     create sparse matrix A
c
      call setup_matrix(s_a, dim)
c
c    now copy sparse matrix back to dense matrix g_a */
c
      tbeg = ga_wtime()
      g_a = nga_sprs_array_create_from_sparse(s_a)
      time = ga_wtime()-tbeg

      call ga_dgop(one,time,one,plus)
      time = time/dble(nprocs)
      if (me.eq.0) then
        write(6,'(a)') ' '
        write(6,'(a,f16.8)')
     +   "    Time for create from sparse array operation: %16.8f\n",
     +   time
      endif
      status = nga_destroy(g_a)
      status = nga_sprs_array_destroy(s_a)
      if (me.eq.0) then
        write(6,'(a)') ' '
        write(6,'(a)') "All tests completed" 
      endif
      end subroutine
