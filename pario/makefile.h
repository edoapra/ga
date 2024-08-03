
OSNAME =  $(shell uname | awk '{ print $1}')

#under AIX, AIX52 is defined to use POSIX API in AIX 5.2.0.0 or greater
ifeq ($(OSNAME),AIX) 
     LIB_DEFINES += $(shell /usr/bin/oslevel | awk -F. \
                      '{ if ($$1 > 5 || ($$1 == 5 && $$2 > 1))\
                      print "-DAIX52" }')
#lsdev -C -l aio0
#aio0 Defined  Asynchronous I/O (Legacy)
ifdef USE_OLDAIO
     USE_OLDAIO=Y
else   
     USE_OLDAIO= $(shell /usr/sbin/lsdev -C -l aio0  2>&1|grep Lega|awk ' /Legacy/  {print "Y"}')
endif
      
ifeq ($(USE_OLDAIO),Y)
     LIB_DEFINES += -D_AIO_AIX_SOURCE
endif
endif

#under AIX, there can be problems with AIO and large files
ifdef LARGE_FILES

  ifeq ($(OSNAME),AIX)
    LIB_DEFINES += $(shell /usr/bin/oslevel | awk -F. \
              '{ if ($$1 > 4 || ($$1 == 4 && $$2 > 1))\
               print "-D_LARGE_FILES -D_LARGE_FILE_API" }')

#   asynchronous I/O with large files supported starting with 4.2.1
#   However, there is a bug in IBM libs on PNNL system that prevents us
#   from using AIO under 4.2.1 :-)
#
    AIO_LARGE_FILES = $(shell /usr/bin/oslevel | awk -F. \
               '{ if ($$1 == 4 && $$2 == 2 && $$3 <= 0 ) \
               print  "NO"}')
  endif  
#
# LINUX: kernel 2.4 is needed
#
  ifeq ($(TARGET), LINUX)
    LIB_DEFINES += -D_LARGEFILE64_SOURCE
    LIB_DEFINES += $(shell getconf LFS_CFLAGS)
  endif  

  LIB_DEFINES += -DLARGE_FILES
endif

ifdef LIB_TARGETS
# HPIODIR is used as a filename prefix in test programs
ifdef HPIODIR
 LIB_DEFINES += -DHPIODIR=\'$(HPIODIR)/\'
endif
endif

ifdef USE_LINUXAIO
  LIB_DEFINES += -DLINUXAIO
  COMM_LIBS += -lrt
endif
