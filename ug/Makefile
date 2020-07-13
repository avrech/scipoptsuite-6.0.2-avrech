#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
#*                                                                           *
#*             This file is part of the program and software framework       *
#*                  UG --- Ubquity Generator Framework                       *
#*                                                                           *
#*    Copyright (C) 2002-2019 Konrad-Zuse-Zentrum                            *
#*                            fuer Informationstechnik Berlin                *
#*                                                                           *
#*  UG is distributed under the terms of the ZIB Academic Licence.           *
#*                                                                           *
#*  You should have received a copy of the ZIB Academic License              *
#*  along with UG; see the file COPYING. If not email to scip@zib.de.        *
#*                                                                           *
#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

#@file    Makefile
#@brief   Makefile for UG framework
#@author  Yuji Shinano

#-----------------------------------------------------------------------------
# default settings ( may be overwritten in $(SCIPDIR)/make/make.project )
#-----------------------------------------------------------------------------
VERSION         :=      0.8.8
SOLVER		=       scip
COMM            =       cpp11
# COMM            =       pth

TEST		=	short
TIME		=	360
SETTINGS        =	default
VERBOSE         =       false
OPT             =       opt
COMP            =       gnu
LPS             =       spx2
DET		=       false
ZLIB		= 	true

# paths
SCIPDIR         =       lib/scip
COMMOBJSUBDIR	=	$(COMM)
UGOBJSUBDIRS    =       ug ug_$(SOLVER)

# include default project Makefile from SCIP
include $(SCIPDIR)/make/make.project

# check that SCIP was compiled with PARASCIP = true
SCIPVERSION                     :=$(shell $(SCIPDIR)/bin/scip.$(BASE).$(LPS).$(TPI)$(EXEEXTENSION) -v | sed -e 's/$$/@/')
override PARASCIP       :=$(shell echo "$(SCIPVERSION)" | sed -e 's/.* PARASCIP=\([^@]*\).*/\1/')

# redefine THREADS
THREADS		=	4

# ---------------------------------------------------------------------
# Git hash
# ---------------------------------------------------------------------
UGGITHASH     =       $(shell git describe --always --dirty  | sed 's/^.*-g//')
UGGITHASHFILE   =       $(SRCDIR)/ug/githash.cpp

# ---------------------------------------------------------------------
# Compiler selection
# ---------------------------------------------------------------------

ifeq ($(COMM), mpi)
CXX             =       mpicxx
LINKCXX		=	mpicxx
CXXINFO	        =       $(shell which $(CXX))
CXXEXIST        =       $(shell echo $(CXXINFO) | awk /$(CXX)/)
endif

ifeq ($(COMM), pth)
LDFLAGS		+=	-pthread
endif

ifeq ($(COMM), cpp11)
LDFLAGS		+=	-pthread
endif

ifeq ($(SOLVER), scip)
ifeq ($(COMM), mpi)
CXXFLAGS        +=      -D_COMM_MPI_WORLD -DMPICH_IGNORE_CXX_SEEK -D_SCIP_SOLVER -fPIE
endif
ifeq ($(COMM), pth)
CXXFLAGS        +=      -D_COMM_PTH -D_SCIP_SOLVER  -D_REENTRANT
endif
ifeq ($(COMM), cpp11)
CXXFLAGS        +=      -D_COMM_CPP11 -D_SCIP_SOLVER  -D_REENTRANT
endif
endif

ifeq ($(ZLIB_LDFLAGS),)
ZLIB            =       false
endif
ifeq ($(ZLIB),true)
CXXFLAGS        +=      -DUG_WITH_ZLIB $(ZLIB_FLAGS)
LDFLAGS         +=      $(ZLIB_LDFLAGS)
endif

ifeq ($(HOSTNAME), alibaba)
CXXFLAGS        +=      -i-dynamic -D_ALIBABA
endif

ifeq ($(HOSTNAME), bicegate1)
CXXFLAGS        +=      -i-dynamic -D_ALIBABA
endif

