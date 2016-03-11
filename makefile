# -*- coding: utf-8 -*-

# these flags are maybe set already be petsc. But wont harm to do it here also.
DEBUG=-g   # debug
#DEBUG=-O3 # optimize
CFLAGS = -I. ${DEBUG}
FFLAGS=
CPPFLAGS=-I. ${DEBUG} -std=c++0x
FPPFLAGS=
LOCDIR=
EXAMPLESC=
EXAMPLESF=
MANSEC=
CLEANFILES=
NP=

# get first two characters of hostname.
HOST=$(shell hostname | cut -c1-2)
# we're on dtu cluster
ifeq (${HOST},n-)
	PETSC_DIR=${MODULE_PETSC_BASE_DIR}
else # no 'else if' for gnu make
ifeq (${HOST},T6) # my computer. Lazy.
	export PETSC_ARCH=linux-gnu-c-debug
	export PETSC_DIR=/home/paw/src/petsc
endif
endif

include ${PETSC_DIR}/lib/petsc/conf/variables
include ${PETSC_DIR}/lib/petsc/conf/rules
include ${PETSC_DIR}/lib/petsc/conf/test

.DEFAULT_GOAL := all
.PHONY: all
all: topopt
topopt: main.o TopOpt.o LinearElasticity.o MMA.o OC.o Filter.o PDEFilter.o MPIIO.o
	rm -rf topopt
	-${CLINKER}  $^ -g ${PETSC_SYS_LIB} -o $@

# clean is taken by petsc, thus myclean
myclean:
	rm -rf topopt *.o output* binary* log* makevtu.pyc Restart*
