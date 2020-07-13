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

/**@file    scipParaDiffSubproblemTh.h
 * @brief   ScipParaDiffSubproblem extension for threads communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __SCIP_PARA_DIFF_SUBPROBLEM_TH_H__
#define __SCIP_PARA_DIFF_SUBPROBLEM_TH_H__

#include "ug/paraDef.h"
#ifdef _COMM_PTH
#include "ug/paraCommPth.h"
#endif
#ifdef _COMM_CPP11
#include "ug/paraCommCPP11.h"
#endif
#include "scipParaDiffSubproblem.h"
#include "scip/scip.h"

namespace ParaSCIP
{

class ScipParaSolver;

/** The difference between instance and subproblem: this is base class */
class ScipParaDiffSubproblemTh : public ScipParaDiffSubproblem
{
public:
   /** default constructor */
   ScipParaDiffSubproblemTh()
   {
   }

   /** Constructor */
   ScipParaDiffSubproblemTh(
         SCIP *inScip,
         ScipParaSolver *inScipParaSolver,
         int inNNewBranchVars,
         SCIP_VAR **inNewBranchVars,
         SCIP_Real *inNewBranchBounds,
         SCIP_BOUNDTYPE *inNewBoundTypes,
         int nAddedConss,
         SCIP_CONS **addedConss
         ) : ScipParaDiffSubproblem(inScip, inScipParaSolver,
               inNNewBranchVars, inNewBranchVars, inNewBranchBounds, inNewBoundTypes, nAddedConss, addedConss)
   {
   }

   /** Constructor */
   ScipParaDiffSubproblemTh(
         ScipParaDiffSubproblem *paraDiffSubproblem
         ) : ScipParaDiffSubproblem(paraDiffSubproblem)
   {
   }


   /** destractor */
   ~ScipParaDiffSubproblemTh()
   {
   }

   /** create clone of this object */
   ScipParaDiffSubproblemTh *clone(
         UG::ParaComm *comm
         )
   {
      return(
            new ScipParaDiffSubproblemTh(this)
      );
   }

   int bcast(
         UG::ParaComm *comm,
         int root
         )
   {
      THROW_LOGICAL_ERROR1("bcast is issued in ScipParaDiffSubproblemTh");
   }

   int send(
         UG::ParaComm *comm,
         int dest
         )
   {
      THROW_LOGICAL_ERROR1("send is issued in ScipParaDiffSubproblemTh");
   }

   int receive(
         UG::ParaComm *comm,
         int source
         )
   {
      THROW_LOGICAL_ERROR1("receive is issued in ScipParaDiffSubproblemTh");
   }
};

}

#endif    // __SCIP_PARA_DIFF_SUBPROBLEM_TH_H__

