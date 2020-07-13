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

/**@file    paraCalculationStateMpi.cpp
 * @brief   CalcutationStte object extension for MPI communication
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#include "paraCalculationStateMpi.h"

using namespace UG;

MPI_Datatype
ParaCalculationStateMpi::createDatatype(
      )
{

   const int nBlocks = 18;

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
      MPI_Get_address( &compTime, &startAddress )
   );
   displacements[0] = 0;
   types[0] = MPI_DOUBLE;

   MPI_CALL(
      MPI_Get_address( &rootTime, &address )
   );
   displacements[1] = address - startAddress;
   types[1] = MPI_DOUBLE;

   MPI_CALL(
      MPI_Get_address( &nSolved, &address )
   );
   displacements[2] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &nSent, &address )
   );
   displacements[3] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &nImprovedIncumbent, &address )
   );
   displacements[4] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &terminationState, &address )
   );
   displacements[5] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &nSolvedWithNoPreprocesses, &address )
   );
   displacements[6] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &nSimplexIterRoot, &address )
   );
   displacements[7] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &averageSimplexIter, &address )
   );
   displacements[8] = address - startAddress;
   types[8] = MPI_DOUBLE;

   MPI_CALL(
      MPI_Get_address( &nTransferredLocalCuts, &address )
   );
   displacements[9] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &minTransferredLocalCuts, &address )
   );
   displacements[10] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &maxTransferredLocalCuts, &address )
   );
   displacements[11] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &nRestarts, &address )
   );
   displacements[12] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &minIisum, &address )
   );
   displacements[13] = address - startAddress;
   types[13] = MPI_DOUBLE;

   MPI_CALL(
      MPI_Get_address( &maxIisum, &address )
   );
   displacements[14] = address - startAddress;
   types[14] = MPI_DOUBLE;

   MPI_CALL(
      MPI_Get_address( &minNii, &address )
   );
   displacements[15] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &maxNii, &address )
   );
   displacements[16] = address - startAddress;

   MPI_CALL(
      MPI_Get_address( &dualBound, &address )
   );
   displacements[17] = address - startAddress;
   types[17] = MPI_DOUBLE;

   MPI_CALL(
         MPI_Type_create_struct(nBlocks, blockLengths, displacements, types, &datatype)
         );

   return datatype;

}

void
ParaCalculationStateMpi::send(
      ParaComm *comm,
      int destination,
      int tag
      )
{
   DEF_PARA_COMM( commMpi, comm);

   MPI_Datatype datatype;
   datatype = createDatatype();
   MPI_CALL(
      MPI_Type_commit( &datatype )
   );
   PARA_COMM_CALL(
      commMpi->usend(&compTime, 1, datatype, destination, tag)
   );
   MPI_CALL(
      MPI_Type_free( &datatype )
   );
}

void
ParaCalculationStateMpi::receive(
      ParaComm *comm,
      int source,
      int tag
      )
{
   DEF_PARA_COMM( commMpi, comm);

   MPI_Datatype datatype;
   datatype = createDatatype();
   MPI_CALL(
      MPI_Type_commit( &datatype )
   );
   PARA_COMM_CALL(
      commMpi->ureceive(&compTime, 1, datatype, source, tag)
   );
   MPI_CALL(
      MPI_Type_free( &datatype )
   );
}
