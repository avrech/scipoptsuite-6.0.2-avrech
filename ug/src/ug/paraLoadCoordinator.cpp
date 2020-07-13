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

/**@file    paraLoadCoordinator.cpp
 * @brief   Load coordinator.
 * @author  Yuji Shinano
 *
 *
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifdef _MSC_VER
#include <functional>
#else
#include <unistd.h>
#endif
#include <cmath>
#include <ctime>
#include <cfloat>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <climits>
#include <algorithm>
#include <iomanip>

#ifdef UG_WITH_ZLIB
#include "gzstream.h"
#endif

#include "paraLoadCoordinator.h"
#include "paraInitialStat.h"

using namespace UG;

ParaLoadCoordinator::ParaLoadCoordinator(
#ifdef UG_WITH_UGS
      UGS::UgsParaCommMpi *inCommUgs,
#endif
      ParaComm *inComm,
      ParaParamSet *inParaParamSet,
      ParaInitiator *inParaInitiator,
      bool *inRacingSolversExist,
      ParaTimer *inParaTimer,
      ParaDeterministicTimer *inParaDetTimer
      ) :
      globalSubtreeIdGen(0),
      paraParamSet(inParaParamSet),
      paraInitiator(inParaInitiator),
      racingSolversExist(inRacingSolversExist),
      restarted(false),
      initialNodesGenerated(false),
      runningPhase(RampUpPhase),
      firstCollectingModeState(-1),
      isCollectingModeRestarted(false),
      breakingSolverId(-1),
      nReplaceToBetterNode(0),
      computationIsInterrupted(false),
      interruptedFromControlTerminal(false),
      hardTimeLimitIsReached(false),
      winnerSolverNodesCollected(false),
      interruptIsRequested(false),
      primalUpdated(false),
      restartingRacing(false),
      nRestartedRacing(0),
      paraRacingSolverPool(0),
      // bestDualBoundValueInInterruptedRacingSolvers(-DBL_MAX),
      nSolvedInInterruptedRacingSolvers(-1),
      nNodesLeftInInterruptedRacingSolvers(-1),
      nNormalSelection(-1),
      previousCheckpointTime(0.0),
      statEmptyNodePoolTime(DBL_MAX),
      eps(0.0),
      racingWinner(-1),
      minDepthInWinnerSolverNodes(INT_MAX),
      maxDepthInWinnerSolverNodes(-1),
      racingWinnerParams(0),
      racingTermination(false),
      nSolvedRacingTermination(0),
      merging(false),
      nBoundChangesOfBestNode(0),
      varIndexTable(0),
      mergeInfoHead(0),
      mergeInfoTail(0),
      nTerminated(0),
      nCollectedSolvers(0),
      paraTimer(inParaTimer),
      paraDetTimer(inParaDetTimer),
      previousTabularOutputTime(0.0),
      averageDualBoundGain(0.0),
      nAverageDualBoundGain(0),
      averageLastSeveralDualBoundGains(0.0),
      starvingTime(-1.0),
      hugeImbalanceTime(-1.0),
      paraNodePoolToRestart(0),
      paraNodePoolBufferToRestart(0),
      paraNodePoolBufferToGenerateCPF(0),
      paraNodeToKeepCheckpointFileNodes(0),
      osLogSolvingStatus(0),
      osLogNodesTransfer(0),
      osTabularSolvingStatus(0),
      osLogSubtreeInfo(0),
      osStatisticsFinalRun(0),
      osStatisticsRacingRampUp(0),
      allCompInfeasibleAfterSolution(false),
      minmalDualBoundNormalTermSolvers(DBL_MAX),
      warmStartNodeTransferring(false),
      hugeImbalance(false),
      pendingSolution(0)
{
#ifdef UG_WITH_UGS
   commUgs = inCommUgs;
#endif
   paraComm = inComm;
   lcts.rank = paraComm->getRank();

#ifdef UG_WITH_ZLIB
   if( paraParamSet->getRealParamValue(RestartInRampDownThresholdTime) > 0.0 &&
         !paraParamSet->getBoolParamValue(Checkpoint) )
   {
      std::cout << "RestartInRampDownThresholdTime > 0.0, then Checkpoint must be TRUE." << std::endl;
      exit(1);
   }
#endif

   /** register message handlers */
   for( int i = 0; i < N_TAGS; i++ )
   {
      messageHandler[i] = 0;
   }
   messageHandler[TagNode] = &UG::ParaLoadCoordinator::processTagNode;
   messageHandler[TagSolution] = &UG::ParaLoadCoordinator::processTagSolution;
   messageHandler[TagSolverState] = &UG::ParaLoadCoordinator::processTagSolverState;
   messageHandler[TagCompletionOfCalculation] = &UG::ParaLoadCoordinator::processTagCompletionOfCalculation;
   messageHandler[TagAnotherNodeRequest] = &UG::ParaLoadCoordinator::processTagAnotherNodeRequest;
   messageHandler[TagTerminated] = &UG::ParaLoadCoordinator::processTagTerminated;
   messageHandler[TagHardTimeLimit] = &UG::ParaLoadCoordinator::processTagHardTimeLimit;
   messageHandler[TagInitialStat] = &UG::ParaLoadCoordinator::processTagInitialStat;
   messageHandler[TagAllowToBeInCollectingMode] = &UG::ParaLoadCoordinator::processTagAllowToBeInCollectingMode;
   if( ( paraParamSet->getIntParamValue(UG::RampUpPhaseProcess) == 1 ||
         paraParamSet->getIntParamValue(UG::RampUpPhaseProcess) == 2 ) &&
         paraParamSet->getBoolParamValue(UG::CommunicateTighterBoundsInRacing) )
   {
      messageHandler[TagLbBoundTightenedIndex] = &UG::ParaLoadCoordinator::processTagLbBoundTightened;
      messageHandler[TagUbBoundTightenedIndex] = &UG::ParaLoadCoordinator::processTagUbBoundTightened;
   }
   if( paraParamSet->getBoolParamValue(Deterministic) )
   {
      messageHandler[TagToken] = &UG::ParaLoadCoordinator::processTagToken;
   }
   
   
   // disply ramp-up mode
   if( paraParamSet->getIntParamValue(UG::RampUpPhaseProcess) == 0 )
   {
	   std::cout <<"LC is working with normal ramp-up." << std::endl;
   }
   else if( paraParamSet->getIntParamValue(UG::RampUpPhaseProcess) == 1 )
   {
	   std::cout <<"LC is working with racing ramp-up." << std::endl;
   }
   else if( paraParamSet->getIntParamValue(UG::RampUpPhaseProcess) == 2 )
   {
	   std::cout <<"LC is working with racing ramp-up and with rebuilding tree after racing." << std::endl;
   }

   /* set up status log and transfer log */
   logSolvingStatusFlag = paraParamSet->getBoolParamValue(LogSolvingStatus);
   if( logSolvingStatusFlag )
   {
      std::ostringstream s;
#ifdef UG_WITH_UGS
      if( commUgs )
      {
         s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << commUgs->getMySolverName() << "_"
         << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".status";
      }
      else
      {
         s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".status";
      } 
#else 
      s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
      << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".status";
#endif
      ofsLogSolvingStatus.open(s.str().c_str(), std::ios::app );
      if( !ofsLogSolvingStatus )
      {
         std::cout << "Solving status log file cannot open : file name = " << s.str() << std::endl;
         exit(1);
      }
      osLogSolvingStatus = &ofsLogSolvingStatus;

      /* if initial solution is given, output the primal value */
      if( paraInitiator->getGlobalBestIncumbentSolution() )
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " LC" << " INITIAL_PRIMAL_VALUE "
         << paraInitiator->convertToExternalValue(
               paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue()
               ) << std::endl;
      }
   }

   logSubtreeInfoFlag = paraParamSet->getBoolParamValue(LogSubtreeInfo);
   if( logSubtreeInfoFlag )
   {
      std::ostringstream s;
#ifdef UG_WITH_UGS
      if( commUgs )
      {
         s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << commUgs->getMySolverName() << "_"
         << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".treelog";
      }
      else
      {
         s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".treelog";
      }
#else
      s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
      << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".treelog";
#endif
      ofsLogSubtreeInfo.open(s.str().c_str(), std::ios::app );
      if( !ofsLogSubtreeInfo )
      {
         std::cout << "Sub tree info. log file cannot open : file name = " << s.str() << std::endl;
         exit(1);
      }
      osLogSubtreeInfo = &ofsLogSubtreeInfo;
   }

   logNodesTransferFlag = paraParamSet->getBoolParamValue(LogNodesTransfer);
   if( logNodesTransferFlag )
   {
      std::ostringstream s;
#ifdef UG_WITH_UGS
      if( commUgs )
      {
         s << paraParamSet->getStringParamValue(LogNodesTransferFilePath)
         << commUgs->getMySolverName() << "_"
         << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".transfer";
      }
      else
      {
         s << paraParamSet->getStringParamValue(LogNodesTransferFilePath)
         << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".transfer";
      }
#else
      s << paraParamSet->getStringParamValue(LogNodesTransferFilePath)
      << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << ".transfer";
#endif
      ofsLogNodesTransfer.open(s.str().c_str(), std::ios::app);
      if( !ofsLogNodesTransfer )
      {
         std::cout << "Node transfer log file cannot open : file name = " << s.str() << std::endl;
         exit(1);
      }
      osLogNodesTransfer = &ofsLogNodesTransfer;
   }

   outputTabularSolvingStatusFlag = paraParamSet->getBoolParamValue(OutputTabularSolvingStatus);
   if( outputTabularSolvingStatusFlag || paraParamSet->getBoolParamValue(Quiet) )
   {
      if( paraParamSet->getBoolParamValue(Quiet) )
      {
         osTabularSolvingStatus = &std::cout;
      }
      else
      {
         std::ostringstream s;
#ifdef UG_WITH_UGS
         if( commUgs )
         {
            s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
            << commUgs->getMySolverName() << "_"
            << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << "_T.status";
         }
         else
         {
            s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
            << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << "_T.status";
         }
#else
         s << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << paraInitiator->getParaInstance()->getProbName() << "_LC" << paraComm->getRank() << "_T.status";
#endif
         ofsTabularSolvingStatus.open(s.str().c_str(), std::ios::app );
         if( !ofsTabularSolvingStatus )
         {
            std::cout << "Tabular solving status file cannot open : file name = " << s.str() << std::endl;
            exit(1);
         }
         osTabularSolvingStatus = &ofsTabularSolvingStatus;
      }
      if( outputTabularSolvingStatusFlag )
      {
         // output title line 1
         *osTabularSolvingStatus << std::setw(1) << " ";
         *osTabularSolvingStatus << std::setw(8) << std::right << " ";
         *osTabularSolvingStatus << std::setw(15) << std::right << " ";
         *osTabularSolvingStatus << std::setw(12) << std::right << "Nodes";
         *osTabularSolvingStatus << std::setw(10) << std::right << "Active";
         *osTabularSolvingStatus << std::setw(17) << std::right << " ";
         *osTabularSolvingStatus << std::setw(17) << std::right << " ";
         *osTabularSolvingStatus << std::setw(10) << std::right << " ";
         *osTabularSolvingStatus << std::endl;
         // output title line 2
         *osTabularSolvingStatus << std::setw(1) << " ";
         *osTabularSolvingStatus << std::setw(8) << std::right << "Time";
         *osTabularSolvingStatus << std::setw(15) << std::right << "Nodes";
         *osTabularSolvingStatus << std::setw(12) << std::right << "Left";
         *osTabularSolvingStatus << std::setw(10) << std::right << "Solvers";
         *osTabularSolvingStatus << std::setw(17) << std::right << "Best Integer";
         *osTabularSolvingStatus << std::setw(17) << std::right << "Best Node";
         *osTabularSolvingStatus << std::setw(10) << std::right << "Gap";
         *osTabularSolvingStatus << std::setw(17) << std::right << "Best Node(S)";
         *osTabularSolvingStatus << std::setw(10) << std::right << "Gap(S)";
         *osTabularSolvingStatus << std::endl;
      }
   }

   if( !paraParamSet->getBoolParamValue(Quiet) )
   {
      //
      // open statistic files
      //
      std::ostringstream ssfr;
#ifdef UG_WITH_UGS
      if( commUgs )
      {
         ssfr << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << commUgs->getMySolverName() << "_"
         << paraInitiator->getParaInstance()->getProbName() << "_statistics_final_LC" << paraComm->getRank();
      }
      else
      {
         ssfr << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << paraInitiator->getParaInstance()->getProbName() << "_statistics_final_LC" << paraComm->getRank();
      }
#else
      ssfr << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
      << paraInitiator->getParaInstance()->getProbName() << "_statistics_final_LC" << paraComm->getRank();
#endif
      ofsStatisticsFinalRun.open(ssfr.str().c_str(), std::ios::app);
      if( !ofsStatisticsFinalRun )
      {
         std::cout << "Statistics file for final run cannot open : file name = " << ssfr.str() << std::endl;
         exit(1);
      }
      osStatisticsFinalRun = &ofsStatisticsFinalRun;

      if( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 1 ||
            paraParamSet->getIntParamValue(RampUpPhaseProcess) == 2
            )  /** racing ramp-up */
      {
         std::ostringstream ssrru;
#ifdef UG_WITH_UGS
         if( commUgs )
         {
            ssrru << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
            << commUgs->getMySolverName() << "_"
            << paraInitiator->getParaInstance()->getProbName() << "_statistics_racing_LC" << paraComm->getRank();
         }
         else
         {
            ssrru << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
            << paraInitiator->getParaInstance()->getProbName() << "_statistics_racing_LC" << paraComm->getRank();
         }
#else
         ssrru << paraParamSet->getStringParamValue(LogSolvingStatusFilePath)
         << paraInitiator->getParaInstance()->getProbName() << "_statistics_racing_LC" << paraComm->getRank();
#endif
         ofsStatisticsRacingRampUp.open(ssrru.str().c_str(), std::ios::app);
         if( !ofsStatisticsRacingRampUp )
         {
            std::cout << "Statistics file for racing ramp-up cannot open : file name = " << ssrru.str() << std::endl;
            exit(1);
         }
         osStatisticsRacingRampUp = &ofsStatisticsRacingRampUp;
      }
   }

   paraSolverPool = new ParaSolverPoolForMinimization(    // always minimization problem
                    paraParamSet->getRealParamValue(MultiplierForCollectingMode),
                    paraParamSet->getRealParamValue(BgapCollectingMode),
                    paraParamSet->getRealParamValue(MultiplierForBgapCollectingMode),
                    1,                // paraSolver origin rank
                    paraComm, paraParamSet, paraTimer);
   // always minimization problem
   if( paraParamSet->getBoolParamValue(CleanUp) )
   {
      paraNodePool = new ParaNodePoolForCleanUp(paraParamSet->getRealParamValue(BgapCollectingMode));
   }
   else
   {
      paraNodePool = new ParaNodePoolForMinimization(paraParamSet->getRealParamValue(BgapCollectingMode));
   }

   eps = paraInitiator->getEpsilon();

   lastCheckpointTimeStr[0] = ' ';
   lastCheckpointTimeStr[1] = '\0';

   if( paraParamSet->getIntParamValue(NSolverNodesStartBreaking) == 0 ||
         paraParamSet->getIntParamValue(NStopBreaking) == 0 )
   {
      isBreakingFinised = true;
   }
   else
   {
      isBreakingFinised = false;
   }

   if( paraParamSet->getBoolParamValue(NChangeIntoCollectingModeNSolvers) )
   {
      paraParamSet->setIntParamValue(NChangeIntoCollectingMode, std::max((paraSolverPool->getNSolvers()/2), (size_t)1) );
   }

   if( paraParamSet->getBoolParamValue(Deterministic) )
   {
      assert(paraDetTimer);
   }

   if( !EPSEQ( paraParamSet->getRealParamValue(RandomNodeSelectionRatio), 0.0, MINEPSILON ) )
   {
      nNormalSelection = -1;
   }

   if( !paraParamSet->getBoolParamValue(CollectOnce) || 
       !( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 1 || paraParamSet->getIntParamValue(RampUpPhaseProcess) == 2 ) )
   { 
      winnerSolverNodesCollected = true;
   }

   if ( paraParamSet->getRealParamValue(UG::FinalCheckpointGeneratingTime) > 0.0 )
   {
      paraParamSet->setRealParamValue(UG::TimeLimit, -1.0);
      std::cout << "** FinalCheckpointGeneratingTime is specified, then TimeLimit is omitted. ** " << std::endl;
   }

   if( paraParamSet->getBoolParamValue(UG::GenerateReducedCheckpointFiles) )
   {
      if( !( paraInitiator->getPrefixWarm() &&
            paraParamSet->getBoolParamValue(UG::MergeNodesAtRestart) &&
            !paraParamSet->getBoolParamValue(UG::DualBoundGainTest) ) )
      {
         std::cout << "** -w opttion = " << paraInitiator->getPrefixWarm() << " **" << std::endl;
         std::cout << "** MergeNodesAtRestart = " << paraParamSet->getBoolParamValue(UG::MergeNodesAtRestart) << std::endl;
         std::cout << "** DualBoundGainTest = " << paraParamSet->getBoolParamValue(UG::DualBoundGainTest) << std::endl;
         std::cout << "** GenerateReducedCheckpointFiles is specified, then this solver shuld be executed with"
               << "-w option and MergeNodesAtRestart = TRUE and DualBoundGainTest = FALSE ** " << std::endl;
         exit(1);
      }
      paraNodePoolBufferToGenerateCPF = new ParaNodePoolForMinimization(paraParamSet->getRealParamValue(BgapCollectingMode));
   }

   nBoundChangesOfBestNode = paraParamSet->getIntParamValue(UG::NBoundChangesOfMergeNode);
}

ParaLoadCoordinator::~ParaLoadCoordinator(
      )
{
   if( nTerminated == 0 &&
         ( ( paraRacingSolverPool && paraRacingSolverPool->getNumActiveSolvers() > 0 ) ||
               paraSolverPool->getNumActiveSolvers() > 0 ||
               interruptIsRequested
               )
     )
   {
      terminateAllSolvers();
      runningPhase = TerminationPhase;
      // if( !racingTermination )
      // {
      //    lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
      //    lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
      // }

      int source;
      int tag;

      for(;;)
      {
         /*******************************************
          *  waiting for any message form anywhere  *
          *******************************************/
         double inIdleTime = paraTimer->getElapsedTime();
         paraComm->probe(&source, &tag);
         lcts.idleTime += ( paraTimer->getElapsedTime() - inIdleTime );
         if( messageHandler[tag] )
         {
            int status = (this->*messageHandler[tag])(source, tag);
            if( status )
            {
               std::ostringstream s;
               s << "[ERROR RETURN form termination message handler]:" <<  __FILE__ <<  "] func = "
                 << __func__ << ", line = " << __LINE__ << " - "
                 << "process tag = " << tag << std::endl;
               abort();
            }
         }
         else
         {
            ABORT_LOGICAL_ERROR3( "No message handler for ", tag, " is not registered" );
         }

#ifdef UG_WITH_UGS
         if( commUgs ) checkAndReadIncumbent();
#endif

         if( nTerminated == paraSolverPool->getNSolvers() ) break;
      }
   }

   /** write final solution */
   paraInitiator->writeSolution("Final Solution");

#ifdef UG_WITH_ZLIB
   /* wite final checkpoint file */
   if( paraParamSet->getBoolParamValue(UG::GenerateReducedCheckpointFiles) )
   {
      assert( paraNodePoolBufferToGenerateCPF );
      while ( paraNodePoolBufferToGenerateCPF->getNumOfNodes() > 0 )
      {
         ParaNode *node = paraNodePoolBufferToGenerateCPF->extractNode();
         paraNodePool->insert(node);
      }
      updateCheckpointFiles();
   }
   else
   {
      if( interruptIsRequested &&
            paraParamSet->getRealParamValue(UG::FinalCheckpointGeneratingTime) > 0.0 )
      {
         updateCheckpointFiles();
      }
   }
#endif

   if( paraNodePool || paraNodeToKeepCheckpointFileNodes )
   {
      int nKeepingNodes = 0;
      if( logNodesTransferFlag )
      {
         if( paraNodeToKeepCheckpointFileNodes )
         {
            assert(!paraNodeToKeepCheckpointFileNodes->isEmpty());
            nKeepingNodes = paraNodeToKeepCheckpointFileNodes->getNumOfNodes();
         }
         *osLogNodesTransfer << std::endl << "ParaLoadCoordinator: # received = " << lcts.nReceived
         << ", # sent = " << lcts.nSent << ", # sent immediately = " << lcts.nSentBackImmediately << ", # deleted = " << lcts.nDeletedInLc
         << ", # failed to send back = " << lcts.nFailedToSendBack
         << ", Max usage of node pool = " << paraNodePool->getMaxUsageOfPool() + nKeepingNodes << std::endl;
         *osLogNodesTransfer << "# sent immediately ( another node ) = " << lcts.nSentBackImmediatelyAnotherNode
         << ", # # failed to send back ( another node ) = " << lcts.nFailedToSendBackAnotherNode << std::endl;
         if( !paraNodePool->isEmpty() ){
            *osLogNodesTransfer << "LoadCoodibator: NodePool in LoadCoordinatator is not empty: "
            << paraNodePool->getNumOfNodes() + nKeepingNodes << " nodes remained" << std::endl;
         }
         else if( paraNodeToKeepCheckpointFileNodes )
         {
            *osLogNodesTransfer << "LoadCoodibator: NodePool in LoadCoordinatator is not empty: "
            << nKeepingNodes << " nodes remained" << std::endl;
         }
      }
      lcts.nMaxUsageOfNodePool = paraNodePool->getMaxUsageOfPool() + nKeepingNodes;
      lcts.nInitialP = paraParamSet->getIntParamValue(NChangeIntoCollectingMode);
      lcts.mMaxCollectingNodes = paraSolverPool->getMMaxCollectingNodes();
      lcts.nNodesInNodePool = paraNodePool->getNumOfNodes() + nKeepingNodes;
   }

   // set final solving status
   if( initialNodesGenerated )
   {
      paraInitiator->setFinalSolverStatus(InitialNodesGenerated);
   }
   else
   {
      if( hardTimeLimitIsReached )
      {
         paraInitiator->setFinalSolverStatus(HardTimeLimitIsReached);
      }
      else
      {
         if( interruptedFromControlTerminal ||
               (!racingTermination && computationIsInterrupted ) )
         {
            paraInitiator->setFinalSolverStatus(ComputingWasInterrupted);
         }
         else
         {
            if( paraNodeToKeepCheckpointFileNodes )
            {
#ifdef UG_WITH_ZLIB
               updateCheckpointFiles();
#endif
               paraInitiator->setFinalSolverStatus(RequestedSubProblemsWereSolved);
            }
            else
            {
               paraInitiator->setFinalSolverStatus(ProblemWasSolved);
            }
            if( outputTabularSolvingStatusFlag )
            {
               outputTabularSolvingStatus(' ');
            }
         }
      }
   }


   // set number of nodes solved and final dual bound value
   if( paraParamSet->getBoolParamValue(UG::GenerateReducedCheckpointFiles) )
   {
      *osTabularSolvingStatus << "*** This is GenerateReducedCheckpointFiles run ***" << std::endl;
      if( paraNodeToKeepCheckpointFileNodes )
      {
         *osTabularSolvingStatus << "*** Current checkpoint-data have " <<
               (paraNodePool->getNumOfNodes() + paraNodeToKeepCheckpointFileNodes->getNumOfNodes())
               << " nodes."
               << " including " << paraNodeToKeepCheckpointFileNodes->getNumOfNodes()
               << " initially keeping nodes by the run-time parameter."
               << std::endl;
      }
      else
      {
         *osTabularSolvingStatus << "*** Current checkpoint-data have " << paraNodePool->getNumOfNodes()
               << " nodes." << std::endl;
      }

   }
   else
   {
      if( initialNodesGenerated )
      {
         paraInitiator->setNumberOfNodesSolved( std::max(1ULL, paraSolverPool->getTotalNodesSolved()) );
         paraInitiator->setDualBound(paraNodePool->getBestDualBoundValue());
         lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(paraNodePool->getBestDualBoundValue());
      }
      else
      {
         if( racingTermination )
         {
            if( ( interruptedFromControlTerminal || hardTimeLimitIsReached ) && paraRacingSolverPool )
            {
               paraInitiator->setNumberOfNodesSolved(paraRacingSolverPool->getNnodesSolvedInBestSolver());
            }
            else
            {
               paraInitiator->setNumberOfNodesSolved(nSolvedRacingTermination);
            }
            if( lcts.globalBestDualBoundValue < minmalDualBoundNormalTermSolvers )
            {
               lcts.globalBestDualBoundValue = minmalDualBoundNormalTermSolvers;
            }
            paraInitiator->setDualBound(lcts.globalBestDualBoundValue);
            lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
         }
         else
         {
            if( !interruptedFromControlTerminal && !computationIsInterrupted && !hardTimeLimitIsReached )
            {
               paraInitiator->setNumberOfNodesSolved( std::max(1ULL, paraSolverPool->getTotalNodesSolved()) );
               if( paraInitiator->getGlobalBestIncumbentSolution() && allCompInfeasibleAfterSolution )
               {
                  paraInitiator->setDualBound(paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue());
                  lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue());
               }
               else
               {
                  if( lcts.globalBestDualBoundValue < minmalDualBoundNormalTermSolvers && !paraNodeToKeepCheckpointFileNodes )
                  {
                     if( paraInitiator->getGlobalBestIncumbentSolution() )
                     {
                        lcts.globalBestDualBoundValue = std::min( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(),
                              minmalDualBoundNormalTermSolvers );
                     }
                     else
                     {
                        lcts.globalBestDualBoundValue = minmalDualBoundNormalTermSolvers;
                     }
                  }
                  paraInitiator->setDualBound(lcts.globalBestDualBoundValue);
                  lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
               }
            }
            else
            {
               if( isRacingStage() )
               {
                  if( paraRacingSolverPool )
                  {
                     paraInitiator->setDualBound(lcts.globalBestDualBoundValue);
                     paraInitiator->setNumberOfNodesSolved(  paraRacingSolverPool->getNnodesSolvedInBestSolver() );
                  }
                  else
                  {
                     ABORT_LOGICAL_ERROR1("Computation is interrupted in racing stage, but no paraRacingSolverPool");
                  }
               }
               else
               {
                  paraInitiator->setNumberOfNodesSolved( std::max(1ULL, paraSolverPool->getTotalNodesSolved()) );
                  paraInitiator->setDualBound(lcts.globalBestDualBoundValue);
               }
               lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
            }
         }
      }

      paraInitiator->outputFinalSolverStatistics( osTabularSolvingStatus, paraTimer->getElapsedTime() );
   }

   if( racingWinnerParams )
   {
      delete racingWinnerParams;
   }

   if( paraSolverPool && osStatisticsFinalRun )
   {
      lcts.nNodesLeftInAllSolvers = paraSolverPool->getNnodesInSolvers();
      *osStatisticsFinalRun << "######### The number of nodes solved in all solvers: "
            << paraSolverPool->getTotalNodesSolved();
      if( paraSolverPool->getTotalNodesSolved() != paraSolverPool->getNnodesSolvedInSolvers() )
      {
         *osStatisticsFinalRun << " : "
               << paraSolverPool->getNnodesSolvedInSolvers();
      }
      *osStatisticsFinalRun << " #########" << std::endl;
   }
   if( paraSolverPool )  delete paraSolverPool;

   lcts.runningTime = paraTimer->getElapsedTime();
   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << lcts.runningTime << " ParaLoadCoordinator_TERMINATED" << std::endl;
      if( paraRacingSolverPool )
      {
         *osLogSolvingStatus << lcts.runningTime << " "
               << paraRacingSolverPool->getNumActiveSolvers() << " active racing ramp-up solvers exist." << std::endl;
      }
   }
#ifdef _DEBUG_LB
   std::cout << lcts.runningTime << " ParaLoadCoordinator_TERMINATED" << std::endl;
   if( paraRacingSolverPool )
   {
      std::cout << lcts.runningTime << " "
            << paraRacingSolverPool->getNumActiveSolvers() << " active racing ramp-up solvers exist." << std::endl;
   }
