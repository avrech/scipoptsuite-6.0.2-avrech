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

/**@file    scipParaRacingRampUpParamSetMpi.cpp
 * @brief   ScipParaRacingRampUpParamSet extension for MPI communication.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#include "scipParaCommMpi.h"
#include "scipDiffParamSetMpi.h"
#include "scipParaRacingRampUpParamSetMpi.h"
#include <cstring>

using namespace UG;
using namespace ParaSCIP;

/** create Datatype */
MPI_Datatype
ScipParaRacingRampUpParamSetMpi::createDatatype(
      )
{
   const int nBlocks = 7;

   MPI_Datatype datatype;

   int blockLengths[nBlocks];
   MPI_Aint displacements[nBlocks];
   MPI_Datatype types[nBlocks];

   MPI_Aint startAddress = 0;
   MPI_Aint address = 0;

   for( int i = 0; i < nBlocks; i++ )
   {
       blockLengths[i] = 1;
       types[i] = MPI_INT;
   }
   MPI_CALL(
      MPI_Get_address( &terminationCriteria, &startAddress )
   );
   displacements[0] = 0;
   MPI_CALL(
      MPI_Get_address( &nNodesLeft, &address )
   );
   displacements[1] = address - startAddress;
   MPI_CALL(
      MPI_Get_address( &timeLimit, &address )
   );
   displacements[2] = address - startAddress;
   MPI_CALL(
      MPI_Get_address( &scipRacingParamSeed, &address )
   );
   displacements[3] = address - startAddress;
   MPI_CALL(
      MPI_Get_address( &permuteProbSeed, &address )
   );
   displacements[4] = address - startAddress;
   MPI_CALL(
      MPI_Get_address( &generateBranchOrderSeed, &address )
   );
   displacements[5] = address - startAddress;
   MPI_CALL(
      MPI_Get_address( &scipDiffParamSetInfo, &address )
   );
   displacements[6] = address - startAddress;

   types[2] = MPI_DOUBLE;

   MPI_CALL(
         MPI_Type_create_struct(nBlocks, blockLengths, displacements, types, &datatype)
         );

   return datatype;

}

int
ScipParaRacingRampUpParamSetMpi::send(
      ParaComm *comm,
      int dest)
{

   DEF_PARA_COMM( commMpi, comm);

   MPI_Datatype datatype;
   datatype = createDatatype();
   MPI_CALL(
      MPI_Type_commit( &datatype )
   );
   PARA_COMM_CALL(
      commMpi->usend(&terminationCriteria, 1, datatype, dest, TagRacingRampUpParamSet)
   );
   MPI_CALL(
      MPI_Type_free( &datatype )
   );

   if( scipDiffParamSetInfo )
   {
      scipDiffParamSet->send(commMpi, dest);
   }

   return 0;

}

int
ScipParaRacingRampUpParamSetMpi::receive(
      ParaComm *comm,
      int source)
{

   DEF_PARA_COMM( commMpi, comm);

   MPI_Datatype datatype;
   datatype = createDatatype();
   MPI_CALL(
      MPI_Type_commit( &datatype )
   );
   PARA_COMM_CALL(
      commMpi->ureceive(&terminationCriteria, 1, datatype, source, TagRacingRampUpParamSet)
   );
   MPI_CALL(
      MPI_Type_free( &datatype )
   );

   if( scipDiffParamSetInfo )
   {
	   DEF_SCIP_PARA_COMM( scipParaComm, comm );
      scipDiffParamSet = scipParaComm->createScipDiffParamSet();
      scipDiffParamSet->receive(commMpi, source);
   }

   return 0;

}
