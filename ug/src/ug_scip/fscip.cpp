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

/**@file    fscip.cpp
 * @brief   FiberSCIP MAIN.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/


#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <cfloat>
#ifdef _COMM_PTH
#include <pthread.h>
#endif
#ifdef _COMM_CPP11
#include <thread>
#endif
#include <signal.h>
#include "ug/paraInstance.h"
#include "ug/paraLoadCoordinator.h"
#include "ug/paraParamSet.h"
#include "ug/paraRacingRampUpParamSet.h"
#include "ug/paraInitiator.h"
#include "ug/paraTimeLimitMonitorTh.h"
#include "ug/paraNodeTh.h"
#include "scip/scip.h"
#include "scipParaCommTh.h"
#include "scipParaInstance.h"
#include "scipParaDeterministicTimer.h"
#include "scipParaSolver.h"
#include "scipParaInitiator.h"
#ifdef UG_WITH_ULIBC
#include "ulibc.h"
#endif
#ifdef UG_WITH_UGS
#include "ugs/ugsDef.h"
#include "ugs/ugsParaCommMpi.h"
#endif

using namespace UG;
using namespace ParaSCIP;

static ScipParaCommTh *comm = 0;
static ParaParamSet *paraParamSet = 0;
#ifdef _COMM_PTH
static pthread_t *solversAndTimer = 0;
#endif
static int nSolvers = 0;
static ParaInitiator *paraInitiator = 0;
static ParaLoadCoordinator *paraLc = 0;
static bool interrupted = false;

struct SolverThreadData_t {
   int argc;
   char **argv;
   ParaTimer *paraTimer;
   int rank;
};

typedef struct SolverThreadData_t SolverThreadData;

extern void
setUserPlugins(ParaInitiator *initiator);
extern void
setUserPlugins(ParaInstance *instance);  /** this should not be used */
extern void
setUserPlugins(ParaSolver *solver);

/** interrupt handler for CTRL-C interrupts */
static
void interruptHandler(
   int                   signum              /**< interrupt signal number */
   )
{  
   // std::cout << "in interrupt hander" << std::endl;
   if (paraLc == 0)
   {
      if (paraInitiator != 0)
          dynamic_cast<ScipParaInitiator *>(paraInitiator)->interrupt(); //this interrupt initSolve, see below
   }
   else
   {
      paraLc->interrupt();
      delete paraLc;
   }

   interrupted = true;

   _exit(1);
}

void
outputCommandLineMessages(
      char **argv
      )
{
   std::cout << std::endl;
   std::cout << "syntax: " << argv[0] << " fscip_param_file problem_file_name "
             << "[-l <logfile>] [-q] [-sl <settings>] [-s <settings>] [-sr <root_settings>] [-w <prefix_warm>] [-sth <number>] [-fsol <solution_file>] [-isol <initial solution file]" << std::endl;
   std::cout << "  -l <logfile>           : copy output into log file" << std::endl;
   std::cout << "  -q                     : suppress screen messages" << std::endl;
   std::cout << "  -sl <settings>         : load parameter settings (.set) file for LC presolving" << std::endl;
   std::cout << "  -s <settings>          : load parameter settings (.set) file for solvers" << std::endl;
   std::cout << "  -sr <root_settings>    : load parameter settings (.set) file for root" << std::endl;
   std::cout << "  -w <prefix_warm>       : warm start file prefix ( prefix_warm_nodes.gz and prefix_warm_solution.txt are read )" << std::endl;
   std::cout << "  -sth <number>          : the number of solver threads used" << std::endl;
   std::cout << "  -fsol <solution file> : specify output solution file" << std::endl;
   std::cout << "  -isol <intial solution file> : specify initial solution file" << std::endl;
}

void
outputParaParamSet(
      // ParaInitiator *paraInitiator
      )
{
   if( !paraParamSet->getBoolParamValue(Quiet) )
   {
      std::ofstream ofsParamsOutputFile;
      std::ostringstream s;
      if( paraInitiator->getPrefixWarm() )
      {
         s << paraInitiator->getPrefixWarm();
      }
      else
      {
         s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << paraInitiator->getParaInstance()->getProbName();
      }
      s << ".prm";
      ofsParamsOutputFile.open(s.str().c_str());
      if( !ofsParamsOutputFile ){
         std::cout << "Cannot open ParaParams output file: file name = " << s.str() << std::endl;
         exit(1);
      }
      paraParamSet->write(&ofsParamsOutputFile);
      ofsParamsOutputFile.close();
   }
}

