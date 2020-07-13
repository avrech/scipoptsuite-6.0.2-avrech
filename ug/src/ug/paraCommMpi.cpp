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

/**@file    paraCommMpi.cpp
 * @brief   ParaComm extension for MPI communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#include "paraCommMpi.h"

using namespace UG;

MPI_Datatype
ParaCommMpi::datatypes[TYPE_LIST_SIZE];

const char *
ParaCommMpi::tagStringTable[] = {
  TAG_STR(TagNode),
  TAG_STR(TagNodeReceived),
  TAG_STR(TagDiffSubproblem),
  TAG_STR(TagRampUp),
  TAG_STR(TagRetryRampUp),
  TAG_STR(TagSolution),
  TAG_STR(TagIncumbentValue),
  TAG_STR(TagGlobalBestDualBoundValue),
  TAG_STR(TagSolverState),
  TAG_STR(TagCompletionOfCalculation),
  TAG_STR(TagAnotherNodeRequest),
  TAG_STR(TagNoNodes),
  TAG_STR(TagInCollectingMode),
  TAG_STR(TagCollectAllNodes),
  TAG_STR(TagOutCollectingMode),
  TAG_STR(TagLCBestBoundValue),
  TAG_STR(TagNotificationId),
  TAG_STR(TagTerminateRequest),
  TAG_STR(TagInterruptRequest),
  TAG_STR(TagTerminated),
  TAG_STR(TagRacingRampUpParamSets),
  TAG_STR(TagWinner),
  TAG_STR(TagLightWeightRootNodeProcess),
  TAG_STR(TagBreaking),
  TAG_STR(TagHardTimeLimit),
  TAG_STR(TagInitialStat),
  TAG_STR(TagAckCompletion),
  TAG_STR(TagToken),
  TAG_STR(TagTerminateSolvingToRestart),
  TAG_STR(TagGivenGapIsReached),
  TAG_STR(TagAllowToBeInCollectingMode),
  TAG_STR(TagTestDualBoundGain),
  TAG_STR(TagNoTestDualBoundGain),
  TAG_STR(TagNoWaitModeSend),
  TAG_STR(TagRestart),
  TAG_STR(TagLbBoundTightenedIndex),
  TAG_STR(TagLbBoundTightenedBound),
  TAG_STR(TagUbBoundTightenedIndex),
  TAG_STR(TagUbBoundTightenedBound),
  TAG_STR(TagCutOffValue),
  TAG_STR(TagChangeSearchStrategy),
  TAG_STR(TagSolverDiffParamSet1),
  TAG_STR(TagSolverDiffParamSet2),
  TAG_STR(TagDiffSubproblem1),
  TAG_STR(TagDiffSubproblem2),
  TAG_STR(TagDiffSubproblem3),
  TAG_STR(TagDiffSubproblem4),
  TAG_STR(TagDiffSubproblem5),
  TAG_STR(TagDiffSubproblem6),
  TAG_STR(TagDiffSubproblem7),
  TAG_STR(TagDiffSubproblem8),
  TAG_STR(TagDiffSubproblem9),
  TAG_STR(TagDiffSubproblem10),
  TAG_STR(TagDiffSubproblem11),
  TAG_STR(TagDiffSubproblem12),
  TAG_STR(TagSolution1),
  TAG_STR(TagInitialStat1)
};

void
ParaCommMpi::lcInit(
      ParaParamSet *paraParamSet
      )
{
   tagTraceFlag = paraParamSet->getBoolParamValue(TagTrace);
   if( tagTraceFlag )
   {
      if( paraParamSet->isStringParamDefaultValue(TagTraceFileName) )
      {
         tos = &std::cout;
      }
      else
      {
         std::ostringstream s;
         s << paraParamSet->getStringParamValue(TagTraceFileName) << myRank;
         ofs.open(s.str().c_str());
         tos = &ofs;
      }
   }
   if( paraParamSet->getBoolParamValue(Deterministic) )
   {
      token[0] = 0;
      token[1] = -1;
   }
}

void
ParaCommMpi::solverInit(
      ParaParamSet *paraParamSet
      )
{
   tagTraceFlag = paraParamSet->getBoolParamValue(TagTrace);
   if( tagTraceFlag )
   {
      if( paraParamSet->isStringParamDefaultValue(TagTraceFileName) )
      {
         tos = &std::cout;
      }
      else
      {
         std::ostringstream s;
         s << paraParamSet->getStringParamValue(TagTraceFileName) << myRank;
         ofs.open(s.str().c_str());
         tos = &ofs;
      }
   }
}

void
ParaCommMpi::abort(
      )
{
   MPI_Abort(MPI_COMM_WORLD, 0);
}

bool
ParaCommMpi::waitToken(
      int tempRank
      )
{
#ifdef _MUTEX_CPP11
   std::lock_guard<std::mutex> lock(tokenAccessLock);
#else
   pthread_mutex_lock(&tokenAccessLock);
#endif
   if( token[0] == myRank )
   {
#ifndef _MUTEX_CPP11
      pthread_mutex_unlock(&tokenAccessLock);
#endif
      return true;
   }
   else
   {
      int previousRank = myRank - 1;
      if( previousRank == 0 )
      {
         if( token[0] != -1 )
         {
            previousRank = myCommSize - 1;
         }
      }
      int receivedTag;
      MPI_Status mpiStatus;
      MPI_CALL (
         MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, myComm, &mpiStatus)
      );
      receivedTag = mpiStatus.MPI_TAG;
      TAG_TRACE (Probe, From, mpiStatus.MPI_SOURCE, receivedTag);
      if( receivedTag == TagToken )
      {
         receive(token, 2, ParaINT, 0, TagToken);
         assert(token[0] == myRank);
#ifndef _MUTEX_CPP11
         pthread_mutex_unlock(&tokenAccessLock);
#endif
         return true;
      }
      else
      {
#ifndef _MUTEX_CPP11
         pthread_mutex_unlock(&tokenAccessLock);
#endif
         return false;
      }
   }
}

void
ParaCommMpi::passToken(
      int tempRank
      )
{
#ifdef _MUTEX_CPP11
   std::lock_guard<std::mutex> lock(tokenAccessLock);
#else
   pthread_mutex_lock(&tokenAccessLock);
#endif
   assert( token[0] == myRank );
   token[0] = ( token[0]  % (myCommSize - 1) ) + 1;
   token[1] = -1;
   send(token, 2, ParaINT, 0, TagToken);
#ifndef _MUTEX_CPP11
   pthread_mutex_unlock(&tokenAccessLock);
#endif
}

bool
ParaCommMpi::passTermToken(
      int tempRank
      )
{
#ifdef _MUTEX_CPP11
   std::lock_guard<std::mutex> lock(tokenAccessLock);
#else
   pthread_mutex_lock(&tokenAccessLock);
#endif
   if( myRank == token[0] )
   {
      if( token[1] == token[0] ) token[1] = -2;
      else if( token[1] == -1 ) token[1] = token[0];
      token[0] = ( token[0]  % (myCommSize - 1) ) + 1;
   }
   else
   {
      THROW_LOGICAL_ERROR4("Invalid token update. Rank = ", getRank(), ", token = ", token[0] );
   }
   send(token, 2, ParaINT, 0, TagToken);
   if( token[1] == -2 )
   {
#ifndef _MUTEX_CPP11
      pthread_mutex_unlock(&tokenAccessLock);
#endif
      return true;
   }
   else
   {
#ifndef _MUTEX_CPP11
      pthread_mutex_unlock(&tokenAccessLock);
#endif
      return false;
   }
}

/** MPI call wrappers */
void
ParaCommMpi::init( int argc, char **argv )
{
   // int provided;
#ifdef UG_WITH_UGS
   if( !commUgs )
   {
      MPI_Init( &argc, &argv );
      /*
      MPI_CALL(
         MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided)
      );
      if (provided < MPI_THREAD_MULTIPLE)
      {
         std::cerr << "Error: the MPI library doesn't provide the required thread level" << std::endl;
         MPI_Abort(MPI_COMM_WORLD, 0);
      }
      */
   }
#else
   MPI_Init( &argc, &argv );
   /*
   MPI_CALL(
      MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided)
   );
   if (provided < MPI_THREAD_MULTIPLE)
   {
      std::cerr << "Error: the MPI library doesn't provide the required thread level" << std::endl;
      MPI_Abort(MPI_COMM_WORLD, 0);
   }
   */
#endif
   startTime = MPI_Wtime();
   char *pprocName = procName;
   MPI_CALL(
      MPI_Get_processor_name(pprocName, &namelen)
   );

   /** if you add tag, you should add tagStringTale too */
   assert( sizeof(tagStringTable)/sizeof(char*) == N_MPI_TAGS );

   /** Data Types */
   datatypes[ParaCHAR] = MPI_CHAR;
   datatypes[ParaSHORT] = MPI_SHORT;
   datatypes[ParaINT] = MPI_INT;
   datatypes[ParaLONG] = MPI_LONG;
   datatypes[ParaUNSIGNED_CHAR] = MPI_UNSIGNED_CHAR;
   datatypes[ParaUNSIGNED_SHORT] = MPI_UNSIGNED_SHORT;
   datatypes[ParaUNSIGNED] = MPI_UNSIGNED;
   datatypes[ParaUNSIGNED_LONG] = MPI_UNSIGNED_LONG;
   datatypes[ParaFLOAT] = MPI_FLOAT;
   datatypes[ParaDOUBLE] = MPI_DOUBLE;
   datatypes[ParaLONG_DOUBLE] = MPI_LONG_DOUBLE;
   datatypes[ParaBYTE] = MPI_BYTE;

#ifdef _ALIBABA
   datatypes[ParaSIGNED_CHAR] = MPI_CHAR;
   datatypes[ParaLONG_LONG] = MPI_LONG;
   datatypes[ParaUNSIGNED_LONG_LONG] = MPI_UNSIGNED_LONG;
   datatypes[ParaBOOL] = MPI_INT;
#else
   datatypes[ParaSIGNED_CHAR] = MPI_SIGNED_CHAR;
   datatypes[ParaLONG_LONG] = MPI_LONG_LONG;
   datatypes[ParaUNSIGNED_LONG_LONG] = MPI_UNSIGNED_LONG_LONG;
   datatypes[ParaBOOL] = MPI_INT;
#endif

}

