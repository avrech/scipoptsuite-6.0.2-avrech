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

/**@file    paraTimeLimitMonitorTh.h
 * @brief   Time limit monitor thread class.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef __PARA_TIME_LIMIT_MONITOR_TH_H__
#define __PARA_TIME_LIMIT_MONITOR_TH_H__

#include <algorithm>
#include "paraDef.h"
#ifdef _COMM_PTH
#include "paraCommPth.h"
#endif
#ifdef _COMM_CPP11
#include "paraCommCPP11.h"
#endif

namespace UG
{

class ParaTimeLimitMonitorTh
{
protected:
#ifdef _COMM_PTH
   ParaCommPth  *paraComm;                  /**< ParaCommunicator object */
#endif
#ifdef _COMM_CPP11
   ParaCommCPP11  *paraComm;                  /**< ParaCommunicator object */
#endif
   double       hardTimeLimit;              /**< hard time limit */
public:
   ParaTimeLimitMonitorTh(){ THROW_LOGICAL_ERROR1("Default constructor of ParaTimeLimitMonitor is called"); }
   ParaTimeLimitMonitorTh(
#ifdef _COMM_PTH
       ParaCommPth *comm,
#endif
#ifdef _COMM_CPP11
       ParaCommCPP11 *comm,
#endif
       double timelimit
       ) : paraComm(comm)
   {
      // hardTimeLimit = timelimit + std::min(60.0, 0.1 * timelimit);   // set hard time limit + 60 seconds longer than time limit
      hardTimeLimit = timelimit + 3;   // set hard time limit + 60 seconds longer than time limit
   }

   virtual ~ParaTimeLimitMonitorTh(){}
   void run(){
#ifdef _MSC_VER
     _sleep(static_cast<unsigned int>(hardTimeLimit));
#else
      sleep(static_cast<unsigned int>(hardTimeLimit));
#endif
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, 0, TagHardTimeLimit)
      );
      std::cout << "****** send TagHardTimeLimit message *****" << std::endl;
   }
};

}

#endif // __PARA_TIME_LIMIT_MONITOR_TH_H__

