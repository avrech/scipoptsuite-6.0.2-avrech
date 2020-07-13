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

/**@file    scipParaRacingRampUpParamSetTh.cpp
 * @brief   ScipParaRacingRampUpParamSet extension for threads communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#include "scipParaCommTh.h"
#include "scipDiffParamSetTh.h"
#include "scipParaRacingRampUpParamSetTh.h"
#include <cstring>

using namespace ParaSCIP;

/** create Datatype */
ScipParaRacingRampUpParamSetTh *
ScipParaRacingRampUpParamSetTh::createDatatype(
      )
{
   ScipDiffParamSetTh *scipDiffParamSetTh = dynamic_cast<ScipDiffParamSetTh *>(scipDiffParamSet);

   if( scipDiffParamSetInfo )
   {
      return new ScipParaRacingRampUpParamSetTh(
            terminationCriteria,
            nNodesLeft,
            timeLimit,
            scipRacingParamSeed,
            permuteProbSeed,
            generateBranchOrderSeed,
            scipDiffParamSetTh->clone()
            );
   }
   else
   {
      return new ScipParaRacingRampUpParamSetTh(
            terminationCriteria,
            nNodesLeft,
            timeLimit,
            scipRacingParamSeed,
            permuteProbSeed,
            generateBranchOrderSeed,
            0
            );
   }
}

int
ScipParaRacingRampUpParamSetTh::send(
      UG::ParaComm *comm,
      int dest)
{

   DEF_SCIP_PARA_COMM( commTh, comm);

   PARA_COMM_CALL(
      commTh->uTypeSend((void *)createDatatype(), UG::ParaRacingRampUpParamType, dest, UG::TagRacingRampUpParamSet)
   );

   return 0;

}

int
ScipParaRacingRampUpParamSetTh::receive(
      UG::ParaComm *comm,
      int source)
{

   DEF_SCIP_PARA_COMM( commTh, comm);

   ScipParaRacingRampUpParamSetTh *received;
   PARA_COMM_CALL(
      commTh->uTypeReceive((void **)&received, UG::ParaRacingRampUpParamType, source, UG::TagRacingRampUpParamSet)
   );

   terminationCriteria = received->terminationCriteria;
   nNodesLeft = received->nNodesLeft;
   timeLimit = received->timeLimit;
   scipRacingParamSeed = received->scipRacingParamSeed;
   permuteProbSeed = received->permuteProbSeed;
   generateBranchOrderSeed = received->generateBranchOrderSeed;
   scipDiffParamSetInfo = received->scipDiffParamSetInfo;
   if( scipDiffParamSetInfo )
   {
      ScipDiffParamSetTh *scipDiffParamSetTh = dynamic_cast<ScipDiffParamSetTh *>(received->scipDiffParamSet);
      scipDiffParamSet = scipDiffParamSetTh->clone();
   }

   delete received;

   return 0;

}
