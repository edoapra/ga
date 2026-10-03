      program minidift
      use ieee_exceptions
      implicit none
#include "mafdecls.fh"
#include "global.fh"
#include "mpif.h"
      integer g_2
      integer g_2ceri
      integer nbf,mapa(2),mapb(40),segb
      integer i,j,i0, i1, j0, j1
      integer adrc,ldc,ibuf,global,heap,stack
      logical status
      integer*4 ierr
      logical :: is_supported
      logical*4 false4
      integer*4 required, provided
      false4=.false.
      call pbeginf()
c      call mpi_init(ierr)
c      if(ierr.ne.0) 
c     c     call ga_error('failed: mpi_init',ierr)
     

      nbf = 500
      global=3*nbf*nbf
      stack=2*nbf*nbf
      heap=nbf*nbf

      call ga_initialize_ltd(ma_sizeof(mt_dbl,global,mt_byte))
      if ( ga_uses_ma() ) then
        if (.not.ma_init(mt_dbl, stack, heap+global))
     &      call ga_error(' mainit err',0)
      else
        if (.not.ma_init(mt_dbl,stack,heap))
     &      call ga_error(' mainit err',1)
      endif
      ! this must happen after GA and before MA!
c      call ga_initialize()


      if(.not.ga_create(mt_dbl,nbf,nbf,'2ceri',nbf,0,g_2ceri))
     A      call ga_error('failed: create g_c',20)
      call ga_zero(g_2ceri)
c      call ga_fill(g_2ceri,1.0d00)
      mapa(1)=1
      mapa(2)=nbf
      mapb(1)=1
      segb=max(ga_nnodes()-2,2)
      mapb(segb)=nbf
      do i=2,segb-1
         mapb(i)=(i-1)*nbf/(segb-1)
      enddo
      if(ga_nodeid().eq.0) then
         do i=1,segb
            write(6,*) i,mapb(i)
         enddo
      endif
      if (.not.ga_create_irreg(MT_DBL,nbf,nbf,'g_2',
     &       mapa,1,mapb,segb,g_2)) 
     A     call ga_error('failed: create g_c',20)
      call ga_distribution(g_2,
     .     ga_nodeid(), i0, i1, j0, j1)
      write(6,*) ga_nodeid(),' g2 matrix patch owned',i0, i1, j0, j1
      if (i0.gt.0 .and. i0.le.i1) then
         write(6,*) ga_nodeid(),'doing work'
         call ga_access(g_2, i0, i1, j0, j1, adrc, ldc)
         do j=j0,j1
            ibuf=adrc+(j-1)*nbf+i0-1
c            write(6,*) ga_nodeid(),' ibuf - adrc ',ibuf-adrc
            if(ibuf-adrc+i1-i0+1.gt.nbf*nbf) then
               call ga_error('out of bounds',
     c              ibuf-adrc-nbf*nbf+i1-i0+1)
            endif
            call dcopy(i1-i0+1, dble(j),0, dbl_mb(ibuf), 1)
         enddo
         call ga_release_update(g_2, i0, i1, j0, j1)
      endif
cdbg      call ga_print(g_2)
      call ga_copy(g_2,g_2ceri)
      call ga_symmetrize(g_2ceri)
      call gA_dgemm('t', 'n', nbf, nbf, nbf, 1d0,
     c     g_2ceri, g_2ceri, 1d0, g_2)
cdbg      call ga_print(g_2ceri)
      if (.not. ga_destroy(g_2)) call
     C     ga_error('ga_destroy failure ',0)
      if (.not. ga_destroy(g_2ceri)) call
     C     ga_error('ga_destroy failure ',0)

      call ga_terminate
      call MPi_FINALIZE(ierr)
      if(ierr.ne.0) 
     c     call ga_error('failed: mpi_finalize',ierr)
      stop
      end
      SUBROUTINE DCOPY(N,DX,INCX,DY,INCY)
*
*  -- Reference BLAS level1 routine (version 3.4.0) --
*  -- Reference BLAS is a software package provided by Univ. of Tennessee,    --
*  -- Univ. of California Berkeley, Univ. of Colorado Denver and NAG Ltd..--
*     November 2011
*
*     .. Scalar Arguments ..
      INTEGER INCX,INCY,N
*     ..
*     .. Array Arguments ..
      DOUBLE PRECISION DX(*),DY(*)
*     ..
*
*  =====================================================================
*
*     .. Local Scalars ..
      INTEGER I,IX,IY,M,MP1
*     ..
*     .. Intrinsic Functions ..
      INTRINSIC MOD
*     ..
      IF (N.LE.0) RETURN
      IF (INCX.EQ.1 .AND. INCY.EQ.1) THEN
*
*        code for both increments equal to 1
*
*
*        clean-up loop
*
         M = MOD(N,7)
         IF (M.NE.0) THEN
            DO I = 1,M
               DY(I) = DX(I)
            END DO
            IF (N.LT.7) RETURN
         END IF   
         MP1 = M + 1
         DO I = MP1,N,7
            DY(I) = DX(I)
            DY(I+1) = DX(I+1)
            DY(I+2) = DX(I+2)
            DY(I+3) = DX(I+3)
            DY(I+4) = DX(I+4)
            DY(I+5) = DX(I+5)
            DY(I+6) = DX(I+6)
         END DO
      ELSE      
*
*        code for unequal increments or equal increments
*          not equal to 1
*
         IX = 1
         IY = 1
         IF (INCX.LT.0) IX = (-N+1)*INCX + 1
         IF (INCY.LT.0) IY = (-N+1)*INCY + 1
         DO I = 1,N
            DY(IY) = DX(IX)
            IX = IX + INCX
            IY = IY + INCY
         END DO
      END IF
      RETURN
      END
c $Id$
 
