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

/**@file    paraSolverStateTh.cpp
 * @brief   ParaSolverState extension for threads communication.
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
#include "paraSolverStateTh.h"

using namespace UG;

ParaSolverStateTh *
ParaSolverStateTh::createDatatype(
      )
{
   return new ParaSolverStateTh(
         racingStage,
         notificationId,
         lcId,
         globalSubtreeIdInLc,
         nNodesSolved,
         nNodesLeft,
         bestDualBoundValue,
         globalBestPrimalBoundValue,
         detTime,
         averageDualBoundGain
         );
}

void
ParaSolverStateTh::send(
      ParaComm *comm,
      int destination,
      int tag
      )
{
   assert(nNodesLeft >= 0);
   assert(bestDualBoundValue >= -1e+10);
   DEF_PARA_COMM( commTh, comm);

   PARA_COMM_CALL(
      commTh->uTypeSend((void *)createDatatype(), ParaSolverStateType, destination, tag)
   );
}

void
ParaSolverStateTh::receive(
      ParaComm *comm,
      int source,
      int tag
      )
{
   DEF_PARA_COMM( commTh, comm);

   ParaSolverStateTh *received;
   PARA_COMM_CALL(
      commTh->uTypeReceive((void **)&received, ParaSolverStateType, source, tag)
   );

   racingStage = received->racingStage;
   notificationId = received->notificationId;
   lcId = received->lcId;
   globalSubtreeIdInLc = received->globalSubtreeIdInLc;
   nNodesSolved = received->nNodesSolved;
   nNodesLeft = received->nNodesLeft;
   bestDualBoundValue = received->bestDualBoundValue;
   globalBestPrimalBoundValue = received->globalBestPrimalBoundValue;
   detTime = received->detTime;
   averageDualBoundGain = received->averageDualBoundGain;

   delete received;

}