#-----------------------------------------------------------------------------
# UG framwork
#-----------------------------------------------------------------------------
UGSHORTNAME     =       ug
UGNAME          =       $(UGSHORTNAME)-$(VERSION)
UGOBJ           =       ug/paraLoadCoordinator.o \
			ug/paraLoadCoordinatorTerminationState.o \
			ug/paraNode.o \
			ug/paraParamSet.o \
			ug/paraSolver.o \
			ug/paraSolverPool.o \
			ug/paraSolverTerminationState.o \
			ug/paraSysTimer.o \
			ug/gzstream.o \
			ug/uggithash.o


ifeq ($(COMM),pth)
UGOBJ		+=	ug/paraCalculationStateTh.o \
			ug/paraCommPth.o \
			ug/paraNodeTh.o \
			ug/paraSolverStateTh.o \
			ug/paraSolverTerminationStateTh.o
endif

ifeq ($(COMM),cpp11)
UGOBJ		+=	ug/paraCalculationStateTh.o \
			ug/paraCommCPP11.o \
			ug/paraNodeTh.o \
			ug/paraSolverStateTh.o \
			ug/paraSolverTerminationStateTh.o
endif

ifeq ($(COMM),mpi)
UGOBJ      	+=	ug/paraCalculationStateMpi.o \
			ug/paraCommMpi.o \
			ug/paraNodeMpi.o \
			ug/paraParamSetMpi.o \
			ug/paraSolverStateMpi.o \
			ug/paraSolverTerminationStateMpi.o \
			ug/paraTimerMpi.o
endif

ifeq ($(COMM),pth)
MAINDEP		=	$(SRCDIR)/depend.ug.$(OPT).$(COMM)
endif
ifeq ($(COMM),cpp11)
MAINDEP		=	$(SRCDIR)/depend.ug.$(OPT).$(COMM)
endif
ifeq ($(COMM),mpi)
MAINDEP		=	$(SRCDIR)/depend.ug.$(OPT).$(COMM)
endif

UG              =       $(UGNAME).$(BASE)
UGOBJFILES      =       $(addprefix $(OBJDIR)/$(COMM)/,$(UGOBJ))
UGSRC           =       $(addprefix $(SRCDIR)/,$(UGOBJ:.o=.cpp))
UGINCSRC        =       $(addprefix $(SRCDIR)/,$(UGOBJ:.o=.h))
UGDEP           =       $(SRCDIR)/depend.ug.$(OPT).$(COMM)

ALLSRC          +=      $(UGSRC)

#-----------------------------------------------------------------------------
# Main Program
#-----------------------------------------------------------------------------
#-----------------------------------------------------------------------------
# ParaSCIP end FiberSCIP
#-----------------------------------------------------------------------------
ifeq ($(SOLVER),scip)
ifeq ($(COMM),pth)
UGSCIPLIBSHORTNAME=     fscip
UGSCIPLIBNAME     =     $(SCIPLIBSHORTNAME)-$(VERSION)
MAINSHORTNAME   =       fscip
UGSCIPLIBOBJ	=	ug_scip/fscip.o \
			ug_scip/scipParaCommTh.o \
			ug_scip/scipDiffParamSetTh.o \
			ug_scip/scipParaInitialStatTh.o \
			ug_scip/scipParaInstanceTh.o \
			ug_scip/scipParaRacingRampUpParamSet.o \
			ug_scip/scipParaRacingRampUpParamSetTh.o \
			ug_scip/scipParaSolutionTh.o
endif

ifeq ($(COMM),cpp11)
UGSCIPLIBSHORTNAME=     fscip
UGSCIPLIBNAME     =     $(SCIPLIBSHORTNAME)-$(VERSION)
MAINSHORTNAME   =       fscip
UGSCIPLIBOBJ	=	ug_scip/fscip.o \
			ug_scip/scipParaCommTh.o \
			ug_scip/scipDiffParamSetTh.o \
			ug_scip/scipParaInitialStatTh.o \
			ug_scip/scipParaInstanceTh.o \
			ug_scip/scipParaRacingRampUpParamSet.o \
			ug_scip/scipParaRacingRampUpParamSetTh.o \
			ug_scip/scipParaSolutionTh.o
endif

