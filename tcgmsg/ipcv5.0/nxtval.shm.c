/* $Id: nxtval.shm.c,v 1.3 2002-01-28 19:42:04 d3h325 Exp $ */

#include "tcgmsgP.h"
long *nxtval_shmem;


#define LEN 2
long nxtval_counter=0;
#define INCR 1                 /* increment for NXTVAL */
#define BUSY -1L               /* indicates somebody else updating counter*/


#if defined(__i386__) && defined(__GNUC__)
#   define TESTANDSET testandset
#   define LOCK if(nproc>1)acquire_spinlock((int*)(nxtval_shmem+1))
#   define UNLOCK if(nproc>1)release_spinlock((int*)(nxtval_shmem+1))

static inline int testandset(int *spinlock)
{
  int ret;
  __asm__ __volatile__("xchgl %0, %1"
        : "=r"(ret), "=m"(*spinlock)
        : "0"(1), "m"(*spinlock));

  return ret;
}

static void acquire_spinlock(int *mutex)
{
int loop=0, maxloop =100;
   while (TESTANDSET(mutex)){
      loop++;
      if(loop==maxloop){ usleep(1); loop=0; }
  }
}

static release_spinlock(int *mutex)
{
   *mutex =0;
}

#endif

#ifndef LOCK
#   define LOCK  if(nproc>1)Error("nxtval: sequential version with silly mproc ", (Integer) *mproc);
#   define UNLOCK
#endif


Integer NXTVAL_(mproc)
     Integer  *mproc;
/*
  Get next value of shared counter.

  mproc > 0 ... returns requested value
  mproc < 0 ... server blocks until abs(mproc) processes are queued
                and returns junk
  mproc = 0 ... indicates to server that I am about to terminate

*/
{
  long shmem_swap();
  long local;
  long sync_type= INTERNAL_SYNC_TYPE;
  long nproc=  NNODES_(); 
  long server=nproc-1; 

     if (DEBUG_) {
       (void) printf("%2ld: nxtval: mproc=%ld\n",NODEID_(), *mproc);
       (void) fflush(stdout);
     }

     if (*mproc < 0) {
           SYNCH_(&sync_type);
           /* reset the counter value to zero */
           if( NODEID_() == server) nxtval_counter = 0;
           SYNCH_(&sync_type);
     }
     if (*mproc > 0) {
           LOCK;
             local = nxtval_counter;
             nxtval_counter += INCR;
           UNLOCK;
     }

     return (Integer)local;
}