void
outputSolverParams(
      // ParaInitiator *paraInitiator
      )
{
   if( !paraParamSet->getBoolParamValue(Quiet) )
   {
      std::ofstream ofsSolverParamsOutputFile;
      std::ostringstream s;
      if( paraInitiator->getPrefixWarm() )
      {
         s << paraInitiator->getPrefixWarm();
      }
      else
      {
         s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << paraInitiator->getParaInstance()->getProbName();
      }
      s << "_solver.prm";
      ofsSolverParamsOutputFile.open(s.str().c_str());
      if( !ofsSolverParamsOutputFile ){
         std::cout << "Cannot open Solver parameters output file: file name = " << s.str() << std::endl;
         exit(1);
      }
      paraInitiator->writeSolverParameters(&ofsSolverParamsOutputFile);
      ofsSolverParamsOutputFile.close();
   }
}

void *
runSolverThread(
      void *threadData
      )
{
   SolverThreadData *solverThreadData = static_cast<SolverThreadData *>(threadData);

   assert( solverThreadData->rank < comm->getSize() );
// #ifdef _COMM_CPP11
   comm->solverInit(solverThreadData->rank, paraParamSet);
// #endif
#ifdef _COMM_PTH
   comm->waitUntilRegistered();
#endif

#ifdef UG_WITH_ULIBC
   printf("ULIBC_bind_pthread_thread( comm->getRank(): %d )\n", comm->getRank());
   ULIBC_bind_pthread_thread( comm->getRank() );
#endif

#ifndef SCIP_THREADSAFE_MESSAGEHDLRS
   SCIPmessageSetDefaultHandler();
#endif

#ifdef _PLACEME
   /*
    * Do placement
    */

   int ierr = placeme(1, PLACEME_SCHEME_DEFAULT, PLACEME_LEVEL_DEFAULT, 1, "HLRN| ");
   switch(ierr) {
     case PLACEME_SUCCESS:
       fprintf(stdout,"task %2d: pinning successful\n", comm->getRank());
       break;
     case PLACEME_NOTDONE:
       fprintf(stdout,"task %2d: pinning not changed on request\n", comm->getRank());
       break;
     default:
       fprintf(stdout,"task %2d: pinning not successful, left unchanged\n", comm->getRank());
   }
#endif

   int argc = solverThreadData->argc;
   char **argv = solverThreadData->argv;
   ParaTimer *paraTimer = solverThreadData->paraTimer;
   ParaDeterministicTimer *detTimer = 0;

#ifdef _COMM_PTH
   sigset_t sigsBlock;
   sigemptyset(&sigsBlock);
   sigaddset(&sigsBlock, SIGINT);
   pthread_sigmask(SIG_BLOCK, &sigsBlock, NULL);
#endif

   if( paraParamSet->getBoolParamValue(Deterministic) )
   {
      detTimer = new ScipParaDeterministicTimer();
   }

   ParaInstance *paraInstance = comm->createParaInstance();
   // setUserPlugins(paraInstance);       // instance data should not be read from original data file
   paraInstance->bcast(comm, 0, paraParamSet->getIntParamValue(InstanceTransferMethod));
   ParaSolver *paraSolver = new ScipParaSolver(argc, argv, comm, paraParamSet, paraInstance, detTimer, paraTimer->getElapsedTime(), true );
   setUserPlugins(paraSolver);

   if( paraParamSet->getBoolParamValue(StatisticsToStdout) )
   {
      comm->lockApp();
      std::cout << "After Rank " << comm->getRank() << " Solver initialized 1: " << paraTimer->getElapsedTime() << std::endl;
      comm->unlockApp();
   }

   if( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 0 || paraSolver->isWarmStarted() )
   {
      paraSolver->run();
   }
   else if( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 1 ||
         paraParamSet->getIntParamValue(RampUpPhaseProcess) == 2 ) // racing ramp-up
   {
      int source;
      int tag;
      comm->probe(&source, &tag);
      if( tag == TagRacingRampUpParamSet )
      {
         ParaRacingRampUpParamSet *racingRampUpParamSet = comm->createParaRacingRampUpParamSet();
         PARA_COMM_CALL(
               racingRampUpParamSet->receive(comm, 0)
               );
         if( paraParamSet->getBoolParamValue(StatisticsToStdout) )
         {
            comm->lockApp();
            std::cout << "After Rank " << comm->getRank() << " Solver initialized 2: " << paraTimer->getElapsedTime() << std::endl;
            comm->unlockApp();
         }
         paraSolver->run( racingRampUpParamSet );
      }
      else
      {
         if( tag == TagTerminateRequest )
         {
            PARA_COMM_CALL(
                  comm->receive( NULL, 0, ParaBYTE, source, TagTerminateRequest )
                  );
            // when solver is deleted, solver's destructor sends termination status
         }
         else
         {
            THROW_LOGICAL_ERROR2("Invalid Tag is received in ParaSCIP solver main: ", tag )
         }
      }
   }
   else
   {
      THROW_LOGICAL_ERROR2("Invalid RampUpPhaseProcess: ", paraParamSet->getIntParamValue(RampUpPhaseProcess) )
   }
// #ifdef _COMM_CPP11
   delete solverThreadData;
// #endif
   delete paraSolver;
   if( detTimer ) delete detTimer;

   // ULIBC_unbind_thread();

   return 0;
}

