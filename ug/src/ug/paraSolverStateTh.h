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

/**@file    paraSolverStateTh.h
 * @brief   ParaSolverState extension for threads communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_SOLVER_STATE_TH_H__
#define __PARA_SOLVER_STATE_TH_H__

#include "paraSolverState.h"

namespace UG
{

/** ParaSolver state object for notification message */
class ParaSolverStateTh : public ParaSolverState
{
   /** create ParaNode datatype */
   ParaSolverStateTh *createDatatype();
public:
   /** default constructor */
   ParaSolverStateTh(
         )
   {
   }

   /** constructor */
   ParaSolverStateTh(
         int          inRacingStage,
         unsigned int inNotificationId,
         int inLcId,
         int inGlobalSubtreeId,
         long long inNodesSolved,
         int inNodesLeft,
         double inBestDualBoundValue,
         double inGlobalBestPrimalBoundValue,
         double inDetTime,
         double inAverageDualBoundGain
         ) : ParaSolverState(inRacingStage, inNotificationId, inLcId, inGlobalSubtreeId,
               inNodesSolved, inNodesLeft, inBestDualBoundValue, inGlobalBestPrimalBoundValue, inDetTime, inAverageDualBoundGain)
   {
   }

   /** destractor */
   ~ParaSolverStateTh(
         )
   {
   }
   void send(ParaComm *comm, int destination, int tag);
   void receive(ParaComm *comm, int source, int tag);
};

#define DEF_PARA_SOLVER_STATE( para_state, state ) ParaSolverStateTh *para_state = dynamic_cast< ParaSolverStateTh* >(state)

}

#endif // __PARA_SOLVER_STATE_TH_H__

