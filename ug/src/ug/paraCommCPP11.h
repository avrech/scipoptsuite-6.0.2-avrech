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

/**@file    paraCommCPP11.h
 * @brief   ParaComm extension for C++11 thread communication
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_COMM_CPP11_H__
#define __PARA_COMM_CPP11_H__

#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <iostream>
#include <ostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <cassert>
#include "paraDef.h"
#include "paraComm.h"
#include "paraSysTimer.h"
#include "paraParamSetTh.h"
#include "paraTimerTh.h"
// #include "paraPthLock.h"
// #include "paraPthCondVar.h"

// #define HashEntry( tid ) ( hashCode(tid) % UG::ThreadTableSize )

#define TAG_TRACE( call, fromTo, sourceDest, tag ) \
   if( tagTraceFlag )  \
   {  \
      if( tag >= 0 ) \
      { \
         /* std::cout << " call = " << #call << ", Rank = " << getRank() << ", tag = " << tag << ", " << tagStringTable[tag] << std::endl; */ \
         *getOstream() << timer.getRTimeInterval() << " [Rank = " << getRank() << "] " << #call << " " << #fromTo  \
         << " " << sourceDest << " with Tag = " << tagStringTable[tag] << std::endl; \
      } \
      else \
      { \
         /* std::cout << " call = " << #call << ", Rank = " << getRank() << ", tag = " << tag << std::endl; */  \
         *getOstream() << timer.getRTimeInterval() << " [Rank = " << getRank() << "] " << #call << " " << #fromTo  \
         << " " << sourceDest << " as broadcast" << std::endl; \
      } \
}

namespace UG
{

static const int USER_TYPE_FIRST                = TYPE_LAST + 1;
static const int ParaInstanceType               = USER_TYPE_FIRST +  0;
static const int ParaSolutionType               = USER_TYPE_FIRST +  1;
static const int ParaParamSetType               = USER_TYPE_FIRST +  2;
static const int ParaNodeType                   = USER_TYPE_FIRST +  3;
static const int ParaSolverStateType            = USER_TYPE_FIRST +  4;
static const int ParaCalculationStateType       = USER_TYPE_FIRST +  5;
static const int ParaSolverTerminationStateType = USER_TYPE_FIRST +  6;
static const int ParaRacingRampUpParamType      = USER_TYPE_FIRST +  7;
static const int ParaSolverDiffParamType        = USER_TYPE_FIRST +  8;
static const int ParaInitialStatType            = USER_TYPE_FIRST +  9;

static const int ThreadTableSize = 751;

class MessageQueueElement
{
   int source;                 // source thread rank of this message
   int count;                  // number of elements of the data type
   int dataTypeId;             // data type id
   int tag;                    // -1 : in case of broadcast message
   void *data;                 // NOTE : For basic data types, this is copy of sender side memory.
                               //        When the memory is copied at receive function, the memory
                               //        have to be freed.
                               //        For user defined data type, this is the receiver side memory,
                               //        because it is better to allocate memory in the sender side for
                               //        mutex locking. Sender side functions have to allocate memory.
                               //        In this case, memory do not hvae to freed. The memory is for
                               //        receiver side.
   MessageQueueElement *next;
public:
   MessageQueueElement(
         ) : source(-1), count(0), dataTypeId(-1), tag(-1), data(0), next(0)
   {
   }
   MessageQueueElement(
         int inSource,
         int inCount,
         int inDataTypeId,
         int inTag,
         void *inData
         ) : source(inSource), count(inCount), dataTypeId(inDataTypeId), tag(inTag), data(inData), next(0)
   {
   }
   ~MessageQueueElement(
         )
   {
   }

   int getSource(
         )
   {
      return source;
   }

   int getCount(
         )
   {
      return count;
   }

   int getDataTypeId(
         )
   {
      return dataTypeId;
   }

   int getTag(
         )
   {
      return tag;
   }

   void *getData(
         )
   {
      return data;
   }

   MessageQueueElement *getNext(
         )
   {
      return next;
   }

   void link(
         MessageQueueElement *nextElement
         )
   {
      next = nextElement;
   }
};

class MessageQueueTableElement
{
   MessageQueueElement *head;
   MessageQueueElement *tail;
public:
   MessageQueueTableElement(
           ) : head(0), tail(0)
   {
   }

   ~MessageQueueTableElement(
         )
   {
      while( head )
      {
         MessageQueueElement *next = head->getNext();
         delete head;
         head = next;
      }
   }

   MessageQueueElement *checkElement(
         int source,
         int datatypeId,
         int tag
         )
   {
      MessageQueueElement *ret = 0;
      for( MessageQueueElement *current = head; current; current = current->getNext() )
      {
         if( current->getSource() == source
               && current->getDataTypeId() == datatypeId
               && current->getTag() == tag )
         {
            ret = current;
            break;
         }
      }
      return ret;
   }

