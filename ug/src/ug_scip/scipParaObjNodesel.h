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

/**@file   objnodesel.h
 * @brief  C++ wrapper for node selectors
 * @author Yuji Shinano
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_PARA_OBJNODESEL_H__
#define __SCIP_PARA_OBJNODESEL_H__

#include <cstring>
#include "scipParaComm.h"
#include "scipParaSolver.h"
#include "objscip/objnodesel.h"
#include "scip/scipdefplugins.h"

namespace ParaSCIP
{

class ScipParaSolver;

/** @brief C++ wrapper for primal heuristics
 *
 *  This class defines the interface for node selectors implemented in C++. Note that there is a pure virtual
 *  function (this function has to be implemented). This function is: scip_comp().
 *
 *  - \ref NODESEL "Instructions for implementing a  node selector"
 *  - \ref NODESELECTORS "List of available node selectors"
 *  - \ref type_nodesel.h "Corresponding C interface"
 */
class ScipParaObjNodesel : public scip::ObjNodesel
{
   ScipParaSolver *scipParaSolver;

   int getNBoundChanges(SCIP_NODE* node);

public:
   /** default constructor */
   ScipParaObjNodesel(
      ScipParaSolver *solver
      )
      : scip::ObjNodesel::ObjNodesel(solver->getScip(), "ScipParaObjNodesel", "Node selector when a node is sent to LC",
        0, 0), scipParaSolver(solver)
   {
   }

   /** destructor */
   virtual ~ScipParaObjNodesel()
   {
   }

   /** node selection method of node selector
    *
    *  @see SCIP_DECL_NODESELSELECT(x) in @ref type_nodesel.h
    */
   virtual SCIP_DECL_NODESELSELECT(scip_select);

   /** node comparison method of node selector
    *
    *  @see SCIP_DECL_NODESELCOMP(x) in @ref type_nodesel.h
    */
   virtual SCIP_DECL_NODESELCOMP(scip_comp);
  
   void reset()
   {
   }

};

} /* namespace scip */

#endif
