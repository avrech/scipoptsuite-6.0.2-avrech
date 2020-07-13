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

/**@file    paraSolverTerminationStateTh.cpp
 * @brief   ParaSolverTerminationState extension for threads communication.
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
#include "paraSolverTerminationStateTh.h"

using namespace UG;

ParaSolverTerminationStateTh *
ParaSolverTerminationStateTh::createDatatype(
      )
{
   return new ParaSolverTerminationStateTh(
         interrupted,
         rank,
         totalNSolved,
         minNSolved,
         maxNSolved,
         totalNSent,
         totalNImprovedIncumbent,
         nParaNodesReceived,
         nParaNodesSolved,
         nParaNodesSolvedAtRoot,
         nParaNodesSolvedAtPreCheck,
         nTransferredLocalCutsFromSolver,
         minTransferredLocalCutsFromSolver,
         maxTransferredLocalCutsFromSolver,
         nTotalRestarts,
         minRestarts,
         maxRestarts,
         nTightened,
         nTightenedInt,
         runningTime,
         idleTimeToFirstParaNode,
         idleTimeBetweenParaNodes,
         idleTimeAfterLastParaNode,
         idleTimeToWaitNotificationId,
         idleTimeToWaitAckCompletion,
         idleTimeToWaitToken,
         totalRootNodeTime,
         minRootNodeTime,
         maxRootNodeTime,
         detTime
         );
}

void
ParaSolverTerminationStateTh::send(
      ParaComm *comm,
      int destination,
      int tag
      )
{
   DEF_PARA_COMM( commTh, comm);

   PARA_COMM_CALL(
      commTh->uTypeSend((void *)createDatatype(), ParaSolverTerminationStateType, destination, tag)
   );
}

void
ParaSolverTerminationStateTh::receive(
      ParaComm *comm,
      int source,
      int tag
      )
{
   DEF_PARA_COMM( commTh, comm);

   ParaSolverTerminationStateTh *received;
   PARA_COMM_CALL(
      commTh->uTypeReceive((void **)&received, ParaSolverTerminationStateType, source, tag)
   );
   interrupted = received->interrupted;
   rank = received->rank;
   totalNSolved = received->totalNSolved;
   minNSolved = received->minNSolved;
   maxNSolved = received->maxNSolved;
   totalNSent = received->totalNSent;
   totalNImprovedIncumbent = received->totalNImprovedIncumbent;
   nParaNodesReceived = received->nParaNodesReceived;
   nParaNodesSolved = received->nParaNodesSolved;
   nParaNodesSolvedAtRoot = received->nParaNodesSolvedAtRoot;
   nParaNodesSolvedAtPreCheck = received->nParaNodesSolvedAtPreCheck;
   nTransferredLocalCutsFromSolver = received->nTransferredLocalCutsFromSolver;
   minTransferredLocalCutsFromSolver= received->minTransferredLocalCutsFromSolver;
   maxTransferredLocalCutsFromSolver= received->maxTransferredLocalCutsFromSolver;
   nTotalRestarts= received->nTotalRestarts;
   minRestarts= received->minRestarts;
   maxRestarts= received->maxRestarts;
   nTightened = received->nTightened;
   nTightenedInt = received->nTightenedInt;
   runningTime = received->runningTime;
   idleTimeToFirstParaNode = received->idleTimeToFirstParaNode;
   idleTimeBetweenParaNodes = received->idleTimeBetweenParaNodes;
   idleTimeAfterLastParaNode = received->idleTimeAfterLastParaNode;
   idleTimeToWaitNotificationId = received->idleTimeToWaitNotificationId;
   idleTimeToWaitAckCompletion = received->idleTimeToWaitAckCompletion;
   idleTimeToWaitToken = received->idleTimeToWaitToken;
   totalRootNodeTime = received->totalRootNodeTime;
   minRootNodeTime = received->minRootNodeTime;
   maxRootNodeTime = received->maxRootNodeTime;
   detTime = received->detTime;

   delete received;
}
