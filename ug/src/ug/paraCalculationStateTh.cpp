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

/**@file    paraCalculationStateTh.cpp
 * @brief   CalcutationStte object extension for threads communication
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifdef _COMM_PTH
#include "paraCommPth.h"
#endif
#ifdef _COMM_CPP11
#include "paraCommCPP11.h"
#endif
#include "paraCalculationStateTh.h"

using namespace UG;

ParaCalculationStateTh*
ParaCalculationStateTh::createDatatype(
      )
{
   return new ParaCalculationStateTh(
         compTime,
         rootTime,
         nSolved,
         nSent,
         nImprovedIncumbent,
         terminationState,
         nSolvedWithNoPreprocesses,
         nSimplexIterRoot,
         averageSimplexIter,
         nTransferredLocalCuts,
         minTransferredLocalCuts,
         maxTransferredLocalCuts,
         nRestarts,
         minIisum,
         maxIisum,
         minNii,
         maxNii,
         dualBound
         );
}

void
ParaCalculationStateTh::send(
      ParaComm *comm,
      int destination,
      int tag
      )
{
   DEF_PARA_COMM( commTh, comm);

   PARA_COMM_CALL(
      commTh->uTypeSend(createDatatype(), ParaCalculationStateType, destination, tag)
   );
}

void
ParaCalculationStateTh::receive(
      ParaComm *comm,
      int source,
      int tag
      )
{
   DEF_PARA_COMM( commTh, comm);

   ParaCalculationStateTh *received;

   PARA_COMM_CALL(
      commTh->uTypeReceive((void **)&received, ParaCalculationStateType, source, tag)
   );

   compTime = received->compTime;
   rootTime = received->rootTime;
   nSolved = received->nSolved;
   nSent = received->nSent;
   nImprovedIncumbent = received->nImprovedIncumbent;
   terminationState = received->terminationState;
   nSolvedWithNoPreprocesses = received->nSolvedWithNoPreprocesses;
   nSimplexIterRoot = received->nSimplexIterRoot;
   averageSimplexIter = received->averageSimplexIter;
   nTransferredLocalCuts = received->nTransferredLocalCuts;
   minTransferredLocalCuts = received->nTransferredLocalCuts;
   maxTransferredLocalCuts = received->maxTransferredLocalCuts;
   nRestarts = received->nRestarts;
   minIisum = received->minIisum;
   maxIisum = received->maxIisum;
   minNii = received->minNii;
   maxNii = received->maxNii;
   dualBound = received->dualBound;

   delete received;

}
