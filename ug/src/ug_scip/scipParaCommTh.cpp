/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*             This file is part of the program and software framework       */
/*                  UG --- Ubquity Generator Framework                       */
/*                                                                           */
/*    Copyright (C) 2002-2019 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  UG is distributed under the terms of the ZIB Academic Licence.           */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with UG; see the file COPYING. If not email to scip@zib.de.        */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file    scipParaCommTh.cpp
 * @brief   SCIP ParaComm extension for threads communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#include "scipParaCommTh.h"
#include "scipParaInstanceTh.h"
#include "scipParaDiffSubproblemTh.h"
#include "scipParaSolutionTh.h"
#include "scipParaInitialStatTh.h"
#include "scipParaRacingRampUpParamSetTh.h"
#include "scipDiffParamSetTh.h"

using namespace ParaSCIP;

/*******************************************************************************
* transfer object factory
*******************************************************************************/
UG::ParaDiffSubproblem *
ScipParaCommTh::createParaDiffSubproblem(
    )
{ 
    return new ScipParaDiffSubproblemTh(); 
}

UG::ParaInitialStat* 
ScipParaCommTh::createParaInitialStat(
    )
{ 
    return new ScipParaInitialStatTh(); 
}

UG::ParaRacingRampUpParamSet* 
ScipParaCommTh::createParaRacingRampUpParamSet(
    )
{ 
    return new ScipParaRacingRampUpParamSetTh(); 
}

UG::ParaInstance*
ScipParaCommTh::createParaInstance(
    )
{ 
    return new ScipParaInstanceTh(); 
}

UG::ParaSolution*
ScipParaCommTh::createParaSolution(
    )
{ 
    return new ScipParaSolutionTh(); 
}

ScipParaInstance*
ScipParaCommTh::createScipParaInstance(
    SCIP *scip, 
    int method
    )
{
    return new ScipParaInstanceTh(scip, method);
}

ScipParaSolution*
ScipParaCommTh::createScipParaSolution(
    ScipParaSolver *solver,
    SCIP_Real objval, 
    int inNvars, 
    SCIP_VAR ** vars, 
    SCIP_Real *vals
    )
{
    return new ScipParaSolutionTh(solver, objval, inNvars, vars, vals);
}

ScipParaSolution*
ScipParaCommTh::createScipParaSolution(
    SCIP_Real objval, 
    int inNvars, 
    int *inIndicesAmongSolvers,
    SCIP_Real *vals
    )
{
    return new ScipParaSolutionTh(objval, inNvars, inIndicesAmongSolvers, vals);
}

ScipParaDiffSubproblem*
ScipParaCommTh::createScipParaDiffSubproblem(
         SCIP *scip,
         ScipParaSolver *scipParaSolver,
         int nNewBranchVars,
         SCIP_VAR **newBranchVars,
         SCIP_Real *newBranchBounds,
         SCIP_BOUNDTYPE *newBoundTypes,
         int nAddedConss,
         SCIP_CONS **addedConss
         )
{
    return new ScipParaDiffSubproblemTh(
         scip,
         scipParaSolver,
         nNewBranchVars,
         newBranchVars,
         newBranchBounds,
         newBoundTypes,
         nAddedConss,
         addedConss
         );
}

ScipParaInitialStat*
ScipParaCommTh::createScipParaInitialStat(
         SCIP *scip
         )
{
    return new ScipParaInitialStatTh(
         scip
         );
}

ScipParaInitialStat*
ScipParaCommTh::createScipParaInitialStat(
            int inMaxDepth,
            int inMaxTotalDepth,
            int inNVarBranchStatsDown,
            int inNVarBranchStatsUp,
            int *inIdxLBranchStatsVarsDown,
            int *inNVarBranchingDown,
            int *inIdxLBranchStatsVarsUp,
            int *inNVarBranchingUp,
            SCIP_Real *inDownpscost,
            SCIP_Real *inDownvsids,
            SCIP_Real *inDownconflen,
            SCIP_Real *inDowninfer,
            SCIP_Real *inDowncutoff,
            SCIP_Real *inUppscost,
            SCIP_Real *inUpvsids,
            SCIP_Real *inUpconflen,
            SCIP_Real *inUpinfer,
            SCIP_Real *inUpcutoff
         )
{
    return new ScipParaInitialStatTh(
            inMaxDepth,
            inMaxTotalDepth,
            inNVarBranchStatsDown,
            inNVarBranchStatsUp,
            inIdxLBranchStatsVarsDown,
            inNVarBranchingDown,
            inIdxLBranchStatsVarsUp,
            inNVarBranchingUp,
            inDownpscost,
            inDownvsids,
            inDownconflen,
            inDowninfer,
            inDowncutoff,
            inUppscost,
            inUpvsids,
            inUpconflen,
            inUpinfer,
            inUpcutoff
         );
}

ScipParaRacingRampUpParamSet *
ScipParaCommTh::createScipParaRacingRampUpParamSet(
         int inTerminationCriteria,
         int inNNodesLeft,
         double inTimeLimit,
         int inScipRacingParamSeed,
         int inPermuteProbSeed,
         int inGenerateBranchOrderSeed,
         ScipDiffParamSet *inScipDiffParamSet
         )
{
    return new ScipParaRacingRampUpParamSetTh(
               inTerminationCriteria,
               inNNodesLeft,
               inTimeLimit,
               inScipRacingParamSeed,
               inPermuteProbSeed,
               inGenerateBranchOrderSeed,
               inScipDiffParamSet
               );
}

ScipDiffParamSet *
ScipParaCommTh::createScipDiffParamSet()
{
    return new ScipDiffParamSetTh();
}

ScipDiffParamSet *
ScipParaCommTh::createScipDiffParamSet(
        SCIP *scip
        )
{
    return new ScipDiffParamSetTh(scip);
}