#endif


   lcts.isCheckpointState = false;
   if( paraParamSet->getBoolParamValue(StatisticsToStdout) )
   {
      std::cout << lcts.toString() << std::endl;
   }

   if( osStatisticsFinalRun )
   {
      *osStatisticsFinalRun << lcts.toString();
   }

   if( paraRacingSolverPool && osStatisticsFinalRun )
   {
      *osStatisticsFinalRun << "***** <<< NOTE >>> "
            << paraRacingSolverPool->getNumActiveSolvers()
            << " active racing ramp-up solvers exist." << std::endl;
      *osStatisticsFinalRun << paraRacingSolverPool->getStrActiveSolerNumbers() << std::endl;
   }

   if( paraRacingSolverPool )
   {
      delete paraRacingSolverPool;
      *racingSolversExist = true;
   }

   if( osStatisticsFinalRun )
   {
      osStatisticsFinalRun->flush();
   }

   if( paraNodePool ) delete paraNodePool;
   if( paraNodePoolToRestart ) delete paraNodePoolToRestart;
   if( paraNodePoolBufferToRestart ) delete paraNodePoolBufferToRestart;
   if( paraNodePoolBufferToGenerateCPF ) delete paraNodePoolBufferToGenerateCPF;
   if( paraNodeToKeepCheckpointFileNodes ) delete paraNodeToKeepCheckpointFileNodes;
}

int
ParaLoadCoordinator::processTagNode(
      int source,
      int tag
      )
{
   ParaNode *paraNode = paraComm->createParaNode();
   paraNode->receive(paraComm, source);

#ifdef UG_DEBUG_SOLUTION
#ifdef UG_DEBUG_SOLUTION_OPT_PATH
   if( paraNode->getDiffSubproblem() && (!paraNode->getDiffSubproblem()->isOptimalSolIncluded()) )
   {
      delete paraNode;
      PARA_COMM_CALL(
             paraComm->send( NULL, 0, ParaBYTE, source, TagNodeReceived);
             );
      return 0;
   }
#endif
#endif

   // std::cout << paraTimer->getElapsedTime()<< " S." << source << " nodeId = " << paraNode->toSimpleString() << " is recived" << std::endl;

   lcts.nReceived++;

   if( paraNodePoolToRestart )
   {
      delete paraNode;
      return 0;
   }

   if( ( paraInitiator->getGlobalBestIncumbentSolution() &&
         paraNode->getDualBoundValue() < paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() ) ||
         !( paraInitiator->getGlobalBestIncumbentSolution() ) )
   {
      paraNode->setGlobalSubtreeId(paraComm->getRank(), createNewGlobalSubtreeId());
      /** in the case that ParaNode received from LoadCoordinator is not implemented yet */
      ParaNode *ancestorNode = paraSolverPool->getCurrentNode(source);
      paraNode->setAncestor(
            new ParaNodeGenealogicalLocalPtr( ancestorNode->getNodeId(), ancestorNode ));
      ancestorNode->addDescendant(
            new ParaNodeGenealogicalLocalPtr( paraNode->getNodeId(), paraNode) );
      if( hugeImbalance )
      {
         paraNodePoolBufferToRestart->insert(paraNode);
      }
      else
      {
         paraNodePool->insert(paraNode);
      }
      if( merging )
      {
         addNodeToMergeNodeStructs(paraNode);
      }

      if( paraParamSet->getBoolParamValue(RacingStatBranching) &&
            source == racingWinner &&
            !winnerSolverNodesCollected )
      {
         if( minDepthInWinnerSolverNodes > paraNode->getDepth() )
         {
            minDepthInWinnerSolverNodes = paraNode->getDepth();
         }
      }

      if( !( paraSolverPool->getNumInactiveSolvers() > 0 && sendParaNodesToIdleSolvers() ) )
                                              // In racing stage, some solver could take time for solving a node
                                              // Therefore, some solver could stay idle so long time in paraSolverPool
                                              // However, node collecting has to terminated to sending switch out message
      {
         // paraNodePool->insert(paraNode);
         /** switch-out request might be delayed to reach the target solver.
          * This behavior affects load balancing sometimes.
          *  Therefore, if additional nodes are received, sends switch-out request again.
         //if( runningPhase != RampUpPhase && !(paraSolverPool->isInCollectingMode()) )
         //{
            // paraSolverPool->enforcedSwitchOutCollectingMode(source);
         //}
         ** I don't want to do this. So, I decided to wait message after each notification when LC in collecting mode if necessary */
         // without consideration of keeping nodes in checkpoint file
         double globalBestDualBoundValueLocal =
            std::max (
                  std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
                  lcts.globalBestDualBoundValue );
         if( runningPhase != RampUpPhase && paraSolverPool->isInCollectingMode() &&
              ( paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
               > ( paraParamSet->getRealParamValue(MultiplierForCollectingMode) *
                     paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraSolverPool->getMCollectingNodes() )
                     )
                     )
         {
            paraSolverPool->switchOutCollectingMode();
            firstCollectingModeState = 1;
            isCollectingModeRestarted = false;
         }
      }
   }
   else
   {
      assert( !( EPSLT( paraNode->getDualBoundValue(), paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), eps) ) );
#ifdef UG_DEBUG_SOLUTION
      if( paraNode->getDiffSubproblem() && paraNode->getDiffSubproblem()->isOptimalSolIncluded() )
      {
         throw "Optimal solution going to be killed.";
      }
#endif
      delete paraNode;
      lcts.nDeletedInLc++;
      if( runningPhase != RampUpPhase && !(paraSolverPool->isInCollectingMode()) &&
            paraSolverPool->getNumInactiveSolvers() > ( paraSolverPool->getNSolvers() * 0.1 )  &&
            paraNodePool->isEmpty() )
      { // inactive solver exists but cannot send a ParaNode to it
         paraSolverPool->switchInCollectingMode(paraNodePool);
         if( firstCollectingModeState == -1 && paraSolverPool->isInCollectingMode() ) firstCollectingModeState = 0;
      }
   }

   PARA_COMM_CALL(
          paraComm->send( NULL, 0, ParaBYTE, source, TagNodeReceived);
          );

   return 0;
}

int
ParaLoadCoordinator::processTagSolution(
      int source,
      int tag
      )
{
   ParaSolution *sol = paraComm->createParaSolution();
   sol->receive(paraComm, source);

#ifdef _DEBUG_DET
   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << paraTimer->getElapsedTime()
      << " S." << source << " R.SOL "
      << paraInitiator->convertToExternalValue(
            sol->getObjectiveFuntionValue()
            ) << std::endl;
   }
#endif

   if( updateSolution(sol) ) 
   {
      delete sol;
      if( paraInitiator->canGenerateSpecialCutOffValue() )
      {
         sendCutOffValue(source);
      }
      sendIncumbentValue(source);
      primalUpdated = true;
      allCompInfeasibleAfterSolution = true;
      if( logSolvingStatusFlag )
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " I.SOL "
         << paraInitiator->convertToExternalValue(
               paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue()
               ) << std::endl;
      }
#ifdef _DEBUG_LB
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " I.SOL "
      << paraInitiator->convertToExternalValue(
            paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue()
            ) << std::endl;
#endif
      /** output tabular solving status */
      if( outputTabularSolvingStatusFlag )
      {
         outputTabularSolvingStatus('*');
      }
#ifdef UG_WITH_ZLIB
      /* Do not have to remove ParaNodes from NodePool. It is checked and removed before sending them */
      /** save incumbent solution */
      char solutionFileNameTemp[256];
      char solutionFileName[256];
      if( paraParamSet->getBoolParamValue(Checkpoint) && paraComm->getRank() == 0  )
      {
         sprintf(solutionFileNameTemp,"%s%s_after_checkpointing_solution_t.gz",
               paraParamSet->getStringParamValue(CheckpointFilePath),
               paraInitiator->getParaInstance()->getProbName() );
         paraInitiator->writeCheckpointSolution(std::string(solutionFileNameTemp));
         sprintf(solutionFileName,"%s%s_after_checkpointing_solution.gz",
               paraParamSet->getStringParamValue(CheckpointFilePath),
               paraInitiator->getParaInstance()->getProbName() );
         if ( rename(solutionFileNameTemp, solutionFileName) )
         {
            std::cout << "after checkpointing solution file cannot be renamed: errno = " << strerror(errno) << std::endl;
            exit(1);
         }
      }
#endif
#ifdef UG_WITH_UGS
      if( commUgs )
      {
         paraInitiator->writeUgsIncumbentSolution(commUgs);
      }
#endif
   }
   else
   {
      delete sol;
   }
   return 0;
}

int
ParaLoadCoordinator::processTagSolverState(
      int source,
      int tag
      )
{

   double globalBestDualBoundValue = -DBL_MAX;
   double globalBestDualBoundValueLocal = -DBL_MAX;

   ParaSolverState *solverState = paraComm->createParaSolverState();
   solverState->receive(paraComm, source, tag);

   if( paraDetTimer
         && paraDetTimer->getElapsedTime() < solverState->getDeterministicTime() )

   {
      paraDetTimer->update( solverState->getDeterministicTime() - paraDetTimer->getElapsedTime() );
   }

   if( solverState->isRacingStage() )
   {
      globalBestDualBoundValueLocal = globalBestDualBoundValue =
               std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() );
      /** not update paraSolverPool. The solver is inactive in paraSolverPool */
      if( logSolvingStatusFlag )
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " | "
         << paraInitiator->convertToExternalValue(
               solverState->getSolverLocalBestDualBoundValue()
               );
         if( !paraInitiator->getGlobalBestIncumbentSolution() ||
               paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) > displayInfOverThisValue )
         {
            *osLogSolvingStatus << " ( Inf )";
         }
         else
         {
            *osLogSolvingStatus << " ( " << paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) * 100 << "% )";
         }
         *osLogSolvingStatus << " [ " << solverState->getNNodesLeft() << " ]";
         *osLogSolvingStatus << " ** G.B.: " << paraInitiator->convertToExternalValue(globalBestDualBoundValue);
         if( !paraInitiator->getGlobalBestIncumbentSolution() ||
               paraInitiator->getGap(globalBestDualBoundValue) > displayInfOverThisValue )
         {
            *osLogSolvingStatus << " ( Inf ) ";
         }
         else
         {
            *osLogSolvingStatus << " ( " << paraInitiator->getGap(globalBestDualBoundValue) * 100 << "% ) ";
         }
         *osLogSolvingStatus << "[ " << paraSolverPool->getNnodesInSolvers() << ", " << paraNodePool->getNumOfNodes()
         << " ( " <<  paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
         <<" ) ] ** RR" << std::endl;
         // <<" ) ] ** RR " << solverState->getDeterministicTime() << std::endl;  // for debug

      }
#ifdef _DEBUG_LB
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " | "
      << paraInitiator->convertToExternalValue(
            solverState->getSolverLocalBestDualBoundValue()
            );
      if( !paraInitiator->getGlobalBestIncumbentSolution() ||
            paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) > displayInfOverThisValue )
      {
         std::cout << " ( Inf )";
      }
      else
      {
         std::cout << " ( " << paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) * 100 << "% )";
      }
      std::cout << " [ " << solverState->getNNodesLeft() << " ]";
      globalBestDualBoundValue =
               std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() );
      std::cout << " ** G.B.: " << paraInitiator->convertToExternalValue(globalBestDualBoundValue);
      if( !paraInitiator->getGlobalBestIncumbentSolution() ||
            paraInitiator->getGap(globalBestDualBoundValue) > displayInfOverThisValue )
      {
         std::cout << " ( Inf ) ";
      }
      else
      {
         std::cout << " ( " << paraInitiator->getGap(globalBestDualBoundValue) * 100 << "% ) ";
      }
      std::cout << "[ " << paraSolverPool->getNnodesInSolvers() << ", " << paraNodePool->getNumOfNodes()
      << " ( " <<  paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
      <<" ) ] ** RR" << std::endl;
#endif
   }
   else
   {
      double solverDualBoundGain = 0.0;
      double sum = 0.0;
      if(paraSolverPool->getNumOfNodesSolved(source) == 0
            && solverState->getNNodesSolved() > 0
            && !paraSolverPool->getCurrentNode(source)->isRootNode() )
      {
         solverDualBoundGain = solverState->getSolverLocalBestDualBoundValue() - paraSolverPool->getCurrentNode(source)->getDualBoundValue();
         if( logSolvingStatusFlag )
         {
            *osLogSolvingStatus << paraTimer->getElapsedTime()
            << " S." << source << " G " <<  paraSolverPool->getCurrentNode(source)->toSimpleString()
            << ", a:" << averageDualBoundGain
            << ", ma:" << averageLastSeveralDualBoundGains
            << ", g:" << solverDualBoundGain;
            if( paraSolverPool->isDualBounGainTesting(source) )
            {
               *osLogSolvingStatus << ", T";
            }
            *osLogSolvingStatus << ", st:" << solverState->getAverageDualBoundGain()*paraParamSet->getRealParamValue(DualBoundGainBranchRatio)
            << std::endl;
         }
         lastSeveralDualBoundGains.push_back(solverDualBoundGain);
         for(int i = 0; i < static_cast<int>(lastSeveralDualBoundGains.size()); i++ )
         {
            sum += lastSeveralDualBoundGains[i];
         }
         averageLastSeveralDualBoundGains = sum/lastSeveralDualBoundGains.size();
         averageDualBoundGain =
               averageDualBoundGain * ( static_cast<double>(nAverageDualBoundGain)/(static_cast<double>(nAverageDualBoundGain) + 1.0) )
               + solverDualBoundGain * ( 1.0/(static_cast<double>(nAverageDualBoundGain) + 1.0 ) );
         if( lastSeveralDualBoundGains.size() > 7 )
         {
            lastSeveralDualBoundGains.pop_front();
         }
         nAverageDualBoundGain++;
      }
      paraSolverPool->updateSolverStatus(source,
                                      solverState->getNNodesSolved(),
                                      solverState->getNNodesLeft(),
                                      solverState->getSolverLocalBestDualBoundValue(),
                                      paraNodePool
                                      );
      if( paraNodeToKeepCheckpointFileNodes && paraNodeToKeepCheckpointFileNodes->getNumOfNodes() > 0 )
      {
         globalBestDualBoundValue = paraNodeToKeepCheckpointFileNodes->getBestDualBoundValue();
         globalBestDualBoundValueLocal =
               std::max (
                     std::min(
                           std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
                           minmalDualBoundNormalTermSolvers ),
                     lcts.globalBestDualBoundValue );
      }
      else
      {
         if( paraParamSet->getBoolParamValue(UG::GenerateReducedCheckpointFiles)  )
         {
            globalBestDualBoundValueLocal = globalBestDualBoundValue =
                  std::min(
                        std::min(
                              std::min( paraNodePool->getBestDualBoundValue(), paraNodePoolBufferToGenerateCPF->getBestDualBoundValue() ),
                              paraSolverPool->getGlobalBestDualBoundValue() ),
                  lcts.globalBestDualBoundValue );
         }
         else
         {
            globalBestDualBoundValueLocal = globalBestDualBoundValue =
                  std::max (
                        std::min(
                              std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
                              minmalDualBoundNormalTermSolvers ),
                        lcts.globalBestDualBoundValue );
         }
      }

      if( logSolvingStatusFlag )
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " | "
         << paraInitiator->convertToExternalValue(
               solverState->getSolverLocalBestDualBoundValue()
               );
         if( !paraInitiator->getGlobalBestIncumbentSolution() ||
               paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) > displayInfOverThisValue )
         {
            *osLogSolvingStatus << " ( Inf )";
         }
         else
         {
            *osLogSolvingStatus << " ( " << paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) * 100 << "% )";
         }
         *osLogSolvingStatus << " [ " << solverState->getNNodesLeft() << " ]";
         *osLogSolvingStatus << " ** G.B.: " << paraInitiator->convertToExternalValue(globalBestDualBoundValue);
         if( !paraInitiator->getGlobalBestIncumbentSolution() ||
               paraInitiator->getGap(globalBestDualBoundValue) > displayInfOverThisValue )
         {
            *osLogSolvingStatus << " ( Inf ) ";
         }
         else
         {
            *osLogSolvingStatus << " ( " << paraInitiator->getGap(globalBestDualBoundValue) * 100 << "% ) ";
         }
         *osLogSolvingStatus << "[ " << paraSolverPool->getNnodesInSolvers() << ", " << paraNodePool->getNumOfNodes()
         << " ( " <<  paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
         <<" ) ] **";
         if( runningPhase == RampUpPhase ) *osLogSolvingStatus << " R";
         if( paraSolverPool->isInCollectingMode() )
         {
            *osLogSolvingStatus << " C";
            if( paraSolverPool->isSolverInCollectingMode(source) )
            {
               *osLogSolvingStatus << " 1";
            }
            else
            {
               *osLogSolvingStatus << " 0";
            }

         }
         *osLogSolvingStatus << " " << solverState->getDeterministicTime();   // for debug
         if( paraNodePoolBufferToRestart )
         {
            *osLogSolvingStatus << " " << paraNodePoolBufferToRestart->getNumOfNodes();  // for debug
         }
         *osLogSolvingStatus << std::endl;

         /** log the number of nodes transfer */
         if( paraParamSet->getIntParamDefaultValue(NNodesTransferLogging) > 0 &&
               paraSolverPool->getNnodesSolvedInSolvers() > lcts.nNodesOutputLog )
         {
            *osLogSolvingStatus << paraTimer->getElapsedTime() << " = ";
            *osLogSolvingStatus << paraSolverPool->getNnodesSolvedInSolvers()
                  << " " << (paraSolverPool->getNnodesInSolvers() + paraNodePool->getNumOfNodes())
                  << " s " << lcts.nSent << " r " << lcts.nReceived
                  << std::endl;
            lcts.nNodesOutputLog =
                  ( ( paraSolverPool->getNnodesSolvedInSolvers()/paraParamSet->getIntParamDefaultValue(NNodesTransferLogging) )+1)
                  * paraParamSet->getIntParamDefaultValue(NNodesTransferLogging);
         }

         if( paraParamSet->getRealParamValue(TNodesTransferLogging) > 0.0 &&
               paraTimer->getElapsedTime() > lcts.tNodesOutputLog )
         {
            *osLogSolvingStatus << paraTimer->getElapsedTime() << " = ";
            *osLogSolvingStatus << paraSolverPool->getNnodesSolvedInSolvers()
                  << " " << (paraSolverPool->getNnodesInSolvers() + paraNodePool->getNumOfNodes())
                  << " s " << lcts.nSent << " r " << lcts.nReceived
                  << std::endl;
            lcts.tNodesOutputLog = (static_cast<int>(paraTimer->getElapsedTime()/paraParamSet->getRealParamValue(TNodesTransferLogging)) + 1.0)
                  * paraParamSet->getRealParamDefaultValue(TNodesTransferLogging);
         }
      }

      ParaNode *node = paraSolverPool->getCurrentNode(source);
      if( node->getMergeNodeInfo() != 0 && solverState->getNNodesSolved() > 2 )  // I stand on the safety side. we can write "> 1"
      {
         mergeNodes(node);
         if( paraParamSet->getBoolParamValue(UG::GenerateReducedCheckpointFiles) )
         {
            // std::cout << "S." << source << " ParaNode is saved to Buffer." << std::endl;
            assert( !node->getMergeNodeInfo() );
            paraNodePoolBufferToGenerateCPF->insert(node);
         }
      }

#ifdef _DEBUG_LB
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " | "
      << paraInitiator->convertToExternalValue(
            solverState->getSolverLocalBestDualBoundValue()
            );
      if( !paraInitiator->getGlobalBestIncumbentSolution() ||
            paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) > displayInfOverThisValue )
      {
         std::cout << " ( Inf )";
      }
      else
      {
         std::cout << " ( " << paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) * 100 << "% )";
      }
      std::cout << " [ " << solverState->getNNodesLeft() << " ]";
      globalBestDualBoundValue =
         std::max (
               std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
               lcts.globalBestDualBoundValue );
      std::cout << " ** G.B.: " << paraInitiator->convertToExternalValue(globalBestDualBoundValue);
      if( !paraInitiator->getGlobalBestIncumbentSolution() ||
            paraInitiator->getGap(globalBestDualBoundValue) > displayInfOverThisValue )
      {
         std::cout << " ( Inf ) ";
      }
      else
      {
         std::cout << " ( " << paraInitiator->getGap(globalBestDualBoundValue) * 100 << "% ) ";
      }
      std::cout << "[ " << paraSolverPool->getNnodesInSolvers() << ", " << paraNodePool->getNumOfNodes()
      << " ( " <<  paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
      <<" ) ] **";
      if( runningPhase == RampUpPhase ) std::cout << " R";
      if( paraSolverPool->isInCollectingMode() )
      {
         std::cout << " C";
         if( paraSolverPool->isSolverInCollectingMode(source) )
         {
            std::cout << " 1";
         }
         else
         {
            std::cout << " 0";
         }

      }
      std::cout << std::endl;
#endif

   }

   /** the following should be before noticationId back to the source solver */
   if( paraParamSet->getBoolParamValue(DistributeBestPrimalSolution) )
   {
      if( paraInitiator->getGlobalBestIncumbentSolution() &&
            paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue()
            < solverState->getGlobalBestPrimalBoundValue() )
      {
         paraInitiator->getGlobalBestIncumbentSolution()->send(paraComm, source);
      }
   }

   if( paraParamSet->getBoolParamValue(CheckGapInLC) )
   {
      // std::cout << "gap = " <<
      //      paraInitiator->getGap(globalBestDualBoundValue)*1000  <<
      //      ", gap value = " << paraInitiator->getGapValue()*1000 << std::endl;
      if( paraInitiator->getAbsgap(globalBestDualBoundValue ) < paraInitiator->getAbsgapValue() ||
            paraInitiator->getGap(globalBestDualBoundValue) < paraInitiator->getGapValue() )
      {
         // std::cout << "current dual = "  << paraInitiator->convertToExternalValue(solverState->getSolverLocalBestDualBoundValue()) <<std::endl;
         // std::cout << "current gap = " << paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) <<std::endl;
         for( unsigned int i = 1; i <= paraSolverPool->getNSolvers(); i++ )
         {
            PARA_COMM_CALL(
                  paraComm->send( NULL, 0, ParaBYTE, i, TagGivenGapIsReached )
            );
         }
      }
      else
      {
         if( paraInitiator->getAbsgap(solverState->getSolverLocalBestDualBoundValue()) < paraInitiator->getAbsgapValue() ||
               paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) < paraInitiator->getGapValue() )
         {
            PARA_COMM_CALL(
                  paraComm->send( NULL, 0, ParaBYTE, source, TagGivenGapIsReached )
            );
         }
      }
   }

   double lcBestDualBoundValue = paraNodePool->getBestDualBoundValue();
   PARA_COMM_CALL(
         paraComm->send( &lcBestDualBoundValue, 1, ParaDOUBLE, source, TagLCBestBoundValue)
         );
   unsigned int notificationId = solverState->getNotificaionId();
   PARA_COMM_CALL(
         paraComm->send( &notificationId, 1, ParaUNSIGNED, source, TagNotificationId)
         );

   if( paraParamSet->getBoolParamValue(InitialNodesGeneration) &&
         (signed)( paraSolverPool->getNnodesInSolvers() + paraNodePool->getNumOfNodes() ) >= paraParamSet->getIntParamValue(NumberOfInitialNodes) )
   {
      for(unsigned int i = 1; i <= paraSolverPool->getNSolvers(); i++ )
      {
         int nCollect = -1;
         if( paraSolverPool->isSolverActive(i) )
         {
            PARA_COMM_CALL(
                  paraComm->send( &nCollect, 1, ParaINT, i, TagCollectAllNodes )
            );
         }
      }
      initialNodesGenerated = true;
   }
   else
   {
      if( runningPhase != RampUpPhase  )
      {
         if( paraNodePool->getNumOfGoodNodes(
               globalBestDualBoundValueLocal
               ) > 0 )
         {
            statEmptyNodePoolTime = DBL_MAX;
            isCollectingModeRestarted = false;
         }
         else   // paraNodePool is empty in terms of the number of good nodes
         {
            if( paraSolverPool->getNumInactiveSolvers() > 0 )
            {
               if( !isCollectingModeRestarted )
               {
                  double tempTime = paraSolverPool->getSwichOutTime();
                  paraSolverPool->switchOutCollectingMode();
                  paraSolverPool->setSwichOutTime(tempTime);
                  paraSolverPool->switchInCollectingMode(paraNodePool);
                  if( logSolvingStatusFlag )
                  {
                     *osLogSolvingStatus << paraTimer->getElapsedTime()
                     << " Collecting mode is restarted with " << paraSolverPool->getNLimitCollectingModeSolvers()
                     << std::endl;
                  }
                  if( paraSolverPool->isInCollectingMode() ) isCollectingModeRestarted = true;
               }
            }
            else  // no inactive solvers
            {
               if( isCollectingModeRestarted )
               {
                  statEmptyNodePoolTime = DBL_MAX;  // node pool is  empty, but it becomes empty soon again, it is better to restart collecting mode
                  isCollectingModeRestarted = false;
               }
            }
            if( paraParamSet->getBoolParamValue(Deterministic) )
            {
               if( ( paraDetTimer->getElapsedTime() - statEmptyNodePoolTime ) < 0 )
               {
                  statEmptyNodePoolTime = paraDetTimer->getElapsedTime();
               }
            }
            else
            {
               if( ( paraTimer->getElapsedTime() - statEmptyNodePoolTime ) < 0 )
               {
                  statEmptyNodePoolTime = paraTimer->getElapsedTime();
               }
            }
         }

         if( ( paraParamSet->getBoolParamValue(Deterministic) &&
               ( paraDetTimer->getElapsedTime() - statEmptyNodePoolTime ) > paraParamSet->getRealParamValue(TimeToIncreaseCMS) ) ||
               ( !paraParamSet->getBoolParamValue(Deterministic) &&
               ( paraTimer->getElapsedTime() - statEmptyNodePoolTime ) > paraParamSet->getRealParamValue(TimeToIncreaseCMS) ) )
         {
            if( paraNodePool->getNumOfNodes() == 0 && paraSolverPool->canIncreaseLimitNLimitCollectingModeSolvers() )
               // ramp-up may collect nodes having not so good nodes. As long as nodes exist, the limit number should not be increased.
            {
               double tempTime = paraSolverPool->getSwichOutTime();
               paraSolverPool->switchOutCollectingMode();
               paraSolverPool->setSwichOutTime(tempTime);
               paraSolverPool->incNLimitCollectingModeSolvers();
               paraSolverPool->switchInCollectingMode(paraNodePool);
               if( logSolvingStatusFlag )
               {
                  *osLogSolvingStatus << paraTimer->getElapsedTime()
                  << " Limit number of collecting mode solvers extends to " << paraSolverPool->getNLimitCollectingModeSolvers()
                  << ", p = " << paraParamSet->getIntParamValue(UG::NChangeIntoCollectingMode)*paraSolverPool->getMCollectingNodes()
                  << std::endl;
                  if( outputTabularSolvingStatusFlag )
                  {
                     *osTabularSolvingStatus <<
                           "Limit number of collecting mode solvers extends to " <<
                           paraSolverPool->getNLimitCollectingModeSolvers() <<
                           " after " << paraTimer->getElapsedTime() << " seconds." << std::endl;
                  }
#ifdef _DEBUG_LB
                  std::cout << paraTimer->getElapsedTime()
                        << " Limit number of collecting mode solvers extends to " << paraSolverPool->getNLimitCollectingModeSolvers()
                        << std::endl;
#endif
               }
               // isCollectingModeRestarted = false;
            }
            else   // cannot increase the number of collecting mode solvers
            {
               double tempTime = paraSolverPool->getSwichOutTime();
               paraSolverPool->switchOutCollectingMode();
               paraSolverPool->setSwichOutTime(tempTime);
               paraSolverPool->switchInCollectingMode(paraNodePool);
               if( logSolvingStatusFlag )
               {
                  *osLogSolvingStatus << paraTimer->getElapsedTime()
                  << " Collecting mode is restarted with " << paraSolverPool->getNLimitCollectingModeSolvers()
                  << std::endl;
               }
               /*
               if( !isCollectingModeRestarted )
               {
                  paraSolverPool->switchOutCollectingMode();
                  paraSolverPool->switchInCollectingMode(paraNodePool);
                  if( logSolvingStatusFlag )
                  {
                     *osLogSolvingStatus << paraTimer->getElapsedTime()
                     << " Collecting mode is restarted with " << paraSolverPool->getNLimitCollectingModeSolvers()
                     << std::endl;
                  }
                  isCollectingModeRestarted = true;
               }
               */
            }
            if( paraSolverPool->isInCollectingMode() )
            {
               isCollectingModeRestarted = true;
               statEmptyNodePoolTime = DBL_MAX;
            }
            /*
            if( !isCollectingModeRestarted )
            {
               paraSolverPool->switchOutCollectingMode();
               paraSolverPool->switchInCollectingMode(paraNodePool);
               if( logSolvingStatusFlag )
               {
                  *osLogSolvingStatus << paraTimer->getElapsedTime()
                  << " Collecting mode is restarted with " << paraSolverPool->getNLimitCollectingModeSolvers()
                  << std::endl;
               }
               isCollectingModeRestarted = true;
            }
            else
            {
               if( paraNodePool->getNumOfNodes() == 0 && paraSolverPool->canIncreaseLimitNLimitCollectingModeSolvers() )
                  // ramp-up may collect nodes having not so good nodes. As long as nodes exist, the limit number should not be increased.
               {
                  paraSolverPool->switchOutCollectingMode();
                  paraSolverPool->incNLimitCollectingModeSolvers();
                  paraSolverPool->switchInCollectingMode(paraNodePool);
                  if( logSolvingStatusFlag )
                  {
                     *osLogSolvingStatus << paraTimer->getElapsedTime()
                     << " Limit number of collecting mode solvers extends to " << paraSolverPool->getNLimitCollectingModeSolvers()
                     << std::endl;
                     if( outputTabularSolvingStatusFlag )
                     {
                        *osTabularSolvingStatus <<
                              "Limit number of collecting mode solvers extends to " <<
                              paraSolverPool->getNLimitCollectingModeSolvers() <<
                              " after " << paraTimer->getElapsedTime() << " seconds." << std::endl;
                     }
   #ifdef _DEBUG_LB
                     std::cout << paraTimer->getElapsedTime()
                           << " Limit number of collecting mode solvers extends to " << paraSolverPool->getNLimitCollectingModeSolvers()
                           << std::endl;
   #endif
                  }
                  isCollectingModeRestarted = false;
               }
               else   // cannot increase the number of collecting mode solvers
               {
                  paraSolverPool->switchOutCollectingMode();
                  paraSolverPool->switchInCollectingMode(paraNodePool);
                  if( logSolvingStatusFlag )
                  {
                     *osLogSolvingStatus << paraTimer->getElapsedTime()
                     << " Collecting mode is restarted with " << paraSolverPool->getNLimitCollectingModeSolvers()
                     << std::endl;
                  }
                  isCollectingModeRestarted = true;
               }
            }
            */
            // statEmptyNodePoolTime = DBL_MAX;
         }
      }

      if( !( solverState->isRacingStage() ) &&
            runningPhase != RampUpPhase && !(paraSolverPool->isInCollectingMode()) &&
            ( (signed)paraNodePool->getNumOfGoodNodes(
                  globalBestDualBoundValueLocal
                  ) < paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraSolverPool->getMCollectingNodes() ) )
      {
         paraSolverPool->switchInCollectingMode(paraNodePool);
         if( firstCollectingModeState == -1 && paraSolverPool->isInCollectingMode() ) firstCollectingModeState = 0;
      }

      if( !isBreakingFinised )
      {
         if( (!solverState->isRacingStage()) && runningPhase == NormalRunningPhase )
         {
            if( (signed)paraNodePool->getNumOfNodes() > paraParamSet->getIntParamValue(NStopBreaking) ||
                  (signed)paraSolverPool->getNnodesInSolvers() < paraParamSet->getIntParamValue(NStopBreaking) )
            {
               isBreakingFinised = true;
            }
            else
            {
               if( breakingSolverId == -1 )
               {
                  if( paraSolverPool->getNumOfNodesLeftInBestSolver()
                        > paraParamSet->getIntParamValue(NSolverNodesStartBreaking) )
                  {
                     breakingSolverId = paraSolverPool->getBestSolver();
                     assert( breakingSolverId != -1 );
                     double targetBound = ( paraSolverPool->getGlobalBestDualBoundValue()*
                           paraParamSet->getRealParamValue(MultiplierForBreakingTargetBound) );
                     int nLimitTransfer = paraParamSet->getIntParamValue(NTransferLimitForBreaking);
                     PARA_COMM_CALL(
                           paraComm->send( &targetBound, 1, ParaDOUBLE, breakingSolverId, TagBreaking )
                     );
                     PARA_COMM_CALL(
                           paraComm->send( &nLimitTransfer, 1, ParaINT, breakingSolverId, TagBreaking )
                     );
                  }
                  else
                  {
                     if( ( ( paraSolverPool->getGlobalBestDualBoundValue()
                           + paraParamSet->getRealParamValue(ABgapForSwitchingToBestSolver)*3 ) >
                           solverState->getSolverLocalBestDualBoundValue() ) &&
                                 (signed)paraSolverPool->getNnodesInSolvers() >
                                 std::max(paraParamSet->getIntParamValue(NStopBreaking)*2, (int)paraSolverPool->getNSolvers()*2 ) &&
                         solverState->getNNodesLeft() >  ( paraSolverPool->getNnodesInSolvers()*0.5 ) )
                     {
                        breakingSolverId = source;
                        double targetBound = ( solverState->getSolverLocalBestDualBoundValue()*
                              paraParamSet->getRealParamValue(MultiplierForBreakingTargetBound) );
                        int nLimitTransfer = paraParamSet->getIntParamValue(NTransferLimitForBreaking);
                        PARA_COMM_CALL(
                              paraComm->send( &targetBound, 1, ParaDOUBLE, breakingSolverId, TagBreaking )
                        );
                        PARA_COMM_CALL(
                              paraComm->send( &nLimitTransfer, 1, ParaINT, breakingSolverId, TagBreaking )
                        );
                     }
                  }
               }
            }
         }
      }
      else   // isBootstrapFinised
      {
         if( runningPhase == NormalRunningPhase &&
               (signed)paraNodePool->getNumOfNodes() < paraParamSet->getIntParamValue(NStopBreaking) &&
               (signed)paraSolverPool->getNnodesInSolvers()
               > std::max(paraParamSet->getIntParamValue(NStopBreaking)*2, (int)paraSolverPool->getNSolvers()*2 ) )
         {
            // break again. several solvers can be in breaking situation. That is, braking message can be sent to breaking solver
            isBreakingFinised = false;
            breakingSolverId = -1;
         }
      }
   }

   // if( lcts.globalBestDualBoundValue < globalBestDualBoundValue )
   // {
   //    lcts.globalBestDualBoundValue = globalBestDualBoundValue;
   //   lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(globalBestDualBoundValue);
   // }

   lcts.globalBestDualBoundValue = std::max(lcts.globalBestDualBoundValue, globalBestDualBoundValue);
   lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(globalBestDualBoundValue);

   delete solverState;
   return 0;
}

