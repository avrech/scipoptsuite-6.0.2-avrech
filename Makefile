#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
#*                                                                           *
#*                  This file is part of the program and library             *
#*         SCIP --- Solving Constraint Integer Programs                      *
#*                                                                           *
#*    Copyright (C) 2002-2019 Konrad-Zuse-Zentrum                            *
#*                            fuer Informationstechnik Berlin                *
#*                                                                           *
#*  SCIP is distributed under the terms of the ZIB Academic Licence.         *
#*                                                                           *
#*  You should have received a copy of the ZIB Academic License              *
#*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      *
#*                                                                           *
#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

#@file    Makefile
#@brief   SCIP Optimization Suite Makefile
#@author  Tobias Achterberg
#@author  Ambros Gleixner
#@author  Gerald Gamrath
#@author  Matthias Miltenberger

# version of the SCIP Optimization Suite
OPTSUITEVERSION = 6.0.2

TAR		=	tar

BASEDIR		:=	$(CURDIR)

INSTALLDIR	=	/usr/local
LIBDIR		=	lib
LIBOBJDIR	=       obj
LIBOBJZIMPLDIR	=	obj/zimpl
SCIPDIR		=	$(BASEDIR)/scip
SOPLEXDIR	=	$(BASEDIR)/soplex
ZIMPLDIR	=	$(BASEDIR)/zimpl
GCGDIR		=	$(BASEDIR)/gcg
UGDIR		=	$(BASEDIR)/ug

OPT		=	opt
LPSOPT		=	opt
ZIMPL		=	true
ZIMPLOPT	=	opt
IPOPT		=	false
SYM		=	none

GTEST	=	false

SPX_LEGACY	=	false
LEGACY		= 	false
ifeq ($(LEGACY),true)
SPX_LEGACY 	= 	true
endif

.PHONY:		all
all:		scipbinary

.PHONY:		clean
clean:
		@$(MAKE) -C $(SCIPDIR) clean
		@$(MAKE) -C $(SOPLEXDIR) clean OPT=$(LPSOPT)
ifneq ($(ZIMPL),false)
		@$(MAKE) -C $(ZIMPLDIR) clean OPT=$(ZIMPLOPT)
endif

.PHONY:		cleanlib
cleanlib:
		@$(MAKE) -C $(SCIPDIR) cleanlib
		@$(MAKE) -C $(SOPLEXDIR) cleanlib OPT=$(LPSOPT)
ifneq ($(ZIMPL),false)
		@$(MAKE) -C $(ZIMPLDIR) clean OPT=$(ZIMPLOPT)
endif

.PHONY:		check
check:		test

.PHONY:		test
test:
		@$(MAKE) -C $(SCIPDIR) test $^

.PHONY:		scipbinary
scipbinary:	$(SOPLEXDIR) $(ZIMPLDIR) $(SCIPDIR)
		@$(MAKE) -f Makefile.doit SCIPDIR=$(SCIPDIR) SOPLEXDIR=$(SOPLEXDIR) ZIMPL=$(ZIMPL) ZIMPLDIR=$(ZIMPLDIR) SPX_LEGACY=$(SPX_LEGACY)

.PHONY:		scipoptlib
scipoptlib:	$(LIBDIR) $(LIBOBJDIR) $(LIBOBJZIMPLDIR) $(LIBOBJSUBDIRS) $(SOPLEXDIR) $(ZIMPLDIR) $(SCIPDIR)
ifeq ($(SHARED),true)
		@echo "** compile shared libraries with compiler flag -fPIC"
else
		@echo "** compile libraries statically with compiler flag -fPIC"
endif
ifneq ($(ZIMPL),false)
		@$(MAKE) -C $(ZIMPLDIR) clean SHARED=$(SHARED) OPT=$(ZIMPLOPT)
endif
		@$(MAKE) scipbinary SHARED=$(SHARED) IPOPT=$(IPOPT) SPX_LEGACY=$(SPX_LEGACY)
		@$(MAKE) -f Makefile.doit $@ VERSION=$(OPTSUITEVERSION) SCIPDIR=$(SCIPDIR) SOPLEXDIR=$(SOPLEXDIR) ZIMPL=$(ZIMPL) ZIMPLDIR=$(ZIMPLDIR)

.PHONY:		install
install:	scipoptlib
		@$(MAKE) -C $(SCIPDIR) INSTALLDIR=$(abspath $(INSTALLDIR)) install
		@$(MAKE) -C $(SOPLEXDIR) INSTALLDIR=$(abspath $(INSTALLDIR)) install
		@$(MAKE) -f Makefile.doit installscipopt VERSION=$(OPTSUITEVERSION) SCIPDIR=$(SCIPDIR) INSTALLDIR=$(abspath $(INSTALLDIR)) LIBDIR=$(LIBDIR)

.PHONY:		soplex
soplex:		$(SOPLEXDIR) $(SCIPDIR)
		@$(MAKE) -f Makefile.doit soplex SCIPDIR=$(SCIPDIR) SOPLEXDIR=$(SOPLEXDIR)

.PHONY:		testsoplex
testsoplex:		$(SOPLEXDIR)
		@$(MAKE) -C $(SOPLEXDIR) check $^

.PHONY:		gcg
gcg:		$(SOPLEXDIR) $(ZIMPLDIR) $(SCIPDIR) $(GCGDIR)
		@$(MAKE) -f Makefile.doit gcg SCIPDIR=$(SCIPDIR) SOPLEXDIR=$(SOPLEXDIR) ZIMPL=$(ZIMPL) ZIMPLDIR=$(ZIMPLDIR) GCGDIR=$(GCGDIR) SPX_LEGACY=$(SPX_LEGACY) GTEST=${GTEST}

.PHONY:		testgcg
testgcg:
		@$(MAKE) -C $(GCGDIR) test $^

.PHONY:		ug
ug:		$(SOPLEXDIR) $(ZIMPLDIR) $(SCIPDIR) $(UGDIR)
		@$(MAKE) -f Makefile.doit ug SCIPDIR=$(SCIPDIR) SOPLEXDIR=$(SOPLEXDIR) ZIMPL=$(ZIMPL) ZIMPLDIR=$(ZIMPLDIR) UGDIR=$(UGDIR) PARASCIP=true SPX_LEGACY=$(SPX_LEGACY)

.PHONY:		testug
testug:
		@$(MAKE) -C $(UGDIR) test $^

$(LIBDIR):
		@-mkdir -p $(LIBDIR)

$(LIBOBJDIR):
		@-mkdir -p $(LIBOBJDIR)

$(LIBOBJZIMPLDIR):
		@-mkdir -p $(LIBOBJZIMPLDIR)
