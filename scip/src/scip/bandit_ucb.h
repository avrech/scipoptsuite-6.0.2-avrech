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

/**@file   bandit_ucb.h
 * @ingroup INTERNALAPI
 * @brief  internal methods for UCB bandit algorithm
 * @author Gregor Hendel
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_BANDIT_UCB_H__
#define __SCIP_BANDIT_UCB_H__


#include "blockmemshell/memory.h"
#include "scip/def.h"
#include "scip/type_bandit.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** include virtual function table for UCB bandit algorithms */
SCIP_RETCODE SCIPincludeBanditvtableUcb(
   SCIP*                 scip                /**< SCIP data structure */
   );

/*
 * Callback methods of bandit algorithm
 */

/** callback to free bandit specific data structures */
SCIP_DECL_BANDITFREE(SCIPbanditFreeUcb);

/** selection callback for bandit selector */
SCIP_DECL_BANDITSELECT(SCIPbanditSelectUcb);

/** update callback for bandit algorithm */
SCIP_DECL_BANDITUPDATE(SCIPbanditUpdateUcb);

/** reset callback for bandit algorithm */
SCIP_DECL_BANDITRESET(SCIPbanditResetUcb);

/** internal method to create and reset UCB bandit algorithm */
SCIP_RETCODE SCIPbanditCreateUcb(
   BMS_BLKMEM*           blkmem,             /**< block memory */
   BMS_BUFMEM*           bufmem,             /**< buffer memory */
   SCIP_BANDITVTABLE*    vtable,             /**< virtual function table for UCB bandit algorithm */
   SCIP_BANDIT**         ucb,                /**< pointer to store bandit algorithm */
   SCIP_Real*            priorities,         /**< nonnegative priorities for each action, or NULL if not needed */
   SCIP_Real             alpha,              /**< parameter to increase confidence width */
   int                   nactions,           /**< the positive number of actions for this bandit algorithm */
   unsigned int          initseed            /**< initial random seed */
   );

#ifdef __cplusplus
}

#endif

#endif