int
ParaLoadCoordinator::processTagCompletionOfCalculation(
      int source,
      int tag
      )
{
   ParaCalculationState *calcState = paraComm->createParaCalculationState();
   calcState->receive(paraComm, source, tag);
   if( paraRacingSolverPool && paraRacingSolverPool->isActive(source) ) // racing root node termination
   {
      writeTransferLogInRacing(source, calcState);
   }
   else
   {
      writeTransferLog(source, calcState);
   }

   if( !winnerSolverNodesCollected &&
         racingWinner == source )
   {
      winnerSolverNodesCollected = true;
      if( merging )
      {
         generateMergeNodesCandidates();   // Anyway,merge nodes candidates have to be generated,
                                           // even if running with InitialNodesGeneration
         merging = false;
      }
      if( paraParamSet->getBoolParamValue(InitialNodesGeneration) &&
           (signed)paraNodePool->getNumOfNodes() >= paraParamSet->getIntParamValue(NumberOfInitialNodes) )
      {
         initialNodesGenerated = true;
      }
   }

   int calcTerminationState = calcState->getTerminationState();

   if( logSolvingStatusFlag )
   {
      switch ( calcTerminationState )
      {
      case CompTerminatedNormally:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " > " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      case CompTerminatedByAnotherNode:
      {
         /** When starts with racing ramp-up, solvers except winner should be terminated in this state */
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(INTERRUPTED_BY_ANOTHER_NODE) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      case CompTerminatedByInterruptRequest:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(INTERRUPTED) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      case CompTerminatedInRacingStage:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(TERMINATED_IN_RACING_STAGE) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      case CompInterruptedInRacingStage:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(INTERRUPTED_IN_RACING_STAGE) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      case CompInterruptedInMerging:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(INTERRUPTED_IN_MERGING) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      case CompTerminatedByTimeLimit:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(INTERRUPTED_BY_TIME_LIMIT) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      default:
         THROW_LOGICAL_ERROR2("Invalid termination: termination state = ", calcState->getTerminationState() )
      }

      if( paraParamSet->getBoolParamValue(CheckEffectOfRootNodePreprocesses) &&
            calcState->getNSolvedWithNoPreprocesses() > 0 )
      {
         *osLogSolvingStatus << " SOLVED_AT_ROOT ( DEPTH = " << paraSolverPool->getCurrentNode(source)->getDepth()
         << ", Gap = " << paraInitiator->getGap(paraSolverPool->getCurrentNode(source)->getDualBoundValue()) * 100  << "%, TrueGap = "
         << paraInitiator->getGap(paraSolverPool->getCurrentNode(source)->getInitialDualBoundValue()) * 100  << "% ) [ "
         << calcState->getNSolvedWithNoPreprocesses() << " ]";
      }
      *osLogSolvingStatus << ", ct:" << calcState->getCompTime()
            << ", nr:" << calcState->getNRestarts()
            << ", n:" << calcState->getNSolved()
            << ", rt:" << calcState->getRootTime()
            << ", avt:" << calcState->getAverageNodeCompTimeExcpetRoot()
            << std::endl;
   }

#ifdef _DEBUG_LB
   switch ( calcTerminationState )
   {
   case CompTerminatedNormally:
   {
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " >";
      break;
   }
   case CompTerminatedByAnotherNode:
   {
      /** When starts with racing ramp-up, solvers except winner should be terminated in this state */
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " >(INTERRUPTED_BY_ANOTHER_NODE)";
      break;
   }
   case CompTerminatedByInterruptRequest:
   {
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " >(INTERRUPTED)";
      break;
   }
   case CompTerminatedInRacingStage:
   {
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " >(TERMINATED_IN_RACING_STAGE)";
      break;
   }
   case CompInterruptedInRacingStage:
   {
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " >(INTERRUPTED_IN_RACING_STAGE)";
      break;
   }
   default:
      THROW_LOGICAL_ERROR2("Invalid termination: termination state = ", calcState->getTerminationState() )
   }

   if( paraParamSet->getBoolParamValue(CheckEffectOfRootNodePreprocesses) &&
         calcState->getNSolvedWithNoPreprocesses() > 0 )
   {
      std::cout << " SOLVED_AT_ROOT ( DEPTH = " << paraSolverPool->getCurrentNode(source)->getDepth()
      << ", Gap = " << paraInitiator->getGap(paraSolverPool->getCurrentNode(source)->getDualBoundValue()) * 100  << "%, TrueGap = "
      << paraInitiator->getGap(paraSolverPool->getCurrentNode(source)->getInitialDualBoundValue()) * 100  << "% ) [ "
      << calcState->getNSolvedWithNoPreprocesses() << " ]";
   }
   std::cout << std::endl;
#endif

   switch ( calcTerminationState )
   {
   case CompTerminatedNormally:
   {
      writeSubtreeInfo(source, calcState);
      ParaNode *node = paraSolverPool->getCurrentNode(source);
      if( node->getMergeNodeInfo() )
      {
         node->setDualBoundValue(paraInitiator->convertToExternalValue(calcState->getDualBoundValue()));
         mergeNodes(node);
      }
      if( (!paraRacingSolverPool) || (!paraRacingSolverPool->isActive(source) ) )  // RacingSolverPool is inactivated below
      {
         if( paraNodePoolToRestart )
         {
            ParaNode *solvingNode = paraSolverPool->extractCurrentNodeAndInactivate(source, paraNodePool);
            if( solvingNode->getAncestor() )
            {
               delete solvingNode;
            }
            else
            {
               paraNodePoolToRestart->insert(solvingNode); // to stand a safety side about timing issue. two branch nodes may be romved.
            }
            return 0;
         }

         if( paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) > 0.0 &&
               runningPhase == TerminationPhase )
         {
            // Exceptional case (reqested to a node whoes root node was solving), but it happend.
            ParaNode *solvingNode = paraSolverPool->extractCurrentNodeAndInactivate(source, paraNodePool);
            if( solvingNode->areNodesCollected() )
            {
               if( logSolvingStatusFlag )
               {
                  *osLogSolvingStatus << paraTimer->getElapsedTime()
                  << " Nodes generated by S." << source << " from " << solvingNode->toSimpleString() << " are collected to LC." << std::endl;
               }
               delete solvingNode;
#ifdef UG_WITH_ZLIB
               updateCheckpointFiles();
#endif
               sendInterruptRequest();
            }
            else
            {
               delete solvingNode;
            }
         }
         else
         {
            if( paraSolverPool->isSolverInCollectingMode(source) )
            {
               paraSolverPool->inactivateSolver(source, calcState->getNSolved(),paraNodePool);
               // reschedule collecting mode
               if( !paraNodePoolBufferToRestart )
               {
                  double tempTime = paraSolverPool->getSwichOutTime();
                  paraSolverPool->switchOutCollectingMode();
                  paraSolverPool->setSwichOutTime(tempTime);
                  paraSolverPool->switchInCollectingMode(paraNodePool);
               }
            }
            else
            {
               paraSolverPool->inactivateSolver(source, calcState->getNSolved(),paraNodePool);
            }
         }
         paraSolverPool->addTotalNodesSolved(calcState->getNSolved());
      }
      // std::cout << "Rank" << source
      //     << ", lcts.best = " << lcts.externalGlobalBestDualBoundValue
      //      << ", bound = " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue())
      //     << ", gap = " << std::setprecision(5) << paraInitiator->getGap(calcState->getDualBoundValue())*100 << "%" << std::endl;
      if( !EPSEQ( calcState->getDualBoundValue(), -DBL_MAX, paraInitiator->getEpsilon() ) )
      {
         allCompInfeasibleAfterSolution = false;
         if( EPSLE(lcts.globalBestDualBoundValue, calcState->getDualBoundValue(), paraInitiator->getEpsilon()) &&
               minmalDualBoundNormalTermSolvers > calcState->getDualBoundValue() )
         {
            if( paraInitiator->getGlobalBestIncumbentSolution() )
            {
               minmalDualBoundNormalTermSolvers = std::min( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), calcState->getDualBoundValue() );
            }
            else
            {
               minmalDualBoundNormalTermSolvers = calcState->getDualBoundValue();
            }
         }
         /*
         if( // maximal dual bound value of terminated solver should be taken.
             // Therefore, the gap value is better than the real value
               calcState->getDualBoundValue() <
               std::min(
                     std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
                     minmalDualBoundNormalTermSolvers ) &&
               lcts.globalBestDualBoundValue < calcState->getDualBoundValue() )
         {
            lcts.globalBestDualBoundValue = calcState->getDualBoundValue();
            lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
            // std::cout << "Updated Rank" << source
            //      << ", lcts.best = " << lcts.externalGlobalBestDualBoundValue
            //      << ", bound = " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue())
            //      << ", gap = " << std::setprecision(5) << paraInitiator->getGap(calcState->getDualBoundValue())*100 << "%" << std::endl;
         }
         */
      }
      // DO NOT send ParaNode here!
      // Send ParaNode after solver termination state is received.
      if( paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) > 0.0 &&
            runningPhase == TerminationPhase )
      {
         if( (!paraRacingSolverPool) || (!paraRacingSolverPool->isActive(source) ) )
         {
            PARA_COMM_CALL(
                  paraComm->send( NULL, 0, ParaBYTE, source, TagTerminateRequest )
            );
         }
      }
      break;
   }
   case CompInterruptedInRacingStage:
   {
      // DO NOT send ParaNode here!
      // Send ParaNode after solver termination state is received and RacingSolver is inactivated.
      // Do not have to update counters of ParaSolverPool.
      break;
   }
   case CompTerminatedByAnotherNode:
   {
      /** in this case the following two numbers should be different */
      /** # Total > # Solved */
      paraSolverPool->addNumNodesSolved(calcState->getNSolved());
      paraSolverPool->addTotalNodesSolved(calcState->getNSolved());
      if( paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) > 0.0 &&
            runningPhase == TerminationPhase )
      {
         if( (!paraRacingSolverPool) || (!paraRacingSolverPool->isActive(source) ) )
         {
            PARA_COMM_CALL(
                  paraComm->send( NULL, 0, ParaBYTE, source, TagTerminateRequest )
            );
         }
      }
      break;
   }
   case CompTerminatedByInterruptRequest:
   {
      if( paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) > 0.0 &&
            runningPhase == TerminationPhase )
      {
         if( (!paraRacingSolverPool) || (!paraRacingSolverPool->isActive(source) ) )
         {
            writeSubtreeInfo(source, calcState);
            ParaNode *solvingNode = paraSolverPool->extractCurrentNodeAndInactivate(source, paraNodePool);
            if( solvingNode->areNodesCollected() )
            {
               if( logSolvingStatusFlag )
               {
                  *osLogSolvingStatus << paraTimer->getElapsedTime()
                  << " Nodes generated by S." << source << " from " << solvingNode->toSimpleString() << " are collected to LC." << std::endl;
               }
               delete solvingNode;
               nCollectedSolvers++;
#ifdef UG_WITH_ZLIB
               updateCheckpointFiles();
#endif               
               sendInterruptRequest();
            }
            else
            {
               paraNodePool->insert(solvingNode);
               sendInterruptRequest();
            }
         }
         break;
      }
      if( (!paraRacingSolverPool) || (!paraRacingSolverPool->isActive(source) ) )  // RacingSolverPool is inactivated below
      {
         // paraRacingSolverPool entry is inactivated, when it receives ParaSolverTerminationState message in below.
         ParaNode *solvingNode = paraSolverPool->extractCurrentNodeAndInactivate(source, paraNodePool);
         if( paraNodePoolToRestart )
         {
            if( solvingNode->getAncestor() )
            {
               delete solvingNode;
            }
            else
            {
               paraNodePoolToRestart->insert(solvingNode);
            }
         }
         else
         {
            paraNodePool->insert(solvingNode);
         }
      }
      //
      // no update lcts.globalBestDualBoundValue and lcts.externalGlobalBestDualBoundValue
      // just use SolerState update
      //
      break;
   }
   case CompTerminatedInRacingStage:
   {
      racingTermination = true; // even if interruptIsRequested, 
                                // solver should have been terminated before receiveing it
      if( osStatisticsRacingRampUp )
      {
         *osStatisticsRacingRampUp << "######### Solver Rank = " <<
               source << " is terminated in racing stage #########" << std::endl;
      }
      nSolvedRacingTermination = calcState->getNSolved();

      if( !EPSEQ( calcState->getDualBoundValue(), -DBL_MAX, paraInitiator->getEpsilon() ) &&
            EPSEQ( minmalDualBoundNormalTermSolvers, DBL_MAX, paraInitiator->getEpsilon() ) )
      {
         if( paraInitiator->getGlobalBestIncumbentSolution() )
         {
            minmalDualBoundNormalTermSolvers = std::min( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), calcState->getDualBoundValue() );
         }
         else
         {
            minmalDualBoundNormalTermSolvers = calcState->getDualBoundValue();
         }
      }
      if( EPSLE(lcts.globalBestDualBoundValue, calcState->getDualBoundValue(), paraInitiator->getEpsilon()) &&
            minmalDualBoundNormalTermSolvers < calcState->getDualBoundValue() )
      {
         if( paraInitiator->getGlobalBestIncumbentSolution() )
         {
            minmalDualBoundNormalTermSolvers = std::min( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), calcState->getDualBoundValue() );
         }
         else
         {
            minmalDualBoundNormalTermSolvers = calcState->getDualBoundValue();
         }
      }
      if( paraInitiator->getGlobalBestIncumbentSolution() &&
            ( EPSEQ( calcState->getDualBoundValue(), paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), paraInitiator->getEpsilon() ) ||
                  EPSEQ( calcState->getDualBoundValue(), -DBL_MAX, paraInitiator->getEpsilon() ) ) )
      {
         lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
         lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
      }

      /*
      if( paraInitiator->getGlobalBestIncumbentSolution() &&
            EPSEQ( calcState->getDualBoundValue(), -DBL_MAX, paraInitiator->getEpsilon() ) )
      {
         lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
         lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
      }
      else
      {
         if( lcts.globalBestDualBoundValue < calcState->getDualBoundValue() )
         {
            lcts.globalBestDualBoundValue = calcState->getDualBoundValue();
            lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
         }
      }
      */

      /*
      if( !interruptedFromControlTerminal && !hardTimeLimitIsReached &&
            !computationIsInterrupted
            && paraInitiator->getGlobalBestIncumbentSolution() )
      {
         lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
         lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
      }
      */
      break;
   }
   case CompInterruptedInMerging:
   {
      ParaNode *solvingNode = paraSolverPool->extractCurrentNodeAndInactivate(source, paraNodePool);
      if( paraParamSet->getBoolParamValue(UG::GenerateReducedCheckpointFiles) )
      {
         // std::cout << "S." << source << " is interrupted." << std::endl;
         if( solvingNode->getMergeNodeInfo() )
         {
            regenerateMergeNodesCandidates(solvingNode);
            paraNodePool->insert(solvingNode);
         }
         // else, node shuld be in paraNodePoolBufferToGenerateCPF.
      }
      else
      {
         assert(solvingNode->getMergeNodeInfo());
         regenerateMergeNodesCandidates(solvingNode);
         paraNodePool->insert(solvingNode);
      }
      break;
   }
   case CompTerminatedByTimeLimit:
   {
      if( paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) > 0.0 &&
            runningPhase == TerminationPhase )
      {
         if( (!paraRacingSolverPool) || (!paraRacingSolverPool->isActive(source) ) )
         {
            writeSubtreeInfo(source, calcState);
            ParaNode *solvingNode = paraSolverPool->extractCurrentNodeAndInactivate(source, paraNodePool);
            if( solvingNode->areNodesCollected() )
            {
               if( logSolvingStatusFlag )
               {
                  *osLogSolvingStatus << paraTimer->getElapsedTime()
                  << " Nodes generated by S." << source << " from " << solvingNode->toSimpleString() << " are collected to LC." << std::endl;
               }
               delete solvingNode;
               nCollectedSolvers++;
#ifdef UG_WITH_ZLIB
               updateCheckpointFiles();
#endif
               sendInterruptRequest();
            }
            else
            {
               paraNodePool->insert(solvingNode);
               sendInterruptRequest();
            }
         }
         break;
      }
      if( (!paraRacingSolverPool) || (!paraRacingSolverPool->isActive(source) ) )  // RacingSolverPool is inactivated below
      {
         // paraRacingSolverPool entry is inactivated, when it receives ParaSolverTerminationState message in below.
         ParaNode *solvingNode = paraSolverPool->extractCurrentNodeAndInactivate(source, paraNodePool);
         if( paraNodePoolToRestart )
         {
            if( solvingNode->getAncestor() )
            {
               delete solvingNode;
            }
            else
            {
               paraNodePoolToRestart->insert(solvingNode);
            }
         }
         else
         {
            paraNodePool->insert(solvingNode);
         }
      }
      //
      // no update lcts.globalBestDualBoundValue and lcts.externalGlobalBestDualBoundValue
      // just use SolerState update
      //
      break;
   }
   default:
      THROW_LOGICAL_ERROR2("Invalid termination: termination state = ", calcState->getTerminationState() )
   }

   if( calcState->getTerminationState() == CompTerminatedByTimeLimit )
   {
      std::cout << "####### Rank " << paraComm->getRank() << " solver terminated with timelimit in solver side. #######" << std::endl;
      std::cout << "####### Final statistics may be messed up!" << std::endl;
   }

   delete calcState;

   ParaSolverTerminationState *termState = paraComm->createParaSolverTerminationState();
   termState->receive(paraComm, source, TagTerminated);

   if( paraDetTimer )
   {
      if( paraDetTimer->getElapsedTime() < termState->getDeterministicTime() )
      {
         paraDetTimer->update( termState->getDeterministicTime() - paraDetTimer->getElapsedTime() );
      }
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, source, TagAckCompletion )
      );
   }

   switch( termState->getInterruptedMode() )
   {
   case 2: /** checkpoint; This is normal termination */
   {
      /** in order to save termination status to check point file, keep this information to solver pool */
      paraSolverPool->setTermState(source, termState);
      // don't delete termState! it is saved in paraSolverPool
      if( runningPhase != TerminationPhase &&
            !paraNodePoolToRestart &&
            calcTerminationState != CompTerminatedByTimeLimit )
      {
         if( paraNodePool->isEmpty() )
         {
            lcts.nFailedToSendBack++;
            if( runningPhase != RampUpPhase && !(paraSolverPool->isInCollectingMode()) )
            {
               paraSolverPool->switchInCollectingMode(paraNodePool);
               if( firstCollectingModeState == -1 && paraSolverPool->isInCollectingMode() ) firstCollectingModeState = 0;
            }
         }
         else
         {
            if( sendParaNodesToIdleSolvers() )
            {
               lcts.nSentBackImmediately++;
            }
         }
      }
      break;
   }
   case 3: /** racing ramp-up */
   {
      if( osStatisticsRacingRampUp )
      {
         *osStatisticsRacingRampUp << termState->toString(paraInitiator);
         osStatisticsRacingRampUp->flush();
      }
      // nTerminated++;      We should not count this, We should always send Term from LC!
      inactivateRacingSolverPool(source);
      /*  Anyway, incumbent value is sent to all Solvers except its generator. In such case, this is not necessary.
      double globalBestIncumbentValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
      PARA_COMM_CALL(
            paraComm->send( &globalBestIncumbentValue, 1, ParaDOUBLE, source, TagIncumbentValue )
      );
      */
      if( runningPhase != TerminationPhase &&
            calcTerminationState != CompTerminatedByTimeLimit )
      {
         if( paraNodePool->isEmpty() )
         {
            lcts.nFailedToSendBack++;
            if( runningPhase != RampUpPhase && !(paraSolverPool->isInCollectingMode()) )
            {
               paraSolverPool->switchInCollectingMode(paraNodePool);
               if( firstCollectingModeState == -1 && paraSolverPool->isInCollectingMode() ) firstCollectingModeState = 0;
            }
         }
         else
         {
            if( sendParaNodesToIdleSolvers() )
            {
               lcts.nSentBackImmediately++;
            }
         }
      }
      delete termState;
      break;
   }
   default:  /** unexpected mode */
      THROW_LOGICAL_ERROR4("Unexpected termination state received from rank = ", source,
            ", interrupted mode = ", termState->getInterruptedMode());
   }

   if( paraParamSet->getBoolParamValue(Quiet) && racingTermination )
   {
      /** in this case, do not have to wait statistical information from the other solvers */
      nTerminated = 1;
      delete this;
#ifdef _COMM_PTH
      _exit(0);
#else
      exit(0);
#endif
   }

   if( source == breakingSolverId )
   {
      breakingSolverId = -1;
      isBreakingFinised = false;
   }
   return 0;
}