ifeq ($(COMM),mpi)
UGSCIPLIBSHORTNAME=     parascip
UGSCIPLIBNAME     =     $(SCIPLIBSHORTNAME)-$(VERSION)
MAINSHORTNAME   =       parascip
UGSCIPLIBOBJ	=	ug_scip/parascip.o \
			ug_scip/scipParaCommMpi.o \
			ug_scip/scipDiffParamSetMpi.o \
			ug_scip/scipParaDiffSubproblemMpi.o \
			ug_scip/scipParaInitialStatMpi.o \
			ug_scip/scipParaInstance.o \
			ug_scip/scipParaInstanceMpi.o \
			ug_scip/scipParaRacingRampUpParamSet.o \
			ug_scip/scipParaRacingRampUpParamSetMpi.o \
			ug_scip/scipParaSolutionMpi.o
endif

MAINNAME        =       $(MAINSHORTNAME)
UGSCIPLIBOBJ	+=	ug_scip/scipDiffParamSet.o \
			ug_scip/scipParaDiffSubproblem.o \
			ug_scip/scipParaInitialStat.o \
			ug_scip/scipParaInitiator.o \
			ug_scip/scipParaObjBranchRule.o \
			ug_scip/scipParaObjCommPointHdlr.o \
			ug_scip/scipParaObjMessageHdlr.o \
			ug_scip/scipParaObjNodesel.o \
			ug_scip/scipParaSolution.o \
			ug_scip/scipParaSolver.o

MAINOBJ		=	$(UGSCIPLIBOBJ) ug_scip/scipDefaultUserPlugins.o

MAINSRC		=	$(addprefix $(SRCDIR)/,$(MAINOBJ:.o=.cpp))

ifeq ($(COMM),pth)
MAINDEP		=	$(SRCDIR)/depend.fscip.$(OPT)
endif
ifeq ($(COMM),cpp11)
MAINDEP		=	$(SRCDIR)/depend.fscip.$(OPT)
endif
ifeq ($(COMM),mpi)
MAINDEP		=	$(SRCDIR)/depend.parascip.$(OPT)
endif

UGSCIPLIB         =    	$(UGSCIPLIBNAME).$(BASE)
UGSCIPLIBFILE     =    	$(LIBDIR)/libugscip$(UGSCIPLIB).$(COMM).$(LIBEXT)
# UGSCIPLIBOBJFILES =     $(addprefix $(LIBOBJDIR)/,$(UGSCIPLIBOBJ))
UGSCIPLIBOBJFILES =     $(addprefix $(OBJDIR)/$(COMM)/,$(UGOBJ))
UGSCIPLIBOBJFILES +=	$(addprefix $(OBJDIR)/$(COMM)/,$(UGSCIPLIBOBJ))
UGSCIPLIBSRC      =     $(addprefix $(SRCDIR)/,$(UGSCIPLIBOBJ:.o=.cpp))
UGSCIPLIBDEP      =     $(SRCDIR)/depend.sciplib.$(OPT)
UGSCIPLIBLINK     =     $(LIBDIR)/lib$(UGSCIPLIBSHORTNAME).$(BASE).$(LIBEXT)
UGSCIPLIBSHORTLINK =    $(LIBDIR)/lib$(UGSCIPLIBSHORTNAME).$(LIBEXT)

endif

#-----------------------------------------------------------------------------
# MAIN and LIB
#-----------------------------------------------------------------------------

MAIN		=	$(MAINNAME)-$(VERSION).$(BASE).$(LPS)$(EXEEXTENSION)
MAINFILE	=	$(BINDIR)/$(MAIN)
MAINSHORTLINK	=	$(BINDIR)/$(MAINNAME)
MAINOBJFILES	=	$(addprefix $(OBJDIR)/$(COMM)/,$(MAINOBJ))


#-----------------------------------------------------------------------------
# Rules
#-----------------------------------------------------------------------------

ifeq ($(VERBOSE),false)
.SILENT:	$(MAINFILE) $(MAINOBJFILES) $(MAINSHORTLINK) $(UGOBJFILES)
endif

.PHONY: all
all:            $(MAINFILE) $(MAINSHORTLINK) $(UGSCIPLIBFILE)

