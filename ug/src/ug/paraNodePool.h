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

/**@file    paraNodePool.h
 * @brief   ParaNode Pool.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_NODE_POOL_H__
#define __PARA_NODE_POOL_H__
#include <map>
#include <queue>
#include <cfloat>
#include <cmath>
#include "paraNode.h"
#include "paraInstance.h"
#include "paraMergeNodesStructs.h"

namespace UG
{

static const double eps = 1.0e-12;

class ParaNodeSortCriterion
{
public:
   bool operator()(const ParaNodePtr& n1, const ParaNodePtr& n2) const
   {

      return EPSLT(n1->getDualBoundValue(),n2->getDualBoundValue(),eps) ||
              ( EPSEQ(n1->getDualBoundValue(),n2->getDualBoundValue(),eps) &&
                  n1->getDiffSubproblem() && n2->getDiffSubproblem() &&
                  n1->getDiffSubproblem()->getNBoundChanges() < n2->getDiffSubproblem()->getNBoundChanges() );

   }
};

class ParaNodeSortCriterionForCleanUp
{
public:
   bool operator()(const ParaNodePtr& n1, const ParaNodePtr& n2) const
   {

      return EPSGT(n1->getDualBoundValue(),n2->getDualBoundValue(),eps) ||
              ( EPSEQ(n1->getDualBoundValue(),n2->getDualBoundValue(),eps) &&
                  n1->getDiffSubproblem() && n2->getDiffSubproblem() &&
                  n1->getDiffSubproblem()->getNBoundChanges() > n2->getDiffSubproblem()->getNBoundChanges() );

   }
};

class ParaNodePool
{
protected:
   double        bgap;
   size_t        maxUsageOfPool;

public:
   ParaNodePool(double inBgap) : bgap(inBgap), maxUsageOfPool(0)  {}
   virtual ~ParaNodePool(){}
   virtual void insert(ParaNodePtr node) = 0;
   virtual bool isEmpty() = 0;
   virtual ParaNodePtr extractNode() = 0;
   virtual ParaNodePtr extractNodeRandomly() = 0;
   virtual double getBestDualBoundValue() = 0;
   virtual unsigned int getNumOfGoodNodes(double globalBestBound) = 0;
   virtual size_t getNumOfNodes() = 0;
   virtual int removeBoundedNodes(double incumbentValue) = 0;
   virtual void updateDualBoundsForSavingNodes() = 0;
#ifdef UG_WITH_ZLIB
   virtual int writeParaNodesToCheckpointFile(gzstream::ogzstream &out) = 0;
#endif
   virtual int removeMergedNodes(ParaMergedNodeListElement *head) = 0;
   virtual const std::string toString() = 0;
   size_t getMaxUsageOfPool(){ return maxUsageOfPool; }
};

class ParaNodePoolForMinimization : virtual public ParaNodePool
{
   std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion > ascendingPool;
public:
   ParaNodePoolForMinimization(double inBgap) : ParaNodePool(inBgap)  {}
   ~ParaNodePoolForMinimization(
         )
   {
      if( ascendingPool.size() > 0 )
      {
         std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
         for( p = ascendingPool.begin(); p != ascendingPool.end(); )
         {
            if( p->second ) delete p->second;
            ascendingPool.erase(p++);
         }
      }
   }

   void insert(
         ParaNodePtr paraNode
         )
   {
      ascendingPool.insert(std::make_pair(paraNode,paraNode));
      if( maxUsageOfPool < ascendingPool.size() )
      {
         maxUsageOfPool = ascendingPool.size();
      }
   }

   bool isEmpty(){ return ( ascendingPool.size() == 0 ); }

   ParaNodePtr extractNode(
         )
   {
      ParaNodePtr extracted = 0;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
      p = ascendingPool.begin();
      while( p != ascendingPool.end() )
      {
         if( p->second->getMergeNodeInfo() )
         {
            assert( p->second->getMergeNodeInfo()->status != ParaMergeNodeInfo::PARA_MERGING );
            if( p->second->getMergingStatus() == 4 )  // merging representative was already deleted
            {
               delete p->second;              // this delete is not counted in statistics
               ascendingPool.erase(p++);
            }
            else
            {
               if( p->second->getMergeNodeInfo()->status == ParaMergeNodeInfo::PARA_MERGE_CHECKING_TO_OTHER_NODE )
               {
                  assert(dynamic_cast<ParaNodePtr>(p->second)->getMergeNodeInfo()->mergedTo->status == ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE );
                  p++;
               }
               else
               {
                  extracted = p->second;
                  ascendingPool.erase(p);
                  break;
               }
            }
         }
         else
         {
            extracted = p->second;
            ascendingPool.erase(p);
            break;
         }
      }
      assert( ( p == ascendingPool.end() && (!extracted) ) || ( p != ascendingPool.end() && extracted ) );
      return extracted;
   }

   ParaNodePtr extractNodeRandomly(
         )
   {
      ParaNodePtr extracted = 0;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
      p = ascendingPool.begin();
      size_t nNodes = ascendingPool.size();
      if( nNodes == 0 ) return extracted;
      int pos = 0;
      if( nNodes > 10 )
      {
         // select nodes randomly
         pos = rand()% static_cast<int>( nNodes * 0.8 ) + static_cast<int>(nNodes*0.1);
         if( pos < (static_cast<int>(nNodes*0.1)) || pos > nNodes*0.9 )
         {
            THROW_LOGICAL_ERROR4("Invalid pos in extractNodeRandomly in ParaNodePool: pos = ", pos, ", nNodes = ", nNodes);
         }
         for( int j = 0; j < pos; j++ )
         {
            p++;
         }
      }
      assert( p != ascendingPool.end() );
      if( p == ascendingPool.end() ) // should not happen
      {
         p = ascendingPool.begin();
      }
      if( p->second->getMergeNodeInfo() )
      {
         assert( p->second->getMergeNodeInfo()->status != ParaMergeNodeInfo::PARA_MERGING );
         if( p->second->getMergingStatus() == 4 )  // merging representative was already deleted
         {
            delete p->second;              // this delete is not counted in statistics
            ascendingPool.erase(p);
         }
         else
         {
            if( p->second->getMergeNodeInfo()->status == ParaMergeNodeInfo::PARA_MERGE_CHECKING_TO_OTHER_NODE )
            {
               assert(dynamic_cast<ParaNodePtr>(p->second)->getMergeNodeInfo()->mergedTo->status == ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE );
               std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator pp;
               pp = ascendingPool.begin();
               while( pp != p )
               {
                  if( dynamic_cast<ParaNodePtr>(pp->second) == dynamic_cast<ParaNodePtr>(p->second)->getMergeNodeInfo()->mergedTo->paraNode )
                  {
                     extracted = pp->second;
                     ascendingPool.erase(pp);
                     break;
                  }
                  pp++;
               }
            }
            else
            {
               extracted = p->second;
               ascendingPool.erase(p);
            }
         }
      }
      else
      {
         extracted = p->second;
         ascendingPool.erase(p);
      }
      if( !extracted )
      {
         /** check nodes from the head of this pool */
         p = ascendingPool.begin();
         while( p != ascendingPool.end() )
         {
            if( p->second->getMergeNodeInfo() )
            {
               assert( p->second->getMergeNodeInfo()->status != ParaMergeNodeInfo::PARA_MERGING );
               if( p->second->getMergingStatus() == 4 )  // merging representative was already deleted
               {
                  delete p->second;              // this delete is not counted in statistics
                  ascendingPool.erase(p++);
               }
               else
               {
                  if( p->second->getMergeNodeInfo()->status == ParaMergeNodeInfo::PARA_MERGE_CHECKING_TO_OTHER_NODE )
                  {
                     assert(dynamic_cast<ParaNodePtr>(p->second)->getMergeNodeInfo()->mergedTo->status == ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE );
                     p++;
                  }
                  else
                  {
                     extracted = p->second;
                     ascendingPool.erase(p);
                     break;
                  }
               }
            }
            else
            {
               extracted = p->second;
               ascendingPool.erase(p);
               break;
            }
         }
      }
      assert( ( p == ascendingPool.end() && (!extracted) ) || ( p != ascendingPool.end() && extracted ) );
      return extracted;
   }

   void updateDualBoundsForSavingNodes(
         )
   {
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
      for( p = ascendingPool.begin(); p != ascendingPool.end(); ++p )
      {
         if( p->second->getAncestor() == 0 )
         {
            p->second->updateInitialDualBoundToSubtreeDualBound();
         }
      }
   }