int
ParaLoadCoordinator::processTagAnotherNodeRequest(
      int source,
      int tag
      )
{
   double bestDualBoundValue;
   PARA_COMM_CALL(
         paraComm->receive( &bestDualBoundValue, 1, ParaDOUBLE, source, TagAnotherNodeRequest)
         );
   if( paraNodePool->isEmpty() || paraSolverPool->currentSolvingNodehaeDescendant(source) )
   {
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, source, TagNoNodes)
            );
      lcts.nFailedToSendBackAnotherNode++;
   }
   else
   {
      ParaNode *paraNode = 0;
      while( !paraNodePool->isEmpty() )
      {
         paraNode = paraNodePool->extractNode();
         if( !paraNode ) break;
         if( ( paraInitiator->getGlobalBestIncumbentSolution() &&
               paraNode->getDualBoundValue() < paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() ) ||
               !( paraInitiator->getGlobalBestIncumbentSolution() ) )
         {
            break;
         }
         else
         {
#ifdef UG_DEBUG_SOLUTION
            if( paraNode->getDiffSubproblem() && paraNode->getDiffSubproblem()->isOptimalSolIncluded() )
            {
                throw "Optimal solution going to be killed.";
            }
#endif
            delete paraNode;
            paraNode = 0;
            lcts.nDeletedInLc++;
         }
      }
      if( paraNode )
      {
         if( ( paraSolverPool->getDualBoundValue(source) - paraNode->getDualBoundValue()) > 0.0 &&
               ( REALABS( ( paraSolverPool->getDualBoundValue(source) - paraNode->getDualBoundValue() )
                     / std::max( std::abs(paraNode->getDualBoundValue()), 1.0) ) > paraParamSet->getRealParamValue(BgapStopSolvingMode) ) )
         {
            paraSolverPool->sendSwitchOutCollectingModeIfNecessary(source);
            ParaNode *solvingNode = paraSolverPool->extractCurrentNodeAndInactivate(source, paraNodePool);
            solvingNode->setDualBoundValue(bestDualBoundValue);
            solvingNode->setInitialDualBoundValue(bestDualBoundValue);
            paraNodePool->insert(solvingNode);
            if( solvingNode->getMergeNodeInfo() )
            {
               mergeNodes(solvingNode);
            }
            // without consideration of keeping nodes in checkpoint file
            double globalBestDualBoundValueLocal =
               std::max (
                     std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
                     lcts.globalBestDualBoundValue );
            paraSolverPool->activateSolver(source, paraNode,
                  paraNodePool->getNumOfGoodNodes(globalBestDualBoundValueLocal), averageLastSeveralDualBoundGains);
            // paraNode->send(paraComm, source); // send the node in acitivateSolver
            lcts.nSent++;
            lcts.nSentBackImmediatelyAnotherNode++;
            writeTransferLog(source);
            if( logSolvingStatusFlag )
            {
               *osLogSolvingStatus << paraTimer->getElapsedTime()
               << " S." << source << " <(ANOTHER_NODE) "
               << paraInitiator->convertToExternalValue(
                     paraNode->getDualBoundValue() );
               if( paraInitiator->getGlobalBestIncumbentSolution() )
               {
                  if( paraInitiator->getGap(paraNode->getDualBoundValue()) > displayInfOverThisValue )
                  {
                     *osLogSolvingStatus << " ( Inf )";
                  }
                  else
                  {
                     *osLogSolvingStatus << " ( " << paraInitiator->getGap(paraNode->getDualBoundValue()) * 100 << "% )";
                  }
               }
               *osLogSolvingStatus << std::endl;
            }
#ifdef _DEBUG_LB
            std::cout << paraTimer->getElapsedTime()
            << " S." << source << " <(ANOTHER_NODE) "
            << paraInitiator->convertToExternalValue(
                  paraNode->getDualBoundValue() );
            if( paraInitiator->getGlobalBestIncumbentSolution() )
            {
               if( paraInitiator->getGap(paraNode->getDualBoundValue()) > displayInfOverThisValue )
               {
                  std::cout << " ( Inf )";
               }
               else
               {
                  std::cout << " ( " << paraInitiator->getGap(paraNode->getDualBoundValue()) * 100 << "% )";
               }
            }
            std::cout << std::endl;
#endif
         }
         else
         {
            paraNodePool->insert(paraNode);
            PARA_COMM_CALL(
                  paraComm->send( NULL, 0, ParaBYTE, source, TagNoNodes)
                  );
            lcts.nFailedToSendBackAnotherNode++;
         }
      }
      else
      {
         PARA_COMM_CALL(
               paraComm->send( NULL, 0, ParaBYTE, source, TagNoNodes)
               );
         lcts.nFailedToSendBackAnotherNode++;
      }
   }
   return 0;
}

int
ParaLoadCoordinator::processTagTerminated(
      int source,
      int tag
      )
{
   ParaSolverTerminationState *paraSolverTerminationState = paraComm->createParaSolverTerminationState();
   paraSolverTerminationState->receive(paraComm, source, tag);

   if( paraDetTimer )
   {
      if( paraDetTimer->getElapsedTime() < paraSolverTerminationState->getDeterministicTime() )
      {
         paraDetTimer->update( paraSolverTerminationState->getDeterministicTime() - paraDetTimer->getElapsedTime() );
      }
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, source, TagAckCompletion )
      );
   }

   if( osStatisticsFinalRun )
   {
      *osStatisticsFinalRun << paraSolverTerminationState->toString(paraInitiator);
      osStatisticsFinalRun->flush();
   }
   if( paraParamSet->getBoolParamValue(StatisticsToStdout) )
   {
      std::cout << paraSolverTerminationState->toString(paraInitiator) << std::endl;
   }
   /* do not have to do this. All ParaSolvers should be inactivated.
   if( paraSolverPool->isSolverActive(source) )   // if TimeLimit is specified, solver can be active
   {
      paraSolverPool->inactivateSolver(source, 0, paraNodePool );   // 0: the number of solved should be counted in completion of calculation process
   }
   */
   if( (!racingTermination) && paraSolverTerminationState->getInterruptedMode() == 1 )
   {
      computationIsInterrupted = true;
   }

   /*
   if( (!racingTermination) && (!racingTermination && !interruptIsRequested ) )
   {
      assert( paraNodePool->getNumOfNodes() == 0 );
      if( !EPSEQ( paraSolverTerminationState->getDualBoundValue(), -DBL_MAX, paraInitiator->getEpsilon() ) &&
            lcts.globalBestDualBoundValue > paraSolverTerminationState->getDualBoundValue() )
      {
         lcts.globalBestDualBoundValue = paraSolverTerminationState->getDualBoundValue();
         lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
      }
   }
   */

   nTerminated++;

   delete paraSolverTerminationState;

   return 0;
}

int
ParaLoadCoordinator::processTagHardTimeLimit(
      int source,
      int tag
      )
{
   PARA_COMM_CALL(
         paraComm->receive( NULL, 0, ParaBYTE, source, TagHardTimeLimit)
         );
   hardTimeLimitIsReached = true;
   return 0;
}

int
ParaLoadCoordinator::processTagInitialStat(
      int source,
      int tag
      )
{
   ParaInitialStat *initialStat = paraComm->createParaInitialStat();
   initialStat->receive(paraComm, source);
   if( maxDepthInWinnerSolverNodes < initialStat->getMaxDepth() )
   {
      maxDepthInWinnerSolverNodes = initialStat->getMaxDepth();
   }
   paraInitiator->accumulateInitialStat(initialStat);
   delete initialStat;
   return 0;
}

int
ParaLoadCoordinator::processTagToken(
      int source,
      int tag
      )
{
   int token[2];
   PARA_COMM_CALL(
         paraComm->receive( token, 2, ParaINT, source, TagToken)
         );
   PARA_COMM_CALL(
         paraComm->send( token, 2, ParaINT, token[0], TagToken )
   );

   paraComm->setToken(0, token);    // for debug

   return 0;
}

int
ParaLoadCoordinator::processTagAllowToBeInCollectingMode(
      int source,
      int tag
      )
{
   PARA_COMM_CALL(
         paraComm->receive( NULL, 0, ParaBYTE, source, TagAllowToBeInCollectingMode)
         );
   paraSolverPool->setCollectingIsAllowed(source);

   return 0;
}

int
ParaLoadCoordinator::processTagLbBoundTightened(
      int source,
      int tag
      )
{
   int tightenedIdex;
   double tightenedBound;
   PARA_COMM_CALL(
         paraComm->receive( (void *)&tightenedIdex, 1, ParaINT, source, TagLbBoundTightenedIndex )
         );
   PARA_COMM_CALL(
         paraComm->receive( (void *)&tightenedBound, 1, ParaDOUBLE, source, TagLbBoundTightenedBound )
         );
   if( EPSLT(paraInitiator->getTightenedVarLbs(tightenedIdex), tightenedBound, MINEPSILON ) )
   {
	  // std::cout << "From Rank " << source << ": in initiator LB = " << paraInitiator->getTightenedVarLbs(tightenedIdex) << ", rv = " << tightenedBound << std::endl;
      paraInitiator->setTightenedVarLbs(tightenedIdex, tightenedBound);
      if( paraRacingSolverPool && ( paraRacingSolverPool->getNumInactiveSolvers() == 0 ) )
      {
         for( int i = 1; i <= paraRacingSolverPool->getNumActiveSolvers(); i++ )
         {
            if( i != source )
            {
               PARA_COMM_CALL(
                     paraComm->send( (void *)&tightenedIdex, 1, UG::ParaINT, i, UG::TagLbBoundTightenedIndex )
                     );
               PARA_COMM_CALL(
                     paraComm->send( (void *)&tightenedBound, 1, UG::ParaDOUBLE, i, UG::TagLbBoundTightenedBound )
                     );
            }
         }
         // std::cout << "From Rank " << source << ": broadcast tightened lower bond. idx = " << tightenedIdex << ", bound = " << tightenedBound << std::endl;
      }
   }

   return 0;
}

int
ParaLoadCoordinator::processTagUbBoundTightened(
      int source,
      int tag
      )
{
   int tightenedIdex;
   double tightenedBound;
   PARA_COMM_CALL(
         paraComm->receive( (void *)&tightenedIdex, 1, ParaINT, source, TagUbBoundTightenedIndex )
         );
   PARA_COMM_CALL(
         paraComm->receive( (void *)&tightenedBound, 1, ParaDOUBLE, source, TagUbBoundTightenedBound )
         );
   if( EPSGT(paraInitiator->getTightenedVarUbs(tightenedIdex), tightenedBound, MINEPSILON ) )
   {
	  // std::cout << "From Rank " << source << ": in initiator UB = " << paraInitiator->getTightenedVarUbs(tightenedIdex) << ", rv = " << tightenedBound << std::endl;
      paraInitiator->setTightenedVarUbs(tightenedIdex, tightenedBound);
      if( paraRacingSolverPool && ( paraRacingSolverPool->getNumInactiveSolvers() == 0 ) )
      {
         for( int i = 1; i <= paraRacingSolverPool->getNumActiveSolvers(); i++ )
         {
            if( i != source )
            {
               PARA_COMM_CALL(
                     paraComm->send( (void *)&tightenedIdex, 1, UG::ParaINT, i, UG::TagUbBoundTightenedIndex )
                     );
               PARA_COMM_CALL(
                     paraComm->send( (void *)&tightenedBound, 1, UG::ParaDOUBLE, i, UG::TagUbBoundTightenedBound )
                     );
            }
         }
         // std::cout << "From Rank " << source << ": broadcast tightened upper bond. idx = " << tightenedIdex << ", bound = " << tightenedBound << std::endl;
      }
   }

   return 0;
}

void
ParaLoadCoordinator::outputTabularSolvingStatus(
      char incumbent
      )
{
   *osTabularSolvingStatus << std::setw(1) << incumbent;
   *osTabularSolvingStatus << std::setw(8) << std::right << std::setprecision(0) << std::fixed << paraTimer->getElapsedTime();
   if( !restarted &&
         ( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 1 ||
               paraParamSet->getIntParamValue(RampUpPhaseProcess) == 2 )
               && !racingWinnerParams )
   {
      /** racing ramp-up stage now */
      if( !racingTermination && paraRacingSolverPool )
      {
         *osTabularSolvingStatus << std::setw(15) << std::right << paraRacingSolverPool->getNnodesSolvedInBestSolver();
         *osTabularSolvingStatus << std::setw(12) << std::right << paraRacingSolverPool->getNnodesLeftInBestSolver();
         *osTabularSolvingStatus << std::setw(10) << std::right << paraRacingSolverPool->getNumActiveSolvers();
         if( paraInitiator->getGlobalBestIncumbentSolution() )
         {
            *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                  paraInitiator->convertToExternalValue(
                  paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue());
         }
         else
         {
            *osTabularSolvingStatus << std::setw(17) << std::right << "-";
         }
         if( paraRacingSolverPool->getNnodesSolvedInBestSolver() == 0 )
         {
            *osTabularSolvingStatus << std::setw(17) << std::right << "-";
         }
         else
         {
            if( EPSEQ( lcts.globalBestDualBoundValue,-DBL_MAX, paraInitiator->getEpsilon() ))
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << "-";
            }
            else
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                     lcts.externalGlobalBestDualBoundValue;
            }
         }
      }
      else  // One of ParaSolvers terminates in racing stage
      {
         if( nSolvedRacingTermination > 0 )
         {
            *osTabularSolvingStatus << std::setw(15) << std::right << nSolvedRacingTermination;
            *osTabularSolvingStatus << std::setw(12) << std::right << 0;
            *osTabularSolvingStatus << std::setw(10) << std::right << 0;
            if( paraInitiator->getGlobalBestIncumbentSolution() )
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                     paraInitiator->convertToExternalValue(
                     paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue());
            }
            else
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << "-";
            }
            // *osTabularSolvingStatus << std::setw(17) << std::right << "-";
            if( EPSEQ( lcts.globalBestDualBoundValue,-DBL_MAX, paraInitiator->getEpsilon() ))
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << "-";
            }
            else
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                     lcts.externalGlobalBestDualBoundValue;
            }
         }
         else   // should be interrupted
         {
            *osTabularSolvingStatus << std::setw(15) << std::right << nSolvedInInterruptedRacingSolvers;
            *osTabularSolvingStatus << std::setw(12) << std::right << nNodesLeftInInterruptedRacingSolvers;
            *osTabularSolvingStatus << std::setw(10) << std::right << 0;
            if( paraInitiator->getGlobalBestIncumbentSolution() )
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                     paraInitiator->convertToExternalValue(
                           paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue());
            }
            else
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << "-";
            }
            *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                  lcts.externalGlobalBestDualBoundValue;
         }
      }
      if( !paraNodeToKeepCheckpointFileNodes &&
            (!paraInitiator->getGlobalBestIncumbentSolution() ||
            paraInitiator->getGap(lcts.globalBestDualBoundValue) > displayInfOverThisValue ||
            ( paraSolverPool->isActive() && paraSolverPool->getNumActiveSolvers() == 0 && paraNodePool->getNumOfNodes() == 0 )
            ) )
      {
         *osTabularSolvingStatus << std::setw(10) << std::right << "-";
      }
      else
      {
         *osTabularSolvingStatus << std::setw(9) << std::right << std::setprecision(2) <<
               paraInitiator->getGap(lcts.globalBestDualBoundValue) * 100 << "%";
      }
   }
   else
   {
      *osTabularSolvingStatus << std::setw(15) << std::right << paraSolverPool->getNnodesSolvedInSolvers();
      *osTabularSolvingStatus << std::setw(12) << std::right << ( paraSolverPool->getNnodesInSolvers() + paraNodePool->getNumOfNodes() );
      *osTabularSolvingStatus << std::setw(10) << std::right << paraSolverPool->getNumActiveSolvers();
      if( paraInitiator->getGlobalBestIncumbentSolution() )
      {
         *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
               paraInitiator->convertToExternalValue(
               paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue());
      }
      else
      {
         *osTabularSolvingStatus << std::setw(17) << std::right << "-";
      }

      if( paraParamSet->getBoolParamValue(UG::GenerateReducedCheckpointFiles)  )
      {
         // lcts.globalBestDualBoundValue = std::min(std::min( paraNodePool->getBestDualBoundValue(), paraNodePoolForBuffering->getBestDualBoundValue() ),
         //       paraSolverPool->getGlobalBestDualBoundValue() );
         // lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue( lcts.globalBestDualBoundValue );

         // *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<  lcts.externalGlobalBestDualBoundValue;
         if( EPSEQ( lcts.globalBestDualBoundValue,-DBL_MAX, paraInitiator->getEpsilon() ))
         {
            *osTabularSolvingStatus << std::setw(17) << std::right << "-";
         }
         else
         {
            *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                  lcts.externalGlobalBestDualBoundValue;
         }
         if(  paraInitiator->getGap( lcts.globalBestDualBoundValue ) > displayInfOverThisValue )
         {
            *osTabularSolvingStatus << std::setw(10) << std::right << "-";
         }
         else
         {
            *osTabularSolvingStatus << std::setw(10) << std::right << std::setprecision(2) <<
                  paraInitiator->getGap( lcts.globalBestDualBoundValue ) * 100 << "%";
         }
      }
      else
      {
         if( !paraNodeToKeepCheckpointFileNodes &&
               ( paraSolverPool->getNumActiveSolvers() == 0 &&
               ( paraSolverPool->getNnodesSolvedInSolvers() == 0 || paraNodePool->getNumOfNodes() == 0 )
               ) )
         {
            if( paraInitiator->getGlobalBestIncumbentSolution() )
            {
               lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
               /*
               *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                     paraInitiator->convertToExternalValue(
                     paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue());
               *osTabularSolvingStatus << std::setw(10) << std::right << std::setprecision(2) <<
                     paraInitiator->getGap( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() ) * 100 << "%";
                     */
               *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                     paraInitiator->convertToExternalValue( lcts.globalBestDualBoundValue );
               *osTabularSolvingStatus << std::setw(10) << std::right << std::setprecision(2) <<
                     paraInitiator->getGap( lcts.globalBestDualBoundValue ) * 100 << "%";
            }
            else
            {
               if( EPSEQ( lcts.globalBestDualBoundValue,-DBL_MAX, paraInitiator->getEpsilon() ))
               {
                  *osTabularSolvingStatus << std::setw(17) << std::right << "-";
               }
               else
               {
                  *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                        lcts.externalGlobalBestDualBoundValue;
               }
               if(  paraInitiator->getGap( lcts.globalBestDualBoundValue ) > displayInfOverThisValue )
               {  
                  *osTabularSolvingStatus << std::setw(10) << std::right << "-";
               }
               else
               {  
                  *osTabularSolvingStatus << std::setw(10) << std::right << std::setprecision(2) <<
                        paraInitiator->getGap( lcts.globalBestDualBoundValue ) * 100 << "%";
               }
            }
            // *osTabularSolvingStatus << std::setw(17) << std::right << "-";
         }
         else
         {
            //*osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
            //      lcts.externalGlobalBestDualBoundValue;
            if( EPSEQ( lcts.globalBestDualBoundValue,-DBL_MAX, paraInitiator->getEpsilon() ))
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << "-";
            }
            else
            {
               *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                     lcts.externalGlobalBestDualBoundValue;
            }
            if(  paraInitiator->getGap( lcts.globalBestDualBoundValue ) > displayInfOverThisValue )
            {  
               *osTabularSolvingStatus << std::setw(10) << std::right << "-";
            }
            else
            {  
               *osTabularSolvingStatus << std::setw(10) << std::right << std::setprecision(2) <<
                     paraInitiator->getGap( lcts.globalBestDualBoundValue ) * 100 << "%";
            }
         }
         /*
         if( !paraNodeToKeepCheckpointFileNodes &&
               ( !paraInitiator->getGlobalBestIncumbentSolution() ||
               paraInitiator->getGap(lcts.globalBestDualBoundValue) > displayInfOverThisValue ||
               ( paraSolverPool->isActive() && paraSolverPool->getNumActiveSolvers() == 0 && paraNodePool->getNumOfNodes() == 0 )
               ) )
         {
            *osTabularSolvingStatus << std::setw(10) << std::right << "-";
         }
         else
         {
            *osTabularSolvingStatus << std::setw(9) << std::right << std::setprecision(2) <<
                  paraInitiator->getGap(lcts.globalBestDualBoundValue) * 100 << "%";
         }
         */ 
      }

      if( paraSolverPool->getNumActiveSolvers() > 0 )
      {
         if( paraSolverPool->getGlobalBestDualBoundValue() >= -1e+10 )
         {
            *osTabularSolvingStatus << std::setw(17) << std::right << std::setprecision(4) <<
                  paraInitiator->convertToExternalValue( paraSolverPool->getGlobalBestDualBoundValue() );
         }
         else
         {
            *osTabularSolvingStatus << std::setw(17) << std::right << "-";
         }
         if(  paraInitiator->getGap( paraSolverPool->getGlobalBestDualBoundValue() ) > displayInfOverThisValue )
         {
            *osTabularSolvingStatus << std::setw(10) << std::right << "-";
         }
         else
         {
            *osTabularSolvingStatus << std::setw(10) << std::right << std::setprecision(2) <<
                  paraInitiator->getGap( paraSolverPool->getGlobalBestDualBoundValue() ) * 100 << "%";
         }
      }
      else
      {
         // *osTabularSolvingStatus << std::setw(17) << std::right << "-";
      }

   }
   *osTabularSolvingStatus << std::endl;
}

void
ParaLoadCoordinator::run(
      )
{

   int source;
   int tag;

   for(;;)
   {
      if( paraSolverPool->getNumActiveSolvers() == 0 )
      {
         if( paraNodePool->isEmpty() )                             // paraNodePool has to be checked
                                                                   // because node cannot send in a parameter settings
         {
            if( runningPhase != TerminationPhase )
            {
               /*
               if( !interruptedFromControlTerminal
                     && !computationIsInterrupted
                     && !hardTimeLimitIsReached
                     && paraInitiator->getGlobalBestIncumbentSolution() )
               {
                  lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
                  lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
               }
               */
               /* No active solver exists */
               terminateAllSolvers();
               runningPhase = TerminationPhase;
               /*
               if( !racingTermination )
               {
                  lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
                  lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
               }
               */
            }
            else // runningPhase == TerminationPhase
            {
               if( ( paraRacingSolverPool &&
                     // paraSolverPool->getNumInactiveSolvers() == (paraRacingSolverPool->getNumActiveSolvers() + nTerminated ) ) ||
                     // paraSolverPool->getNumInactiveSolvers() == nTerminated  )
                     paraSolverPool->getNSolvers() == (paraRacingSolverPool->getNumActiveSolvers() + nTerminated ) ) ||
                     paraSolverPool->getNSolvers() == nTerminated  )
               {
                  break;
               }
            }
         }
         else
         {
            if( initialNodesGenerated  )
            {
               if( runningPhase != TerminationPhase )
               {
                  lcts.globalBestDualBoundValue = std::max( paraNodePool->getBestDualBoundValue(), lcts.globalBestDualBoundValue );
                  lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
#ifdef UG_WITH_ZLIB
                  updateCheckpointFiles();
#endif
                  /* No active solver exists */
                  terminateAllSolvers();
                  runningPhase = TerminationPhase;
               }
               else  // runningPhase == TerminationPhase
               {
                  // if( paraSolverPool->getNumInactiveSolvers() == nTerminated  )
                  if( paraSolverPool->getNSolvers() == nTerminated  )
                  {
                     break;
                  }
               }
            }
         }
         if( ( paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) > 0.0 &&
               runningPhase == TerminationPhase ) )
         {
            break;
         }
      }

      if( paraParamSet->getIntParamValue(NIdleSolversToTerminate) > 0 &&
            firstCollectingModeState == 1 &&
            (signed)paraSolverPool->getNumInactiveSolvers() >=  paraParamSet->getIntParamValue(NIdleSolversToTerminate)
            )
      {
         if( runningPhase != TerminationPhase )
         {
            lcts.globalBestDualBoundValue = std::max( paraNodePool->getBestDualBoundValue(), lcts.globalBestDualBoundValue );
            lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
#ifdef UG_WITH_ZLIB
            updateCheckpointFiles();
#endif
            /* No active solver exists */
            terminateAllSolvers();
            std::cout << "### REACHED TO THE SPECIFIED NUMBER OF IDLER SOLVERS, then EXIT ###" << std::endl;
            exit(1);  // try to terminate all solvers, but do not have to wait until all solvers have terminated.
                      // Basically, this procedure is to kill the ug[*,*].
         } // if already in TerminaitonPhase, just keep on running.
      }

      if( !paraRacingSolverPool && paraSolverPool->getNumActiveSolvers() == 0 &&
               paraParamSet->getRealParamValue(TimeLimit) > 0.0 &&
               paraTimer->getElapsedTime() > paraParamSet->getRealParamValue(TimeLimit) )
      {
         hardTimeLimitIsReached = true;
         break;
      }

      if( racingTermination && !paraRacingSolverPool && paraSolverPool->getNumActiveSolvers() == 0 && paraNodePool->getNumOfNodes() == 1 )
      {
         /*
          * special timining problem
          *
          * 1113.58 S.4 I.SOL 0
          * 1113.58 S.3 is the racing winner! Selected strategy 2.
          * 1113.58 S.4 >(TERMINATED_IN_RACING_STAGE)
          *
          */
         break;
      }

      /*******************************************
       *  waiting for any message form anywhere  *
       *******************************************/
      double inIdleTime = paraTimer->getElapsedTime();
      paraComm->probe(&source, &tag);
      lcts.idleTime += ( paraTimer->getElapsedTime() - inIdleTime );
      if( messageHandler[tag] )
      {
         int status = (this->*messageHandler[tag])(source, tag);
         if( status )
         {
            std::ostringstream s;
            s << "[ERROR RETURN form Message Hander]:" <<  __FILE__ <<  "] func = "
              << __func__ << ", line = " << __LINE__ << " - "
              << "process tag = " << tag << std::endl;
            abort();
         }
      }
      else
      {
         THROW_LOGICAL_ERROR3( "No message hander for ", tag, " is not registered" );
      }

#ifdef UG_WITH_UGS
      if( commUgs ) checkAndReadIncumbent();
#endif

      /** completion message may delay */
      if( paraRacingSolverPool && paraRacingSolverPool->getNumActiveSolvers() == 0 )
      {
         delete paraRacingSolverPool;
         paraRacingSolverPool = 0;
         if( racingTermination )
         {
            break;
         }
      }

      /** output tabular solving status */
      if( outputTabularSolvingStatusFlag &&
            paraSolverPool->getNumActiveSolvers() != 0 &&
            ( ( ( paraParamSet->getBoolParamValue(Deterministic) &&
                  paraParamSet->getBoolParamValue(DeterministicTabularSolvingStatus) ) &&
                  ( paraDetTimer->getElapsedTime() - previousTabularOutputTime ) >
                                 paraParamSet->getRealParamValue(TabularSolvingStatusInterval) ) ||
            ( ( !paraParamSet->getBoolParamValue(Deterministic) ||
                  !paraParamSet->getBoolParamValue(DeterministicTabularSolvingStatus) ) &&
                  ( paraTimer->getElapsedTime() - previousTabularOutputTime ) >
               paraParamSet->getRealParamValue(TabularSolvingStatusInterval) ) ) )
      {
         outputTabularSolvingStatus(' ');
         if( paraParamSet->getBoolParamValue(Deterministic) )
         {
            if( paraParamSet->getBoolParamValue(DeterministicTabularSolvingStatus) )
            {
               previousTabularOutputTime = paraDetTimer->getElapsedTime();
            }
            else
            {
               previousTabularOutputTime = paraTimer->getElapsedTime();
            }
         }
         else
         {
            previousTabularOutputTime = paraTimer->getElapsedTime();
         }
      }

      switch ( runningPhase )
      {
      case RampUpPhase:
      {
         if( racingTermination ||
               (  paraParamSet->getRealParamValue(TimeLimit) > 0.0 &&
                 paraTimer->getElapsedTime() > paraParamSet->getRealParamValue(TimeLimit) ) )
         {
            sendInterruptRequest();
            runningPhase = TerminationPhase;
         }
         else
         {
            if( paraSolverPool->getNumInactiveSolvers() == 0 )
            {
               // without consideration of keeping nodes in checkpoint file
               double globalBestDualBoundValueLocal =
                  std::max (
                        std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
                        lcts.globalBestDualBoundValue );
               if( paraParamSet->getBoolParamValue(DualBoundGainTest) )
               {
                  if( paraSolverPool->isActive() &&
                       ( paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
                                       > paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraParamSet->getRealParamValue(MultiplierForCollectingMode) ||
                        ( paraNodePool->getNumOfNodes()
                                       > paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraParamSet->getRealParamValue(MultiplierForCollectingMode)*2  &&
                                       paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal ) > 0  ) ) )
                  {
                     sendRampUpToAllSolvers();
                     runningPhase = NormalRunningPhase;
                  }
               }
               else
               {
                  if( paraSolverPool->isActive() &&
                       ( (signed)paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
                                       > paraParamSet->getIntParamValue(NChangeIntoCollectingMode) ||
                        ( (signed)paraNodePool->getNumOfNodes()
                                       > paraParamSet->getIntParamValue(NChangeIntoCollectingMode) * 2  &&
                                       paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal ) > 0  ) ) )
                  {
                     sendRampUpToAllSolvers();
                     runningPhase = NormalRunningPhase;
                  }
               }
            }
            else
            {
               if( winnerSolverNodesCollected && ( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 1 || paraParamSet->getIntParamValue(RampUpPhaseProcess) == 2 ) )
               {
                  // maybe some solver hard to interrupt in a large scale execution
                  // without consideration of keeping nodes in checkpoint file
                  double globalBestDualBoundValueLocal =
                     std::max (
                           std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
                           lcts.globalBestDualBoundValue );
                  if( paraSolverPool->isActive() &&
                        ( paraNodePool->getNumOfNodes()
                                       > paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraParamSet->getRealParamValue(MultiplierForCollectingMode)*2  &&
                                       paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal ) > 0  ) )
                  {
                     sendRampUpToAllSolvers();
                     runningPhase = NormalRunningPhase;
                  }
               }
            }
            (void) sendParaNodesToIdleSolvers();
         }
         break;
      }
      case NormalRunningPhase:
      {
         if( racingTermination ||
             (  paraParamSet->getRealParamValue(TimeLimit) > 0.0 &&
               paraTimer->getElapsedTime() > paraParamSet->getRealParamValue(TimeLimit) ) ||
               ( paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) > 0.0 &&
               paraTimer->getElapsedTime() > paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) ) )
         {
            sendInterruptRequest();
            runningPhase = TerminationPhase;
         }
         else
         {
            (void) sendParaNodesToIdleSolvers();
         }
         if( isCollectingModeRestarted && paraNodePool->isEmpty() &&
               ( (!paraRacingSolverPool) || ( paraRacingSolverPool && paraRacingSolverPool->getWinner() > 0 ) ) )
         {
            if( ( paraParamSet->getBoolParamValue(Deterministic) &&
                  ( paraDetTimer->getElapsedTime() - statEmptyNodePoolTime ) > (paraParamSet->getRealParamValue(TimeToIncreaseCMS)*2) ) ||
                  ( !paraParamSet->getBoolParamValue(Deterministic) &&
                  ( paraTimer->getElapsedTime() - statEmptyNodePoolTime ) > (paraParamSet->getRealParamValue(TimeToIncreaseCMS)*2) ) )
            {
               sendRetryRampUpToAllSolvers();
               runningPhase = RampUpPhase;
            }
         }

