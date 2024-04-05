# Makefile for use with GNU make

THIS_MAKEFILE_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
THIS_DIR:=$(shell basename "${THIS_MAKEFILE_DIR}")
THIS_MAKEFILE:=$(lastword $(MAKEFILE_LIST))

.POSIX:
.SUFFIXES:
.SUFFIXES: .o .c .a

CONFIGFILE ?= config.mk
$(info Using config file ${CONFIGFILE})
include ${CONFIGFILE}

CC ?= cc
AWK ?= awk
AR ?= ar
RANLIB ?= ranlib
SED ?= sed

WIN=
DEBUG=0
ifeq ($(WIN),)
  WIN=0
  ifneq ($(findstring w64,$(CC)),) # e.g. mingw64
    WIN=1
  endif
endif

CFLAGS+=${CFLAG_O} ${CFLAGS_OPT}
CFLAGS+=${CFLAGS_AUTO}
CFLAGS+=-I.

ifeq ($(VERBOSE),1)
  CFLAGS+= ${CFLAGS_VECTORIZE_OPTIMIZED} ${CFLAGS_VECTORIZE_MISSED} ${CFLAGS_VECTORIZE_ALL}
endif

VERSION= $(shell (git describe --always --dirty --tags 2>/dev/null || echo "v0.0.0-jsonwriter") | sed 's/^v//')

ifneq ($(findstring emcc,$(CC)),) # emcc
  NO_THREADING=1
endif

ifeq ($(NO_THREADING),1)
  CFLAGS+= -DNO_THREADING
endif

ifeq ($(DEBUG),0)
  CFLAGS+= -DNDEBUG -O3  ${CFLAGS_LTO}
else
  CFLAGS += ${CFLAGS_DEBUG}
endif

ifeq ($(DEBUG),1)
  DBG_SUBDIR+=dbg
else
  DBG_SUBDIR+=rel
endif

ifeq ($(WIN),0)
  BUILD_SUBDIR=$(shell uname)/${DBG_SUBDIR}
  WHICH=which
  EXE=
  CFLAGS+= -fPIC
else
  BUILD_SUBDIR=win/${DBG_SUBDIR}
  WHICH=where
  EXE=.exe
  CFLAGS+= -fpie
  CFLAGS+= -D__USE_MINGW_ANSI_STDIO -D_ISOC99_SOURCE -Wl,--strip-all
endif

CFLAGS+= -std=gnu11 -Wno-gnu-statement-expression -Wshadow -Wall -Wextra -Wno-missing-braces -pedantic -D_GNU_SOURCE

CFLAGS+= ${JSONWRITER_OPTIONAL_CFLAGS}

CCBN=$(shell basename ${CC})
THIS_LIB_BASE=$(shell cd .. && pwd)
INCLUDE_DIR=${THIS_LIB_BASE}/include
BUILD_DIR=${THIS_LIB_BASE}/build/${BUILD_SUBDIR}/${CCBN}

NO_UTF8_CHECK=1

LIB_SUFFIX?=
JSONWRITER_OBJ=${BUILD_DIR}/objs/jsonwriter.o
LIBJSONWRITER_A=libjsonwriter${LIB_SUFFIX}.a
LIBJSONWRITER=${BUILD_DIR}/lib/${LIBJSONWRITER_A}
LIBJSONWRITER_INSTALL=${LIBDIR}/${LIBJSONWRITER_A}

JSONWRITER_OBJ_OPTS=
ifeq ($(NO_UTF8_CHECK),1)
  JSONWRITER_OBJ_OPTS+= -DNO_UTF8_CHECK
endif

help:
	@echo "Make options:"
	@echo "  `basename ${MAKE}` build|install|uninstall|clean"
	@echo
	@echo "Optional make variables:"
	@echo "  [CONFIGFILE=config.mk] [NO_UTF8_CHECK=1] [VERBOSE=1] [LIBDIR=${LIBDIR}] [INCLUDEDIR=${INCLUDEDIR}] [LIB_SUFFIX=]"
	@echo

build: ${LIBJSONWRITER}

${LIBJSONWRITER}: ${JSONWRITER_OBJ}
	@mkdir -p `dirname "$@"`
	@rm -f $@
	$(AR) rcv $@ $?
	$(RANLIB) $@
	$(AR) -t $@ # check it is there
	@echo Built $@

install: ${LIBJSONWRITER_INSTALL}
	@mkdir -p  $(INCLUDEDIR)
	@cp -pR *.h $(INCLUDEDIR)
	@echo "include files copied to $(INCLUDEDIR)"

${LIBJSONWRITER_INSTALL}: ${LIBJSONWRITER}
	@mkdir -p `dirname "$@"`
	@cp -p ${LIBJSONWRITER} "$@"
	@echo "libjsonwriter installed to $@"

uninstall:
	@rm -rf ${INCLUDEDIR}/jsonwriter*
	 rm  -f ${LIBDIR}/libjsonwriter*

clean:
	rm -rf ${BUILD_DIR}/objs ${LIBJSONWRITER}

.PHONY: build install uninstall clean  ${LIBJSONWRITER_INSTALL}

${BUILD_DIR}/objs/jsonwriter.o: jsonwriter.c jsonwriter.h
	@mkdir -p `dirname "$@"`
	${CC} ${CFLAGS} -DJSONWRITER_VERSION=\"${VERSION}\" -I${INCLUDE_DIR} ${JSONWRITER_OBJ_OPTS} -o $@ -c $<
