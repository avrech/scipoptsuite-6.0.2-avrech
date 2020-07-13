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

/**@file    paraCalculationState.h
 * @brief   Base class for calculation state.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_CALCULATION_STATE_H__
#define __PARA_CALCULATION_STATE_H__

#include <climits>
#include <cfloat>
#include "paraComm.h"

namespace UG
{

/** Calculation state in a ParaSolver */
class ParaCalculationState
{
protected:
   double compTime;              /**< computation time of this ParaNode */
   double rootTime;              /**< computation time of the root node */
   int    nSolved;               /**< the number of nodes solved   */
   int    nSent;                 /**< the number of ParaNodes sent */
   int    nImprovedIncumbent;    /**< the number of improved solution generated in this ParaSolver */
   int    terminationState;           /**< indicate whether if this computation is terminationState or not. 0: no, 1: terminationState */
   int    nSolvedWithNoPreprocesses;  /**< number of nodes solved when it is solved with no preprocesses */
   int    nSimplexIterRoot;           /**< number of simplex iteration at root node */
   double averageSimplexIter;         /**< average number of simplex iteration except root node */
   int    nTransferredLocalCuts;      /**< number of local cuts transferred from a ParaNode */
   int    minTransferredLocalCuts;    /**< minimum number of local cuts transferred from a ParaNode */
   int    maxTransferredLocalCuts;    /**< maximum number of local cuts transferred from a ParaNode */
   int    nRestarts;                  /**< number of restarts */
   double minIisum;                   /**< minimum sum of integer infeasibility */
   double maxIisum;                   /**< maximum sum of integer infeasibility */
   int    minNii;                     /**< minimum number of integer infeasibility */
   int    maxNii;                     /**< maximum number of integer infeasibility */
   double dualBound;                  /**< final dual bound value */
public:
   /** default constructor */
   ParaCalculationState(
         )
         : compTime(0.0), rootTime(0.0), nSolved(-1), nSent(-1),
         nImprovedIncumbent(-1), terminationState(-1), nSolvedWithNoPreprocesses(-1),
         nSimplexIterRoot(0), averageSimplexIter(0.0),
         nTransferredLocalCuts(0), minTransferredLocalCuts(INT_MAX), maxTransferredLocalCuts(INT_MIN),
         nRestarts(0), minIisum(0.0), maxIisum(0.0),
         minNii(0), maxNii(0), dualBound(-DBL_MAX)
   {
   }
   /** constructor */
   ParaCalculationState(
         double inCompTime,                   /**< computation time of this ParaNode */
         double inRootTime,                   /**< computation time of the root node */
         int    inNSolved,                    /**< the number of nodes solved   */
         int    inNSent,                      /**< the number of ParaNodes sent */
         int    inNImprovedIncumbent,         /**< the number of improved solution generated in this ParaSolver */
         int    inTerminationState,           /**< indicate whether if this computation is terminationState or not. 0: no, 1: terminationState */
         int    inNSolvedWithNoPreprocesses,  /**< number of nodes solved when it is solved with no preprocesses */
         int    inNSimplexIterRoot,           /**< number of simplex iteration at root node */
         double inAverageSimplexIter,         /**< average number of simplex iteration except root node */
         int    inNTransferredLocalCuts,      /**< number of local cuts transferred from a ParaNode */
         int    inMinTransferredLocalCuts,    /**< minimum number of local cuts transferred from a ParaNode */
         int    inMaxTransferredLocalCuts,    /**< maximum number of local cuts transferred from a ParaNode */
         int    inNRestarts,                  /**< number of restarts */
         double inMinIisum,                   /**< minimum sum of integer infeasibility */
         double inMaxIisum,                   /**< maximum sum of integer infeasibility */
         int    inMinNii,                     /**< minimum number of integer infeasibility */
         int    inMaxNii,                     /**< maximum number of integer infeasibility */
         double inDualBound                   /**< final dual Bound value */
         )
         : compTime(inCompTime), rootTime(inRootTime), nSolved(inNSolved),
         nSent(inNSent), nImprovedIncumbent(inNImprovedIncumbent),
         terminationState(inTerminationState), nSolvedWithNoPreprocesses(inNSolvedWithNoPreprocesses),
         nSimplexIterRoot(inNSimplexIterRoot), averageSimplexIter(inAverageSimplexIter),
         nTransferredLocalCuts(inNTransferredLocalCuts), minTransferredLocalCuts(inMaxTransferredLocalCuts), maxTransferredLocalCuts(inMaxTransferredLocalCuts),
         nRestarts(inNRestarts), minIisum(inMinIisum), maxIisum(inMaxIisum), minNii(inMinNii), maxNii(inMaxNii),
         dualBound(inDualBound)
   {
   }
   /** destructor */
   virtual ~ParaCalculationState(
         )
   {
   }

   /** getter of compTime */
   double getCompTime(
         )
   {
      return compTime;
   }

   /** getter of rootTime */
   double getRootTime(
         )
   {
      return rootTime;
   }

   /** getter of nRestarts */
   int getNRestarts(
         )
   {
      return nRestarts;
   }

   /** getter of average node computaion time excpet root */
   double getAverageNodeCompTimeExcpetRoot(
         )
   {
      if( nSolved > 1 )
      {
         return ((compTime - rootTime)/(nSolved - 1));
      }
      else
      {
         return 0.0;
      }
   }

   /** getter of nSolved */
   int getNSolved(
         )
   {
      return nSolved;
   }

   /** getter of nSent */
   int getNSent(
         )
   {
      return nSent;
   }

   /** getter of nImprovedIncumbent */
   int getNImprovedIncumbent(
         )
   {
      return nImprovedIncumbent;
   }

   /** getter of nInterrupted */
   int getTerminationState(
         )
   {
      return terminationState;
   }

   /** getter of nInterrupted */
   int getNSolvedWithNoPreprocesses(
         )
   {
      return nSolvedWithNoPreprocesses;
   }

   /** getter of final dual bound value */
   double getDualBoundValue(
         )
   {
      return dualBound;
   }

   /** stringfy ParaCalculationState */
   std::string toString(
         )
   {
      std::ostringstream s;
      if( terminationState )
      {
         s << "Termination state of this computation was " << terminationState << " : [ "
         << compTime << " sec. computed ]"
         << nSolved << " nodes were solved, "
         << nSent << " nodes were sent, "
         << nImprovedIncumbent << " improved solutions were found";
      }
      else
      {
         s << "Computation was normally terminated: [ "
         << compTime << " sec. computed ]"
         << nSolved << " nodes were solved, "
         << nSent << " nodes were sent, "
         << nImprovedIncumbent << " improved solutions were found";
      }
      return s.str();
   }

   /** stringfy ParaCalculationState */
   std::string toSimpleString(
         )
   {
      std::ostringstream s;

      s << compTime
            << ", "
            << rootTime
            << ", "
            << nSolved
            << ", "
            << nSent
            << ", "
            << nImprovedIncumbent
            << ", "
            << nSimplexIterRoot
            << ", "
            << averageSimplexIter
            << ", "
            << nRestarts
            << ", ";

      if( maxNii > 0 )
      {
         s << minIisum
               << ", "
               << maxIisum
               << ", "
               << minNii
               << ", "
               << maxNii;
      }
      else
      {
         s << ", -, -, -, -";
      }
      s << ", " << dualBound;

      return s.str();
   }

   virtual void send(ParaComm *comm, int destination, int tag) = 0;
   virtual void receive(ParaComm *comm, int source, int tag) = 0;

};

}

#endif // __PARA_CALCULATION_STATE_H__