#ifdef UG_WITH_ZLIB
         if( paraParamSet->getRealParamValue(RestartInRampDownThresholdTime) > 0.0 )
         {
            if( paraSolverPool->getNumActiveSolvers()
                  < paraSolverPool->getNSolvers()*paraParamSet->getRealParamValue(RestartInRampDownActiveSolverRatio) )
            {
               if( starvingTime < 0.0 )
               {
                  starvingTime = paraTimer->getElapsedTime();
               }
            }
            else
            {
               starvingTime = -1.0;
            }
            // std::cout << "active solvers:" << paraSolverPool->getNumActiveSolvers() << std::endl;
            if( starvingTime > 0 &&
                  ( paraTimer->getElapsedTime() - starvingTime )
                  > paraParamSet->getRealParamValue(RestartInRampDownThresholdTime)  &&
                  ( paraSolverPool->getNnodesInSolvers() + paraNodePool->getNumOfNodes() )
                  > paraSolverPool->getNumActiveSolvers()*10 )
            {
               hugeImbalance = false;
               restartInRampDownPhase();
            }
         }
#endif

         if( paraParamSet->getRealParamValue(HugeImbalanceThresholdTime) > 0.0 )
         {
            if( paraSolverPool->getNumActiveSolvers()
                  < paraSolverPool->getNSolvers()*paraParamSet->getRealParamValue(HugeImbalanceActiveSolverRatio) )
            {
               if( hugeImbalanceTime < 0.0 )
               {
                  changeSearchStrategyOfAllSolversToBestBoundSearch();
                  hugeImbalanceTime = paraTimer->getElapsedTime();
               }
            }
            else
            {
               changeSearchStrategyOfAllSolversToOriginalSearch();
               hugeImbalanceTime = -1.0;
            }
            // std::cout << "active solvers:" << paraSolverPool->getNumActiveSolvers() << std::endl;
            if( hugeImbalanceTime > 0 &&
                  ( paraTimer->getElapsedTime() - hugeImbalanceTime )
                  > paraParamSet->getRealParamValue(HugeImbalanceThresholdTime)  &&
                  paraSolverPool->getNnodesInSolvers() > paraSolverPool->getNumActiveSolvers()*100 )
            {
               hugeImbalance = true;
               if( !paraNodePoolBufferToRestart )
               {
                  paraNodePoolBufferToRestart =  new ParaNodePoolForMinimization(paraParamSet->getRealParamValue(BgapCollectingMode));
               }
               // reschedule collecting mode
               double tempTime = paraSolverPool->getSwichOutTime();
               paraSolverPool->switchOutCollectingMode();
               paraSolverPool->setSwichOutTime(tempTime);
               paraSolverPool->switchInCollectingMode(paraNodePool);
            }
            if( hugeImbalance &&
                  ( paraNodePoolBufferToRestart->getNumOfNodes() > paraSolverPool->getNSolvers() ||
                    paraSolverPool->getNnodesInSolvers() < paraSolverPool->getNumActiveSolvers() * 5 ||
                        paraSolverPool->getNumActiveSolvers() == 0 ) )
            {
               hugeImbalance = false;
               hugeImbalanceTime = -1.0;
               while( !paraNodePoolBufferToRestart->isEmpty() )
               {
                  paraNodePool->insert(paraNodePoolBufferToRestart->extractNode());
               }
               (void) sendParaNodesToIdleSolvers();
               paraSolverPool->setSwichOutTime(-1.0); // restart collecting
            }
         }

         break;
      }
      case TerminationPhase:
      {
         break;
      }
      default:
      {
         THROW_LOGICAL_ERROR2( "Undefined running phase: ", static_cast<int>(runningPhase) );
      }
      }
#ifdef UG_WITH_ZLIB
      if( paraParamSet->getBoolParamValue(Checkpoint) &&
            ( paraTimer->getElapsedTime() - previousCheckpointTime )
            > paraParamSet->getRealParamValue(CheckpointInterval) )
      {
         if( !( interruptIsRequested &&
               paraParamSet->getRealParamValue(UG::FinalCheckpointGeneratingTime) > 0.0 ) )
         {
            updateCheckpointFiles();
            previousCheckpointTime = paraTimer->getElapsedTime();
         }
      }
#endif
   }
}

#ifdef UG_WITH_ZLIB
void
ParaLoadCoordinator::restartInRampDownPhase(
      )
{
   updateCheckpointFiles();
   /** send interrupt request */
   int stayAlive = 0;   // exit!
   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      PARA_COMM_CALL(
            paraComm->send( &stayAlive, 1, ParaINT, i, TagInterruptRequest )
      );
   }

   /** the pupose of the updateCheckpoitFiles is two
    *  1. Can be killed during restaart, for example, in a case that a solver cannot be intterrupted so long time
    *  2. To update initial dual bound values
    */
   // updateCheckpointFiles();

   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << paraTimer->getElapsedTime()
      << " Interrupt all solvers to restart"
      << std::endl;
      if( outputTabularSolvingStatusFlag )
      {
         *osTabularSolvingStatus <<
               "Interrupt all solvers to restart after "
               << paraTimer->getElapsedTime() << " seconds." << std::endl;
      }
   }

   exit(1);   // Terminate LoadCoordinator. Restart did not work well over 10,000 solvers.

   paraNodePoolToRestart =  new ParaNodePoolForMinimization(paraParamSet->getRealParamValue(BgapCollectingMode));

   /** for a timing issue, paraNodePool may not be empty. paraNodes in the pool should be just recived,
    * because paraSolverPool was empty. Then, no check for the ancestors.
    */
   while( !paraNodePool->isEmpty() )
   {
      ParaNode *node = paraNodePool->extractNode();
      delete node;
   }
   while( !paraNodePoolBufferToRestart->isEmpty() )
   {
      ParaNode *node = paraNodePoolBufferToRestart->extractNode();
      delete node;
   }

   /*******************************************
    *  waiting for any message form anywhere  *
    *******************************************/
   for(;;)
   {
      int source;
      int tag;
      double inIdleTime = paraTimer->getElapsedTime();
      paraComm->probe(&source, &tag);
      lcts.idleTime += ( paraTimer->getElapsedTime() - inIdleTime );
      if( messageHandler[tag] )
      {
         int status = (this->*messageHandler[tag])(source, tag);
         if( status )
         {
            std::ostringstream s;
            s << "[ERROR RETURN form Message Hander]:" <<  __FILE__ <<  "] func = "
              << __func__ << ", line = " << __LINE__ << " - "
              << "process tag = " << tag << std::endl;
            abort();
         }
      }
      else
      {
         THROW_LOGICAL_ERROR3( "No message hander for ", tag, " is not registered" );
      }

#ifdef UG_WITH_UGS
      if( commUgs ) checkAndReadIncumbent();
#endif

      if( paraSolverPool->getNumActiveSolvers() == 0 )
      {
         break;
      }

   }

   if( !paraNodePool->isEmpty() )
   {
      std::cout << "Logical error occurred during restart in ramp-down phase." << std::endl;
      std::cout << "You can restart from the chakepoint file." << std::endl;
      // exit(1);
      abort();
   }

   while( !paraNodePoolToRestart->isEmpty() )
   {
      ParaNode *node = paraNodePoolToRestart->extractNode();
      node->setDualBoundValue(node->getInitialDualBoundValue());
      paraNodePool->insert(node);
   }

   delete paraNodePoolToRestart;
   paraNodePoolToRestart = 0;

   runningPhase = RampUpPhase;
   /** initialize paraSolerPool */
   paraSolverPool->reinitToRestart();

   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, i, TagRestart )
      );
   }

   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << paraTimer->getElapsedTime()
      << " Restart"
      << std::endl;
      if( outputTabularSolvingStatusFlag )
      {
         *osTabularSolvingStatus <<
               "Restart after "
               << paraTimer->getElapsedTime() << " seconds." << std::endl;
      }
   }

   (void) sendParaNodesToIdleSolvers();
}
#endif 

bool
ParaLoadCoordinator::sendParaNodesToIdleSolvers(
      )
{
   if( merging || initialNodesGenerated ||
       runningPhase == TerminationPhase || hugeImbalance ||
         ( !restarted &&
            // paraParamSet->getBoolParamValue(RacingStatBranching) &&
            ( !winnerSolverNodesCollected ||
                  ( paraRacingSolverPool &&
                        paraRacingSolverPool->getNumInactiveSolvers() < paraRacingSolverPool->getNumActiveSolvers() )
            )
         )
       )
   {
      return false;
   }

   bool sentNode = false;
   while( paraSolverPool->getNumInactiveSolvers() > 0 && !paraNodePool->isEmpty() )
   {
      ParaNode *paraNode = 0;
      while( !paraNodePool->isEmpty() )
      {
         if( nNormalSelection >= 0 && !warmStartNodeTransferring )
         {
            if( nNormalSelection > static_cast<int>( 1.0 / paraParamSet->getRealParamValue(RandomNodeSelectionRatio) ) )
            {
               paraNode = paraNodePool->extractNodeRandomly();
            }
            else
            {
               paraNode = paraNodePool->extractNode();
            }
         }
         else
         {
            paraNode = paraNodePool->extractNode();
         }
         if( !paraNode ) break;
         assert( !paraNode->getMergeNodeInfo() ||
               ( paraNode->getMergeNodeInfo() &&
               paraNode->getMergeNodeInfo()->status == ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE &&
               paraNode->getMergeNodeInfo()->mergedTo == 0 ) );

         if( paraParamSet->getBoolParamValue(UG::GenerateReducedCheckpointFiles) &&
               ( ( !paraNode->getMergeNodeInfo() ) ||
                 ( paraNode->getMergeNodeInfo() &&
                       paraNode->getMergeNodeInfo()->nMergedNodes <= 0 ) ) )
         {
            assert( !paraNode->getMergeNodeInfo() || ( paraNode->getMergeNodeInfo() &&  paraNode->getMergeNodeInfo()->nMergedNodes == 0 ) );
            if( paraNode->getMergeNodeInfo() )
            {
               ParaMergeNodeInfo *mNode = paraNode->getMergeNodeInfo();
               if( mNode->origDiffSubproblem )
               {
                  paraNode->setDiffSubproblem(mNode->origDiffSubproblem);
                  delete mNode->mergedDiffSubproblem;
                  mNode->mergedDiffSubproblem = 0;
                  mNode->origDiffSubproblem = 0;
               }
               paraNode->setMergeNodeInfo(0);
               paraNode->setMergingStatus(-1);
               deleteMergeNodeInfo(mNode);
            }
            paraNodePoolBufferToGenerateCPF->insert(paraNode);
            /*
            if( logSolvingStatusFlag )
            {
               *osLogSolvingStatus << paraTimer->getElapsedTime()
               << " node saved to the buffer. Dual bound:"
               << paraInitiator->convertToExternalValue( paraNode->getDualBoundValue() ) << std::endl;
            }
            */
            paraNode = 0;

            continue;
         }

         if( ( paraInitiator->getGlobalBestIncumbentSolution() &&
               ( paraNode->getDualBoundValue() < paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() ||
                     ( paraInitiator->isObjIntegral() &&
                     static_cast<int>(ceil( paraNode->getDualBoundValue() ) )
                     < static_cast<int>(paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() ) )
                     ) ) ||
               !( paraInitiator->getGlobalBestIncumbentSolution() ) )
         {
            if( paraInitiator->getAbsgap(paraNode->getDualBoundValue() ) > paraInitiator->getAbsgapValue() ||
                  paraInitiator->getGap(paraNode->getDualBoundValue()) > paraInitiator->getGapValue() )
            {
               break;
            }
            else
            {
#ifdef UG_DEBUG_SOLUTION
               if( paraNode->getDiffSubproblem() && paraNode->getDiffSubproblem()->isOptimalSolIncluded() )
               {
                  throw "Optimal solution going to be killed.";
               }
#endif
               delete paraNode;
               paraNode = 0;
               lcts.nDeletedInLc++;
               if( nNormalSelection >= 0 && !warmStartNodeTransferring )
               {
                  if( nNormalSelection > static_cast<int>( 1.0 / paraParamSet->getRealParamValue(RandomNodeSelectionRatio) ) )
                  {
                     nNormalSelection = 0;
                  }
                  else
                  {
                     nNormalSelection++;
                  }
               }
               /*
               std::cout << "dual bound = " << paraNode->getDualBoundValue() << std::endl;
               std::cout << "agap(dual bound) = " << paraInitiator->getAbsgap(paraNode->getDualBoundValue())
                         << ", agap = " << paraInitiator->getAbsgapValue() << std::endl;
               std::cout << "gap(dual bound) = " << paraInitiator->getGap(paraNode->getDualBoundValue())
                         << ", gap = " << paraInitiator->getGapValue() << std::endl;
               break;
               */
            }
         }
         else
         {
#ifdef UG_DEBUG_SOLUTION
            if( paraNode->getDiffSubproblem() && paraNode->getDiffSubproblem()->isOptimalSolIncluded() )
            {
               throw "Optimal solution going to be killed.";
            }
#endif
            delete paraNode;
            paraNode = 0;
            lcts.nDeletedInLc++;
         }
      }

      if( paraNode )
      {
         if( nNormalSelection >= 0 && !warmStartNodeTransferring )
         {
            if( nNormalSelection > static_cast<int>( 1.0 / paraParamSet->getRealParamValue(RandomNodeSelectionRatio) ) )
            {
               nNormalSelection = 0;
            }
            else
            {
               nNormalSelection++;
            }
         }

         if( paraNode->getMergeNodeInfo() && paraNode->getMergeNodeInfo()->nMergedNodes == 0 )
         {
            ParaMergeNodeInfo *mNode = paraNode->getMergeNodeInfo();
            paraNode->setDiffSubproblem(mNode->origDiffSubproblem);
            paraNode->setMergeNodeInfo(0);
            paraNode->setMergingStatus(-1);
            delete mNode->mergedDiffSubproblem;
            mNode->mergedDiffSubproblem = 0;
            mNode->origDiffSubproblem = 0;
            deleteMergeNodeInfo(mNode);
         }
         if( paraParamSet->getBoolParamValue(RacingStatBranching) &&
               paraNode->isSameParetntNodeSubtreeIdAs( NodeId() ) &&  //  if parent is the root node
               paraNode->getDiffSubproblem()                          //  paraNode deos not root
               )
         {
            paraInitiator->setInitialStatOnDiffSubproblem(
                  minDepthInWinnerSolverNodes, maxDepthInWinnerSolverNodes,
                  paraNode->getDiffSubproblem());
         }
         // without consideration of keeping nodes in checkpoint file
         double globalBestDualBoundValueLocal =
            std::max (
                  std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
                  lcts.globalBestDualBoundValue );
         int destination = paraSolverPool->activateSolver(paraNode,
               paraRacingSolverPool, (runningPhase==RampUpPhase),
               paraNodePool->getNumOfGoodNodes(globalBestDualBoundValueLocal), averageLastSeveralDualBoundGains );
         if( destination < 0 )
         {
            /** cannot activate */
            paraNodePool->insert(paraNode);
            return sentNode;
         }
         else
         {
            lcts.nSent++;
            writeTransferLog(destination);
            sentNode = true;
            if( runningPhase == RampUpPhase &&
                  paraParamSet->getIntParamValue(RampUpPhaseProcess) == 0 &&
                  paraParamSet->getBoolParamValue(CollectOnce) &&
                  // paraSolverPool->getNumActiveSolvers() < paraSolverPool->getNSolvers()/2
                  paraSolverPool->getNumInactiveSolvers() > 0 &&
                  paraSolverPool->getNumActiveSolvers()*2 <
                  ( paraSolverPool->getNSolvers() + paraParamSet->getIntParamValue(NChangeIntoCollectingMode) )
                  )
            {
               int nCollect = -1;
               PARA_COMM_CALL(
                     paraComm->send( &nCollect, 1, ParaINT, destination, TagCollectAllNodes )
               );
            }
            if( logSolvingStatusFlag )
            {
               *osLogSolvingStatus << paraTimer->getElapsedTime()
               << " S." << destination << " < "
               << paraInitiator->convertToExternalValue(
                     paraNode->getDualBoundValue() );
               if( paraInitiator->getGlobalBestIncumbentSolution() )
               {
                  if( paraInitiator->getGap(paraNode->getDualBoundValue()) > displayInfOverThisValue )
                  {
                     *osLogSolvingStatus << " ( Inf )";
                  }
                  else
                  {
                     *osLogSolvingStatus << " ( " << paraInitiator->getGap(paraNode->getDualBoundValue()) * 100 << "% )";
                  }
               }
               if( paraParamSet->getBoolParamValue(LightWeightRootNodeProcess) &&
                     runningPhase != RampUpPhase && (!paraRacingSolverPool) &&
                     paraSolverPool->getNumInactiveSolvers() >
                         ( paraSolverPool->getNSolvers() * paraParamSet->getRealParamValue(RatioToApplyLightWeightRootProcess) ) )
               {
                  *osLogSolvingStatus << " L";
               }
               if( paraNode->getMergeNodeInfo() )
               {
                  *osLogSolvingStatus << " M(" << paraNode->getMergeNodeInfo()->nMergedNodes + 1 << ")";
                  if( paraNode->getMergeNodeInfo()->nMergedNodes < 1 )
                  {
                     std::cout << "node id = " << (paraNode->getNodeId()).toString() << std::endl;
                     abort();
                  }
               }
               if( paraNode->getDiffSubproblem() )
               {
                  *osLogSolvingStatus << " " << paraNode->toSimpleString() << ", " << paraNode->getDiffSubproblem()->getNBoundChanges();
                  *osLogSolvingStatus << paraNode->getDiffSubproblem()->toStringStat();
               }
               // for debug
               // *osLogSolvingStatus << " " << paraNode->toSimpleString();
               *osLogSolvingStatus << std::endl;
            }
#ifdef _DEBUG_LB
            std::cout << paraTimer->getElapsedTime()
            << " S." << destination << " < "
            << paraInitiator->convertToExternalValue(
                  paraNode->getDualBoundValue() );
            if( paraInitiator->getGlobalBestIncumbentSolution() )
            {
               if( paraInitiator->getGap(paraNode->getDualBoundValue()) > displayInfOverThisValue )
               {
                  std::cout << " ( Inf )";
               }
               else
               {
                  std::cout << " ( " << paraInitiator->getGap(paraNode->getDualBoundValue()) * 100 << "% )";
               }
            }
            if( paraParamSet->getBoolParamValue(LightWeightRootNodeProcess) &&
                  runningPhase != RampUpPhase && (!paraRacingSolverPool) &&
                  paraSolverPool->getNumInactiveSolvers() >
                     ( paraSolverPool->getNSolvers() * paraParams->getRealParamValue(RatioToApplyLightWeightRootProcess) ) )
            {
               std::cout << " L";
            }
            std::cout << std::endl;
#endif
         }
      }
      else
      {
         break;
      }
   }
   return sentNode;
}

#ifdef UG_WITH_ZLIB
void
ParaLoadCoordinator::updateCheckpointFiles(
      )
{
   time_t timer;
   char timeStr[30];

   if( paraNodePoolToRestart )
   {
      return;   // Interrupting all solvers;
   }

   if( paraNodePoolBufferToGenerateCPF &&  paraNodePoolBufferToGenerateCPF->getNumOfNodes() > 0 )
   {
      return;   // Collecting nodes.
   }

   /** get checkpoint time */
   time(&timer);
   /** make checkpoint time string */
#ifdef _MSC_VER
   int bufsize = 256;
   ctime_s(timeStr, bufsize, &timer);
#else
   ctime_r(&timer, timeStr);
#endif
   for( int i = 0; timeStr[i] != '\0' && i < 26; i++ )
   {
      if( timeStr[i] == ' ') timeStr[i] = '_';
      if( timeStr[i] == '\n' ) timeStr[i] = '\0';
   }
   char *newCheckpointTimeStr = &timeStr[4];    // remove a day of the week
   // std::cout << "lstCheckpointTimeStr = " << lastCheckpointTimeStr << std::endl;
   // std::cout << "newCheckpointTimeStr = " << newCheckpointTimeStr << std::endl;
   if( strcmp(newCheckpointTimeStr,lastCheckpointTimeStr) == 0 )
   {
      int l = strlen(newCheckpointTimeStr);
      newCheckpointTimeStr[l] = 'a';
      newCheckpointTimeStr[l+1] = '\0';
   }

   /** save nodes information */
   char nodesFileName[256];
   sprintf(nodesFileName,"%s%s_%s_nodes_LC%d.gz",
         paraParamSet->getStringParamValue(CheckpointFilePath),
         paraInitiator->getParaInstance()->getProbName(),newCheckpointTimeStr, paraComm->getRank());
   gzstream::ogzstream checkpointNodesStream;
   checkpointNodesStream.open(nodesFileName, std::ios::out | std::ios::binary);
   if( !checkpointNodesStream )
   {
      std::cout << "Checkpoint file for ParaNodes cannot open. file name = " << nodesFileName << std::endl;
      exit(1);
   }
   paraSolverPool->updateDualBoundsForSavingNodes();
   paraNodePool->updateDualBoundsForSavingNodes();
   int n = 0;
   if( paraNodeToKeepCheckpointFileNodes )
   {
      n += paraNodeToKeepCheckpointFileNodes->writeParaNodesToCheckpointFile(checkpointNodesStream);
   }
   n += paraSolverPool->writeParaNodesToCheckpointFile(checkpointNodesStream);
   n += paraNodePool->writeParaNodesToCheckpointFile(checkpointNodesStream);
   checkpointNodesStream.close();
   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << paraTimer->getElapsedTime()
      << " Checkpoint: " << n << " ParaNodes were saved" <<  std::endl;
   }
#ifdef _DEBUG_LB
   std::cout << paraTimer->getElapsedTime()
   << " Checkpoint: " << n << " ParaNodes were saved" <<  std::endl;
#endif

   if( outputTabularSolvingStatusFlag )
   {
      *osTabularSolvingStatus <<
            "Storing check-point data after " <<
            paraTimer->getElapsedTime() << " seconds. " <<
            n << " nodes were saved." << std::endl;
   }

   /** save incumbent solution */
   char solutionFileName[256];
   if( paraComm->getRank() == 0 )
   {
      sprintf(solutionFileName,"%s%s_%s_solution.gz",
            paraParamSet->getStringParamValue(CheckpointFilePath),
            paraInitiator->getParaInstance()->getProbName(),newCheckpointTimeStr);
      paraInitiator->writeCheckpointSolution(std::string(solutionFileName));
   }

   /** save Solver statistics */
   char solverStatisticsFileName[256];
   sprintf(solverStatisticsFileName,"%s%s_%s_solverStatistics_LC%d.gz",
         paraParamSet->getStringParamValue(CheckpointFilePath),
         paraInitiator->getParaInstance()->getProbName(),newCheckpointTimeStr, paraComm->getRank());
   gzstream::ogzstream checkpointSolverStatisticsStream;
   checkpointSolverStatisticsStream.open(solverStatisticsFileName, std::ios::out | std::ios::binary);
   if( !checkpointSolverStatisticsStream )
   {
      std::cout << "Checkpoint file for SolverStatistics cannot open. file name = " << solverStatisticsFileName << std::endl;
      exit(1);
   }
   int nSolverInfo = paraSolverPool->writeSolverStatisticsToCheckpointFile(checkpointSolverStatisticsStream);
   checkpointSolverStatisticsStream.close();

   /** save LoadCoordinator statistics */
   char loadCoordinatorStatisticsFileName[256];
   sprintf(loadCoordinatorStatisticsFileName,"%s%s_%s_loadCoordinatorStatistics_LC%d.gz",
         paraParamSet->getStringParamValue(CheckpointFilePath),
         paraInitiator->getParaInstance()->getProbName(),newCheckpointTimeStr, paraComm->getRank());
   gzstream::ogzstream loadCoordinatorStatisticsStream;
   loadCoordinatorStatisticsStream.open(loadCoordinatorStatisticsFileName, std::ios::out | std::ios::binary);
   if( !loadCoordinatorStatisticsStream )
   {
      std::cout << "Checkpoint file for SolverStatistics cannot open. file name = " << loadCoordinatorStatisticsFileName << std::endl;
      exit(1);
   }
   // double globalBestDualBoundValue =
   //   std::max (
   //      std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
   //      lcts.globalBestDualBoundValue );
   // double externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(globalBestDualBoundValue);
   writeLoadCoordinatorStatisticsToCheckpointFile(loadCoordinatorStatisticsStream, nSolverInfo,
         lcts.globalBestDualBoundValue, lcts.externalGlobalBestDualBoundValue );
   loadCoordinatorStatisticsStream.close();

   if( lastCheckpointTimeStr[0] == ' ' )
   {
      /** the first time for checkpointing */
      if( racingWinnerParams )
      {
         /** save racing winner params */
         char racingWinnerParamsName[256];
         sprintf(racingWinnerParamsName,"%s%s_racing_winner_params.gz",
               paraParamSet->getStringParamValue(CheckpointFilePath),
               paraInitiator->getParaInstance()->getProbName());
         gzstream::ogzstream racingWinnerParamsStream;
         racingWinnerParamsStream.open(racingWinnerParamsName, std::ios::out | std::ios::binary);
         if( !racingWinnerParamsStream )
         {
            std::cout << "Racing winner parameter file cannot open. file name = " << racingWinnerParamsName << std::endl;
            exit(1);
         }
         racingWinnerParams->write(racingWinnerParamsStream);
         racingWinnerParamsStream.close();
      }
   }
   else
   {
      /** remove old check point files */
      sprintf(nodesFileName,"%s%s_%s_nodes_LC%d.gz",
            paraParamSet->getStringParamValue(CheckpointFilePath),
            paraInitiator->getParaInstance()->getProbName(),lastCheckpointTimeStr, paraComm->getRank());
      if( paraComm->getRank() == 0 )
      {
         sprintf(solutionFileName,"%s%s_%s_solution.gz",
               paraParamSet->getStringParamValue(CheckpointFilePath),
               paraInitiator->getParaInstance()->getProbName(),lastCheckpointTimeStr);
      }
      sprintf(solverStatisticsFileName,"%s%s_%s_solverStatistics_LC%d.gz",
            paraParamSet->getStringParamValue(CheckpointFilePath),
            paraInitiator->getParaInstance()->getProbName(),lastCheckpointTimeStr, paraComm->getRank());
      sprintf(loadCoordinatorStatisticsFileName,"%s%s_%s_loadCoordinatorStatistics_LC%d.gz",
            paraParamSet->getStringParamValue(CheckpointFilePath),
            paraInitiator->getParaInstance()->getProbName(),lastCheckpointTimeStr, paraComm->getRank());
      if( remove(nodesFileName) )
      {
         std::cout << "checkpoint nodes file cannot be removed: errno = " << strerror(errno) << std::endl;
         exit(1);
      }
      if ( remove(solutionFileName) )
      {
         std::cout << "checkpoint solution file cannot be removed: errno = " << strerror(errno) << std::endl;
         exit(1);
      }
      if ( remove(solverStatisticsFileName) )
      {
         std::cout << "checkpoint SolverStatistics file cannot be removed: errno = " << strerror(errno) << std::endl;
         exit(1);
      }
      if ( remove(loadCoordinatorStatisticsFileName) )
      {
         std::cout << "checkpoint LoadCoordinatorStatistics file cannot be removed: errno = " << strerror(errno) << std::endl;
         exit(1);
      }
   }

   char afterCheckpointingSolutionFileName[256];
   sprintf(afterCheckpointingSolutionFileName,"%s%s_after_checkpointing_solution.gz",
         paraParamSet->getStringParamValue(CheckpointFilePath),
         paraInitiator->getParaInstance()->getProbName() );
   gzstream::igzstream afterCheckpointingSolutionStream;
   afterCheckpointingSolutionStream.open(afterCheckpointingSolutionFileName, std::ios::in | std::ios::binary);
   if( afterCheckpointingSolutionStream  )
   {
      /** afater checkpointing solution file exists */
      afterCheckpointingSolutionStream.close();
      if ( remove(afterCheckpointingSolutionFileName) )
      {
         std::cout << "after checkpointing solution file cannot be removed: errno = " << strerror(errno) << std::endl;
         exit(1);
      }
   }

   /** update last checkpoint time string */
   strcpy(lastCheckpointTimeStr,newCheckpointTimeStr);
}

