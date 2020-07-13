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

/**@file    paraNodeMpi.cpp
 * @brief   ParaNode extension for MIP communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#include <mpi.h>
#include "paraNodeMpi.h"

using namespace UG;

MPI_Datatype
ParaNodeMpi::createDatatype(
      )
{
   const int nBlocks = 15;

   MPI_Datatype datatype;

   MPI_Aint startAddress = 0;
   MPI_Aint address = 0;

   int blockLengths[nBlocks];
   MPI_Aint displacements[nBlocks];
   MPI_Datatype types[nBlocks];

   for( int i = 0; i < nBlocks; i++ ){
      blockLengths[i] = 1;
      types[i] = MPI_INT;
   }

   MPI_CALL(
      MPI_Get_address( &nodeId.subtreeId.lcId, &startAddress )
   );
   displacements[0] = 0;

   MPI_CALL(
      MPI_Get_address( &nodeId.subtreeId.globalSubtreeIdInLc, &address )
   );
   displacements[1] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &nodeId.subtreeId.solverId, &address )
   );
   displacements[2] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &nodeId.seqNum, &address )
   );
   displacements[3] = address - startAddress;
#ifdef _ALIBABA
   types[3] = MPI_LONG;
#else
   types[3] = MPI_LONG_LONG;
#endif

   MPI_CALL(
      MPI_Get_address( &generatorNodeId.subtreeId.lcId, &address )
   );
   displacements[4] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &generatorNodeId.subtreeId.globalSubtreeIdInLc, &address )
   );
   displacements[5] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &generatorNodeId.subtreeId.solverId, &address )
   );
   displacements[6] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &generatorNodeId.seqNum, &address )
   );
   displacements[7] = address - startAddress;
#ifdef _ALIBABA
   types[7] = MPI_LONG;
#else
   types[7] = MPI_LONG_LONG;
#endif

   MPI_CALL(
      MPI_Get_address( &depth, &address )
   );
   displacements[8] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &dualBoundValue, &address )
   );
   displacements[9] = address - startAddress;
   types[9] = MPI_DOUBLE;

   MPI_CALL(
      MPI_Get_address( &initialDualBoundValue, &address )
   );
   displacements[10] = address - startAddress;
   types[10] = MPI_DOUBLE;

   MPI_CALL(
      MPI_Get_address( &estimatedValue, &address )
   );
   displacements[11] = address - startAddress;
   types[11] = MPI_DOUBLE;

   MPI_CALL(
      MPI_Get_address( &diffSubproblemInfo, &address )
   );
   displacements[12] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &basisInfo, &address )
   );
   displacements[13] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &mergingStatus, &address )
   );
   displacements[14] = address - startAddress;

   MPI_CALL(
         MPI_Type_create_struct(nBlocks, blockLengths, displacements, types, &datatype)
         );

   return datatype;
}

int
ParaNodeMpi::bcast(
      ParaComm *comm,
      int root
      )
{
    DEF_PARA_COMM( commMpi, comm);

    MPI_Datatype datatype;
    datatype = createDatatype();
    MPI_CALL(
       MPI_Type_commit( &datatype )
    );
    PARA_COMM_CALL(
       commMpi->ubcast(&nodeId.subtreeId.lcId, 1, datatype, root)
    );
    MPI_CALL(
       MPI_Type_free( &datatype )
    );

   // root node does not have diffSubproblem
   if( diffSubproblemInfo )
   {
      if( commMpi->getRank() != root )
      {
         diffSubproblem = commMpi->createParaDiffSubproblem();
      }
      diffSubproblem->bcast(commMpi, root);
   }
   return 0;
}

int
ParaNodeMpi::send(
      ParaComm *comm,
      int destination
      )
{
    DEF_PARA_COMM( commMpi, comm);

    MPI_Datatype datatype;
    datatype = createDatatype();
    MPI_CALL(
       MPI_Type_commit( &datatype )
    );
    PARA_COMM_CALL(
       commMpi->usend(&nodeId.subtreeId.lcId, 1, datatype, destination, TagNode)
    );
    MPI_CALL(
       MPI_Type_free( &datatype )
    );
   // root node does not have diffSubproblem
   if( diffSubproblemInfo ) diffSubproblem->send(commMpi, destination);
   return 0;
}

int
ParaNodeMpi::receive(ParaComm *comm, int source){
   DEF_PARA_COMM( commMpi, comm);

   MPI_Datatype datatype;
   datatype = createDatatype();
   MPI_CALL(
      MPI_Type_commit( &datatype )
   );
   PARA_COMM_CALL(
      commMpi->ureceive(&nodeId.subtreeId.lcId, 1, datatype, source, TagNode)
   );
   MPI_CALL(
      MPI_Type_free( &datatype )
   );

   if( diffSubproblemInfo )
   {
      diffSubproblem = commMpi->createParaDiffSubproblem();
      diffSubproblem->receive(commMpi, source);
   }

   return 0;
}