   MessageQueueElement *extarctElement(
         bool *sentMessage,
         int source,
         int datatypeId,
         int tag
         )
   {
      MessageQueueElement *ret = 0;

      MessageQueueElement *prev = head;
      MessageQueueElement *current = head;
      while( current )
      {
         MessageQueueElement *next = current->getNext();
         if( current->getSource() == source
               && current->getDataTypeId() == datatypeId
               && current->getTag() == tag )
         {
            ret = current;
            ret->link(0);
            if( current == head )
            {
               if( current == tail )
               {
                  head = 0;
                  tail = 0;
                  *sentMessage = false;
               }
               else
               {
                  head=next;
               }
            }
            else
            {
               if( current == tail )
               {
                  tail = prev;
               }
               prev->link(next);
            }
            break;
         }
         else
         {
            prev = current;
            current = next;
         }
      }
      return ret;
   }

   // This method is only for desctructor or ParaCommCPP11. No lock is necessary.
   MessageQueueElement *extarctElement(
         bool              *sentMessage
         )
   {
      if( head == 0 ) return 0;

      MessageQueueElement *current = head;
      MessageQueueElement *next = current->getNext();
      current->link(0);
      if( current == tail )
      {
         head = 0;
         tail = 0;
         *sentMessage = false;
      }
      else
      {
         head=next;
      }
      return current;
   }

   void enqueue(
         std::condition_variable& sentMsg,
         std::mutex&              queueLockMutex,
         bool                     *sentMessage,
         MessageQueueElement      *newElement
         )
   {
      if( tail == 0 )
      {
         head = tail = newElement;
      }
      else
      {
         tail->link(newElement);
         tail = newElement;
      }
      *sentMessage = true;
      sentMsg.notify_one();
   }

   MessageQueueElement *getHead(
         )
   {
      return head;
   }

   bool isEmpty(
         )
   {
      bool empty = true;
      if( head ) empty = false;
      return empty;
   }

   void waitMessage(
         std::condition_variable& sentMsg,
         std::mutex&              queueLockMutex,
         bool                     *sentMessage
         )
   {
      std::unique_lock<std::mutex> lk(queueLockMutex);
      sentMsg.wait(lk, [&sentMessage]{return (*sentMessage == true);});
   }

   void waitMessage(
         std::condition_variable& sentMsg,
         std::mutex&              queueLockMutex,
         bool                     *sentMessage,
         int                      source,
         int                      datatypeId,
         int                      tag
         )
   {
      for(;;)
      {
         std::unique_lock<std::mutex> lk(queueLockMutex);
         sentMsg.wait(lk, [&sentMessage]{return (*sentMessage == true);});
         MessageQueueElement *current = head;
         while( current )
         {
            MessageQueueElement *next = current->getNext();
            if( current->getSource() == source
                  && current->getDataTypeId() == datatypeId
                  && current->getTag() == tag )
            {
               break;
            }
            current = next;
         }
         if( current ) break;
         *sentMessage = false;
      }
   }

   void waitMessage(
         std::condition_variable& sentMsg,
         std::mutex&              queueLockMutex,
         bool                     *sentMessage,
         int                      source,
         int                      *tag
         )
   {
      for(;;)
      {
         std::unique_lock<std::mutex> lk(queueLockMutex);
         sentMsg.wait(lk, [&sentMessage]{return (*sentMessage == true);});
         MessageQueueElement *current = head;
         while( current )
         {
            MessageQueueElement *next = current->getNext();
            if( current->getSource() == source )
            {
               *tag = current->getTag();
               break;
            }
            current = next;
         }
         if( current ) break;
         *sentMessage = false;
      }
   }

};

class ThreadsTableElement
{
   int                rank;         /*< rank of this thread */
   std::ostream       *tos;         /*< tag trace stream for this thread */
public:
   ThreadsTableElement(
         ) : rank(0), tos(0)
   {
   }

   ThreadsTableElement(
         int       inRank,
         ParaParamSet *paraParamSet
         ) : rank(inRank), tos(0)
   {
      if( paraParamSet->getBoolParamValue(TagTrace) )
      {
         if( paraParamSet->isStringParamDefaultValue(TagTraceFileName) )
         {
            tos = &std::cout;
         }
         else
         {
            std::ostringstream s;
            s << paraParamSet->getStringParamValue(TagTraceFileName) << inRank;
            std::ofstream  *ofs = new std::ofstream();
            ofs->open(s.str().c_str());
            tos = ofs;
         }
      }
   }