void
ParaLoadCoordinator::writeLoadCoordinatorStatisticsToCheckpointFile(
      gzstream::ogzstream &loadCoordinatorStatisticsStream,
      int nSolverInfo,
      double globalBestDualBoundValue,
      double externalGlobalBestDualBoundValue
      )
{
   loadCoordinatorStatisticsStream.write((char *)&nSolverInfo, sizeof(int));
   lcts.isCheckpointState = true;
   if( paraNodeToKeepCheckpointFileNodes )
   {
      lcts.nMaxUsageOfNodePool = paraNodePool->getMaxUsageOfPool() + paraNodeToKeepCheckpointFileNodes->getNumOfNodes();
      lcts.nNodesInNodePool = paraNodePool->getNumOfNodes() + paraNodeToKeepCheckpointFileNodes->getNumOfNodes();
   }
   else
   {
      lcts.nMaxUsageOfNodePool = paraNodePool->getMaxUsageOfPool();
      lcts.nNodesInNodePool = paraNodePool->getNumOfNodes();
   }
   lcts.nNodesLeftInAllSolvers = paraSolverPool->getNnodesInSolvers();
   lcts.globalBestDualBoundValue = std::max( globalBestDualBoundValue, lcts.globalBestDualBoundValue );
   lcts.externalGlobalBestDualBoundValue = externalGlobalBestDualBoundValue;
   lcts.runningTime = paraTimer->getElapsedTime();
   lcts.write(loadCoordinatorStatisticsStream);
}

void
ParaLoadCoordinator::warmStart(
      )
{
   restarted = true;
   /** write previous statistics information */
   writePreviousStatisticsInformation();

   /** try to read racing winner params */
   char racingWinnerParamsName[256];
   sprintf(racingWinnerParamsName,"%s%s_racing_winner_params.gz",
         paraParamSet->getStringParamValue(CheckpointFilePath),
         paraInitiator->getParaInstance()->getProbName());
   gzstream::igzstream racingWinnerParamsStream;
   racingWinnerParamsStream.open(racingWinnerParamsName, std::ios::in | std::ios::binary);
   if( racingWinnerParamsStream )
   {
      assert(!racingWinnerParams);
      racingWinnerParams = paraComm->createParaRacingRampUpParamSet();
      racingWinnerParams->read(paraComm, racingWinnerParamsStream);
      racingWinnerParamsStream.close();
      for( int i = 1; i < paraComm->getSize(); i++ )
      {
         /** send racing winner params: NOTE: should not broadcast. if we do it, solver routine need to recognize staring process */
         PARA_COMM_CALL(
               racingWinnerParams->send(paraComm, i)
         );
      }
   }

   /** set solution and get internal incumbent value */
   char afterCheckpointingSolutionFileName[256];
   sprintf(afterCheckpointingSolutionFileName,"%s%s_after_checkpointing_solution.gz",
         paraParamSet->getStringParamValue(CheckpointFilePath),
         paraInitiator->getParaInstance()->getProbName() );
   double incumbentValue = paraInitiator->readSolutionFromCheckpointFile(afterCheckpointingSolutionFileName);
   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      /** send internal incumbent value */
      PARA_COMM_CALL(
            paraComm->send( &incumbentValue, 1, ParaDOUBLE, i, TagIncumbentValue )
      );
      if( paraParamSet->getBoolParamValue(DistributeBestPrimalSolution) && paraInitiator->getGlobalBestIncumbentSolution() )
      {
         paraInitiator->getGlobalBestIncumbentSolution()->send(paraComm, i);
      }
      /** send internal global dual bound value */
      PARA_COMM_CALL(
            paraComm->send( &lcts.globalBestDualBoundValue, 1, ParaDOUBLE, i, TagGlobalBestDualBoundValueAtWarmStart )
      );
   }
   if( !paraParamSet->getBoolParamValue(Quiet) )
   {
      paraInitiator->writeSolution("[Warm started from "+std::string(paraInitiator->getPrefixWarm())+" : the solution from the checkpoint file]");
   }

   int n = 0;
   ParaNode *paraNode;
   bool onlyBoundChanges = false;
   if( !paraParamSet->getBoolParamValue(TransferLocalCuts) && !paraParamSet->getBoolParamValue(TransferConflictCuts) )
   {
      onlyBoundChanges = true;
   }
   if( paraParamSet->getBoolParamValue(MergeNodesAtRestart) )
   {
      initMergeNodesStructs();
   }

   ParaNodePoolForMinimization tempParaNodePool(paraParamSet->getRealParamValue(BgapCollectingMode));

   if( paraParamSet->getIntParamValue(AddDualBoundCons) == 3 )
   {
      /** NOT implemented */
      for(;;)
      {
         paraNode = paraInitiator->readParaNodeFromCheckpointFile(onlyBoundChanges);
         //       paraParamSet->getBoolParamValue(MergingNodeStatusInCheckpointFile) );
         if( paraNode == 0 )
            break;
         n++;
         paraNode->setDualBoundValue(paraNode->getInitialDualBoundValue());
         paraNodePool->insert(paraNode);   /** in order to sort ParaNodes, insert paraNodePool once */
         if( paraParamSet->getBoolParamValue(MergeNodesAtRestart) )
         {
            addNodeToMergeNodeStructs(paraNode);
         }
      }
   }
   else
   {
      for(;;)
      {
         paraNode = paraInitiator->readParaNodeFromCheckpointFile(onlyBoundChanges);
         //      paraParamSet->getBoolParamValue(MergingNodeStatusInCheckpointFile) );
         if( paraNode == 0 )
            break;
#ifdef UG_DEBUG_SOLUTION
#ifdef UG_DEBUG_SOLUTION_OPT_PATH
         if( paraNode->getDiffSubproblem() && (!paraNode->getDiffSubproblem()->isOptimalSolIncluded()) )
         {
            delete paraNode;
            paraNode = 0;
            continue;
         }
#endif
#endif 
         n++;
         paraNode->setDualBoundValue(paraNode->getInitialDualBoundValue());
         // paraNodePool->insert(paraNode);   /** in order to sort ParaNodes, insert paraNodePool once */
         if( paraParamSet->getBoolParamValue(MergeNodesAtRestart) || paraParamSet->getIntParamValue(NNodesToKeepInCheckpointFile) )
         {
            tempParaNodePool.insert(paraNode);  /** in order to sort ParaNodes with new dual value, insert it to tempParaNodePool once */
            // addNodeToMergeNodeStructs(paraNode);
         }
         else
         {
            paraNodePool->insert(paraNode);   /** in order to sort ParaNodes, insert paraNodePool once */
         }
      }
   }

   if( paraParamSet->getIntParamValue(NNodesToKeepInCheckpointFile) > 0 )
   {
      if( (signed)tempParaNodePool.getNumOfNodes() <= paraParamSet->getIntParamValue(NNodesToKeepInCheckpointFile) )
      {
         std::cout << "### NNodesToKeepInCheckpointFile is specified to " << paraParamSet->getIntParamValue(NNodesToKeepInCheckpointFile) <<
               ", but the number of nodes in checkpoint file is " << tempParaNodePool.getNumOfNodes() << ". ###" << std::endl;
         exit(-1);
      }
      paraNodeToKeepCheckpointFileNodes = new ParaNodePoolForMinimization(paraParamSet->getRealParamValue(BgapCollectingMode));
      for(int i = 0; i < paraParamSet->getIntParamValue(NNodesToKeepInCheckpointFile); i++ )
      {
         ParaNode *tempParaNode = tempParaNodePool.extractNode();
         paraNodeToKeepCheckpointFileNodes->insert(tempParaNode);
      }
      if( !paraParamSet->getBoolParamValue(MergeNodesAtRestart) )
      {
         while( tempParaNodePool.getNumOfNodes() > 0 )
         {
            ParaNode *tempParaNode = tempParaNodePool.extractNode();
            paraNodePool->insert(tempParaNode);
         }
      }
      std::cout << "### NNodesToKeepInCheckpointFile is specified to "
            << paraParamSet->getIntParamValue(NNodesToKeepInCheckpointFile)
            << " ###" << std::endl;
      std::cout << "### The number of no process nodes = " << paraNodeToKeepCheckpointFileNodes->getNumOfNodes()
            << " ###" << std::endl;
      std::cout << "### The number of nodes will process = " << paraNodePool->getNumOfNodes()
            << " ###" << std::endl;
   }

   // std::cout << "insrt to node pool: " << paraTimer->getElapsedTime() << std::endl;

   if( paraParamSet->getBoolParamValue(MergeNodesAtRestart) )
   {
      int nMerge = 0;
      ParaNode *tempNode = 0;
      while( ( tempNode = tempParaNodePool.extractNode() ) )
      {
         if( nBoundChangesOfBestNode < 0 )
         {
            if( tempNode->getDiffSubproblem() )
            {
               nBoundChangesOfBestNode = tempNode->getDiffSubproblem()->getNBoundChanges();
            }
            else
            {
               nBoundChangesOfBestNode = 0;
            }
         }
         paraNodePool->insert(tempNode);
         if( paraParamSet->getIntParamValue(UG::NMergingNodesAtRestart) < 0 ||
               nMerge < paraParamSet->getIntParamValue(UG::NMergingNodesAtRestart) )
         {
            addNodeToMergeNodeStructs(tempNode);
            nMerge++;
         }
      }
      generateMergeNodesCandidates();
   }

   // std::cout << "merging finished:" << paraTimer->getElapsedTime() << std::endl;

   if( logSolvingStatusFlag )
   {
      if( paraInitiator->getGlobalBestIncumbentSolution() )
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " Warm started from "
         << paraInitiator->getPrefixWarm()
         << " : " << n << " ParaNodes read. Current incumbent value = "
         <<  paraInitiator->convertToExternalValue(
               paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() )
               << std::endl;
      }
      else
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " Warm started from "
         << paraInitiator->getPrefixWarm()
         << " : " << n << " ParaNodes read. No solution is generated." << std::endl;
      }
   }
#ifdef _DEBUG_LB
   std::cout << paraTimer->getElapsedTime()
   << " Warm started from "
   << paraInitiator->getPrefixWarm()
   << " : " << n << " ParaNodes read. Current incumbent value = "
   <<  paraInitiator->convertToExternalValue(
         paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() )
         << std::endl;
#endif
   runningPhase = RampUpPhase;
   warmStartNodeTransferring = true;
   (void) sendParaNodesToIdleSolvers();
   warmStartNodeTransferring = false;
   if( paraSolverPool->getNumInactiveSolvers() > 0 )
   {
      runningPhase = RampUpPhase;
   }
   else
   {
      // without consideration of keeping nodes in checkpoint file
      double globalBestDualBoundValueLocal =
         std::max (
               std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
               lcts.globalBestDualBoundValue );
      if( paraParamSet->getBoolParamValue(CollectOnce) )
      {
         if( paraParamSet->getBoolParamValue(DualBoundGainTest) )
         {
            if( ( paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
                                 > paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraParamSet->getRealParamValue(MultiplierForCollectingMode) ||
                  ( paraNodePool->getNumOfNodes()
                                 > paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraParamSet->getRealParamValue(MultiplierForCollectingMode)*2  &&
                                 paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal ) > 0  ) ) )
            {
               sendRampUpToAllSolvers();
               runningPhase = NormalRunningPhase;
            }
            else
            {
               runningPhase = RampUpPhase;
            }
         }
         else
         {
            if( ( (signed)paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
                                 > paraParamSet->getIntParamValue(NChangeIntoCollectingMode) ||
                  ( (signed)paraNodePool->getNumOfNodes()
                                 > paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*2  &&
                                 paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal ) > 0  ) ) )
            {
               sendRampUpToAllSolvers();
               runningPhase = NormalRunningPhase;
            }
            else
            {
               runningPhase = RampUpPhase;
            }
         }
      }
      else
      {
         if( paraParamSet->getBoolParamValue(DualBoundGainTest) )
         {
            if( ( paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal )
                                 > paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraParamSet->getRealParamValue(MultiplierForCollectingMode) ||
                  ( paraNodePool->getNumOfNodes()
                                 > paraParamSet->getIntParamValue(NChangeIntoCollectingMode)*paraParamSet->getRealParamValue(MultiplierForCollectingMode)*2  &&
                                 paraNodePool->getNumOfGoodNodes( globalBestDualBoundValueLocal ) > 0  ) ) )
            {
               sendRampUpToAllSolvers();
               runningPhase = NormalRunningPhase;
            }
            else
            {
               runningPhase = RampUpPhase;
            }
         }
         else
         {
            sendRampUpToAllSolvers();
            runningPhase = NormalRunningPhase;
         }
      }
   }
   // std::cout << "before run:" << paraTimer->getElapsedTime() << std::endl;
   // exit(1);
   run();
}

void
ParaLoadCoordinator::writePreviousStatisticsInformation(
      )
{
   /* read previous LoadCoordinator statistics */
   char loadCoordinatorStatisticsFileName[256];
   sprintf(loadCoordinatorStatisticsFileName,"%s_loadCoordinatorStatistics_LC0.gz", paraInitiator->getPrefixWarm() );
   gzstream::igzstream  loadCoordinatorStatisticsStream;
   loadCoordinatorStatisticsStream.open(loadCoordinatorStatisticsFileName, std::ios::in | std::ios::binary);
   if( !loadCoordinatorStatisticsStream )
   {
      std::cout << "checkpoint LoadCoordinatorStatistics file cannot open: file name = " <<  loadCoordinatorStatisticsFileName << std::endl;
      exit(1);
   }
   int nSolverStatistics;
   loadCoordinatorStatisticsStream.read((char *)&nSolverStatistics, sizeof(int));
   ParaLoadCoordinatorTerminationState *prevLcts = new ParaLoadCoordinatorTerminationState();
   if( !prevLcts->read(paraComm, loadCoordinatorStatisticsStream) )
   {
      std::cout << "checkpoint LoadCoordinatorStatistics file cannot read: file name = " <<  loadCoordinatorStatisticsFileName << std::endl;
      exit(1);
   }
   loadCoordinatorStatisticsStream.close();

   /* open Solver statistics file */
   char solverStatisticsFileName[256];
   sprintf(solverStatisticsFileName,"%s_solverStatistics_LC0.gz", paraInitiator->getPrefixWarm() );
   gzstream::igzstream  solverStatisticsStream;
   solverStatisticsStream.open(solverStatisticsFileName, std::ios::in | std::ios::binary);
   if( !solverStatisticsStream )
   {
      std::cout << "checkpoint SolverStatistics file cannot open: file name = " <<  solverStatisticsFileName << std::endl;
      exit(1);
   }

   /* opne output statistics file */
   char previousStatisticsFileName[256];
   sprintf(previousStatisticsFileName, "%s_statistics_w%05lld_LC0",
         paraInitiator->getPrefixWarm(),
         prevLcts->nWarmStart);
   std::ofstream ofsStatistics;
   ofsStatistics.open(previousStatisticsFileName);
   if( !ofsStatistics )
   {
      std::cout << "previous statistics file cannot open : file name = " << previousStatisticsFileName << std::endl;
      exit(1);
   }

   /* read and write solver statistics */
   for( int i = 0; i < nSolverStatistics; i++ )
   {
      ParaSolverTerminationState *psts = paraComm->createParaSolverTerminationState();
      if( !psts->read(paraComm, solverStatisticsStream) )
      {
         std::cout << "checkpoint SolverStatistics file cannot read: file name = " <<  solverStatisticsFileName << std::endl;
         exit(1);
      }
      ofsStatistics << psts->toString(paraInitiator);
      delete psts;
   }

   /* write LoadCoordinator statistics */
   ofsStatistics << prevLcts->toString();

   /* update warm start counter */
   lcts.nWarmStart = prevLcts->nWarmStart + 1;
   lcts.globalBestDualBoundValue = std::max( prevLcts->globalBestDualBoundValue, lcts.globalBestDualBoundValue );
   lcts.externalGlobalBestDualBoundValue = prevLcts->externalGlobalBestDualBoundValue;
   delete prevLcts;

   /* close solver statistics file and output file */
   solverStatisticsStream.close();
   ofsStatistics.close();
}

#endif // End of UG_WITH_ZLIB

void
ParaLoadCoordinator::run(
      ParaNode *paraNode
      )
{
   assert(!paraRacingSolverPool);
   // without consideration of keeping nodes in checkpoint file
   double globalBestDualBoundValueLocal =
      std::max (
            std::min( paraSolverPool->getGlobalBestDualBoundValue(), paraNodePool->getBestDualBoundValue() ),
            lcts.globalBestDualBoundValue );
   int destination = paraSolverPool->activateSolver(paraNode,
         paraRacingSolverPool, (runningPhase==RampUpPhase),
         paraNodePool->getNumOfGoodNodes(globalBestDualBoundValueLocal), averageLastSeveralDualBoundGains );
   lcts.nSent++;
   if( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 0 &&
         paraParamSet->getBoolParamValue(CollectOnce)
         )
   {
      int nCollect = -1;
      PARA_COMM_CALL(
            paraComm->send( &nCollect, 1, ParaINT, destination, TagCollectAllNodes )
      );
   }
   if( paraParamSet->getBoolParamValue(Deterministic) )
   {
      int token[2];
      token[0] = 1;
      token[1] = -1;
      PARA_COMM_CALL(
            paraComm->send( token, 2, ParaINT, token[0], TagToken )
      );
   }
   writeTransferLog(destination);
   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << paraTimer->getElapsedTime()
      << " S." << destination << " < "
      << paraInitiator->convertToExternalValue(
            paraNode->getDualBoundValue() );
      if( paraInitiator->getGlobalBestIncumbentSolution() )
      {
         if( paraInitiator->getGap(paraNode->getDualBoundValue()) > displayInfOverThisValue )
         {
            *osLogSolvingStatus << " ( Inf )";
         }
         else
         {
            *osLogSolvingStatus << " ( " << paraInitiator->getGap(paraNode->getDualBoundValue()) * 100 << "% )";
         }
      }
      if( paraParamSet->getBoolParamValue(LightWeightRootNodeProcess) &&
            runningPhase != RampUpPhase && (!paraRacingSolverPool) &&
            paraSolverPool->getNumInactiveSolvers() >
               ( paraSolverPool->getNSolvers() * paraParamSet->getRealParamValue(RatioToApplyLightWeightRootProcess) ) )
      {
         *osLogSolvingStatus << " L";
      }
      *osLogSolvingStatus << std::endl;
   }
#ifdef DEBUG_LB
   std::cout << paraTimer->getElapsedTime()
   << " S." << destination << " > "
   << paraInitiator->convertToExternalValue(
         paraNode->getDualBoundValue() );
   if( paraInitiator->getGlobalBestIncumbentSolution() )
   {
      if( paraInitiator->getGap(paraNode->getDualBoundValue()) > displayInfOverThisValue )
      {
         std::cout << " ( Inf )";
      }
      else
      {
         std::cout << " ( " << paraInitiator->getGap(paraNode->getDualBoundValue()) * 100 << "% )";
      }
   }
   if( paraParamSet->getBoolParamValue(LightWeightRootNodeProcess) &&
         runningPhase != RampUpPhase && (!paraRacingSolverPool) &&
         paraSolverPool->getNumInactiveSolvers() >
            ( paraSolverPool->getNSolvers() * paraParams->getRealParamValue(RatioToApplyLightWeightRootProcess) ) )
   {
      std::cout << " L";
   }
   std::cout << std::endl;
#endif
   run();
}

int
ParaLoadCoordinator::processRacingRampUpTagSolverState(
      int source,
      int tag
      )
{

   ParaSolverState *solverState = paraComm->createParaSolverState();
   solverState->receive(paraComm, source, tag);

#ifdef _DEBUG_DET
   if( paraDetTimer )
   {
      std::cout << "Rank " << source << ": ET = " << paraDetTimer->getElapsedTime() << ", Det time = " << solverState->getDeterministicTime() << std::endl;
   }
#endif

   if( paraDetTimer
         && paraDetTimer->getElapsedTime() < solverState->getDeterministicTime() )

   {
      paraDetTimer->update( solverState->getDeterministicTime() - paraDetTimer->getElapsedTime() );
   }

   assert(solverState->isRacingStage());
   if( !restartingRacing )  // restartingRacing means that LC is terminating racing solvers.
   {
      paraRacingSolverPool->updateSolverStatus(source,
                                      solverState->getNNodesSolved(),
                                      solverState->getNNodesLeft(),
                                      solverState->getSolverLocalBestDualBoundValue());
      assert( paraRacingSolverPool->getNumNodesLeft(source) == solverState->getNNodesLeft() );
   }
   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << paraTimer->getElapsedTime()
      << " S." << source << " | "
      << paraInitiator->convertToExternalValue(
            solverState->getSolverLocalBestDualBoundValue()
            );
      if( !paraInitiator->getGlobalBestIncumbentSolution() ||
            paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) > displayInfOverThisValue
            || solverState->getNNodesLeft() == 0 )
      {
         *osLogSolvingStatus << " ( Inf )";
      }
      else
      {
         *osLogSolvingStatus << " ( " << paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) * 100 << "% )";
      }
      *osLogSolvingStatus << " [ " << solverState->getNNodesLeft() << " ]";
      double globalBestDualBoundValue = paraRacingSolverPool->getGlobalBestDualBoundValue();
      *osLogSolvingStatus << " ** G.B.: " << paraInitiator->convertToExternalValue(globalBestDualBoundValue);
      if( !paraInitiator->getGlobalBestIncumbentSolution() ||
            paraInitiator->getGap(globalBestDualBoundValue) > displayInfOverThisValue )
      {
         *osLogSolvingStatus << " ( Inf ) ";
      }
      else
      {
         *osLogSolvingStatus << " ( " << paraInitiator->getGap(globalBestDualBoundValue) * 100 << "% ) ";
      }
      *osLogSolvingStatus << "[ " << paraRacingSolverPool->getNnodesLeftInBestSolver()
 //     <<" ] ** RR" << std::endl;
      <<" ] ** RR " << solverState->getDeterministicTime() << std::endl;   // for debug
   }
#ifdef _DEBUG_LB
   std::cout << paraTimer->getElapsedTime()
   << " S." << source << " | "
   << paraInitiator->convertToExternalValue(
         solverState->getSolverLocalBestDualBoundValue()
         );
   if( !paraInitiator->getGlobalBestIncumbentSolution() ||
         paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) > displayInfOverThisValue
         || solverState->getNNodesLeft() == 0 )
   {
      std::cout << " ( Inf )";
   }
   else
   {
      std::cout << " ( " << paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) * 100 << "% )";
   }
   std::cout << " [ " << solverState->getNNodesLeft() << " ]";
   double globalBestDualBoundValue = paraRacingSolverPool->getGlobalBestDualBoundValue();
   std::cout << " ** G.B.: " << paraInitiator->convertToExternalValue(globalBestDualBoundValue);
   if( !paraInitiator->getGlobalBestIncumbentSolution() ||
         paraInitiator->getGap(globalBestDualBoundValue) > displayInfOverThisValue )
   {
      std::cout << " ( Inf ) ";
   }
   else
   {
      std::cout << " ( " << paraInitiator->getGap(globalBestDualBoundValue) * 100 << "% ) ";
   }
   std::cout << "[ " << paraRacingSolverPool->getNnodesLeftInBestSolver()
   <<" ] ** RR" << std::endl;
#endif

   if( !paraParamSet->getBoolParamValue(NoUpperBoundTransferInRacing) )
   {
      /** the following should be before noticationId back to the source solver */
      if( paraParamSet->getBoolParamValue(DistributeBestPrimalSolution) )
      {
         if( paraInitiator->getGlobalBestIncumbentSolution() &&
               paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue()
               < solverState->getGlobalBestPrimalBoundValue() )
         {
            paraInitiator->getGlobalBestIncumbentSolution()->send(paraComm, source);
         }
      }
   }

   if( paraParamSet->getBoolParamValue(CheckGapInLC) )
   {
      if( paraInitiator->getAbsgap(solverState->getSolverLocalBestDualBoundValue() ) <
            paraInitiator->getAbsgapValue() ||
            paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) <
            paraInitiator->getGapValue()
            )
      {
         // std::cout << "current dual = "  << paraInitiator->convertToExternalValue(solverState->getSolverLocalBestDualBoundValue()) <<std::endl;
         // std::cout << "pool best dual = " << paraInitiator->convertToExternalValue(paraRacingSolverPool->getGlobalBestDualBoundValue()) << std::endl;
         // std::cout << "current gap = " << paraInitiator->getGap(solverState->getSolverLocalBestDualBoundValue()) <<std::endl;
         PARA_COMM_CALL(
               paraComm->send( NULL, 0, ParaBYTE, source, TagGivenGapIsReached )
         );
      }
   }

   double lcBestDualBoundValue = paraRacingSolverPool->getGlobalBestDualBoundValue();
   PARA_COMM_CALL(
         paraComm->send( &lcBestDualBoundValue, 1, ParaDOUBLE, source, TagLCBestBoundValue)
         );
   unsigned int notificationId = solverState->getNotificaionId();
   PARA_COMM_CALL(
         paraComm->send( &notificationId, 1, ParaUNSIGNED, source, TagNotificationId)
         );

   if( lcts.globalBestDualBoundValue < paraRacingSolverPool->getGlobalBestDualBoundValue() )
   {
      lcts.globalBestDualBoundValue = paraRacingSolverPool->getGlobalBestDualBoundValue();
      lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
   }
   delete solverState;
   return 0;
}

int
ParaLoadCoordinator::processRacingRampUpTagCompletionOfCalculation(
      int source,
      int tag
      )
{
   ParaCalculationState *calcState = paraComm->createParaCalculationState();
   calcState->receive(paraComm, source, tag);
   writeTransferLogInRacing(source, calcState);
   if( logSolvingStatusFlag )
   {
      switch ( calcState->getTerminationState() )
      {
      case CompTerminatedInRacingStage:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(TERMINATED_IN_RACING_STAGE) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      case CompTerminatedByInterruptRequest:
      case CompInterruptedInRacingStage:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(INTERRUPTED_BY_TIME_LIMIT or INTERRUPTED_BY_SOME_SOLVER_TERMINATED_IN_RACING_STAGE) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      case CompTerminatedByTimeLimit:
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " S." << source << " >(INTERRUPTED_BY_TIME_LIMIT) " << paraInitiator->convertToExternalValue(calcState->getDualBoundValue());
         break;
      }
      default:
         THROW_LOGICAL_ERROR2("Invalid termination: termination state = ", calcState->getTerminationState() )
      }
      *osLogSolvingStatus << ", ct:" << calcState->getCompTime()
            << ", nr:" << calcState->getNRestarts()
            << ", n:" << calcState->getNSolved()
            << ", rt:" << calcState->getRootTime()
            << ", avt:" << calcState->getAverageNodeCompTimeExcpetRoot()
            << std::endl;
   }