#ifdef UG_WITH_ZLIB
   int writeParaNodesToCheckpointFile(
         gzstream::ogzstream &out
         )
   {
      int n = 0;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
      for( p = ascendingPool.begin(); p != ascendingPool.end(); ++p )
      {
         if( p->second->getAncestor() == 0 )
         {
            p->second->write(out);
            n++;
         }
      }
      return n;
   }
#endif

   double getBestDualBoundValue(
         )
   {
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
      p = ascendingPool.begin();
      if( p != ascendingPool.end() )
      {
         return p->second->getDualBoundValue();
      }
      else
      {
         return DBL_MAX;  // no nodes exist
      }
   }

   unsigned int getNumOfGoodNodes(
         double globalBestBound
         )
   {
      /** The following code is not a good idea,
       * because only a node is received from a solver, LC can switch out
      if( globalBestBound > getBestDualBoundValue() )
         globalBestBound = getBestDualBoundValue();
      */
      int num = 0;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
      for( p = ascendingPool.begin(); p != ascendingPool.end() &&
              ( ( ( p->second->getDualBoundValue() ) - globalBestBound ) /
                    std::max( fabs(globalBestBound) , 1.0 ) ) < bgap;
            ++p )
      {
         num++;
      }
      return num;
   }

   size_t getNumOfNodes(){ return ascendingPool.size(); }

   int removeBoundedNodes(
         double incumbentValue
         )
   {
      int nDeleted = 0;
      if( ascendingPool.size() > 0 )
      {
         std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
         for( p = ascendingPool.begin(); p != ascendingPool.end(); )
         {
            assert( p->second );
            if( !p->second->getMergeNodeInfo() )
            {
               if( p->second->getDualBoundValue() > incumbentValue || p->second->getMergingStatus() == 4 )
               {
                  nDeleted++;
                  delete p->second;
                  ascendingPool.erase(p++);
               }
               else
               {
                  p++;
               }
            }
            else
            {
               if( p->second->getMergeNodeInfo()->status == ParaMergeNodeInfo::PARA_MERGE_CHECKING_TO_OTHER_NODE )
               {
                  if( p->second->getDualBoundValue() > incumbentValue || p->second->getMergingStatus() == 4 )
                   {
                      nDeleted++;
                      delete p->second;
                      ascendingPool.erase(p++);
                   }
                   else
                   {
                      p++;
                   }
               }
               else
               {
                  p++;
               }
            }
         }
      }
      return nDeleted;
   }

   int removeMergedNodes(
         ParaMergedNodeListElement *head
         )
   {
      int nDeleted = 0;
      assert( ascendingPool.size() > 0 );
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
      for( p = ascendingPool.begin(); p != ascendingPool.end() && head; )
      {
         assert( p->second );
         if( p->second->getMergingStatus() == 2 )
         {
            ParaMergedNodeListElement *prev = head;
            ParaMergedNodeListElement *cur = head;
            for( ; cur; cur=cur->next )
            {
               if( p->second == cur->node )
               {
                  break;
               }
               prev = cur;
            }
            assert(cur);
            if( prev == head )
            {
               if( cur == prev )
               {
                  head = head->next;
               }
               else
               {
                  assert( cur == prev->next );
                  prev->next = prev->next->next;
               }
            }
            else
            {
               assert( cur == prev->next );
               prev->next = prev->next->next;
            }
            assert(  p->second == cur->node );
            assert( cur->node->getMergeNodeInfo() );
            delete cur;
            nDeleted++;
            delete p->second;
            ascendingPool.erase(p++);
         }
         else
         {
            p++;
         }
      }
      assert(!head);
      return nDeleted;
   }

   const std::string toString(
         )
   {
      std::ostringstream s;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterion >::iterator p;
      for( p = ascendingPool.begin(); p != ascendingPool.end(); ++p )
      {
         s << p->second->toString();
      }
      return s.str();
   }
};

