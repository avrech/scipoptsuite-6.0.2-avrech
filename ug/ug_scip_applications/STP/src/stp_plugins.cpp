/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2019 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   stp_plugins.cpp
 * @brief  Steiner Tree Solver user plugins
 * @author Yuji Shinano
 */

/*--+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include "ug_scip/scipUserPlugins.h"
#include "ug_scip/scipParaSolver.h"
#include "ug_scip/scipParaInitiator.h"

#include "scip/scipdefplugins.h"
#include "reader_stp.h"
#include "cons_stp.h"
#include "heur_tm.h"
#include "heur_local.h"
#include "heur_prune.h"
#include "heur_ascendprune.h"
#include "heur_slackprune.h"
#include "heur_rec.h"
#include "pricer_stp.h"
#include "event_bestsol.h"
#include "probdata_stp.h"
#include "dialog_stp.h"
#include "prop_stp.h"
#include "branch_stp.h"

using namespace UG;
using namespace ParaSCIP;

/* to provide rank ans size to user  */
static ParaComm *paraComm = 0;

class StpUserPlugins : public ScipUserPlugins {
   void operator()(SCIP *scip)
   {

      /* include stp pricer */
      // SCIP_CALL_ABORT( SCIPincludePricerStp(scip) );

      /* include steiner tree reader */
      SCIP_CALL_ABORT( SCIPincludeReaderStp(scip) );

      /* include default SCIP plugins */
      // SCIP_CALL_ABORT( SCIPincludeDefaultPlugins(scip) );

      /* include STP dialog */
      SCIP_CALL_ABORT( SCIPincludeDialogStp(scip) );

      /* include steiner tree constraint handler */
      SCIP_CALL_ABORT( SCIPincludeConshdlrStp(scip) );
#if 1
      /* include Takahashi Matsuyama heuristic */
      SCIP_CALL_ABORT( SCIPStpIncludeHeurTM(scip) );

      /* include local heuristics */
      SCIP_CALL_ABORT( SCIPStpIncludeHeurLocal(scip) );
#if 1
      /* include recombination heuristic */
      SCIP_CALL_ABORT( SCIPStpIncludeHeurRec(scip) );
#endif
#endif
#if 1
      /* include pruning heuristic */
      SCIP_CALL_ABORT( SCIPStpIncludeHeurPrune(scip) );
#endif

#if 0
      /* include ascend-and-prune heuristic */
      SCIP_CALL_ABORT( SCIPincludeHeurAscendPrune(scip) );
#endif

#if 1
      /* include slack-and-prune heuristic */
      SCIP_CALL_ABORT( SCIPStpIncludeHeurSlackPrune(scip) );
#endif

      /* include event handler for printing primal solution development */
      SCIP_CALL_ABORT( SCIPincludeEventHdlrBestsol(scip) );

      /* include branching rule */
      SCIP_CALL_ABORT( SCIPincludeBranchruleStp(scip) );

      /* include propagator */
      SCIP_CALL_ABORT( SCIPincludePropStp(scip) );

      SCIP_CALL_ABORT( SCIPsetSubscipsOff(scip, TRUE) );

      /* set STP-specific default parameters */
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "presolving/maxrestarts", 0) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "display/freq", 1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "limits/maxsol", 400) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "lp/rowagelimit", 30) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "separating/maxroundsroot", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "separating/maxrounds", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "separating/maxstallroundsroot", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "separating/maxcutsroot", 100000) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "separating/maxcuts", 1000) );   // todo tune
      
      SCIP_CALL_ABORT( SCIPsetRealParam(scip, "separating/minefficacyroot", 0.01) ); // todo tune
      SCIP_CALL_ABORT( SCIPsetRealParam(scip, "separating/minorthoroot", 0.4) ); // todo tune > 0.4
      SCIP_CALL_ABORT( SCIPsetRealParam(scip, "separating/minortho", 0.4) ); // todo tune > 0.4 best soplex: 0.8
      SCIP_CALL_ABORT( SCIPsetRealParam(scip, "separating/objparalfac", 0.01) ); // todo tune < 0.1
      
      SCIP_CALL_ABORT( SCIPsetRealParam(scip, "separating/intsupportfac", 0.0) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "branching/relpscost/maxproprounds", 0) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/alns/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/coefdiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/feaspump/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/fracdiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/farkasdiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/guideddiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/linesearchdiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/nlpdiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/objpscostdiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/pscostdiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/randrounding/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/rootsoldiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/shiftandpropagate/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/shifting/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/subnlp/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/undercover/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/veclendiving/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/zirounding/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/oneopt/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/rounding/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "heuristics/locks/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "propagating/probing/maxprerounds", 0) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "propagating/pseudoobj/timingmask", 5) );
      SCIP_CALL_ABORT( SCIPsetIntParam(scip, "propagating/redcost/freq", -1) );
      SCIP_CALL_ABORT( SCIPsetRealParam(scip, "branching/relpscost/maxreliable", 1.0) );

   }

   void writeUserSolution(SCIP *scip, int nSolvers, double dual)
   {
      SCIPprobdataSetDualBound(scip, dual);
      SCIPprobdataSetNSolvers(scip, nSolvers);
      SCIPprobdataWriteLogfileEnd(scip);
   }

   void newSubproblem(
         SCIP *scip,
         const ScipParaDiffSubproblemBranchLinearCons *linearConss,
         const ScipParaDiffSubproblemBranchSetppcCons *setppcConss)
   {
      if( linearConss && setppcConss )
      {
         initReceivedSubproblem(scip, linearConss->nLinearConss, linearConss->consNames, setppcConss->nSetppcConss, setppcConss->consNames);
      } else if( linearConss && !setppcConss )
      {
         initReceivedSubproblem(scip, linearConss->nLinearConss, linearConss->consNames, 0, 0);
      } else if( !linearConss && setppcConss )
      {
         initReceivedSubproblem(scip, 0, 0, setppcConss->nSetppcConss, setppcConss->consNames);
      } else
      {
         initReceivedSubproblem(scip, 0, 0, 0, 0);
      }

   }

};


void
setUserPlugins(ParaInitiator *inInitiator)
{
   ScipParaInitiator *initiator = dynamic_cast<ScipParaInitiator *>(inInitiator);
   initiator->setUserPlugins(new StpUserPlugins());
   paraComm = initiator->getParaComm();
}

void
setUserPlugins(ParaInstance *inInstance)
{
   ScipParaInstance *instance = dynamic_cast<ScipParaInstance *>(inInstance);
   instance->setUserPlugins(new StpUserPlugins());
}

void
setUserPlugins(ParaSolver *inSolver)
{
   ScipParaSolver *solver = dynamic_cast<ScipParaSolver *>(inSolver);
   solver->setUserPlugins(new StpUserPlugins());
   if( !paraComm )
   {
      paraComm = solver->getParaComm();
   }
}

extern "C"
int getUgRank()
{
   return paraComm->getRank();
}

extern "C"
int getUgSize()
{
   return paraComm->getSize();
}

extern "C"
const char*
getBranchLinearConsName(const char* names, int i)
{
   const char *name = names;
   for( int j = 0; j < i; j++)
   {
      assert(*name);
      name += (std::strlen(name) + 1);
   }
   assert(*name);
   return name;
}

extern "C"
const char*
getBranchSetppcConsName(const char* names, int i)
{
   const char *name = names;
   for( int j = 0; j < i; j++)
   {
      assert(*name);
      name += (std::strlen(name) + 1);
   }
   assert(*name);
   return name;
}




