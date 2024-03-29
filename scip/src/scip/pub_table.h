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

/**@file   pub_table.h
 * @ingroup PUBLICCOREAPI
 * @brief  public methods for displaying statistic tables
 * @author Tristan Gally
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_PUB_TABLE_H__
#define __SCIP_PUB_TABLE_H__


#include "scip/def.h"
#include "scip/type_set.h"
#include "scip/type_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@addtogroup PublicTableMethods
 *
 * @{
 */

/** gets user data of statistics table */
SCIP_EXPORT
SCIP_TABLEDATA* SCIPtableGetData(
   SCIP_TABLE*           table               /**< statistics table */
   );

/** sets user data of statistics table; user has to free old data in advance! */
SCIP_EXPORT
void SCIPtableSetData(
   SCIP_TABLE*           table,              /**< statistics table */
   SCIP_TABLEDATA*       tabledata           /**< new statistics table user data */
   );

/** gets name of statistics table */
SCIP_EXPORT
const char* SCIPtableGetName(
   SCIP_TABLE*           table               /**< statistics table */
   );

/** gets description of statistics table */
SCIP_EXPORT
const char* SCIPtableGetDesc(
   SCIP_TABLE*           table               /**< statistics table */
   );

/** gets position of statistics table */
SCIP_EXPORT
int SCIPtableGetPosition(
   SCIP_TABLE*           table               /**< statistics table */
   );

/** gets earliest stage of statistics table */
SCIP_EXPORT
SCIP_STAGE SCIPtableGetEarliestStage(
   SCIP_TABLE*           table               /**< statistics table */
   );

/** is statistics table currently active? */
SCIP_EXPORT
SCIP_Bool SCIPtableIsActive(
   SCIP_TABLE*           table               /**< statistics table */
   );

/** is statistics table initialized? */
SCIP_EXPORT
SCIP_Bool SCIPtableIsInitialized(
   SCIP_TABLE*           table               /**< statistics table */
   );

/* @} */

#ifdef __cplusplus
}
#endif

#endif