class ParaNodePoolForCleanUp : virtual public ParaNodePool
{
   std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp > descendingPool;
public:
   ParaNodePoolForCleanUp(double inBgap) : ParaNodePool(inBgap)  {}
   ~ParaNodePoolForCleanUp(
         )
   {
      if( descendingPool.size() > 0 )
      {
         std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::iterator p;
         for( p = descendingPool.begin(); p != descendingPool.end(); )
         {
            if( p->second ) delete p->second;
            descendingPool.erase(p++);
         }
      }
   }

   void insert(
         ParaNodePtr paraNode
         )
   {
      descendingPool.insert(std::make_pair(paraNode,paraNode));
      if( maxUsageOfPool < descendingPool.size() )
      {
         maxUsageOfPool = descendingPool.size();
      }
   }

   bool isEmpty(){ return ( descendingPool.size() == 0 ); }

   ParaNodePtr extractNode(
         )
   {
      ParaNodePtr extracted = 0;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::iterator p;
      p = descendingPool.begin();
      while( p != descendingPool.end() )
      {
         if( p->second->getMergeNodeInfo() )
         {
            THROW_LOGICAL_ERROR1("Nodes merging was used in clean up process!");
         }
         extracted = p->second;
         descendingPool.erase(p);
         break;
      }
      assert( ( p == descendingPool.end() && (!extracted) ) || ( p != descendingPool.end() && extracted ) );
      return extracted;
   }

