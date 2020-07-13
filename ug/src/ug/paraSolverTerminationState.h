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

/**@file    paraSolverTerminationState.h
 * @brief   This class contains solver termination state which is transferred form Solver to LC.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_SOLVER_TERMINATION_STATE_H__
#define __PARA_SOLVER_TERMINATION_STATE_H__

#include "paraComm.h"
#include "paraInitiator.h"
#ifdef UG_WITH_ZLIB
#include "gzstream.h"
#endif

namespace UG
{

/** Calculation state in a ParaSolver */
class ParaSolverTerminationState
{
protected:

   int          interrupted;                /**< indicate that this solver is interrupted or not. 0: not interrupted, 1: interrputed
                                                                                                  2: checkpoint, 3: racing-ramp up */
   int          rank;                       /**< rank of this solver */
   /** Counters related to this ParaSolver */
   int          totalNSolved;               /**< accumulated number of nodes solved in this ParaSolver */
   int          minNSolved;                 /**< minimum number of subtree nodes rooted from ParaNode */
   int          maxNSolved;                 /**< maximum number of subtree nodes rooted from ParaNode */
   int          totalNSent;                 /**< accumulated number of nodes sent from this ParaSolver */
   int          totalNImprovedIncumbent;    /**< accumulated number of improvements of incumbent value in this ParaSolver */
   int          nParaNodesReceived;         /**< number of ParaNodes received in this ParaSolver */
   int          nParaNodesSolved;           /**< number of ParaNodes solved ( received ) in this ParaSolver */
   int          nParaNodesSolvedAtRoot;     /**< number of ParaNodes solved at root node before sending  */
   int          nParaNodesSolvedAtPreCheck; /**< number of ParaNodes solved at pre-checking of root node solvability */
   int          nTransferredLocalCutsFromSolver;      /**< number of local cuts transferred from this Solver */
   int          minTransferredLocalCutsFromSolver;    /**< minimum number of local cuts transferred from this Solver  */
   int          maxTransferredLocalCutsFromSolver;    /**< maximum number of local cuts transferred from this Solver  */
   int          nTotalRestarts;             /**< number of total restarts */
   int          minRestarts;                /**< minimum number of restarts */
   int          maxRestarts;                /**< maximum number of restarts */
   int          nTightened;                 /**< number of tightened variable bounds during racing stage */
   int          nTightenedInt;              /**< number of tightened integral variable bounds during racing stage */
   /** times of this solver */
   double       runningTime;                /**< this solver running time */
   double       idleTimeToFirstParaNode;    /**< idle time to start solving the first ParaNode */
   double       idleTimeBetweenParaNodes;   /**< idle time between ParaNodes processing */
   double       idleTimeAfterLastParaNode;  /**< idle time after the last ParaNode was solved */
   double       idleTimeToWaitNotificationId; /**< idle time to wait notification Id messages */
   double       idleTimeToWaitAckCompletion;  /**< idle time to wait ack completion message */
   double       idleTimeToWaitToken;        /**< idle time to wait token */
   /** times for root node process */
   double       totalRootNodeTime;          /**< total time consumed by root node processes */
   double       minRootNodeTime;            /**< minimum time consumed by root node processes */
   double       maxRootNodeTime;            /**< maximum time consumed by root node processes */
   double       detTime;                    /**< deterministic time, -1: should be non-deterministic */
public:
   /** default constructor */
   ParaSolverTerminationState(
         ) : interrupted(-1),
         rank(-1),
         totalNSolved(-1),
         minNSolved(-1),
         maxNSolved(-1),
         totalNSent(-1),
         totalNImprovedIncumbent(-1),
         nParaNodesReceived(-1),
         nParaNodesSolved(-1),
         nParaNodesSolvedAtRoot(-1),
         nParaNodesSolvedAtPreCheck(-1),
         nTransferredLocalCutsFromSolver(0),
         minTransferredLocalCutsFromSolver(0),
         maxTransferredLocalCutsFromSolver(0),
         nTotalRestarts(0),
         minRestarts(0),
         maxRestarts(0),
         nTightened(0),
         nTightenedInt(0),
         runningTime(0.0),
         idleTimeToFirstParaNode(0.0),
         idleTimeBetweenParaNodes(0.0),
         idleTimeAfterLastParaNode(0.0),
         idleTimeToWaitNotificationId(0.0),
         idleTimeToWaitAckCompletion(0.0),
         idleTimeToWaitToken(0.0),
         totalRootNodeTime(0.0),
         minRootNodeTime(0.0),
         maxRootNodeTime(0.0),
         detTime(-1.0)
   {
   }
   /** constructor */
   ParaSolverTerminationState(
         int          inInterrupted,                /**< indicate that this solver is interrupted or not. 0: not interrupted, 1: interrputed
                                                                                                          2: checkpoint, 3: racing-ramp up */
         int          inRank,                       /**< rank of this solver */
         int          inTotalNSolved,               /**< accumulated number of nodes solved in this ParaSolver */
         int          inMinNSolved,                   /**< minimum number of subtree nodes rooted from ParaNode */
         int          inMaxNSolved,                   /**< maximum number of subtree nodes rooted from ParaNode */
         int          inTotalNSent,                 /**< accumulated number of nodes sent from this ParaSolver */
         int          inTotalNImprovedIncumbent,    /**< accumulated number of improvements of incumbent value in this ParaSolver */
         int          inNParaNodesReceived,         /**< number of ParaNodes received in this ParaSolver */
         int          inNParaNodesSolved,             /**< number of ParaNodes solved ( received ) in this ParaSolver */
         int          inNParaNodesSolvedAtRoot,     /**< number of ParaNodes solved at root node before sending  */
         int          inNParaNodesSolvedAtPreCheck, /**< number of ParaNodes solved at pre-checking of root node solvability */
         int          inNTransferredLocalCutsFromSolver,      /**< number of local cuts transferred from this Solver */
         int          inMinTransferredLocalCutsFromSolver,    /**< minimum number of local cuts transferred from this Solver  */
         int          inMaxTransferredLocalCutsFromSolver,    /**< maximum number of local cuts transferred from this Solver  */
         int          inNTotalRestarts,             /**< number of total restarts */
         int          inMinRestarts,                /**< minimum number of restarts */
         int          inMaxRestarts,                /**< maximum number of restarts */
         int          inNTightened,                 /**< number of tightened variable bounds during racing stage */
         int          inNTightenedInt,               /**< number of tightened integral variable bounds during racing stage */
         double       inRunningTime,                /**< this solver running time */
         double       inIdleTimeToFirstParaNode,    /**< idle time to start solving the first ParaNode */
         double       inIdleTimeBetweenParaNodes,   /**< idle time between ParaNodes processing */
         double       inIddleTimeAfterLastParaNode, /**< idle time after the last ParaNode was solved */
         double       inIdleTimeToWaitNotificationId,   /**< idle time to wait notification Id messages  */
         double       inIdleTimeToWaitAckCompletion,    /**< idle time to wait ack completion message  */
         double       inIdleTimeToWaitToken,        /**< idle time to wait token  */
         double       inTotalRootNodeTime,          /**< total time consumed by root node processes */
         double       inMinRootNodeTime,            /**< minimum time consumed by root node processes */
         double       inMaxRootNodeTime,            /**< maximum time consumed by root node processes */
         double       inDetTime                     /**< deterministic time, -1: should be non-deterministic */
   )
         : interrupted(inInterrupted),
           rank(inRank),
           totalNSolved(inTotalNSolved),
           minNSolved(inMinNSolved),
           maxNSolved(inMaxNSolved),
           totalNSent(inTotalNSent),
           totalNImprovedIncumbent(inTotalNImprovedIncumbent),
           nParaNodesReceived(inNParaNodesReceived),
           nParaNodesSolved(inNParaNodesSolved),
           nParaNodesSolvedAtRoot(inNParaNodesSolvedAtRoot),
           nParaNodesSolvedAtPreCheck(inNParaNodesSolvedAtPreCheck),
           nTransferredLocalCutsFromSolver(inNTransferredLocalCutsFromSolver),
           minTransferredLocalCutsFromSolver(inMinTransferredLocalCutsFromSolver),
           maxTransferredLocalCutsFromSolver(inMaxTransferredLocalCutsFromSolver),
           nTotalRestarts(inNTotalRestarts),
           minRestarts(inMinRestarts),
           maxRestarts(inMaxRestarts),
           nTightened(inNTightened),
           nTightenedInt(inNTightenedInt),
           runningTime(inRunningTime),
           idleTimeToFirstParaNode(inIdleTimeToFirstParaNode),
           idleTimeBetweenParaNodes(inIdleTimeBetweenParaNodes),
           idleTimeAfterLastParaNode(inIddleTimeAfterLastParaNode),
           idleTimeToWaitNotificationId(inIdleTimeToWaitNotificationId),
           idleTimeToWaitAckCompletion(inIdleTimeToWaitAckCompletion),
           idleTimeToWaitToken(inIdleTimeToWaitToken),
           totalRootNodeTime(inTotalRootNodeTime),
           minRootNodeTime(inMinRootNodeTime),
           maxRootNodeTime(inMaxRootNodeTime),
           detTime(inDetTime)
   {
   }
   /** destructor */
   virtual ~ParaSolverTerminationState(
         )
   {
   }
   /** stringfy ParaCalculationState */
   std::string toString(ParaInitiator *initiator);

   /** getter of interrupted */
   int getInterruptedMode() { return interrupted; }

   /** getter of deterministic time */
   double getDeterministicTime(
         )
   {
      return detTime;
   }

#ifdef UG_WITH_ZLIB
   /** write to checkpoint file */
   void write(gzstream::ogzstream &out);

   /** read from checkpoint file */
   bool read(ParaComm *comm, gzstream::igzstream &in);
#endif 

   virtual void send(ParaComm *comm, int destination, int tag) = 0;
   virtual void receive(ParaComm *comm, int source, int tag) = 0;

};

}

#endif // __PARA_SOLVER_TERMINATION_STATE_H__

