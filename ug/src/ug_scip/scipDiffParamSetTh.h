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

/**@file    scipDiffParamSetTh.h
 * @brief   ScipDiffParamSet extension for threads communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __SCIP_DIFF_PARAM_SET_TH_H__
#define __SCIP_DIFF_PARAM_SET_TH_H__

#ifdef _COMM_PTH
#include "ug/paraCommPth.h"
#endif
#ifdef _COMM_CPP11
#include "ug/paraCommCPP11.h"
#endif
#include "scipDiffParamSet.h"

namespace ParaSCIP
{

/** ScipDiffParamSet class */
class ScipDiffParamSetTh: public ScipDiffParamSet
{

   /** create datatype */
   ScipDiffParamSetTh *createDatatype();

   void setValues(ScipDiffParamSetTh *from);

public:
   /** constructor */
   ScipDiffParamSetTh(
         )
   {
   }

   /** constructor with scip */
   ScipDiffParamSetTh(
         SCIP *scip
         )
         : ScipDiffParamSet(scip)
   {
   }

   /** destructor */
   ~ScipDiffParamSetTh(
         )
   {
   }

   /** create clone */
   ScipDiffParamSetTh *clone();

  /** broadcast scipDiffParamSet */
  int bcast(UG::ParaComm *comm, int root);

  /** send scipDiffParamSet to the rank */
  int send(UG::ParaComm *comm, int destination);

  /** receive scipDiffParamSet from the source rank */
  int receive(UG::ParaComm *comm, int source);

};

typedef ScipDiffParamSet *ScipDiffParamSetPtr;

}

#endif // _SCIP_DIFF_PARAM_SET_TH_H__