#.PHONY: lint
#lint:           $(UGSRC) $(MAINSRC)
#		-rm -f lint.out
#ifeq ($(FILES),)
#		$(SHELL) -ec 'for i in $^; \
#			do \
#			echo $$i; \
#			$(LINT) $(SCIPDIR)/lint/scip.lnt +os\(lint.out\) -u -zero \
#			$(FLAGS) -UNDEBUG -UWITH_READLINE -UROUNDING_FE $$i; \
#			done'
#else
#		$(SHELL) -ec  'for i in $(FILES); \
#			do \
#			echo $$i; \
#			$(LINT) $(SCIPDIR)/lint/scip.lnt +os\(lint.out\) -u -zero \
#			$(FLAGS) -UNDEBUG -UWITH_READLINE -UROUNDING_FE $$i; \
#			done'
#endif


#.PHONY: doc
#doc:
#		cd doc; $(DOXY) $(MAINNAME).dxy

.PHONY: githash
githash::       # do not remove the double-colon
		@echo "#define UG_GITHASH \"$(UGGITHASH)\"" > $(UGGITHASHFILE)

.PHONY: test
test:
		@-(cd check && ln -fs ../$(SCIPDIR)/check/evalcheck.sh);
		@-(cd check && ln -fs ../$(SCIPDIR)/check/check.awk);

ifeq ($(COMM),pth)
		cd check; \
		$(SHELL) ./check_fscip.sh $(TEST) $(MAINFILE) $(SETTINGS) $(notdir $(MAINFILE)).$(HOSTNAME) $(TIME) $(MEM) $(CONTINUE) $(LOCK) $(VERSION) $(LPS) $(THREADS);
endif
ifeq ($(COMM),cpp11)
		cd check; \
		$(SHELL) ./check_fscip.sh $(TEST) $(MAINFILE) $(SETTINGS) $(notdir $(MAINFILE)).$(HOSTNAME) $(TIME) $(MEM) $(CONTINUE) $(LOCK) $(VERSION) $(LPS) $(THREADS);
endif
ifeq ($(COMM),mpi)
		cd check; \
		$(SHELL) ./check_parascip.sh $(TEST) $(MAINFILE) $(SETTINGS) $(notdir $(MAINFILE)).$(HOSTNAME) $(TIME) $(MEM) $(CONTINUE) $(LOCK) $(VERSION) $(LPS) $(THREADS);
endif

# include local targets
-include $(SCIPDIR)/make/local/make.targets

.PHONY: scip
scip:
		@$(MAKE) -C $(SCIPDIR) libs PARASCIP=true $^

$(MAINSHORTLINK):	$(MAINFILE)
		@rm -f $@
		cd $(dir $@) && ln -s $(notdir $(MAINFILE)) $(notdir $@)

$(OBJDIR):
		@-mkdir -p $(OBJDIR)

$(COMMOBJSUBDIR):
		@-mkdir -p $(OBJDIR)/$(COMMOBJSUBDIR)

$(UGOBJSUBDIRS):
		@-mkdir -p $(OBJDIR)/$(COMMOBJSUBDIR)/$@

$(BINDIR):
		@-mkdir -p $(BINDIR)

.PHONY: scipcheck
scipcheck::
ifeq ($(PARASCIP), false)
		@echo "**************************************************"
		@echo "*   You should compile SCIP with PARASCIP=true   *"
		@echo "**************************************************"
		@exit 2
endif

.PHONY: envcheck
envcheck:
ifneq ($(CXXEXIST),)
		@echo "MPI compiler used :" $(CXXINFO)
else
		@echo "***************************************************************"
		@echo "*   [CANNOT FIND MPI COMPILER]                                *"
		@echo "*   You should set up MPI environment first to use ParaSCIP   *"
		@echo "***************************************************************"
		@exit 2
endif

