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

/**@file    scipParaInitialStatMpi.cpp
 * @brief   ScipParaInitialStat extension for MPI communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


// #include "paraTagDefMpi.h"
#include "scipParaInitialStatMpi.h"

using namespace UG;
using namespace ParaSCIP;

/** create ScipDiffSubproblemPreDatatype */
MPI_Datatype
ScipParaInitialStatMpi::createDatatype1(
      )
{
   const int nBlocks = 4;

   MPI_Datatype datatype;

   MPI_Aint startAddress = 0;
   MPI_Aint address = 0;

   int blockLengths[nBlocks];
   MPI_Aint displacements[nBlocks];
   MPI_Datatype types[nBlocks];

   MPI_CALL(
      MPI_Get_address( &maxDepth, &startAddress )
   );
   blockLengths[0] = 1;
   displacements[0] = 0;
   types[0] = MPI_INT;

   MPI_CALL(
      MPI_Get_address( &maxTotalDepth, &address )
   );
   blockLengths[1] = 1;
   displacements[1] = address - startAddress;
   types[1] = MPI_INT;

   MPI_CALL(
      MPI_Get_address( &nVarBranchStatsDown, &address )
   );
   blockLengths[2] = 1;
   displacements[2] = address - startAddress;
   types[2] = MPI_INT;

   MPI_CALL(
      MPI_Get_address( &nVarBranchStatsUp, &address )
   );
   blockLengths[3] = 1;
   displacements[3] = address - startAddress;
   types[3] = MPI_INT;

   MPI_CALL(
         MPI_Type_create_struct(nBlocks, blockLengths, displacements, types, &datatype)
         );

   return datatype;
}

/** create ScipDiffSubproblemDatatype */
MPI_Datatype
ScipParaInitialStatMpi::createDatatype2(
      bool memAllocNecessary
      )
{
   assert( nVarBranchStatsDown != 0 || nVarBranchStatsUp != 0 );

   int nBlocks = 0;

   MPI_Datatype datatype;

   MPI_Aint startAddress = 0;
   MPI_Aint address = 0;

   int blockLengths[17];            // reserve maximum number of elements
   MPI_Aint displacements[17];     // reserve maximum number of elements
   MPI_Datatype types[17];         // reserve maximum number of elements

   if( nVarBranchStatsDown )
   {
      if( memAllocNecessary )
      {
         idxLBranchStatsVarsDown = new int[nVarBranchStatsDown];
         nVarBranchingDown = new int[nVarBranchStatsDown];
         downpscost = new SCIP_Real[nVarBranchStatsDown];
         downvsids = new SCIP_Real[nVarBranchStatsDown];
         downconflen = new SCIP_Real[nVarBranchStatsDown];
         downinfer = new SCIP_Real[nVarBranchStatsDown];
         downcutoff = new SCIP_Real[nVarBranchStatsDown];
      }

      MPI_CALL(
         MPI_Get_address( idxLBranchStatsVarsDown, &startAddress )
      );
      displacements[nBlocks] = 0;
      blockLengths[nBlocks] = nVarBranchStatsDown;
      types[nBlocks] = MPI_INT;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( nVarBranchingDown, &address )
      );
      displacements[nBlocks] = address - startAddress;
      blockLengths[nBlocks] = nVarBranchStatsDown;
      types[nBlocks] = MPI_INT;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( downpscost, &address )
      );
      displacements[nBlocks] = address - startAddress;
      blockLengths[nBlocks] = nVarBranchStatsDown;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( downvsids, &address )
      );
      displacements[nBlocks] = address - startAddress;
      blockLengths[nBlocks] = nVarBranchStatsDown;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( downconflen, &address )
      );
      displacements[nBlocks] = address - startAddress;
      blockLengths[nBlocks] = nVarBranchStatsDown;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( downinfer, &address )
      );
      displacements[nBlocks] = address - startAddress;
      blockLengths[nBlocks] = nVarBranchStatsDown;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( downcutoff, &address )
      );
      displacements[nBlocks] = address - startAddress;
      blockLengths[nBlocks] = nVarBranchStatsDown;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;
   }

   if( nVarBranchStatsUp )
   {
      if( memAllocNecessary )
      {
         idxLBranchStatsVarsUp = new int[nVarBranchStatsUp];
         nVarBranchingUp = new int[nVarBranchStatsUp];
         uppscost = new SCIP_Real[nVarBranchStatsUp];
         upvsids = new SCIP_Real[nVarBranchStatsUp];
         upconflen = new SCIP_Real[nVarBranchStatsUp];
         upinfer = new SCIP_Real[nVarBranchStatsUp];
         upcutoff = new SCIP_Real[nVarBranchStatsUp];
      }

      if( nBlocks == 0 )
      {
         MPI_CALL(
            MPI_Get_address( idxLBranchStatsVarsUp, &startAddress )
         );
         displacements[nBlocks] = 0;
      }
      else
      {
         MPI_CALL(
            MPI_Get_address( idxLBranchStatsVarsUp, &address )
         );
         displacements[nBlocks] = address - startAddress;
      }
      blockLengths[nBlocks] = nVarBranchStatsUp;
      types[nBlocks] = MPI_INT;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( nVarBranchingUp, &address )
      );
      displacements[nBlocks] = address - startAddress;;
      blockLengths[nBlocks] = nVarBranchStatsUp;
      types[nBlocks] = MPI_INT;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( uppscost, &address )
      );
      displacements[nBlocks] = address - startAddress;;
      blockLengths[nBlocks] = nVarBranchStatsUp;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( upvsids, &address )
      );
      displacements[nBlocks] = address - startAddress;;
      blockLengths[nBlocks] = nVarBranchStatsUp;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( upconflen, &address )
      );
      displacements[nBlocks] = address - startAddress;;
      blockLengths[nBlocks] = nVarBranchStatsUp;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( upinfer, &address )
      );
      displacements[nBlocks] = address - startAddress;;
      blockLengths[nBlocks] = nVarBranchStatsUp;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;

      MPI_CALL(
         MPI_Get_address( upcutoff, &address )
      );
      displacements[nBlocks] = address - startAddress;;
      blockLengths[nBlocks] = nVarBranchStatsUp;
      types[nBlocks] = MPI_DOUBLE;
      nBlocks++;
   }

   MPI_CALL(
         MPI_Type_create_struct(nBlocks, blockLengths, displacements, types, &datatype)
   );

   return datatype;
}

