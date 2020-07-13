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

/**@file    paraRacingRampUpParamSet.h
 * @brief   Base class for racing ramp-up parameter set.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_RACING_RAMP_UP_PARAM_SET_H__
#define __PARA_RACING_RAMP_UP_PARAM_SET_H__

#include "paraComm.h"
#ifdef UG_WITH_ZLIB
#include "gzstream.h"
#endif

namespace UG
{

static const int RacingTerminationNotDefined = -1;
static const int RacingTerminateWithNNodesLeft = 0;
static const int RacingTerminateWithTimeLimit = 1;

/** Parameter set for racing ramp-up */
class ParaRacingRampUpParamSet
{
protected:
   int terminationCriteria;             /**< termination criteria of racing ramp-up : 0: number of nodes left, 1: time limit */
   int nNodesLeft;                      /**< stop racing number of nodes left */
   double timeLimit;                    /**< stop racing time limit */
public:
   /** default constructor */
   ParaRacingRampUpParamSet(
         ) : terminationCriteria(RacingTerminationNotDefined), nNodesLeft(-1), timeLimit(-1.0)
   {
   }
   /** constructor */
   ParaRacingRampUpParamSet(
         int inTerminationCriteria,
         int inNNodesLeft,
         double inTimeLimit
         ) : terminationCriteria(inTerminationCriteria), nNodesLeft(inNNodesLeft), timeLimit(inTimeLimit)
   {
   }

   /** destructor */
   virtual ~ParaRacingRampUpParamSet(
         )
   {
   }

   /** get termination criteria */
   int getTerminationCriteria()
   {
      return terminationCriteria;
   }

   /** get stop racing number of nodes left */
   int getStopRacingNNodesLeft()
   {
      return nNodesLeft;
   }

   /** get stop racing time limimt */
   double getStopRacingTimeLimit()
   {
      return timeLimit;
   }

   virtual void setWinnerRank(int rank){}
   virtual int send(ParaComm *comm, int destination ) = 0;
   virtual int receive(ParaComm *comm, int source ) = 0;
#ifdef UG_WITH_ZLIB
   virtual void write(gzstream::ogzstream &out) = 0;
   virtual bool read(ParaComm *comm, gzstream::igzstream &in) = 0;
#endif
   virtual const std::string toString() = 0;
   virtual int getStrategy() = 0;

};

typedef ParaRacingRampUpParamSet *ParaRacingRampUpParamSetPtr;

}

#endif // __PARA_RACING_RAMP_UP_PARAM_SET_H__