#ifdef _DEBUG_LB
   switch ( calcState->getTerminationState() )
   {
   case CompTerminatedInRacingStage:
   {
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " >(TERMINATED_IN_RACING_STAGE)";
      break;
   }
   case CompTerminatedByInterruptRequest:
   case CompInterruptedInRacingStage:
   {
      std::cout << paraTimer->getElapsedTime()
      << " S." << source << " >(INTERRUPTED_BY_TIME_LIMIT or INTERRUPTED_BY_SOME_SOLVER_TERMINATED_IN_RACING_STAGE)";
      break;
   }
   default:
      THROW_LOGICAL_ERROR2("Invalid termination: termination state = ", calcState->getTerminationState() )
   }
   std::cout << std::endl;
#endif

   if( calcState->getTerminationState() == CompTerminatedInRacingStage )
   {
      racingTermination = true; // even if interruptIsRequested, 
                                // solver should have been terminated before receiveing it
      if( osStatisticsRacingRampUp )
      {
         *osStatisticsRacingRampUp << "######### Solver Rank = " <<
               source << " is terminated in racing stage #########" << std::endl;
      }
      nSolvedRacingTermination = calcState->getNSolved();
      if( !EPSEQ( calcState->getDualBoundValue(), -DBL_MAX, paraInitiator->getEpsilon() ) &&
            EPSEQ( minmalDualBoundNormalTermSolvers, DBL_MAX, paraInitiator->getEpsilon() ) )
      {
         if( paraInitiator->getGlobalBestIncumbentSolution() )
         {
            minmalDualBoundNormalTermSolvers = std::min( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), calcState->getDualBoundValue() );
         }
         else
         {
            minmalDualBoundNormalTermSolvers = calcState->getDualBoundValue();
         }
      }
      if( EPSLE(lcts.globalBestDualBoundValue, calcState->getDualBoundValue(), paraInitiator->getEpsilon()) &&
            minmalDualBoundNormalTermSolvers < calcState->getDualBoundValue() )
      {
         if( paraInitiator->getGlobalBestIncumbentSolution() )
         {
            minmalDualBoundNormalTermSolvers = std::min( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), calcState->getDualBoundValue() );
         }
         else
         {
            minmalDualBoundNormalTermSolvers = calcState->getDualBoundValue();
         }
      }
      if( paraInitiator->getGlobalBestIncumbentSolution() &&
            ( EPSEQ( calcState->getDualBoundValue(), paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), paraInitiator->getEpsilon() ) ||
                 EPSEQ( calcState->getDualBoundValue(), -DBL_MAX, paraInitiator->getEpsilon() ) ||
                 EPSEQ( calcState->getDualBoundValue(), DBL_MAX, paraInitiator->getEpsilon() ) ||
                 ( paraParamSet->getBoolParamValue(UG::CommunicateTighterBoundsInRacing) &&
                       // distributed domain propagation could causes the following situation
                       paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() < calcState->getDualBoundValue() ) ||
                  ( calcState->getNSolved() == 0 ) ) )
      {
         lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
         lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
         if( paraInitiator->getGlobalBestIncumbentSolution() )
         {
            minmalDualBoundNormalTermSolvers = std::min( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), lcts.globalBestDualBoundValue );
         }
         else
         {
            minmalDualBoundNormalTermSolvers = lcts.globalBestDualBoundValue;
         }
      }
      /*
      if( paraInitiator->getGlobalBestIncumbentSolution() &&
            paraParamSet->getBoolParamValue(UG::CommunicateTighterBoundsInRacing) &&
            // distributed domain propagation could causes the following situation
            paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() < calcState->getDualBoundValue() &&
            EPSGE( paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(), lcts.globalBestDualBoundValue, paraInitiator->getEpsilon() ) )
      {
         lcts.globalBestDualBoundValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
         lcts.externalGlobalBestDualBoundValue = paraInitiator->convertToExternalValue(lcts.globalBestDualBoundValue);
         minmalDualBoundNormalTermSolvers = lcts.globalBestDualBoundValue;
      }
      */
      assert( !paraInitiator->getGlobalBestIncumbentSolution() ||
              ( paraInitiator->getGlobalBestIncumbentSolution() &&
                EPSEQ(paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(),lcts.globalBestDualBoundValue, paraInitiator->getEpsilon() ) ) );
   }

   if( calcState->getTerminationState() == CompTerminatedByTimeLimit )
   {
      std::cout << "####### Rank " << paraComm->getRank() << " solver terminated with timelimit in solver side. #######" << std::endl;
      std::cout << "####### Final statistics may be messed up!" << std::endl;
   }

   delete calcState;

   ParaSolverTerminationState *termState = paraComm->createParaSolverTerminationState();
   termState->receive(paraComm, source, TagTerminated);

   if( paraDetTimer )
   {
      if( paraDetTimer->getElapsedTime() < termState->getDeterministicTime() )
      {
         paraDetTimer->update( termState->getDeterministicTime() - paraDetTimer->getElapsedTime() );
      }
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, source, TagAckCompletion )
      );
   }

   if( osStatisticsRacingRampUp )
   {
      *osStatisticsRacingRampUp << termState->toString(paraInitiator);
   }

   if( paraParamSet->getBoolParamValue(StatisticsToStdout) )
   {
      std::cout << termState->toString(paraInitiator) << std::endl;
   }

   delete termState;
   // nTerminated++;      We should not count this, We should always send Term from LC!
   inactivateRacingSolverPool(source);

   if( paraParamSet->getBoolParamValue(Quiet) && racingTermination )
   {
      /** in this case, do not have to wait statistical information from the other solvers */
      nTerminated = 1;
      delete this;
#ifdef _COMM_PTH
      _exit(0);
#else
      exit(0);
#endif
   }

   return 0;
}


void
ParaLoadCoordinator::run(
      ParaNode *paraNode,
      int nRacingSolvers,
      ParaRacingRampUpParamSet **racingRampUpParams
      )
{
   /** register message handlers */
   for( int i = 0; i < N_TAGS; i++ )
   {
      racingRampUpMessageHandler[i] = 0;
   }
   racingRampUpMessageHandler[TagSolution] = &UG::ParaLoadCoordinator::processTagSolution;
   racingRampUpMessageHandler[TagSolverState] = &UG::ParaLoadCoordinator::processRacingRampUpTagSolverState;
   racingRampUpMessageHandler[TagCompletionOfCalculation] = &UG::ParaLoadCoordinator::processRacingRampUpTagCompletionOfCalculation;
   racingRampUpMessageHandler[TagAnotherNodeRequest] = &UG::ParaLoadCoordinator::processTagAnotherNodeRequest;
   racingRampUpMessageHandler[TagTerminated] = &UG::ParaLoadCoordinator::processTagTerminated;
   racingRampUpMessageHandler[TagHardTimeLimit] = &UG::ParaLoadCoordinator::processTagHardTimeLimit;
   racingRampUpMessageHandler[TagInitialStat] = &UG::ParaLoadCoordinator::processTagInitialStat;
   if( ( paraParamSet->getIntParamValue(UG::RampUpPhaseProcess) == 1 ||
         paraParamSet->getIntParamValue(UG::RampUpPhaseProcess) == 2 ) &&
         paraParamSet->getBoolParamValue(UG::CommunicateTighterBoundsInRacing) )
   {
      racingRampUpMessageHandler[TagLbBoundTightenedIndex] = &UG::ParaLoadCoordinator::processTagLbBoundTightened;
      racingRampUpMessageHandler[TagUbBoundTightenedIndex] = &UG::ParaLoadCoordinator::processTagUbBoundTightened;
   }
   if( paraParamSet->getBoolParamValue(Deterministic) )
   {
      racingRampUpMessageHandler[TagToken] = &UG::ParaLoadCoordinator::processTagToken;
   }

   /** creates racing solver pool */
   paraRacingSolverPool = new ParaRacingSolverPool(
         1,                // paraSolver origin rank
         paraComm, paraParamSet, paraTimer, paraDetTimer);

   /** activate racing solver with root node */
   PARA_COMM_CALL(
         paraNode->bcast(paraComm, 0)
         );
   paraRacingSolverPool->activate(paraNode);
   lcts.nSent++;
   if( paraParamSet->getBoolParamValue(Deterministic) )
   {
      int token[2];
      token[0] = 1;
      token[1] = -1;
      PARA_COMM_CALL(
            paraComm->send( token, 2, ParaINT, token[0], TagToken )
      );
   }
   if( logNodesTransferFlag )
   {
      for(int i = 1; i < paraComm->getSize(); i++ )
      {
         writeTransferLogInRacing(i);
      }
   }

   /** output start racing to log file, if it is necessary */
   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << paraTimer->getElapsedTime()
      << " All Solvers starts racing "
      << paraInitiator->convertToExternalValue(
            paraNode->getDualBoundValue() ) << std::endl;
   }
#ifdef _DEBUG_LB
   std::cout << paraTimer->getElapsedTime()
   << " All Solvers starts racing "
   << paraInitiator->convertToExternalValue(
         paraNode->getDualBoundValue() ) << std::endl;
#endif

   int source;
   int tag;

   for(;;)
   {
      /*******************************************
       *  waiting for any message form anywhere  *
       *******************************************/
      double inIdleTime = paraTimer->getElapsedTime();
      paraComm->probe(&source, &tag);
      lcts.idleTime += ( paraTimer->getElapsedTime() - inIdleTime );
      if( racingRampUpMessageHandler[tag] )
      {
         int status = (this->*racingRampUpMessageHandler[tag])(source, tag);
         if( status )
         {
            std::ostringstream s;
            s << "[ERROR RETURN form Racing Ramp-up Message Hander]:" <<  __FILE__ <<  "] func = "
              << __func__ << ", line = " << __LINE__ << " - "
              << "process tag = " << tag << std::endl;
            abort();
         }
      }
      else
      {
         THROW_LOGICAL_ERROR3( "No racing ramp-up message hander for ", tag, " is not registered" );
      }

#ifdef UG_WITH_UGS
      if( commUgs ) checkAndReadIncumbent();
#endif

      /** output tabular solving status */
      if( outputTabularSolvingStatusFlag && (!racingTermination) &&
            ( ( ( paraParamSet->getBoolParamValue(Deterministic) &&
                  paraParamSet->getBoolParamValue(DeterministicTabularSolvingStatus) ) &&
                  ( ( paraDetTimer->getElapsedTime() - previousTabularOutputTime ) >
               paraParamSet->getRealParamValue(TabularSolvingStatusInterval) ) ) ||
               ( (!paraParamSet->getBoolParamValue(Deterministic) ||
                     !paraParamSet->getBoolParamValue(DeterministicTabularSolvingStatus) )  &&
                  ( ( paraTimer->getElapsedTime() - previousTabularOutputTime ) >
               paraParamSet->getRealParamValue(TabularSolvingStatusInterval) ) ) ) )
      {
         outputTabularSolvingStatus(' ');
         if( paraParamSet->getBoolParamValue(Deterministic) )
         {
            if( paraParamSet->getBoolParamValue(DeterministicTabularSolvingStatus) )
            {
               previousTabularOutputTime = paraDetTimer->getElapsedTime();
            }
            else
            {
               previousTabularOutputTime = paraTimer->getElapsedTime();
            }
         }
         else
         {
            previousTabularOutputTime = paraTimer->getElapsedTime();
         }
      }

      if( hardTimeLimitIsReached )
          break;

      if( restartingRacing )
      {
         if( paraRacingSolverPool->getNumActiveSolvers() == 0 )
         {
            if( restartRacing() )
            {
               return;  // solved
            }
         }
         continue;
      }

      switch ( runningPhase )
      {
      case RampUpPhase:
      {
         if( !paraRacingSolverPool )
         {
            paraSolverPool->activate();
            run();
            return;
         }
         if( racingTermination )
         {
            if( paraRacingSolverPool->getNumActiveSolvers() == 0 )
            {
               delete paraRacingSolverPool;
               paraRacingSolverPool = 0;
               run();
               return;
            }
            sendInterruptRequest();
            break;
         }

         if( paraParamSet->getRealParamValue(TimeLimit) > 0.0 &&
               paraTimer->getElapsedTime() > paraParamSet->getRealParamValue(TimeLimit) )
         {
            hardTimeLimitIsReached = true;
            sendInterruptRequest();
            // runningPhase = TerminationPhase;     waits until paraRacingSolverPool becomes empty
            break;
         }

         if( paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) > 0.0 &&
               paraTimer->getElapsedTime() > paraParamSet->getRealParamValue(FinalCheckpointGeneratingTime) )
         {
            hardTimeLimitIsReached = true;
            std::cout << "** Program is still in racing stage. FinalCheckpointGeneratingTime is sppecifid, but the checkpoint files would not be generated." << std::endl;
            sendInterruptRequest();
            // runningPhase = TerminationPhase;     waits until paraRacingSolverPool becomes empty
            break;
         }

         if( paraRacingSolverPool->isWinnerDecided(
               ( paraInitiator->getGlobalBestIncumbentSolution() &&
                 EPSLT(paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue(),DBL_MAX, DEFAULT_NUM_EPSILON ) ) ) )
         {
            if( paraParamSet->getBoolParamValue(RestartRacing) &&
                  primalUpdated
                  )
            {
               if( !restartingRacing )
               {
                  for(int i = 1; i < paraComm->getSize(); i++)
                  {
                     PARA_COMM_CALL(
                           paraComm->send( NULL, 0, ParaBYTE, i, TagTerminateSolvingToRestart )
                     );
                  }
                  // primalUpdated = false;
                  restartingRacing = true;
               }
            }
            else
            {
               racingWinner = paraRacingSolverPool->getWinner();
               assert( racingWinner >0 );
               int numNodesLeft = paraRacingSolverPool->getNumNodesLeft(racingWinner);
               paraSolverPool->activateSolver(
                     racingWinner, paraRacingSolverPool->extractNode(), numNodesLeft );
               paraRacingSolverPool->inactivateSolver(racingWinner);
               assert(paraRacingSolverPool->getNumActiveSolvers() >= 0);
               PARA_COMM_CALL(
                     paraComm->send( NULL, 0, ParaBYTE, racingWinner, TagWinner )
               );

               if( numNodesLeft >
                    2.0 * paraParamSet->getIntParamValue(StopRacingNumberOfNodesLeft)*paraParamSet->getRealParamValue(StopRacingNumberOfNodesLeftMultiplier)
                    ||
                    numNodesLeft <= ( paraSolverPool->getNSolvers() * paraParamSet->getRealParamValue(ProhibitCollectOnceMultiplier) )
                    )
               {
                  paraParamSet->setBoolParamValue(CollectOnce,false);
                  paraParamSet->setBoolParamValue(MergeNodesAtRestart,false);
                  paraParamSet->setBoolParamValue(RacingStatBranching,false);
                  paraParamSet->setIntParamValue(RampUpPhaseProcess, 1);
                  winnerSolverNodesCollected = true;
                  std::cout << "Warning: Ramp-Up Phase Process is switched to 1. CollectOnce, MergeNodesAtRestart and RacingStatBranching are switched to FALSE." << std::endl;
                  std::cout << "You should check the following parameter values: StopRacingNumberOfNodesLeft, StopRacingNumberOfNodesLeftMultiplier, ProhibitCollectOnceMultiplier" << std::endl;
               }

               if( numNodesLeft > (signed)paraSolverPool->getNSolvers()
                     ||
                     numNodesLeft > (signed)( paraSolverPool->getNSolvers() * paraParamSet->getRealParamValue(ProhibitCollectOnceMultiplier) )
                     )
               {
                  if( paraParamSet->getBoolParamValue(CollectOnce) )
                  {
                     int nCollect = paraParamSet->getIntParamValue(NCollectOnce);
                     if( nCollect == 0 )
                     {
                        nCollect = ( paraSolverPool->getNSolvers() * 5 );
                     }
                     PARA_COMM_CALL(
                           paraComm->send( &nCollect, 1, ParaINT, racingWinner, TagCollectAllNodes )
                     );
                  }
                  if( paraParamSet->getIntParamValue(RampUpPhaseProcess) == 2 )
                  {
                     merging = true;
                     initMergeNodesStructs();
                  }
               }
               else
               {
                  winnerSolverNodesCollected = true;   // do not wait until all nodes are collected
               }
               racingWinnerParams = racingRampUpParams[racingWinner - 1];
               racingWinnerParams->setWinnerRank(racingWinner);
               racingRampUpParams[racingWinner - 1] = 0;
               for(int i = 1; i < paraComm->getSize(); i++)
               {
                  if( racingRampUpParams[i - 1] )
                  {
                     PARA_COMM_CALL(
                           racingWinnerParams->send(paraComm, i)
                           );
                  }
               }
               /** output winner to log file, if it is necessary */
               if( logSolvingStatusFlag )
               {
                  *osLogSolvingStatus << paraTimer->getElapsedTime()
                  << " S." << racingWinner << " is the racing winner!"
                  << " Selected strategy " << racingWinnerParams->getStrategy()
                  << "." << std::endl;
               }
   #ifdef _DEBUG_LB
               std::cout << paraTimer->getElapsedTime()
               << " S." << racingWinner << " is the racing winner!"
               << " Selected strategy " << racingWinnerParams->getStrategy()
               << "." << std::endl;
   #endif
               if( outputTabularSolvingStatusFlag )
               {
                  *osTabularSolvingStatus <<
                        "Racing ramp-up finished after " <<
                        paraTimer->getElapsedTime() << " seconds." <<
                        " Selected strategy " << racingWinnerParams->getStrategy() <<
                        "." << std::endl;
               }
               // runningPhase = NormalRunningPhase;
               // Keep running as RampUpPhase, but in the run() switching into RampUpPhase in normal running mode
               // delete paraRacingSolverPool;
               run();
               return;
            }
         }
         break;
      }
      default:
      {
         THROW_LOGICAL_ERROR2( "Undefined running phase: ", static_cast<int>(runningPhase) );
      }
      }
   }
   return;
}

void
ParaLoadCoordinator::sendRampUpToAllSolvers(
      )
{
   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, i, TagRampUp )
      );
   }
}

void
ParaLoadCoordinator::sendRetryRampUpToAllSolvers(
      )
{
   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, i, TagRetryRampUp )
      );
   }
}

void
ParaLoadCoordinator::sendInterruptRequest(
      )
{
   int exitSolverRequest = 0;    // do nothing
   if( paraParamSet->getRealParamValue(UG::FinalCheckpointGeneratingTime) > 0.0 &&
         (!paraRacingSolverPool) &&
         ( paraParamSet->getIntParamValue(UG::FinalCheckpointNSolvers) < 0 ||
               ( paraParamSet->getIntParamValue(UG::FinalCheckpointNSolvers) > 0 &&
                     (signed)nCollectedSolvers < paraParamSet->getIntParamValue(UG::FinalCheckpointNSolvers) )
         ) )
   {
      if( !interruptIsRequested && outputTabularSolvingStatusFlag )
      {
         *osTabularSolvingStatus <<
                "Start collecting the final check-point data after " <<
                paraTimer->getElapsedTime() << " seconds. " << std::endl;
      }

      if( paraSolverPool->getNumActiveSolvers() > 0 )
      {
         int bestSolverRank = paraSolverPool->getBestSolver();
         ParaNode *solvingNode = paraSolverPool->getCurrentNode(bestSolverRank);
         if( !solvingNode->getAncestor() )
         {
            solvingNode->collectsNodes();
            exitSolverRequest = 1;  // collect all nodes
            PARA_COMM_CALL(
                  paraComm->send( &exitSolverRequest, 1, ParaINT, bestSolverRank, TagInterruptRequest )
            );
            if( logSolvingStatusFlag )
            {
               *osLogSolvingStatus << paraTimer->getElapsedTime()
               << " S." << bestSolverRank << " TagInterruptRequest with collecting is sent"
               << std::endl;
            }
         }
         else
         {
            exitSolverRequest = 0;    // do nothing
            PARA_COMM_CALL(
                  paraComm->send( &exitSolverRequest, 1, ParaINT, bestSolverRank, TagInterruptRequest )
            );
            if( logSolvingStatusFlag )
            {
               *osLogSolvingStatus << paraTimer->getElapsedTime()
               << " S." << bestSolverRank << " TagInterruptRequest without collecting is sent"
               << std::endl;
            }
         }
      }
   }
   else
   {
      if( paraParamSet->getRealParamValue(UG::FinalCheckpointGeneratingTime) > 0.0 &&
         (!paraRacingSolverPool) )
      {
         for( int i = 1; i < paraComm->getSize(); i++ )
         {
            if( paraSolverPool->isSolverActive(i) )
            {
               PARA_COMM_CALL(
                     paraComm->send( &exitSolverRequest, 1, ParaINT, i, TagInterruptRequest )
               );
            }
         }
      }
      else
      {
         if( interruptIsRequested ) return;
         for( int i = 1; i < paraComm->getSize(); i++ )
         {
            PARA_COMM_CALL(
                  paraComm->send( &exitSolverRequest, 1, ParaINT, i, TagInterruptRequest )
            );
         }
      }
   }

   interruptIsRequested = true;
}

void
ParaLoadCoordinator::terminateAllSolvers(
      )
{
   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, i, TagTerminateRequest )
      );
   }
}

void
ParaLoadCoordinator::writeTransferLog(
      int rank
      )
{
   // output comp infomation to tree log file
   if( logNodesTransferFlag )
   {
      *osLogNodesTransfer << "[Solver-ID: " << rank
      << "] ParaNode was sent " << (paraSolverPool->getCurrentNode(rank))->toString() << std::endl;
   }
}

void
ParaLoadCoordinator::writeTransferLog(
      int rank,
      ParaCalculationState *state
      )
{
   // output comp infomation to tree log file
   if( logNodesTransferFlag )
   {
      *osLogNodesTransfer << "[Solver-ID: " << rank
      << "] Solved " << (paraSolverPool->getCurrentNode(rank))->toString() << std::endl;
      *osLogNodesTransfer << "[Solver-ID: " << rank
      << "] " << state->toString() << std::endl;
   }
}

void
ParaLoadCoordinator::writeTransferLogInRacing(
      int rank
      )
{
   // output comp infomation to tree log file
   if( logNodesTransferFlag )
   {
      *osLogNodesTransfer << "[Solver-ID: " << rank
      << "] ParaNode was sent " << (paraRacingSolverPool->getCurrentNode(rank))->toString() << std::endl;
   }
}

void
ParaLoadCoordinator::writeTransferLogInRacing(
      int rank,
      ParaCalculationState *state
      )
{
   // output comp infomation to tree log file
   if( logNodesTransferFlag )
   {
      *osLogNodesTransfer << "[Solver-ID: " << rank
      << "] Solved " << (paraRacingSolverPool->getCurrentNode(rank))->toString() << std::endl;
      *osLogNodesTransfer << "[Solver-ID: " << rank
      << "] " << state->toString() << std::endl;
   }
}

bool
ParaLoadCoordinator::updateSolution(
      ParaSolution *sol
      )
{
   if( !paraInitiator->getGlobalBestIncumbentSolution() )
      return paraInitiator->tryToSetIncumbentSolution(sol->clone(paraComm),false);
   if( sol->getObjectiveFuntionValue()
         < paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue() )
      return paraInitiator->tryToSetIncumbentSolution(sol->clone(paraComm),false);
   else
      return false;
}

void
ParaLoadCoordinator::sendIncumbentValue(
      int receivedRank
      )
{
   double globalBestIncumbentValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
   if( !paraParamSet->getBoolParamValue(NoUpperBoundTransferInRacing) || !isRacingStage() )
   {
      for( int i = 1; i < paraComm->getSize(); i++ )
      {
         if( i !=  receivedRank )
         {
            PARA_COMM_CALL(
                  paraComm->send( &globalBestIncumbentValue, 1, ParaDOUBLE, i, TagIncumbentValue )
            );
         }

      }
   }
   lcts.nDeletedInLc += paraNodePool->removeBoundedNodes(globalBestIncumbentValue);
}

void
ParaLoadCoordinator::sendCutOffValue(
      int receivedRank
      )
{
   double globalBestCutOffValue = paraInitiator->getGlobalBestIncumbentSolution()->getCutOffValue();
   if( !paraParamSet->getBoolParamValue(NoUpperBoundTransferInRacing) || !isRacingStage() )
   {
      for( int i = 1; i < paraComm->getSize(); i++ )
      {
         if( i !=  receivedRank )
         {
            PARA_COMM_CALL(
                  paraComm->send( &globalBestCutOffValue, 1, ParaDOUBLE, i, TagCutOffValue )
            );
         }

      }
   }
   lcts.nDeletedInLc += paraNodePool->removeBoundedNodes(globalBestCutOffValue);
}

void
ParaLoadCoordinator::inactivateRacingSolverPool(
      int rank
      )
{
   nSolvedInInterruptedRacingSolvers = paraRacingSolverPool->getNnodesSolvedInBestSolver();
   nNodesLeftInInterruptedRacingSolvers = paraRacingSolverPool->getNnodesLeftInBestSolver();
   if( paraRacingSolverPool->isActive(rank) )   // if rank is the winner, it should be inactive
   {
      paraRacingSolverPool->inactivateSolver(rank);
   }
   if( paraSolverPool->isSolverActive(rank) )
   {
      /*
       * special timining problem
       *
       * 1113.58 S.4 I.SOL 0
       * 1113.58 S.3 is the racing winner! Selected strategy 2.
       * 1113.58 S.4 >(TERMINATED_IN_RACING_STAGE)
       *
       */
      if( paraSolverPool->isSolverInCollectingMode(rank) )
      {
         paraSolverPool->inactivateSolver(rank, 0, paraNodePool);
         // reschedule collecting mode
         if( !paraNodePoolBufferToRestart )
         {
            double tempTime = paraSolverPool->getSwichOutTime();
            paraSolverPool->switchOutCollectingMode();
            paraSolverPool->setSwichOutTime(tempTime);
            paraSolverPool->switchInCollectingMode(paraNodePool);
         }
      }
      else
      {
         paraSolverPool->inactivateSolver(rank, 0, paraNodePool);
      }
   }

   if ( (!interruptIsRequested) &&
         (!restartingRacing )&&
         paraRacingSolverPool->getNumActiveSolvers() == 0 )
   {
      /** if the computation is interrupted,
       *  paraRacingSolverPool is needed to output statistics */
      delete paraRacingSolverPool;
      paraRacingSolverPool = 0;
   }

}

void
ParaLoadCoordinator::sendTagToAllSolvers(
      const int tag
      )
{
   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      PARA_COMM_CALL(
            paraComm->send( NULL, 0, ParaBYTE, i, tag )
      );
   }
}

void
ParaLoadCoordinator::writeSubtreeInfo(
      int source,
      ParaCalculationState *calcState
      )
{
   if( logSubtreeInfoFlag )
   {
     ParaNode *node = paraSolverPool->getCurrentNode(source);
     *osLogSubtreeInfo  << paraTimer->getElapsedTime()
           << ", "
           << source
           << ", "
           << node->toSimpleString()
           << ", "
           << calcState->toSimpleString()
           << std::endl;
   }
}

void
ParaLoadCoordinator::initMergeNodesStructs(
      )
{
   assert( paraInitiator->getParaInstance()->getVarIndexRange() > 0 );
   varIndexTable = new ParaFixedValuePtr[paraInitiator->getParaInstance()->getVarIndexRange()];
   for( int i = 0; i < paraInitiator->getParaInstance()->getVarIndexRange(); i++ )
   {
      varIndexTable[i] = 0;
   }
   mergeInfoHead = 0;
   mergeInfoTail = 0;
}