   ParaNodePtr extractNodeRandomly(
         )
   {
      ParaNodePtr extracted = 0;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::iterator p;
      p = descendingPool.begin();
      while( p != descendingPool.end() )
      {
         if( p->second->getMergeNodeInfo() )
         {
            THROW_LOGICAL_ERROR1("Nodes merging was used in clean up process!");
         }
         extracted = p->second;
         descendingPool.erase(p);
         break;
      }
      assert( ( p == descendingPool.end() && (!extracted) ) || ( p != descendingPool.end() && extracted ) );
      return extracted;
   }

   void updateDualBoundsForSavingNodes(
         )
   {
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::iterator p;
      for( p = descendingPool.begin(); p != descendingPool.end(); ++p )
      {
         if( p->second->getAncestor() == 0 )
         {
            p->second->updateInitialDualBoundToSubtreeDualBound();
         }
      }
   }

#ifdef UG_WITH_ZLIB
   int writeParaNodesToCheckpointFile(
         gzstream::ogzstream &out
         )
   {
      int n = 0;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::iterator p;
      for( p = descendingPool.begin(); p != descendingPool.end(); ++p )
      {
         if( p->second->getAncestor() == 0 )
         {
            p->second->write(out);
            n++;
         }
      }
      return n;
   }
#endif

   double getBestDualBoundValue(
         )
   {
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::reverse_iterator rp;
      rp = descendingPool.rbegin();
      if( rp != descendingPool.rend() )
      {
         return rp->second->getDualBoundValue();
      }
      else
      {
         return DBL_MAX;  // no nodes exist
      }
   }

   unsigned int getNumOfGoodNodes(
         double globalBestBound
         )
   {
      /** The following code is not a good idea,
       * because only a node is received from a solver, LC can switch out
      if( globalBestBound > getBestDualBoundValue() )
         globalBestBound = getBestDualBoundValue();
      */
      int num = 0;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::reverse_iterator rp;
      for( rp = descendingPool.rbegin(); rp != descendingPool.rend() &&
              ( ( ( rp->second->getDualBoundValue() ) - globalBestBound ) /
                    std::max( fabs(globalBestBound) , 1.0 ) ) < bgap;
            ++rp )
      {
         num++;
      }
      return num;
   }

   size_t getNumOfNodes(){ return descendingPool.size(); }

   int removeBoundedNodes(
         double incumbentValue
         )
   {
      int nDeleted = 0;
      if( descendingPool.size() > 0 )
      {
         std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::iterator p;
         for( p = descendingPool.begin(); p != descendingPool.end(); )
         {
            assert( p->second );
            if( !p->second->getMergeNodeInfo() )
            {
               if( p->second->getDualBoundValue() > incumbentValue )
               {
                  nDeleted++;
                  delete p->second;
                  descendingPool.erase(p++);
               }
               else
               {
                  p++;
               }
            }
            else
            {
               THROW_LOGICAL_ERROR1("Nodes merging was used in clean up process!");
            }
         }
      }
      return nDeleted;
   }

   int removeMergedNodes(
         ParaMergedNodeListElement *head
         )
   {
      THROW_LOGICAL_ERROR1("Nodes merging was used in clean up process!");
   }

   const std::string toString(
         )
   {
      std::ostringstream s;
      std::multimap<ParaNodePtr, ParaNodePtr, ParaNodeSortCriterionForCleanUp >::iterator p;
      for( p = descendingPool.begin(); p != descendingPool.end(); ++p )
      {
         s << p->second->toString();
      }
      return s.str();
   }
};

}

#endif // __PARA_NODE_POOL_H__
