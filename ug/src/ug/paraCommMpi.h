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

/**@file    paraCommMpi.h
 * @brief   ParaComm extension for MPI communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_COMM_MPI_H__
#define __PARA_COMM_MPI_H__

#ifdef _MUTEX_CPP11
#include <thread>
#include <mutex>
#endif
#include <mpi.h>
#include <stdexcept>
#include <iostream>
#include <ostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include "paraDef.h"
#include "paraComm.h"
#include "paraInstance.h"
#include "paraDiffSubproblem.h"
#include "paraSolution.h"
#include "paraCalculationStateMpi.h"
#include "paraNodeMpi.h"
#include "paraParamSetMpi.h"
#include "paraSolverStateMpi.h"
#include "paraSolverTerminationStateMpi.h"
#include "paraTimerMpi.h"
#ifdef UG_WITH_UGS
#include "ugs/ugsParaCommMpi.h"
#endif 

namespace UG
{

#define MPI_CALL( mpicall ) do \
   { \
      int _error_value_; \
      _error_value_ = ( mpicall ); \
      if( _error_value_ != MPI_SUCCESS ) \
      { \
         std::cout << "[MPI ERROR: " << __FILE__  << "] func = " \
                   << __func__ << ", line = " << __LINE__ << ": " \
                   << "error_code = " << _error_value_ << std::endl; \
         MPI_Abort(MPI_COMM_WORLD, 1); \
      } \
   } while (0)

#define TAG_TRACE( call, fromTo, sourceDest, tag ) \
   if( tagTraceFlag )  \
   {  \
      *tos << (MPI_Wtime() - startTime) << " [Rank = " << myRank << "] " << #call << " " << #fromTo  \
      << " " << sourceDest << " with Tag = " << tagStringTable[tag] << std::endl; \
   }

class ParaCommMpi : public ParaComm
{
protected:
   MPI_Comm      myComm;
   int           myCommSize;
   int           myRank;
   int           namelen;
   char          procName[MPI_MAX_PROCESSOR_NAME];
   bool          tagTraceFlag;
   std::ofstream ofs;
   std::ostream  *tos;
   double        startTime;
   int           token[2];   // index 0: token
                             // index 1: token color
                             //           -1: green
                             //           > 0: yellow ( termination origin solver number )
                             //           -2: red ( means the solver can terminate )
   static        MPI_Datatype datatypes[TYPE_LIST_SIZE];
   static const char *tagStringTable[];

#ifdef _MUTEX_CPP11
   std::mutex                 tokenAccessLock;
#else
   pthread_mutex_t           tokenAccessLock;
#endif

#ifdef UG_WITH_UGS
   UGS::UgsParaCommMpi *commUgs;
#endif

public:
   ParaCommMpi() : myComm(MPI_COMM_NULL), myCommSize(-1), myRank(-1), namelen(-1),
         tagTraceFlag(false), tos(0), startTime(0.0)
#ifdef UG_WITH_UGS
         , commUgs(0)
#endif
   {
#ifndef _MUTEX_CPP11
      pthread_mutex_init(&tokenAccessLock, NULL);
#endif
      token[0]=-1;
      token[1]=-1;
   }
   ParaCommMpi(MPI_Comm comm) : myComm(comm), myCommSize(-1), myRank(-1), namelen(-1),
         tagTraceFlag(false), tos(0), startTime(0.0)
#ifdef UG_WITH_UGS
         , commUgs(0)
#endif
   {
   }
   virtual void init( int argc, char **argv );
   virtual ~ParaCommMpi();
   double getStartTime() { return startTime; }
   int getRank(){ return myRank; }
   int getSize(){ return myCommSize; }
   void lcInit(ParaParamSet *paraParamSet);
   void solverInit(ParaParamSet *paraParamSet);
   void abort();
   bool waitTerminatedMessage(){ return true; }
   bool waitToken(int rank);
   void passToken(int rank);
   bool passTermToken(int rank);
   void setToken(int rank, int *inToken)
   {
#ifdef _MUTEX_CPP11
      std::lock_guard<std::mutex> lock(tokenAccessLock);
#else
      pthread_mutex_lock(&tokenAccessLock);
#endif
      token[0] = inToken[0]; token[1] = inToken[1];
#ifndef _MUTEX_CPP11
      pthread_mutex_unlock(&tokenAccessLock);
#endif
   }

   ParaCalculationState *createParaCalculationState();
   ParaCalculationState *createParaCalculationState(
               double compTime,                   /**< computation time of this ParaNode */
               double rootTime,                   /**< computation time of the root node */
               int    nSolved,                    /**< the number of nodes solved   */
               int    nSent,                      /**< the number of ParaNodes sent */
               int    nImprovedIncumbent,         /**< the number of improved solution generated in this ParaSolver */
               int    terminationState,           /**< indicate whether if this computation is terminationState or not. 0: no, 1: terminationState */
               int    nSolvedWithNoPreprocesses,  /**< number of nodes solved when it is solved with no preprocesses */
               int    nSimplexIterRoot,           /**< number of simplex iteration at root node */
               double averageSimplexIter,         /**< average number of simplex iteration except root node */
               int    nTransferredLocalCuts,      /**< number of local cuts transferred from a ParaNode */
               int    minTransferredLocalCuts,    /**< minimum number of local cuts transferred from a ParaNode */
               int    maxTransferredLocalCuts,    /**< maximum number of local cuts transferred from a ParaNode */
               int    nRestarts,                  /**< number of restarts */
               double minIisum,                   /**< minimum sum of integer infeasibility */
               double maxIisum,                   /**< maximum sum of integer infeasibility */
               int    minNii,                     /**< minimum number of integer infeasibility */
               int    maxNii,                     /**< maximum number of integer infeasibility */
               double dualBound                    /**< final dual bound value */
           );
   ParaNode *createParaNode();
   ParaNode *createParaNode(
               NodeId inNodeId,
               NodeId inGeneratorNodeId,
               int inDepth,
               double inDualBoundValue,
               double inOriginalDualBoundValue,
               double inEstimatedValue,
               ParaDiffSubproblem *inDiffSubproblem
            );
   ParaParamSet *createParaParamSet();
   ParaSolverState *createParaSolverState();
   ParaSolverState *createParaSolverState(
               int racingStage,
               unsigned int notificationId,
               int lcId,
               int globalSubtreeId,
               long long nodesSolved,
               int nodesLeft,
               double bestDualBoundValue,
               double globalBestPrimalBoundValue,
               double detTime,
               double averageDualBoundGain
           );
   ParaSolverTerminationState *createParaSolverTerminationState();
   ParaSolverTerminationState *createParaSolverTerminationState(
               int    interrupted,                /**< indicate that this solver is interrupted or not. 0: not interrupted, 1: interrputed
                                                                                                       2: checkpoint, 3: racing-ramp up */
               int    rank,                       /**< rank of this solver */
               int    totalNSolved,               /**< accumulated number of nodes solved in this ParaSolver */
               int    minNSolved,                 /**< minimum number of subtree nodes rooted from ParaNode */
               int    maxNSolved,                 /**< maximum number of subtree nodes rooted from ParaNode */
               int    totalNSent,                 /**< accumulated number of nodes sent from this ParaSolver */
               int    totalNImprovedIncumbent,    /**< accumulated number of improvements of incumbent value in this ParaSolver */
               int    nParaNodesReceived,         /**< number of ParaNodes received in this ParaSolver */
               int    nParaNodesSolved,           /**< number of ParaNodes solved ( received ) in this ParaSolver */
               int    nParaNodesSolvedAtRoot,     /**< number of ParaNodes solved at root node before sending  */
               int    nParaNodesSolvedAtPreCheck, /**< number of ParaNodes solved at pre-checking of root node solvability */
               int    nTransferredLocalCutsFromSolver,      /**< number of local cuts transferred from this Solver */
               int    minTransferredLocalCutsFromSolver,    /**< minimum number of local cuts transferred from this Solver  */
               int    maxTransferredLocalCutsFromSolver,    /**< maximum number of local cuts transferred from this Solver  */
               int    nTotalRestarts,             /**< number of total restarts */
               int    minRestarts,                /**< minimum number of restarts */
               int    maxRestarts,                /**< maximum number of restarts */
               int    nTightened,                 /**< number of tightened variable bounds during racing stage */
               int    nTightenedInt,              /**< number of tightened integral variable bounds during racing stage */
               double runningTime,                /**< this solver running time */
               double idleTimeToFirstParaNode,    /**< idle time to start solving the first ParaNode */
               double idleTimeBetweenParaNodes,   /**< idle time between ParaNodes processing */
               double iddleTimeAfterLastParaNode, /**< idle time after the last ParaNode was solved */
               double idleTimeToWaitNotificationId,   /**< idle time to wait notification Id messages  */
               double idleTimeToWaitAckCompletion,    /**< idle time to wait ack completion message  */
               double idleTimeToWaitToken,        /**< idle time to wait token  */
               double totalRootNodeTime,          /**< total time consumed by root node processes */
               double minRootNodeTime,            /**< minimum time consumed by root node processes */
               double maxRootNodeTime,            /**< maximum time consumed by root node processes */
               double detTime                     /**< deterministic time, -1: should be non-deterministic */
           );
   ParaTimer *createParaTimer(){ return new ParaTimerMpi(); }
   int bcast( void* buffer, int count, const int datatypeId, int root );
   int send( void* bufer, int count, const int datatypeId, int dest, const int tag );
   int receive( void* bufer, int count, const int datatypeId, int source, const int tag );
   int waitSpecTagFromSpecSource(const int source, const int datatypeId, const int tag, int *receivedTag);
   void probe(int *source, int *tag);
   bool iProbe(int *source, int *tag);  /** return value: true => there is message, false => no message */

   /** For Created Datatypes */
   int ubcast( void* buffer, int count, MPI_Datatype datatype, int root );
   int usend( void* bufer, int count, MPI_Datatype datatype, int dest, int tag );
   int ureceive( void* bufer, int count, MPI_Datatype datatype, int source, int tag );

};

#define DEF_PARA_COMM( para_comm, comm ) ParaCommMpi *para_comm = dynamic_cast< ParaCommMpi* >(comm)

}

#endif  // __PARA_COMM_MPI_H__