/** send solution data to the rank */
void
ScipParaInitialStatMpi::send(ParaComm *comm, int destination)
{
   DEF_PARA_COMM( commMpi, comm);
   MPI_Datatype datatype1;
   datatype1 = createDatatype1();
   MPI_CALL(
      MPI_Type_commit( &datatype1 )
   );
   PARA_COMM_CALL(
      commMpi->usend(&maxDepth, 1, datatype1, destination, TagInitialStat)
   );
   MPI_CALL(
      MPI_Type_free( &datatype1 )
   );

   if( nVarBranchStatsDown !=0 || nVarBranchStatsUp != 0 )
   {
      MPI_Datatype datatype2;
      datatype2 = createDatatype2(false);
      MPI_CALL(
         MPI_Type_commit( &datatype2 )
      );
      if( nVarBranchStatsDown )
      {
         PARA_COMM_CALL(
               commMpi->usend(idxLBranchStatsVarsDown, 1, datatype2, destination, TagInitialStat1)
         );
      }
      else
      {
         if( nVarBranchStatsUp )
         {
            PARA_COMM_CALL(
                  commMpi->usend(idxLBranchStatsVarsUp, 1, datatype2, destination, TagInitialStat1)
            );
         }
      }
      MPI_CALL(
         MPI_Type_free( &datatype2 )
      );
   }
}

/** receive solution data from the source rank */
void
ScipParaInitialStatMpi::receive(ParaComm *comm, int source)
{
   DEF_PARA_COMM( commMpi, comm);
   MPI_Datatype datatype1;
   datatype1 = createDatatype1();
   MPI_CALL(
      MPI_Type_commit( &datatype1 )
   );
   PARA_COMM_CALL(
      commMpi->ureceive(&maxDepth, 1, datatype1, source, TagInitialStat)
   );
   MPI_CALL(
      MPI_Type_free( &datatype1 )
   );

   if( nVarBranchStatsDown !=0 || nVarBranchStatsUp != 0 )
   {
      MPI_Datatype datatype2;
      datatype2 = createDatatype2(true);
      MPI_CALL(
         MPI_Type_commit( &datatype2 )
      );
      if( nVarBranchStatsDown )
      {
         PARA_COMM_CALL(
               commMpi->ureceive(idxLBranchStatsVarsDown, 1, datatype2, source, TagInitialStat1)
         );
      }
      else
      {
         if( nVarBranchStatsUp )
         {
            PARA_COMM_CALL(
                  commMpi->ureceive(idxLBranchStatsVarsUp, 1, datatype2, source, TagInitialStat1)
            );
         }
      }
      MPI_CALL(
         MPI_Type_free( &datatype2 )
      );
   }
}
