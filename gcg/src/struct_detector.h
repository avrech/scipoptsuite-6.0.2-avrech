/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program                         */
/*          GCG --- Generic Column Generation                                */
/*                  a Dantzig-Wolfe decomposition based extension            */
/*                  of the branch-cut-and-price framework                    */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/* Copyright (C) 2010-2019 Operations Research, RWTH Aachen University       */
/*                         Zuse Institute Berlin (ZIB)                       */
/*                                                                           */
/* This program is free software; you can redistribute it and/or             */
/* modify it under the terms of the GNU Lesser General Public License        */
/* as published by the Free Software Foundation; either version 3            */
/* of the License, or (at your option) any later version.                    */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU Lesser General Public License for more details.                       */
/*                                                                           */
/* You should have received a copy of the GNU Lesser General Public License  */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.*/
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   struct_detector.h
 * @brief  data structures for detectors
 * @author Martin Bergner
 * @author Christian Puchert
 * @author Michael Bastubbe
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef GCG_STRUCT_DETECTOR_H__
#define GCG_STRUCT_DETECTOR_H__

#include "type_detector.h"



/** detector data structure */
struct DEC_Detector {
   const char*           name;               /**< name of the detector */
   DEC_DETECTORDATA*     decdata;            /**< custom data structure of the detectors */
   char                  decchar;            /**< display character of detector */
   const char*           description;        /**< description of the detector */
   int                   freqCallRound;      /** frequency the detector gets called in detection loop ,ie it is called in round r if and only if minCallRound <= r <= maxCallRound AND  (r - minCallRound) mod freqCallRound == 0 */
   int                   maxCallRound;       /** last round the detector gets called                              */
   int                   minCallRound;       /** first round the detector gets called (offset in detection loop) */
   int                   freqCallRoundOriginal; /** frequency the detector gets called in detection loop while detecting the original problem */
   int                   maxCallRoundOriginal; /** last round the detector gets called while detecting the original problem */
   int                   minCallRoundOriginal; /** first round the detector gets calles (offset in detection loop) while detecting the original problem */
   int                   priority;           /**< detector priority */
   SCIP_Bool             enabled;            /**< flag to indicate whether detector is enabled */
   SCIP_Bool             enabledOrig;   /**< flag to indicate whether detector is enabled for the original problem */
   SCIP_Bool             enabledFinishing;   /**< flag to indicate whether finishing is enabled */
   SCIP_Bool             enabledPostprocessing;   /**< flag to indicate whether finishing is enabled */
   SCIP_Bool             skip;               /**< should detector be skipped if other detectors found decompositions */
   SCIP_Bool             usefulRecall;       /** is it useful to call this detector on a descendant of the propagated seeed */
   SCIP_Bool             legacymode;         /**< flag to indicate whether (old) DETECTSTRUCTURE method should also be used for detection */
   SCIP_Bool             overruleemphasis;   /**< should the emphasis settings be overruled */
   DEC_DECOMP**          decomps;            /**< decompositions this detector has found */
   int                   ndecomps;           /**< number of decompositions the detector has found */
   SCIP_Real             dectime;            /**< time the detector took to find decompositions */

   DEC_DECL_FREEDETECTOR((*freeDetector));  /**< destructor of detector */
   DEC_DECL_INITDETECTOR((*initDetector));  /**< initialization method of detector */
   DEC_DECL_EXITDETECTOR((*exitDetector));  /**< deinitialization method of detector */
   DEC_DECL_DETECTSTRUCTURE((*detectStructure)); /**< structure detection method of detector */
   DEC_DECL_EXITDETECTOR((*exitDetection));  /**< deinitialization method of detector */
   DEC_DECL_PROPAGATESEEED((*propagateSeeed));
   DEC_DECL_PROPAGATEFROMTOOLBOX((*propagateFromToolbox));
   DEC_DECL_FINISHFROMTOOLBOX((*finishFromToolbox));
   DEC_DECL_FINISHSEEED((*finishSeeed));
   DEC_DECL_POSTPROCESSSEEED((*postprocessSeeed));
   DEC_DECL_SETPARAMAGGRESSIVE((*setParamAggressive));
   DEC_DECL_SETPARAMDEFAULT((*setParamDefault));
   DEC_DECL_SETPARAMFAST((*setParamFast));


};


#endif