ParaCommMpi::~ParaCommMpi()
{
   MPI_Finalize();
}

ParaCalculationState *
ParaCommMpi::createParaCalculationState(
      )
{
   return new ParaCalculationStateMpi();
}

ParaCalculationState *
ParaCommMpi::createParaCalculationState(
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
               double dualBound                   /**< final dual bound value */
           )
{
   return new ParaCalculationStateMpi(
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

ParaNode *
ParaCommMpi::createParaNode(
      )
{
   return new ParaNodeMpi();
}

ParaNode *
ParaCommMpi::createParaNode(
               NodeId inNodeId,
               NodeId inGeneratorNodeId,
               int inDepth,
               double inDualBoundValue,
               double inOriginalDualBoundValue,
               double inEstimatedValue,
               ParaDiffSubproblem *inDiffSubproblem
            )
{
    return new ParaNodeMpi(
                  inNodeId,
                  inGeneratorNodeId,
                  inDepth,
                  inDualBoundValue,
                  inOriginalDualBoundValue,
                  inEstimatedValue,
                  inDiffSubproblem
              );
}

ParaParamSet *
ParaCommMpi::createParaParamSet(
      )
{
   return new ParaParamSetMpi();
}

ParaSolverState *
ParaCommMpi::createParaSolverState(
      )
{
   return new ParaSolverStateMpi();
}

ParaSolverState *
ParaCommMpi::createParaSolverState(
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
           )
{
   return new ParaSolverStateMpi(
                  racingStage,
                  notificationId,
                  lcId,
                  globalSubtreeId,
                  nodesSolved,
                  nodesLeft,
                  bestDualBoundValue,
                  globalBestPrimalBoundValue,
                  detTime,
                  averageDualBoundGain
              );
}


ParaSolverTerminationState *
ParaCommMpi::createParaSolverTerminationState(
      )
{
   return new ParaSolverTerminationStateMpi();
}

ParaSolverTerminationState *
ParaCommMpi::createParaSolverTerminationState(
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
            )
{
   return new ParaSolverTerminationStateMpi(
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
                 iddleTimeAfterLastParaNode,
                 idleTimeToWaitNotificationId,
                 idleTimeToWaitAckCompletion,
                 idleTimeToWaitToken,
                 totalRootNodeTime,
                 minRootNodeTime,
                 maxRootNodeTime,
                 detTime
              );
}

int
ParaCommMpi::bcast(
   void* buffer,
   int count,
   const int datatypeId,
   int root
   )
{
   MPI_CALL(
      MPI_Bcast( buffer, count, datatypes[datatypeId], root, myComm )
   );
   return 0;
}

int
ParaCommMpi::send(
   void* buffer,
   int count,
   const int datatypeId,
   int dest,
   const int tag
   )
{
   MPI_CALL(
      MPI_Send( buffer, count, datatypes[datatypeId], dest, tag, myComm )
   );
   TAG_TRACE (Send, To, dest, tag);
   return 0;
}

int
ParaCommMpi::receive(
   void* buffer,
   int count,
   const int datatypeId,
   int source,
   const int tag
   )
{
   MPI_Status mpiStatus;
   MPI_CALL (
      MPI_Recv( buffer, count, datatypes[datatypeId], source, tag, myComm, &mpiStatus )
   );
   TAG_TRACE (Recv, From, source, tag);
   return 0;
}

int
ParaCommMpi::waitSpecTagFromSpecSource(
      const int source,
      const int datatypeId,
      const int tag,
      int *receivedTag
      )
{
   MPI_Status mpiStatus;
   MPI_CALL (
      MPI_Probe(source, MPI_ANY_TAG, myComm, &mpiStatus)
   );
   (*receivedTag) = mpiStatus.MPI_TAG;
   TAG_TRACE (Probe, From, source, (*receivedTag));
   if( tag == (*receivedTag) )
   {
      return 0;
   }
   else
   {
      return 1;
   }
}

void
ParaCommMpi::probe(
   int* source,
   int* tag
   )
{
   MPI_Status mpiStatus;
   MPI_CALL (
      MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, myComm, &mpiStatus)
   );
   *source = mpiStatus.MPI_SOURCE;
   *tag = mpiStatus.MPI_TAG;
   TAG_TRACE (Probe, From, *source, *tag);
}

bool
ParaCommMpi::iProbe(
   int* source,
   int* tag
   )
{
   int flag;
   MPI_Status mpiStatus;
   MPI_CALL (
      MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, myComm, &flag, &mpiStatus)
   );
   if( flag )
   {
      *source = mpiStatus.MPI_SOURCE;
      *tag = mpiStatus.MPI_TAG;
      TAG_TRACE (Iprobe, From, *source, *tag);
   }
   return flag;
}

int
ParaCommMpi::ubcast(
   void* buffer,
   int count,
   MPI_Datatype datatype,
   int root
   )
{
   MPI_CALL(
      MPI_Bcast( buffer, count, datatype, root, myComm )
   );
   return 0;
}

int
ParaCommMpi::usend(
   void* buffer,
   int count,
   MPI_Datatype datatype,
   int dest,
   const int tag
   )
{
   MPI_CALL (
      MPI_Send( buffer, count, datatype, dest, tag, myComm )
      // MPI_Ssend( buffer, count, datatype, dest, tag, myComm )  // after racing, program hang
   );
   TAG_TRACE (Send, To, dest, tag);
   return 0;
}

int
ParaCommMpi::ureceive(
   void* buffer,
   int count,
   MPI_Datatype datatype,
   int source,
   const int tag
   )
{
   MPI_Status mpiStatus;
   MPI_CALL (
      MPI_Recv( buffer, count, datatype, source, tag, myComm, &mpiStatus )
   );
   TAG_TRACE (Recv, From, source, tag);
   return 0;
}