void
ParaLoadCoordinator::addNodeToMergeNodeStructs(
      ParaNode *node
      )
{
   // Mergeing nodes look better to be restricted. It has to be tested
   if( nBoundChangesOfBestNode < 0 )
   {
      nBoundChangesOfBestNode = node->getDiffSubproblem()->getNBoundChanges();
   }
   if( nBoundChangesOfBestNode > 0 &&
         node->getDiffSubproblem() && node->getDiffSubproblem()->getNBoundChanges() <= nBoundChangesOfBestNode )
   {
      /*
      if( logSolvingStatusFlag )
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " The node " << node->toSimpleString() << " is out of merge candidates."
         << std::endl;
      }
      */
      return;   // prohibit to generate the same merging node twice
   }
   /*
   if( node->getMergingStatus() == 1 )
   {
      if( logSolvingStatusFlag )
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " The node " << node->toSimpleString() << " is out of merge candidates."
         << std::endl;
      }
      return;   // prohibit to generate the same merging node twice
   }
   */

   double startTime = paraTimer->getElapsedTime();
   //
   // create mergeNodeInfo and linked to paraNode
   //
   ParaMergeNodeInfo *mNode = new ParaMergeNodeInfo();
   mNode->status = ParaMergeNodeInfo::PARA_MERGING;
   mNode->nSameValueVariables = -1;
   mNode->nMergedNodes = -1;
   mNode->keyIndex = -1;
   mNode->nFixedVariables = 0;
   mNode->fixedVariables = 0;
   mNode->mergedTo = 0;
   mNode->paraNode = node;
   mNode->origDiffSubproblem = 0;
   mNode->mergedDiffSubproblem = 0;
   mNode->next = 0;
   node->setMergingStatus(0);      // checking
   node->setMergeNodeInfo(mNode);  // set merge node info.

   //
   // make the same value fixed variables links
   //
   /* get fixed variables array */
   if( node->getDiffSubproblem() )
   {
      mNode->nFixedVariables = node->getDiffSubproblem()->getFixedVariables(
            paraInitiator->getParaInstance(),
            &(mNode->fixedVariables));
   }
   else
   {
      mNode->nFixedVariables = 0;
   }
   if( mNode->nFixedVariables == 0 )  // cannot merge!
   {
      delete mNode;
      node->setMergingStatus(3);     // cannot be merged
      node->setMergeNodeInfo(0);
      lcts.addingNodeToMergeStructTime += paraTimer->getElapsedTime() - startTime;
      return;
   }

   //
   // add mergeNode to mergeNodeInfo list
   //
   if( mergeInfoTail == 0 )
   {
      mergeInfoTail = mNode;
      mergeInfoHead = mNode;
   }
   else
   {
      mergeInfoTail->next = mNode;
      mergeInfoTail = mNode;
   }

   for( int i = 0; i < mNode->nFixedVariables; i++ )
   {
      mNode->fixedVariables[i].mnode = mNode;
      ParaFixedValue *fixedValue = 0;
      if( varIndexTable[mNode->fixedVariables[i].index] == 0 )
      {
         fixedValue = new ParaFixedValue();
         fixedValue->value = mNode->fixedVariables[i].value;
         fixedValue->head = 0;
         fixedValue->tail = 0;
         fixedValue->next = 0;
         varIndexTable[mNode->fixedVariables[i].index] = fixedValue;
      }
      else
      {
         ParaFixedValue *prev = varIndexTable[mNode->fixedVariables[i].index];
         for( fixedValue = varIndexTable[mNode->fixedVariables[i].index];
               fixedValue != 0 &&  !EPSEQ( fixedValue->value, mNode->fixedVariables[i].value, DEFAULT_NUM_EPSILON );
               fixedValue = fixedValue->next )
         {
            prev = fixedValue;
         }
         if( fixedValue == 0 )
         {
            fixedValue = new ParaFixedValue();
            fixedValue->value = mNode->fixedVariables[i].value;
            fixedValue->head = 0;
            fixedValue->tail = 0;
            fixedValue->next = 0;
            prev->next = fixedValue;
         }
      }
      assert( fixedValue );
      if( fixedValue->tail == 0 )
      {
         fixedValue->head = &(mNode->fixedVariables[i]);
         fixedValue->tail = &(mNode->fixedVariables[i]);
      }
      else
      {
         fixedValue->tail->next = &(mNode->fixedVariables[i]);
         fixedValue->tail->next->prev = fixedValue->tail;
         fixedValue->tail = &(mNode->fixedVariables[i]);
      }
      for( ParaFixedVariable *p = fixedValue->head; p != fixedValue->tail; p = p->next )
      {
         (p->nSameValue)++;
      }
   }

   lcts.addingNodeToMergeStructTime += paraTimer->getElapsedTime() - startTime;

}

void
ParaLoadCoordinator::generateMergeNodesCandidates(
      )
{
   double startTime = paraTimer->getElapsedTime();

   ParaMergeNodeInfo *mPre = mergeInfoHead;
   ParaMergeNodeInfo *mNode = mergeInfoHead;
   mNode = mergeInfoHead;
   while( mNode )
   {
      assert( mNode->paraNode->getMergeNodeInfo() == mNode );
      if( mNode->status == ParaMergeNodeInfo::PARA_MERGING && mNode->nMergedNodes < 0 )
      {
         // make sorted variables list
         std::multimap<int, ParaSortedVariable, std::greater<int> > descendent;
         for( int i = 0; i < mNode->nFixedVariables; i++ )
         {
            ParaSortedVariable sortedVar;
            sortedVar.idxInFixedVariabes = i;
            sortedVar.fixedVariable = &(mNode->fixedVariables[i]);
            descendent.insert(std::make_pair(mNode->fixedVariables[i].nSameValue, sortedVar));

         }
         //
         //  try to make merge candidates
         //
         std::multimap<int, ParaSortedVariable, std::greater<int> >::iterator pos;
         pos = descendent.begin();
         mNode->keyIndex = pos->second.idxInFixedVariabes;
         mNode->nSameValueVariables = 1;
         ParaFixedVariable *traverse = mNode->fixedVariables[mNode->keyIndex].next;
         int nmNodes = 0;
         for( ;
               traverse;
               traverse = traverse->next )
         {
            if( traverse->mnode->status == ParaMergeNodeInfo::PARA_MERGING && traverse->mnode->nMergedNodes < 0 )
            {
               assert( traverse->mnode != mNode );
               traverse->mnode->mergedTo = mNode;
               traverse->mnode->nMergedNodes = 0;
               traverse->mnode->nSameValueVariables = 1;
               nmNodes++;
            }
         }
         ++pos;
         for( ; pos != descendent.end(); ++pos )
         {
            // check if there are merged nodes in case adding one more variable
            for( traverse = mNode->fixedVariables[pos->second.idxInFixedVariabes].next;
                  traverse;
                  traverse = traverse->next )
            {
               if( traverse->mnode->nMergedNodes == 0 && traverse->mnode->mergedTo == mNode )
               {
                  if( traverse->mnode->nSameValueVariables == mNode->nSameValueVariables )
                  {
                     break;   // at least one node can be merged
                  }
               }
            }
            if( traverse == 0 )  // cannot merge any nodes
            {
               break;
            }

            // merge nodes
            mNode->nSameValueVariables++;
            for( traverse = mNode->fixedVariables[pos->second.idxInFixedVariabes].next;
                  traverse;
                  traverse = traverse->next )
            {
               if( traverse->mnode->nMergedNodes == 0 && traverse->mnode->mergedTo == mNode )
               {
                  if( traverse->mnode->nSameValueVariables == (mNode->nSameValueVariables - 1) )
                  {
                     traverse->mnode->nSameValueVariables++;
                  }
               }
            }
         }

         // if the number of fixed variables is too small, then the merged node is not created
         if( nmNodes < 2 ||     // no merging nodes
               static_cast<int>((mNode->nFixedVariables)*paraParamSet->getRealParamValue(FixedVariablesRatioInMerging)) < 1 || //  0 same value variables are not allowed
               mNode->nSameValueVariables < (mNode->nFixedVariables)*paraParamSet->getRealParamValue(FixedVariablesRatioInMerging) ||
               ( nBoundChangesOfBestNode > 0 && mNode->nSameValueVariables <= nBoundChangesOfBestNode )
            )
         {
            for( ParaFixedVariable *cleanup = mNode->fixedVariables[mNode->keyIndex].next;
                  cleanup;
                  cleanup = cleanup->next )
            {
               if( cleanup->mnode->mergedTo == mNode )
               {
                  cleanup->mnode->nSameValueVariables = -1;
                  cleanup->mnode->nMergedNodes = -1;
                  cleanup->mnode->keyIndex = -1;
                  cleanup->mnode->mergedTo = 0;
                  assert( cleanup->mnode->status == ParaMergeNodeInfo::PARA_MERGING );
               }
            }
            assert( !(mNode->origDiffSubproblem) );
            assert( !(mNode->mergedDiffSubproblem) );
            mNode->paraNode->setMergeNodeInfo(0);
            mNode->paraNode->setMergingStatus(3);   // cannot merged
            ParaMergeNodeInfo *doomed = mNode;
            if( mNode == mergeInfoHead )
            {
               mergeInfoHead = mNode->next;
               mPre = mergeInfoHead;
               mNode = mergeInfoHead;
            }
            else
            {
               mPre->next = mNode->next;
               mNode = mNode->next;
            }
            if( mNode == mergeInfoTail )
            {
               mergeInfoTail = mPre;
            }
            deleteMergeNodeInfo(doomed);
         }
         else  // cleanup and merge nodes
         {
            int nMergedNodes = 0;
            for( ParaFixedVariable *cleanup = mNode->fixedVariables[mNode->keyIndex].next;
                  cleanup;
                  cleanup = cleanup->next )
            {
               if( cleanup->mnode->mergedTo == mNode )
               {
                  if( mNode->nSameValueVariables == cleanup->mnode->nSameValueVariables )
                  {
                     nMergedNodes++;
                     cleanup->mnode->status = ParaMergeNodeInfo::PARA_MERGE_CHECKING_TO_OTHER_NODE;
                  }
                  else
                  {
                     assert( cleanup->mnode->status == ParaMergeNodeInfo::PARA_MERGING );
                     cleanup->mnode->nSameValueVariables = -1;
                     cleanup->mnode->nMergedNodes = -1;
                     cleanup->mnode->keyIndex = -1;
                     cleanup->mnode->mergedTo = 0;
                  }
               }
            }
            mNode->nMergedNodes = nMergedNodes;
            assert(nMergedNodes > 0);
            int n = 0;
            ParaFixedVariable *fixedVariables = new ParaFixedVariable[mNode->nSameValueVariables];
            for( pos = descendent.begin(); pos != descendent.end(); ++pos )
            {
               fixedVariables[n] = *(pos->second.fixedVariable);
               n++;
               if( n == mNode->nSameValueVariables ) break;
            }
            mNode->origDiffSubproblem = mNode->paraNode->getDiffSubproblem();
            mNode->mergedDiffSubproblem = mNode->origDiffSubproblem->createDiffSubproblem(paraComm, paraInitiator, n, fixedVariables );
            delete [] fixedVariables;
            mNode->paraNode->setDiffSubproblem(mNode->mergedDiffSubproblem);
            mNode->status = ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE;
            assert( mNode->mergedTo == 0 );
            mPre = mNode;
            mNode = mNode->next;
         }
      }
      else
      {
         mPre = mNode;
         mNode = mNode->next;
      }
   }

   // remove data which are not used anymore.
   if( varIndexTable )
   {
      for( int i = 0; i < paraInitiator->getParaInstance()->getVarIndexRange(); i++ )
      {
         if( varIndexTable[i] )
         {
            while ( varIndexTable[i] )
            {
               ParaFixedValue *del = varIndexTable[i];
               varIndexTable[i] = varIndexTable[i]->next;
               delete del;
            }
         }
      }
      delete [] varIndexTable;
      varIndexTable = 0;
   }

   lcts.generateMergeNodesCandidatesTime += paraTimer->getElapsedTime() - startTime;
}

void
ParaLoadCoordinator::regenerateMergeNodesCandidates(
      ParaNode *node
      )
{
   double startTime = paraTimer->getElapsedTime();

   ParaMergeNodeInfo *mNode = node->getMergeNodeInfo();
   assert(mNode);
   assert(mNode->paraNode == node);
   assert(mNode->status == ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE);
   assert(mNode->mergedTo == 0);
   node->setMergeNodeInfo(0);
   node->resetDualBoundValue();
   node->setDiffSubproblem(mNode->origDiffSubproblem);
   delete mNode->mergedDiffSubproblem;
   mNode->mergedDiffSubproblem = 0;
   assert( mNode->status != ParaMergeNodeInfo::PARA_MERGING );
   // set new range
   mergeInfoHead =  0;
   mergeInfoTail = 0;
   ParaMergeNodeInfo *mPrev = 0;
   for( ParaFixedVariable *traverse = mNode->fixedVariables[mNode->keyIndex].next;
         traverse;
         traverse = traverse->next )
   {
      if( mergeInfoTail )
      {
         mPrev->next = traverse->mnode;
         mergeInfoTail = traverse->mnode;
         mPrev = traverse->mnode;
      }
      if( mNode == traverse->mnode->mergedTo )
      {
         if( !mergeInfoHead )
         {
            mergeInfoHead = traverse->mnode;
            mergeInfoTail = traverse->mnode;
            mPrev = traverse->mnode;
         }
         assert( traverse->mnode->status == ParaMergeNodeInfo::PARA_MERGE_CHECKING_TO_OTHER_NODE );
      }
   }
   if( mergeInfoHead )
   {
      assert(mergeInfoTail);
      mergeInfoTail->next = 0;
   }

   // remove mnode
   mNode->paraNode->setMergingStatus(-1);  // no merging node
   deleteMergeNodeInfo(mNode);
   if( mergeInfoHead )
   {
      generateMergeNodesCandidates();
   }

   lcts.regenerateMergeNodesCandidatesTime += paraTimer->getElapsedTime() - startTime;
}

void
ParaLoadCoordinator::deleteMergeNodeInfo(
      ParaMergeNodeInfo *mNode
      )
{

   if( mNode->nMergedNodes == 0 && mNode->mergedTo )
   {
      assert(mNode->status == ParaMergeNodeInfo::PARA_MERGE_CHECKING_TO_OTHER_NODE);
      assert(mNode->mergedTo->status ==  ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE);
      assert(mNode->mergedTo->mergedTo == 0);
      mNode->mergedTo->nMergedNodes--;
      if( mNode->mergedTo->nMergedNodes == 0 && mNode->mergedTo->paraNode->getMergeNodeInfo() )
      {
         assert(mNode->mergedTo == mNode->mergedTo->paraNode->getMergeNodeInfo() );
         mNode->mergedTo->paraNode->setDiffSubproblem(mNode->mergedTo->origDiffSubproblem);
         mNode->mergedTo->paraNode->setMergeNodeInfo(0);
         mNode->mergedTo->paraNode->setMergingStatus(-1);
         delete mNode->mergedTo->mergedDiffSubproblem;
         mNode->mergedTo->mergedDiffSubproblem = 0;
         mNode->mergedTo->origDiffSubproblem = 0;
         deleteMergeNodeInfo(mNode->mergedTo);
      }
      mNode->mergedTo = 0;

   }

   if( mNode->status == ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE)
   {
      assert( mNode->mergedTo == 0 );
      if( mNode->paraNode->getMergingStatus() == -1 )  // merging failed
      {
         for( ParaFixedVariable *traverse = mNode->fixedVariables[mNode->keyIndex].next;
               traverse;
               traverse = traverse->next )
         {
            if( traverse->mnode->nMergedNodes == 0 && mNode == traverse->mnode->mergedTo )
            {
               traverse->mnode->mergedTo->nMergedNodes--;
               if( traverse->mnode->mergedTo->nMergedNodes == 0 && traverse->mnode->mergedTo->paraNode->getMergeNodeInfo() )
               {
                  // NOTE; mNode == traverse->mnode->mergedTo:
                  traverse->mnode->mergedTo->paraNode->setDiffSubproblem(traverse->mnode->mergedTo->origDiffSubproblem);
                  traverse->mnode->mergedTo->paraNode->setMergeNodeInfo(0);
                  traverse->mnode->mergedTo->paraNode->setMergingStatus(-1);
                  delete traverse->mnode->mergedTo->mergedDiffSubproblem;
                  traverse->mnode->mergedTo->mergedDiffSubproblem = 0;
                  traverse->mnode->mergedTo->origDiffSubproblem = 0;
                  assert( traverse->mnode->mergedTo->mergedTo == 0);
                  deleteMergeNodeInfo(traverse->mnode->mergedTo);
               }
               traverse->mnode->mergedTo = 0;
               traverse->mnode->paraNode->setMergingStatus(0);
               traverse->mnode->status = ParaMergeNodeInfo::PARA_MERGING;
               traverse->mnode->nMergedNodes = -1;
               traverse->mnode->nSameValueVariables = -1;
               traverse->mnode->keyIndex = -1;
            }
         }
      }
      else
      {
         for( ParaFixedVariable *traverse = mNode->fixedVariables[mNode->keyIndex].next;
               traverse;
               traverse = traverse->next )
         {
            if( traverse->mnode->nMergedNodes == 0 && mNode == traverse->mnode->mergedTo )
            {
               traverse->mnode->mergedTo->nMergedNodes--;
               if( traverse->mnode->mergedTo->nMergedNodes == 0 && traverse->mnode->mergedTo->paraNode->getMergeNodeInfo() )
               {
                  // NOTE; mNode == traverse->mnode->mergedTo:
                  traverse->mnode->mergedTo->paraNode->setDiffSubproblem(traverse->mnode->mergedTo->origDiffSubproblem);
                  traverse->mnode->mergedTo->paraNode->setMergeNodeInfo(0);
                  traverse->mnode->mergedTo->paraNode->setMergingStatus(-1);
                  delete traverse->mnode->mergedDiffSubproblem;
                  traverse->mnode->mergedTo->mergedDiffSubproblem = 0;
                  traverse->mnode->mergedTo->origDiffSubproblem = 0;
                  assert( traverse->mnode->mergedTo->mergedTo == 0);
                  deleteMergeNodeInfo(traverse->mnode->mergedTo);
               }
               traverse->mnode->mergedTo = 0;
               if( traverse->mnode->paraNode->getDualBoundValue() < mNode->paraNode->getDualBoundValue() )
               {
                  traverse->mnode->paraNode->setMergingStatus(0);
                  traverse->mnode->status = ParaMergeNodeInfo::PARA_MERGING;
                  traverse->mnode->nMergedNodes = -1;
                  traverse->mnode->nSameValueVariables = -1;
                  traverse->mnode->keyIndex = -1;
               }
               else
               {
                  traverse->mnode->paraNode->setMergingStatus(4);  // merging representative was deleted -> this node should be deleted
                  traverse->mnode->status = ParaMergeNodeInfo::PARA_DELETED;
                  traverse->mnode->nMergedNodes = -1;
                  traverse->mnode->nSameValueVariables = -1;
                  traverse->mnode->keyIndex = -1;
               }
            }
         }
      }
   }

   if( mNode->fixedVariables )
   {
      for( int i = 0; i < mNode->nFixedVariables; i++ )
      {
         for( ParaFixedVariable *traverse = mNode->fixedVariables[i].prev;
               traverse;
               traverse = traverse->prev
               )
         {
            traverse->nSameValue--;
         }
         if( mNode->fixedVariables[i].prev )
         {
            mNode->fixedVariables[i].prev->next = mNode->fixedVariables[i].next;
            if( mNode->fixedVariables[i].next )
            {
               mNode->fixedVariables[i].next->prev = mNode->fixedVariables[i].prev;
            }
            else
            {
               mNode->fixedVariables[i].prev->next = 0;
            }
         }
         else  // prev == 0
         {
            if( mNode->fixedVariables[i].next )
            {
               mNode->fixedVariables[i].next->prev = 0;
            }
         }

      }
      delete [] mNode->fixedVariables;
   }
   delete mNode;
}

void
ParaLoadCoordinator::mergeNodes(
      ParaNode *node
      )
{
   double startTime = paraTimer->getElapsedTime();

   ParaMergeNodeInfo *mNode = node->getMergeNodeInfo();
   assert( mNode->status == ParaMergeNodeInfo::PARA_MERGED_RPRESENTATIVE );
   assert( mNode->mergedTo == 0 );
   ParaMergedNodeListElement *head = 0;
   ParaMergedNodeListElement *cur = 0;
   int nMerged = 0;
   for( ParaFixedVariable *traverse = mNode->fixedVariables[mNode->keyIndex].next;
         traverse;
         traverse = traverse->next )
   {
      if( traverse->mnode->nMergedNodes == 0 && mNode == traverse->mnode->mergedTo )
      {
         if( head == 0 )
         {
            head = cur = new ParaMergedNodeListElement();
         }
         else
         {
            cur->next = new ParaMergedNodeListElement();
            cur = cur->next;
         }
         cur->node = traverse->mnode->paraNode;
         cur->node->setMergingStatus(2);
         cur->next = 0;
         nMerged++;
#ifdef UG_DEBUG_SOLUTION
         if( cur->node->getDiffSubproblem() && cur->node->getDiffSubproblem()->isOptimalSolIncluded() )
         {
            assert(node->getDiffSubproblem());
            node->getDiffSubproblem()->setOptimalSolIndicator(1);
         }
#endif
      }
   }
   assert( mNode->nMergedNodes == nMerged);
   int delNodes = 0;
   if ( head )
   {
      delNodes = paraNodePool->removeMergedNodes(head);
   }
   assert( delNodes == nMerged );
   if( delNodes != nMerged )
   {
	   THROW_LOGICAL_ERROR4("delNodes != nMerged, delNodes = ", delNodes, ", nMerged = ", nMerged );
   }
   node->setDiffSubproblem(mNode->mergedDiffSubproblem);
   delete mNode->origDiffSubproblem;
   node->setMergeNodeInfo(0);
   node->setMergingStatus(1);
   assert(mNode->mergedTo == 0);
   mNode->mergedDiffSubproblem = 0;
   mNode->origDiffSubproblem = 0;
   deleteMergeNodeInfo(mNode);
   lcts.nDeletedByMerging += nMerged;
   if( logSolvingStatusFlag )
   {
      *osLogSolvingStatus << (nMerged + 1) <<
            " nodes are merged at " <<
            paraTimer->getElapsedTime() << " seconds."
            << "Dual bound: " << paraInitiator->convertToExternalValue( node->getDualBoundValue() )
            << std::endl;
   }

   lcts.mergeNodeTime += paraTimer->getElapsedTime() - startTime;

}

int
ParaLoadCoordinator::restartRacing(
      )
{
   // RESTART RACING MAY NOT WORK WITH DETERMINSTIC MODE: NOT DEBUGGED

   if( paraInitiator->reInit(nRestartedRacing) )
   {
      restartingRacing = false;
      return 1;
   }

   ParaInstance *paraInstance = paraInitiator->getParaInstance();
   paraInstance->bcast(paraComm, 0, paraParamSet->getIntParamValue(InstanceTransferMethod));

   ParaNode *rootNode = paraRacingSolverPool->extractNode();
   rootNode->setDualBoundValue(-DBL_MAX);
   rootNode->setInitialDualBoundValue(-DBL_MAX);
   rootNode->setEstimatedValue(-DBL_MAX);

   /** recreate racing solver pool */
   delete paraRacingSolverPool;
   paraRacingSolverPool = new ParaRacingSolverPool(
         1,                // paraSolver origin rank
         paraComm, paraParamSet, paraTimer, paraDetTimer);

   /** activate racing solver with root node */
   PARA_COMM_CALL(
         rootNode->bcast(paraComm, 0)
         );
   paraRacingSolverPool->activate(rootNode);
   lcts.nSent++;

   nRestartedRacing++;

   if( osStatisticsRacingRampUp )
   {
      *osStatisticsRacingRampUp << "##################################" << std::endl;
      *osStatisticsRacingRampUp << "### Racing restarted " << nRestartedRacing << " times" << std::endl;
      *osStatisticsRacingRampUp << "##################################" << std::endl;
   }

   if( !paraParamSet->getBoolParamValue(Quiet) )
   {
      if( logSolvingStatusFlag )
      {
         *osLogSolvingStatus << paraTimer->getElapsedTime()
         << " Racing Ramp-up restarted." << std::endl;
      }
   }

   if( outputTabularSolvingStatusFlag )
   {
      *osTabularSolvingStatus << "Racing Ramp-up restarted." << std::endl;
   }

   primalUpdated = false;
   restartingRacing = false;

   lcts.globalBestDualBoundValue = -DBL_MAX;
   lcts.externalGlobalBestDualBoundValue = -DBL_MAX;

   return 0;
}

void
ParaLoadCoordinator::changeSearchStrategyOfAllSolversToBestBoundSearch(
      )
{
   /* Not implemented yet
   int bestBoundSearch = 1;
   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      PARA_COMM_CALL(
         paraComm->send( &bestBoundSearch, 1, ParaINT, i, TagChangeSearchStrategy )
      );
   }
   */
}

void
ParaLoadCoordinator::changeSearchStrategyOfAllSolversToOriginalSearch(
   )
{
   /* Not implemented yet
   int originalSearch = 0;
   for( int i = 1; i < paraComm->getSize(); i++ )
   {
      PARA_COMM_CALL(
         paraComm->send( &originalSearch, 1, ParaINT, i, TagChangeSearchStrategy )
      );
   }
   */
}


#ifdef UG_WITH_UGS
int
ParaLoadCoordinator::checkAndReadIncumbent(
      )
{
   int source = -1;
   int tag = -1;

   assert(commUgs);

   while( commUgs->iProbe(&source, &tag) )
   {
      if( source == 0 && tag == UGS::TagUpdateIncumbent )
      {
         if( paraInitiator->readUgsIncumbentSolution(commUgs, source) )
         {
            double globalBestIncumbentValue = paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue();
            if( !paraParamSet->getBoolParamValue(NoUpperBoundTransferInRacing) || !isRacingStage() )
            {
               if( paraInitiator->canGenerateSpecialCutOffValue() )
               {
                  for( int i = 1; i < paraComm->getSize(); i++ )
                  {
                     sendCutOffValue(i);
                     PARA_COMM_CALL(
                           paraComm->send( &globalBestIncumbentValue, 1, ParaDOUBLE, i, TagIncumbentValue )
                     );
                  }
               }
               else
               {
                  for( int i = 1; i < paraComm->getSize(); i++ )
                  {
                     PARA_COMM_CALL(
                           paraComm->send( &globalBestIncumbentValue, 1, ParaDOUBLE, i, TagIncumbentValue )
                     );
                  }
               }

            }
            lcts.nDeletedInLc += paraNodePool->removeBoundedNodes(globalBestIncumbentValue);

            primalUpdated = true;
            allCompInfeasibleAfterSolution = true;
            if( logSolvingStatusFlag )
            {
               *osLogSolvingStatus << paraTimer->getElapsedTime()
               << " S." << source << " I.SOL "
               << paraInitiator->convertToExternalValue(
                     paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue()
                     ) << std::endl;
            }
#ifdef _DEBUG_LB
            std::cout << paraTimer->getElapsedTime()
            << " S." << source << " I.SOL "
            << paraInitiator->convertToExternalValue(
                  paraInitiator->getGlobalBestIncumbentSolution()->getObjectiveFuntionValue()
                  ) << std::endl;
#endif
            /** output tabular solving status */
            if( outputTabularSolvingStatusFlag )
            {
               outputTabularSolvingStatus('*');
            }
#ifdef UG_WITH_ZLIB
            /* Do not have to remove ParaNodes from NodePool. It is checked and removed before sending them */
            /** save incumbent solution */
            char solutionFileNameTemp[256];
            char solutionFileName[256];
            if( paraParamSet->getBoolParamValue(Checkpoint) && paraComm->getRank() == 0  )
            {
               sprintf(solutionFileNameTemp,"%s%s_after_checkpointing_solution_t.gz",
                     paraParamSet->getStringParamValue(CheckpointFilePath),
                     paraInitiator->getParaInstance()->getProbName() );
               paraInitiator->writeCheckpointSolution(std::string(solutionFileNameTemp));
               sprintf(solutionFileName,"%s%s_after_checkpointing_solution.gz",
                     paraParamSet->getStringParamValue(CheckpointFilePath),
                     paraInitiator->getParaInstance()->getProbName() );
               if ( rename(solutionFileNameTemp, solutionFileName) )
               {
                  std::cout << "after checkpointing solution file cannot be renamed: errno = " << strerror(errno) << std::endl;
                  exit(1);
               }
            }
#endif
         }
      }
      else if ( source == 0 && tag == TagTerminated )
      {
         return 1;
      }
      else
      {
         THROW_LOGICAL_ERROR5("Invalid Tag = ", tag, ", from source = ", source, " received.");
      }
   }
   return 0;

}
#endif