   ~ThreadsTableElement(
         )
   {
      if( tos )
      {
         if( tos != &std::cout )
         {
            std::ofstream *ofs = dynamic_cast<std::ofstream *>(tos);
            ofs->close();
            delete ofs;
         }
      }
   }

   int getRank(
         )
   {
      return rank;
   }

   void setRank(
         int r
         )
   {
      rank = r;
   }

   std::ostream *getOstream(
         )
   {
      return tos;
   }
};

class ParaCalculationState;
class ParaNode;
class ParaSolverState;
class ParaSolverTerminationState;
class ParaDiffSubproblem;
class ParaInstance;
class ParaSolution;

class ParaCommCPP11 : public ParaComm
{
protected:
   int                      comSize;
   bool                     tagTraceFlag;
   int                      **token;    // index 0: token
                                        // index 1: token color
                                        //           -1: green
                                        //           > 0: yellow ( termination origin solver number )
                                        //           -2: red ( means the solver can terminate )
   ParaSysTimer             timer;
   static const char        *tagStringTable[];
   static ThreadsTableElement  *threadsTable[ThreadTableSize];   // index is the rank
   static thread_local int  localRank;
   MessageQueueTableElement **messageQueueTable;
   bool                     *sentMessage;
   std::mutex               *queueLockMutex;
   std::condition_variable  *sentMsg;

   std::mutex               *tokenAccessLockMutex;
   std::mutex               applicationLockMutex;
   std::mutex               rankLockMutex;

   void *allocateMemAndCopy(void* buffer, int count, const int datatypeId);
   void copy(void *dest, void *src, int count, int datatypeId);
   void freeMem(void* buffer, int count, const int datatypeId);

public:
   ParaCommCPP11() : comSize(-1),
         tagTraceFlag(false), token(0), messageQueueTable(0), sentMessage(0), queueLockMutex(0), tokenAccessLockMutex(0)
   {
   }

   virtual void init( int argc, char **argv );
   virtual ~ParaCommCPP11();
   double getStartTime(){ // should not used
      return timer.getStartTime();
   }
   int getRank();
   int getSize(){ return comSize; }
   void lcInit(ParaParamSet *paraParamSet);
   void solverInit(ParaParamSet *paraParamSet){}
   void solverInit(int rank, ParaParamSet *paraParamSet);
   void solverReInit(int rank, ParaParamSet *paraParamSet);
   void solverDel(int rank);
   void abort(){std::abort();}
   bool waitTerminatedMessage(){ return false;}
   bool waitToken(int rank);
   void passToken(int rank);
   bool passTermToken(int rank);
   void setToken(int rank, int *inToken);
   std::ostream *getOstream();

   void lockApp()
   {
      applicationLockMutex.lock();
   }
   void unlockApp()
   {
      applicationLockMutex.unlock();
   }

   void lockRank()
   {
      rankLockMutex.lock();
   }
   void unlockRank()
   {
      rankLockMutex.unlock();
   }

   void lockApp(char const *f, int l)
   {
      applicationLockMutex.lock();
      std::cout << "App Locked: " << f << ", " << l << std::endl;
   }

   void lockRank(char const *f, int l)
   {
      rankLockMutex.lock();
      std::cout << "Rank Locked: " << f << ", " << l << std::endl;
   }
   void unlockRank(char const *f, int l)
   {
      rankLockMutex.unlock();
      std::cout << "Rank Unlocked: " << f << ", " << l << std::endl;
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
               double dualBound                   /**< final dual bound value */
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
   ParaTimer *createParaTimer(){ return new ParaTimerTh(); }
   int bcast( void* buffer, int count, const int datatypeId, int root );
   int send( void* bufer, int count, const int datatypeId, int dest, const int tag );
   int receive( void* bufer, int count, const int datatypeId, int source, const int tag );
   int waitSpecTagFromSpecSource(const int source, const int datatypeId, const int tag, int *receivedTag);
   void probe(int *source, int *tag);
   bool iProbe(int *source, int *tag);  /** return value: true => there is message, false => no message */

   /** For Created Datatypes */
   int uTypeSend( void* bufer, const int datatypeId, int dest, int tag );
   int uTypeReceive( void** bufer, const int datatypeId, int source, int tag );
};

#define DEF_PARA_COMM( para_comm, comm ) ParaCommCPP11 *para_comm = dynamic_cast< ParaCommCPP11* >(comm)
#define LOCK_APP( comm ) comm->lockApp(__FILE__, __LINE__)
#define LOCK_RANK( comm ) comm->lockRank(__FILE__, __LINE__)
#define UNLOCK_RANK( comm ) comm->unlockRank(__FILE__, __LINE__)
}

#endif  // __PARA_COMM_CPP11_H__
