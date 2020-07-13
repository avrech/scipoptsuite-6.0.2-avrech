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

/**@file    scipUserPlugins.h
 * @brief   SCIP user plugins.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __SCIP_USER_PLUGINS_H__
#define __SCIP_USER_PLUGINS_H__
#include "scip/scip.h"
#include "scipParaDiffSubproblem.h"

namespace ParaSCIP
{

/** ScipUserPlugins class */
class ScipUserPlugins {
public:
   virtual ~ScipUserPlugins(){}
   virtual void operator()(SCIP *scip) = 0;
   virtual void writeUserSolution(SCIP *scip, int nSolvers, double dual){}
   virtual void newSubproblem(SCIP *scip, const ScipParaDiffSubproblemBranchLinearCons *linearConss, const ScipParaDiffSubproblemBranchSetppcCons *setppcConss){}
};

}

#endif // _SCIP_USER_PLUGINS_H__