.PHONY: clean
clean:		cleanbin $(UGOBJSUBDIRS) $(COMMOBJSUBDIR) $(OBJDIR)
		@-(cd $(OBJDIR)/$(COMMOBJSUBDIR) && rm -f */*.o && rmdir $(UGOBJSUBDIRS));
		@-(cd $(OBJDIR) && rmdir $(COMMOBJSUBDIR));
		@-rmdir $(OBJDIR)

.PHONY: cleanlib
cleanlib:       $(LIBDIR)
		@echo "-> remove library $(UGSCIPLIBFILE)"
		@-rm -f $(UGSCIPLIBFILE) $(UGSCIPLIBLINK) $(UGSCIPLIBSHORTLINK)

.PHONY: cleanbin
cleanbin:       $(BINDIR)
		@echo "-> remove binary $(MAINFILE)"
		@-rm -f $(MAINFILE) $(MAINSHORTLINK)

.PHONY: ugdepend
ugdepend:
		$(SHELL) -ec '$(DCXX) $(FLAGS) $(DFLAGS) $(UGSRC) \
		| sed '\''s|^\([0-9A-Za-z\_]\{1,\}\)\.o *: *$(SRCDIR)/\([0-9A-Za-z_/]*\).c|$$\(BINOBJDIR\)/\2.o: $(SRCDIR)/\2.c|g'\'' \
		>$(UGDEP)'

.PHONY: maindepend
maindepend:
		$(SHELL) -ec '$(DCXX) $(FLAGS) $(DFLAGS) $(MAINSRC) \
		| sed '\''s|^\([0-9A-Za-z\_]\{1,\}\)\.o *: *$(SRCDIR)/\([0-9A-Za-z_/]*\).c|$$\(BINOBJDIR\)/\2.o: $(SRCDIR)/\2.c|g'\'' \
		>$(MAINDEP)'

.PHONY: depend
depend:		ugdepend maindepend

-include	$(MAINDEP)
-include	$(UGDEP)

ifeq ($(SOLVER), scip)
ifeq ($(COMM),pth)
$(MAINFILE):	scipcheck $(BINDIR) $(OBJDIR) $(COMMOBJSUBDIR) $(UGOBJSUBDIRS) $(SCIPLIBFILE) $(LPILIBFILE) $(SPILIBFILE) $(NLPILIBFILE) $(UGOBJFILES) $(MAINOBJFILES)
endif
ifeq ($(COMM),cpp11)
$(MAINFILE):	scipcheck $(BINDIR) $(OBJDIR) $(COMMOBJSUBDIR) $(UGOBJSUBDIRS) $(SCIPLIBFILE) $(LPILIBFILE) $(SPILIBFILE) $(NLPILIBFILE) $(UGOBJFILES) $(MAINOBJFILES)
endif
ifeq ($(COMM),mpi)
$(MAINFILE):	envcheck $(BINDIR) $(OBJDIR) $(COMMOBJSUBDIR) $(UGOBJSUBDIRS) $(SCIPLIBFILE) $(LPILIBFILE) $(SPILIBFILE) $(NLPILIBFILE) $(UGOBJFILES) $(MAINOBJFILES)
endif
		@echo "-> linking $@"
		$(LINKCXX) $(MAINOBJFILES) $(UGOBJFILES) $(LINKCXXSCIPALL) \
		$(OFLAGS) $(LPSLDFLAGS) \
		$(LDFLAGS) $(LINKCXX_o)$@
# 		$(LINKCXX) $(MAINOBJFILES) $(UGOBJFILES) \
#  		$(LINKCXX_L)$(SCIPDIR)/lib $(LINKCXX_l)$(SCIPLIB)$(LINKLIBSUFFIX) \
#               $(LINKCXX_l)$(OBJSCIPLIB)$(LINKLIBSUFFIX) $(LINKCXX_l)$(LPILIB)$(LINKLIBSUFFIX) $(LINKCC_l)$(NLPILIB)$(LINKLIBSUFFIX) \
#               $(OFLAGS) $(LPSLDFLAGS) \
#		$(LDFLAGS) $(LINKCXX_o)$@
endif

ifeq ($(SOLVER),scip)
$(UGSCIPLIBFILE): $(LIBDIR) $(UGSCIPLIBOBJFILES)
		@echo "-> generating library $@"
		-rm -f $@
		 $(LIBBUILD) $(LIBBUILDFLAGS) $(LIBBUILD_o)$@ $(UGSCIPLIBOBJFILES)
ifneq ($(RANLIB),)
		$(RANLIB) $@
endif
endif

$(OBJDIR)/$(COMM)/%.o:	$(SRCDIR)/%.cpp
		@echo "-> compiling $@"
		$(CXX) $(FLAGS) $(OFLAGS) $(BINOFLAGS) $(CXXFLAGS) -c $< $(CXX_o)$@

#---- EOF --------------------------------------------------------------------
