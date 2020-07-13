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

/**@file    paraNodeTh.cpp
 * @brief   ParaNode extension for threads communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifdef _COMM_PTH
#include "paraCommPth.h"
#endif
#ifdef _COMM_CPP11
#include "paraCommCPP11.h"
#endif
#include "paraNodeTh.h"

using namespace UG;

ParaNodeTh *
ParaNodeTh::createDatatype(
    ParaComm *comm
      )
{
   return clone(comm);
}

int
ParaNodeTh::bcast(
      ParaComm *comm,
      int root
      )
{
   DEF_PARA_COMM( commTh, comm);

   if( commTh->getRank() == root )
   {
      for( int i = 0; i < commTh->getSize(); i++ )
      {
         if( i != root )
         {
            ParaNodeTh *sent;
            sent = createDatatype(comm);
            assert(!(sent->mergeNodeInfo));
            sent->mergeNodeInfo = 0;
            PARA_COMM_CALL(
               commTh->uTypeSend((void *)sent, ParaNodeType, i, TagNode)
            );
         }
      }
   }
   else
   {
      ParaNodeTh *received;
      PARA_COMM_CALL(
         commTh->uTypeReceive((void **)&received, ParaNodeType, root, TagNode)
      );
      nodeId = received->nodeId;
      generatorNodeId = received->generatorNodeId;
      depth = received->depth;
      dualBoundValue = received->dualBoundValue;
      initialDualBoundValue = received->initialDualBoundValue;
      estimatedValue = received->estimatedValue;
      diffSubproblemInfo = received->diffSubproblemInfo;
      if( diffSubproblemInfo )
      {
         diffSubproblem = received->diffSubproblem->clone(commTh);
      }
      basisInfo = received->basisInfo;
      mergingStatus = received->mergingStatus;
      delete received;
   }
   return 0;
}

int
ParaNodeTh::send(
      ParaComm *comm,
      int destination
      )
{
    DEF_PARA_COMM( commTh, comm);

    ParaNodeTh *sent;
    sent = createDatatype(comm);
    assert(!(sent->mergeNodeInfo));
    sent->mergeNodeInfo = 0;
    PARA_COMM_CALL(
       commTh->uTypeSend((void *)sent, ParaNodeType, destination, TagNode)
    );

   return 0;
}

int
ParaNodeTh::receive(
      ParaComm *comm,
      int source
      )
{
   DEF_PARA_COMM( commTh, comm);

   ParaNodeTh *received;
   PARA_COMM_CALL(
      commTh->uTypeReceive((void **)&received, ParaNodeType, source, TagNode)
   );
   nodeId = received->nodeId;
   generatorNodeId = received->generatorNodeId;
   depth = received->depth;
   dualBoundValue = received->dualBoundValue;
   initialDualBoundValue = received->initialDualBoundValue;
   estimatedValue = received->estimatedValue;
   diffSubproblemInfo = received->diffSubproblemInfo;
   if( diffSubproblemInfo )
   {
      diffSubproblem = received->diffSubproblem->clone(commTh);
   }
   basisInfo = received->basisInfo;
   mergingStatus = received->mergingStatus;
   delete received;

   return 0;
}
