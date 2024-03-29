/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2019 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   reader_cor.h
 * @ingroup FILEREADERS
 * @brief  COR file reader (MPS format of the core problem for stochastic programs)
 * @author Stephen J. Maher
 *
 * This is a reader for the core file of a stochastic programming instance in SMPS format.
 * The three files that must be read are:
 * - .cor
 * - .tim
 * - .sto
 *
 * Alternatively, it is possible to create a .smps file with the relative path to the .cor, .tim and .sto files.
 * A file reader is available for the .smps file.
 *
 * The core file is in the form of an MPS.
 *
 * Details regarding the SMPS file format can be found at:
 * Birge, J. R.; Dempster, M. A.; Gassmann, H. I.; Gunn, E.; King, A. J. & Wallace, S. W.
 * A standard input format for multiperiod stochastic linear programs
 * IIASA, Laxenburg, Austria, WP-87-118, 1987
 *
 */


/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_READER_COR_H__
#define __SCIP_READER_COR_H__

#include "scip/def.h"
#include "scip/type_reader.h"
#include "scip/type_result.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** includes the cor file reader into SCIP
 *
 *  @ingroup FileReaderIncludes
 */
SCIP_EXPORT
SCIP_RETCODE SCIPincludeReaderCor(
   SCIP*                 scip                /**< SCIP data structure */
   );

/**@addtogroup FILEREADERS
 *
 * @{
 */

/** reads problem from file */
SCIP_EXPORT
SCIP_RETCODE SCIPreadCor(
   SCIP*                 scip,               /**< SCIP data structure */
   const char*           filename,           /**< full path and name of file to read, or NULL if stdin should be used */
   SCIP_RESULT*          result              /**< pointer to store the result of the file reading call */
   );

/*
 * Interface method for the tim and sto readers
 */

/** returns whether the COR file has been successfully read. This is used by the TIM and STO readers. */
SCIP_EXPORT
SCIP_Bool SCIPcorHasRead(
   SCIP_READER*          reader              /**< the file reader itself */
   );

/** returns the number of variable names in the COR problem */
SCIP_EXPORT
int SCIPcorGetNVarNames(
   SCIP_READER*          reader              /**< the file reader itself */
   );

/** returns the number of constraint names in the COR problem */
SCIP_EXPORT
int SCIPcorGetNConsNames(
   SCIP_READER*          reader              /**< the file reader itself */
   );

/** returns the variable name for the given index */
SCIP_EXPORT
const char* SCIPcorGetVarName(
   SCIP_READER*          reader,             /**< the file reader itself */
   int                   i                   /**< the index of the variable that is requested */
   );

/** returns the constraint name for the given index */
SCIP_EXPORT
const char* SCIPcorGetConsName(
   SCIP_READER*          reader,             /**< the file reader itself */
   int                   i                   /**< the index of the constraint that is requested */
   );

/* @} */

#ifdef __cplusplus
}
#endif

#endif
