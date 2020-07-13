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

/**@file    paraLoadCoordinatorTerminationState.h
 * @brief   Load coordinator termination state.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_LOADCOORDINATOR_TERMINATION_STATE_H__
#define __PARA_LOADCOORDINATOR_TERMINATION_STATE_H__

#include <string>
#include <cfloat>
#include "paraComm.h"

#ifdef UG_WITH_ZLIB
#include "gzstream.h"
#endif

namespace UG
{

/** Calculation state in a ParaLoadCoordinator */
class ParaLoadCoordinatorTerminationState
{
public:

   bool                  isCheckpointState;          /**< This state is at checkpoint or not */
   int                   rank;                       /**< rank of this ParaLoadCoordinator */
   /** Counters related to this ParaLoadCoordinator */
   unsigned long long    nWarmStart;                 /**< number of warm starts */
   unsigned long long    nSent;
   unsigned long long    nSentBackImmediately;
   unsigned long long    nSentBackImmediatelyAnotherNode;
   unsigned long long    nReceived;
   unsigned long long    nDeletedInLc;
   unsigned long long    nDeletedByMerging;
   unsigned long long    nFailedToSendBack;
   unsigned long long    nFailedToSendBackAnotherNode;
   unsigned long long    nMaxUsageOfNodePool;
   unsigned long long    nInitialP;
   unsigned long long    mMaxCollectingNodes;
   unsigned long long    nNodesInNodePool;
   unsigned long long    nNodesLeftInAllSolvers;
   unsigned long long    nNodesOutputLog;
   double                tNodesOutputLog;
   /** current dual bound value */
   double                globalBestDualBoundValue;
   double                externalGlobalBestDualBoundValue;
   /** times of this solver */
   double                idleTime;                   /**< idle time of this LoadCoordinator */
   double                runningTime;                /**< this ParaLoadCoordinator running time */
   /** times used for merging */
   double                addingNodeToMergeStructTime;
   double                generateMergeNodesCandidatesTime;
   double                regenerateMergeNodesCandidatesTime;
   double                mergeNodeTime;


   /** default constructor */
   ParaLoadCoordinatorTerminationState(
         )
         : isCheckpointState(true), rank(0), nWarmStart(0), nSent(0), nSentBackImmediately(0),
         nSentBackImmediatelyAnotherNode(0), nReceived(0),
         nDeletedInLc(0), nDeletedByMerging(0), nFailedToSendBack(0), nFailedToSendBackAnotherNode(0),
         nMaxUsageOfNodePool(0), nInitialP(0), mMaxCollectingNodes(0), nNodesInNodePool(0), nNodesLeftInAllSolvers(0), nNodesOutputLog(0), tNodesOutputLog(0.0),
         globalBestDualBoundValue(-DBL_MAX), externalGlobalBestDualBoundValue(-DBL_MAX),
         idleTime(0.0), runningTime(0.0), addingNodeToMergeStructTime(0.0),
         generateMergeNodesCandidatesTime(0.0), regenerateMergeNodesCandidatesTime(0.0),
         mergeNodeTime(0.0)
   {
   }
   /** destructor */
   virtual ~ParaLoadCoordinatorTerminationState(
	        )
   {
   }

   /** stringfy ParaCalculationState */
   std::string toString();

#ifdef UG_WITH_ZLIB
   /** write to checkpoint file */
   void write(gzstream::ogzstream &out);

   /** read from checkpoint file */
   bool read(ParaComm *comm, gzstream::igzstream &in);
#endif

};

}

#endif // __PARA_LOADCOORDINATOR_TERMINATION_STATE_H__

