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

/**@file    paraParamSetTh.h
 * @brief   
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_PARAM_SET_TH_H__
#define __PARA_PARAM_SET_TH_H__
#include "paraParamSet.h"

namespace UG
{

class ParaParamSetTh : public ParaParamSet
{
public:
    ParaParamSetTh(){}
    int bcast(ParaComm *comm, int root)
    {
       THROW_LOGICAL_ERROR1("bcast is called in ParaParamSetTh");
    }
    ~ParaParamSetTh(){}
};

}

#endif  // __PARA_PARAM_SET_TH_H__