void *
runTimeLimitMonitorThread(
      void *threadData
      )
{
// #ifdef _COMM_PTH
//    comm->waitUntilRegistered();
// #endif
// #ifdef _COMM_CPP11
   SolverThreadData *monitorThreadData = static_cast<SolverThreadData *>(threadData);
   comm->solverInit(monitorThreadData->rank, paraParamSet);
#ifdef _COMM_PTH
   comm->waitUntilRegistered();
#endif
   delete monitorThreadData;
// #endif

   ParaTimeLimitMonitorTh *monitor = new ParaTimeLimitMonitorTh(comm, paraParamSet->getRealParamValue(TimeLimit));
   monitor->run();
   delete monitor;

   // ULIBC_unbind_thread();

   return 0;
}

/**************************************************************************************
 *                                                                                    *
 * Command line see outputCommandLineMessages()                                       *                                                    *
 *                                                                                    *
 **************************************************************************************/
int
main (
      int  argc,
      char **argv
     )
{
   static const int solverOrigin = 1;

   bool racingSolversExist = false;

#ifdef UG_WITH_UGS
   char *configFileName = 0;
   for( int i = 1; i < argc; ++i )
   {
      if ( strcmp(argv[i], "-ugsc") == 0 )
      {
         i++;
         if( i < argc )
         {
            configFileName = argv[i];
            break;
         }
         else
         {
            std::cerr << "missing file name after parameter '-ugsc" << std::endl;
            exit(1);
         }
      }
   }

   UGS::UgsParaCommMpi *commUgs = 0;   // commUgs != 0 means ParaXpress runs under UGS
   if( configFileName )
   {
      if( argc < 4 )
      {
         outputCommandLineMessages(argv);
         return 1;
      }
      commUgs = new UGS::UgsParaCommMpi();
      commUgs->init(argc,argv);
   }
   else
   {
      if( argc < 3 )
      {
         outputCommandLineMessages(argv);
         return 1;
      }
   }
#else
   if( argc < 3 )
   {
      outputCommandLineMessages(argv);
      return 1;
   }
#endif

   /** catch SIGINT **/
#ifdef NO_SIGACTION
   (void)signal(SIGINT, interruptHandler);
#else
   struct sigaction newaction;
  
   /* initialize new signal action */
   newaction.sa_handler = interruptHandler;
   newaction.sa_flags = SA_RESTART | SA_NODEFER | SA_RESETHAND;
   (void)sigemptyset(&newaction.sa_mask);
      
   /* set new signal action */
   (void)sigaction(SIGINT, &newaction, NULL);
#endif

   comm = new ScipParaCommTh();
   comm->init(argc,argv);

   ParaTimer *paraTimer = comm->createParaTimer();
   paraTimer->init(comm);

#ifndef SCIP_THREADSAFE_MESSAGEHDLRS
   SCIP_CALL_ABORT( SCIPcreateMesshdlrPThreads(comm->getSize()) );
   SCIPmessageSetDefaultHandler();
#endif

#ifdef _PLACEME
   /*
    * Do placement
    */

   int ierr = placeme(1, PLACEME_SCHEME_DEFAULT, PLACEME_LEVEL_DEFAULT, 1, "HLRN| ");
   switch(ierr) {
     case PLACEME_SUCCESS:
       fprintf(stdout,"task %2d: pinning successful\n", comm->getRank());
       break;
     case PLACEME_NOTDONE:
       fprintf(stdout,"task %2d: pinning not changed on request\n", comm->getRank());
       break;
     default:
       fprintf(stdout,"task %2d: pinning not successful, left unchanged\n", comm->getRank());
   }
#endif

   nSolvers = comm->getSize() - 1;
   paraParamSet = comm->createParaParamSet();
   paraParamSet->read(comm, argv[1]);
   comm->lcInit(paraParamSet);

#ifdef UG_WITH_ULIBC
   ULIBC_init();
   // number of threads with ULIBC == nSolvers
   ULIBC_set_affinity_policy(nSolvers, SCATTER_MAPPING, THREAD_TO_CORE );
#endif

   // ParaInitiator *paraInitiator = new ScipParaInitiator(comm, paraTimer);
   paraInitiator = new ScipParaInitiator(comm, paraTimer);
   setUserPlugins(paraInitiator);
   if( paraInitiator->init(paraParamSet, argc, argv) )
   {
      if( paraInitiator->isSolvedAtInit() )
      {
         paraInitiator->outputFinalSolverStatistics(0, paraTimer->getElapsedTime());
         return 0;
      }
   }

   if( paraParamSet->getBoolParamValue(StatisticsToStdout) )
   {
      std::cout << "After Initiator initialized: " << paraTimer->getElapsedTime() << std::endl;
   }

   ParaInstance *paraInstance = paraInitiator->getParaInstance();
   if( paraParamSet->getIntParamValue(OutputParaParams) > 0 )
   {
      // outputParaParamSet(paraInitiator);
      // outputSolverParams(paraInitiator);
      outputParaParamSet();
      outputSolverParams();
   }

   // Create solver threads and register them to comm
   if( paraParamSet->getRealParamValue(TimeLimit) > 0 )
   {
      SolverThreadData *monitorThreadData = new SolverThreadData;
      monitorThreadData->argc = argc;
      monitorThreadData->argv = argv;
      monitorThreadData->paraTimer = paraTimer;
      monitorThreadData->rank = nSolvers+1;
#ifdef _COMM_PTH
      solversAndTimer = new pthread_t[nSolvers+1];
      pthread_t timeLimitMonitorThread;
      pthread_create(&timeLimitMonitorThread,
            NULL,
            runTimeLimitMonitorThread,
            (void *) monitorThreadData
            );
      solversAndTimer[nSolvers] = timeLimitMonitorThread;
      pthread_detach(timeLimitMonitorThread);
#endif
#ifdef _COMM_CPP11
      std::thread t(runTimeLimitMonitorThread, monitorThreadData);
      t.detach();
#endif
   }
   else
   {
#ifdef _COMM_PTH
      solversAndTimer = new pthread_t[nSolvers];
#endif
   }
// #ifdef _COMM_PTH
//    SolverThreadData solverThreadData;
//    solverThreadData.argc = argc;
//    solverThreadData.argv = argv;
//    solverThreadData.paraTimer = paraTimer;
// #endif
   for( int i = 0; i < nSolvers; i++ )
   {
      SolverThreadData *solverThreadData = new SolverThreadData;
      solverThreadData->argc = argc;
      solverThreadData->argv = argv;
      solverThreadData->paraTimer = paraTimer;
      solverThreadData->rank = (i+1);
#ifdef _COMM_PTH
      pthread_t solverThread;
      pthread_create(&solverThread,
            NULL,
            runSolverThread,
            (void *) solverThreadData
            );
      solversAndTimer[i] = solverThread;
      pthread_detach(solverThread);
#endif
#ifdef _COMM_CPP11
      // comm->solverInit(i+1, paraParamSet);
      std::thread t(runSolverThread, solverThreadData);
      t.detach();
#endif
   }

#ifdef _COMM_PTH

   comm->registedAllSolvers();

   /* set maximum priority to this LC */
   struct sched_param schedParam;
   schedParam.sched_priority= sched_get_priority_max(SCHED_FIFO);
   pthread_setschedparam(pthread_self(), SCHED_FIFO, &schedParam);
#endif

   ParaDeterministicTimer *detTimer = 0;
   if( paraParamSet->getBoolParamValue(Deterministic) )
   {
       detTimer = new ScipParaDeterministicTimer();
   }

   paraInstance->bcast(comm, 0, paraParamSet->getIntParamValue(InstanceTransferMethod));
   paraInitiator->sendSolverInitializationMessage();  // This messages should be received in constructor of the target Solver

   if( paraInitiator->isSolvedAtInit() )
   {
#ifdef UG_WITH_UGS
      paraLc = new ParaLoadCoordinator(commUgs, comm, paraParamSet, paraInitiator, &racingSolversExist, paraTimer, detTimer);
#else
      paraLc = new ParaLoadCoordinator(comm, paraParamSet, paraInitiator, &racingSolversExist, paraTimer, detTimer);
#endif
      delete paraLc;
      delete paraInitiator;
      delete paraParamSet;
      delete paraTimer;
      delete comm;
#ifdef UG_WITH_UGS
      if( commUgs ) delete commUgs;
#endif
      if( detTimer ) delete detTimer;
#ifdef _COMM_PTH
      delete [] solversAndTimer;
#endif
      return 0;
   }
   else
   {
#ifdef UG_WITH_UGS
      paraLc = new ParaLoadCoordinator(commUgs, comm, paraParamSet, paraInitiator, &racingSolversExist, paraTimer, detTimer);
#else
      paraLc = new ParaLoadCoordinator(comm, paraParamSet, paraInitiator, &racingSolversExist, paraTimer, detTimer);
#endif
   }
   if( paraInitiator->isWarmStarted() )
   {
#ifdef UG_WITH_ZLIB
      paraLc->warmStart();
#endif
   }
   else
   {
      if( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 0)
      {
         ParaNode *rootNode = new ParaNodeTh(
               NodeId(), NodeId(), 0, -DBL_MAX, -DBL_MAX, -DBL_MAX,
               paraInitiator->makeRootNodeDiffSubproblem());
         paraLc->run(rootNode);
      }
      else if( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 1 ||
            paraParamSet->getIntParamValue(RampUpPhaseProcess) == 2
            )  // racing ramp-up
      {
         ParaRacingRampUpParamSet **racingRampUpParams = new ParaRacingRampUpParamSet *[comm->getSize()];
         paraInitiator->generateRacingRampUpParameterSets( (comm->getSize()-1), racingRampUpParams );
         for( int i = 1; i < comm->getSize(); i++ )
         {
            PARA_COMM_CALL(
                  racingRampUpParams[i-solverOrigin]->send(comm, i)
                  );
         }
         ParaNode *rootNode = comm->createParaNode(
               NodeId(), NodeId(), 0, -DBL_MAX, -DBL_MAX, -DBL_MAX,
               paraInitiator->makeRootNodeDiffSubproblem());
         paraLc->run(rootNode, (comm->getSize()-1), racingRampUpParams );
         for( int i = 1; i < comm->getSize(); i++ )
         {
            if( racingRampUpParams[i-solverOrigin] ) delete racingRampUpParams[i-solverOrigin];
         }
         delete [] racingRampUpParams;
      }
      else
      {
         THROW_LOGICAL_ERROR2("Invalid RampUpPhaseProcess: ", paraParamSet->getIntParamValue(RampUpPhaseProcess) )
      }
   }

   if ( interrupted )
   {
      std::cout << "*** FiberSCIP process is interrupted. ***" << std::endl;
   }

   delete paraLc;
   if( paraInitiator ) delete paraInitiator;
   delete paraParamSet;
   delete paraTimer;
   if( detTimer ) delete detTimer;
   delete comm;

#ifdef UG_WITH_UGS
   if( commUgs ) delete commUgs;
#endif

#ifdef _COMM_PTH
   delete [] solversAndTimer;
#endif

#ifndef SCIP_THREADSAFE_MESSAGEHDLRS
   SCIPfreeMesshdlrPThreads();
#endif

   // ULIBC_unbind_thread();

   _exit(0);
} /* END main */
