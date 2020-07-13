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

/**@file    scipParaDiffSubproblemMpi.h
 * @brief   ScipParaDiffSubproblem extension for MPI communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __SCIP_PARA_DIFF_SUBPROBLEM_MPI_H__
#define __SCIP_PARA_DIFF_SUBPROBLEM_MPI_H__

#include <mpi.h>
#include "ug/paraComm.h"
#include "scipParaDiffSubproblem.h"

namespace ParaSCIP
{

/** The difference between instance and subproblem: this is base class */
class ScipParaDiffSubproblemMpi : public ScipParaDiffSubproblem
{

   /** create scipDiffSubproblem datatypeCounters */
   MPI_Datatype createDatatypeCounters();
   /** create scipDiffSubproblem datatypeBoundChnages */
   MPI_Datatype createDatatypeBoundChnages(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeBranchLinearConss1 */
   MPI_Datatype createDatatypeBranchLinearConss1(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeBranchLinearConss2 */
   MPI_Datatype createDatatypeBranchLinearConss2(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeBranchSetppcConss1 */
   MPI_Datatype createDatatypeBranchSetppcConss1(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeBranchSetppcConss2 */
   MPI_Datatype createDatatypeBranchSetppcConss2(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeLinearConss1 */
   MPI_Datatype createDatatypeLinearConss1(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeLinearConss2 */
   MPI_Datatype createDatatypeLinearConss2(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeBoundDisjunctions1 */
   MPI_Datatype createDatatypeBoundDisjunctions1(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeBoundDisjunctions2 */
   MPI_Datatype createDatatypeBoundDisjunctions2(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeVarBranchStats */
   MPI_Datatype createDatatypeVarBranchStats(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeVarValueVars1 */
   MPI_Datatype createDatatypeVarValueVars1(bool memAllocNecessary);
   /** create scipDiffSubproblem datatypeVarValueVars2 */
   MPI_Datatype createDatatypeVarValueVars2(bool memAllocNecessary);


   int nBranchLinearConss;    // 0 means that this is not used
   int nBranchSetppcConss;    // 0 means that this is not used

   int nLinearConss;
   int nBoundDisjunctions;
   int nVarBranchStats;
   int nVarValueVars;

public:
   /** default constructor */
   ScipParaDiffSubproblemMpi() : nBranchLinearConss(0), nBranchSetppcConss(0), nLinearConss(0), nBoundDisjunctions(0), nVarBranchStats(0), nVarValueVars(0)
   {
      assert( localInfoIncluded == 0 && nBoundChanges == 0 && nLinearConss == 0 );
   }

   /** Constructor */
   ScipParaDiffSubproblemMpi(
         SCIP *inScip,
         ScipParaSolver *inScipParaSolver,
         int inNNewBranchVars,
         SCIP_VAR **inNewBranchVars,
         SCIP_Real *inNewBranchBounds,
         SCIP_BOUNDTYPE *inNewBoundTypes,
         int nAddedConss,
         SCIP_CONS **addedConss
         ) : ScipParaDiffSubproblem(inScip, inScipParaSolver,
               inNNewBranchVars, inNewBranchVars, inNewBranchBounds,inNewBoundTypes, nAddedConss, addedConss), nBranchLinearConss(0), nBranchSetppcConss(0), nLinearConss(0), nBoundDisjunctions(0), nVarBranchStats(0), nVarValueVars(0)
   {
   }

   /** Constructor */
   ScipParaDiffSubproblemMpi(
         ScipParaDiffSubproblem *paraDiffSubproblem
         ) : ScipParaDiffSubproblem(paraDiffSubproblem), nBranchLinearConss(0), nBranchSetppcConss(0), nLinearConss(0), nBoundDisjunctions(0), nVarBranchStats(0), nVarValueVars(0)
   {
   }


   /** destractor */
   ~ScipParaDiffSubproblemMpi()
   {
   }

   /** create clone of this object */
   ScipParaDiffSubproblemMpi *clone(UG::ParaComm *comm);

   int bcast(UG::ParaComm *comm, int root);

   int send(UG::ParaComm *comm, int dest);

   int receive(UG::ParaComm *comm, int source);
};

}

#endif    // __SCIP_PARA_DIFF_SUBPROBLEM_MPI_H__

