// -*- LSST-C++ -*-
#include <stdlib.h>
#include <stdio.h> 
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <getopt.h>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <math.h>
#include <omp.h>

#include "lsst/mops/common.h"
#include "lsst/mops/KDTree.h"
#include "lsst/mops/MopsDetection.h"
#include "lsst/mops/daymops/findTracklets/findTracklets.h"

#define LEAF_NODE_SIZE 16

#define uint unsigned int

namespace lsst {
namespace mops {

/***
    Prototypes - these don't need to be seen by files which include findTracklets.h
***/

/*****************************************************************
 * Populate 2D vector of detections.
 * Each outer vector contains Detections of equal MJD.
 *****************************************************************/
void groupByImageTime(const std::vector<MopsDetection>&,
		      std::map<double, std::vector<MopsDetection> >&);


/******************************************************************
 * Take 2D vector of detections and create a map linking
 * each MJD vector to its MJD double value.
 ******************************************************************/
void generatePerImageTrees(const std::map<double, std::vector<MopsDetection> > &detectionSets, 
                           std::map<double, KDTree<long int> > &myTreeMap);


/******************************************************************
 * Given a mapping of MJDs to KDTrees of PointAndValue pairs 
 * index by file line number index, generate tracklets for each 
 * query point within a distance determined by maxVelocity.
 ******************************************************************/

void getTracklets(TrackletVector &resultsVec,  
		  const std::map<double, KDTree<long int> > &myTreeMap,
		  const std::vector<MopsDetection> &queryPoints,
		  findTrackletsConfig config);





/*****************************************************************
 *The main function of this file.
 *****************************************************************/
TrackletVector * findTracklets(const std::vector<MopsDetection> &myDets, 
                               findTrackletsConfig config)
{
    //detection vectors, each vector of unique MJD
    std::map<double,  std::vector<MopsDetection> > detectionSets; 

    //vector of RA and dec pairs for later searching
    std::vector<double> queryPoints; 

    //link MJD to KDTree of unique MJD detections
    std::map<double, KDTree<long int> > myTreeMap; 

    groupByImageTime(myDets, 
                     detectionSets);
    
    generatePerImageTrees(detectionSets, myTreeMap);

    //get results
    TrackletVector * resultsVec;

    if (config.outputMethod == RETURN_TRACKLETS) {
        resultsVec = new TrackletVector();
    }
    else if (config.outputMethod == IDS_FILE) {
        resultsVec = new TrackletVector(config.outputFile, false, 0);
    }
    else if (config.outputMethod == IDS_FILE_WITH_CACHE) {
        resultsVec = new TrackletVector(config.outputFile, true, config.outputBufferSize);
    }
    else {
        throw LSST_EXCEPT(BadParameterException, 
                          "findTracklets: got unknown or unimplemented output method.");
    }

    getTracklets(*resultsVec, myTreeMap, 
                 myDets, config);

    if ((config.outputMethod == IDS_FILE) || 
        (config.outputMethod == IDS_FILE_WITH_CACHE)) {

        resultsVec->purgeToFile();
        delete resultsVec;
        resultsVec = NULL;
    }
    

    return resultsVec;
}



/*****************************************************************
 * Populate 2D vector of detections.
 * Each outer vector contains Detections of equal MJD.
 *****************************************************************/
void groupByImageTime(const std::vector<MopsDetection> &myDets, 
		      std::map<double, std::vector<MopsDetection> > &detectionSets)
{
    // maps by default sort on their first parameter.
    // build a map from detection time to all dets at that time. we'll have a nice sorted
    // set of vectors.

    for (unsigned int i = 0; i < myDets.size(); i++) {
        detectionSets[myDets.at(i).getEpochMJD()].push_back(myDets.at(i));
    }
    
}




/******************************************************************
 * Take 2D vector of detections and create a map linking
 * each per-MJD Detection vector to its MJD double value.
 ******************************************************************/
void generatePerImageTrees(const std::map<double, std::vector<MopsDetection> > &detectionSets, 
                           std::map<double, KDTree<long int> > &myTreeMap)
{

    // for each vector representing a single EpochMJD, created
    // a KDTree containing mapping detection RA, Decs -> detection IDs

    std::map<double, std::vector<MopsDetection> >::const_iterator imageIter;

    for(imageIter = detectionSets.begin(); imageIter != detectionSets.end(); imageIter++) {

        const std::vector<MopsDetection> *thisDetVec = &(imageIter->second);
        double thisEpoch = imageIter->first;
        std::vector<PointAndValue<long int> > vecPV;
        
        for(unsigned int j=0; j < thisDetVec->size(); j++) {

            PointAndValue<long int> tempPV;
            std::vector<double> pairRADec;
            
            pairRADec.push_back(convertToStandardDegrees(thisDetVec->at(j).getRA()));                    
            pairRADec.push_back(convertToStandardDegrees(thisDetVec->at(j).getDec()));
            
            tempPV.setPoint(pairRADec);
            tempPV.setValue(thisDetVec->at(j).getID());
            vecPV.push_back(tempPV);
        }
        
        KDTree<long int> tempKDTree(vecPV, 2, LEAF_NODE_SIZE);
        myTreeMap.insert(std::make_pair(thisEpoch, tempKDTree) );
    }
}



/******************************************************************
 * Given a mapping of MJDs to KDTrees of PointAndValue pairs 
 * index by file line number index, generate tracklets for each 
 * query point within a distance determined by maxVelocity.
 ******************************************************************/
void getTracklets(TrackletVector &results,  
		  const std::map<double, KDTree<long int> > &myTreeMap,
		  const std::vector<MopsDetection> &queryPoints,
		  findTrackletsConfig config)
{
  clock_t start = std::clock();
  int nthreads, tid;
  
#pragma omp parallel private(nthreads, tid)
    {
      nthreads = omp_get_num_threads();
      tid = omp_get_thread_num();
      unsigned int workloadSize = queryPoints.size() / nthreads;
      unsigned int myStart = tid*workloadSize;
      unsigned int myEnd = (tid+1)*workloadSize;
      if (tid == nthreads - 1) { myEnd = queryPoints.size();}
#pragma omp critical(stdout)
      {
	if(tid == 0) {
	  std::cout << "Number of threads " << nthreads << std::endl;
	  std::cout << "Number of work items: " << queryPoints.size() << std::endl;
	  std::cout << "Work/CPU: " << workloadSize << std::endl;
	}	
	std::cout << "Thread " << tid << " working on items " << myStart 
		  << " through " << myEnd << std::endl;
      }

      std::cout << "Thread " << tid << " starting work... " << std::endl;
      clock_t threadStart = std::clock();
      unsigned int count = 0;

      std::vector<Tracklet> localResults;
      std::vector<GeometryType> myGeos;
      // we search RA, Dec only.
      myGeos.push_back(RA_DEGREES);
      myGeos.push_back(DEC_DEGREES);

      clock_t queryTime = 0;

      for(unsigned int i=myStart; i<myEnd; i++){
	count++;
	if (count % (workloadSize / 4) == 0) {
	  std::cout << "Thread " << tid << " has finished " 
		    << 100.*(1.0*count) / (1.0*workloadSize) << "% of its work\n";
	}
	
	// vectors of RADecRangeSearch parameters we search
	// exclusively in RA, Dec; the "otherDims" parameters sent to
	// KDTree range search are empty.
	std::vector<double> otherDimsTolerances;
	std::vector<double> otherDimsPt;
	
	  const MopsDetection * curQuery = &(queryPoints.at(i));
	  double queryRA = convertToStandardDegrees(curQuery->getRA());
	  double queryDec = convertToStandardDegrees(curQuery->getDec());
	  double queryMJD = curQuery->getEpochMJD();
	  
	  // iterate through each KDTree of detections, where each KDTree
	  // represents a unique MJD
	  std::map<double, KDTree<long int> >::const_iterator iter;
	  for(iter = myTreeMap.begin(); iter != myTreeMap.end(); iter++) {
	    
            double curMJD = iter->first;     //map key
            const KDTree<long int> *curTree = &(iter->second); //value associated with key
	    
            //only consider this tree if it contains detections
            //that occurred after the current one
            if ((curMJD > queryMJD) && 
                (curMJD - queryMJD >= config.minDt) && 
                (curMJD - queryMJD <= config.maxDt)) {
	      
	      double maxVelocity = config.maxV;
	      double minVelocity = config.minV;
	      
	      double maxDistance = (curMJD - queryMJD) * maxVelocity;
	      double minDistance = (curMJD - queryMJD) * minVelocity;
	      std::vector<double> queryPt;
	      queryPt.push_back(queryRA);
	      queryPt.push_back(queryDec);
	      
	      std::vector<PointAndValue<long int> > queryResults;
	      
	      // do a rectangular search around this point. note that we 
	      // use the haversine great-circle distance in the tree, so we are
	      // sure that we get any object within maxDistance (and a few others)
	      clock_t startQuery = std::clock();
	      queryResults = curTree->RADecRangeSearch(queryPt, maxDistance,
						       otherDimsPt, otherDimsTolerances,
						       myGeos);
	      queryTime += std::clock() - startQuery;
	      // filter the results, getting the items which are actually within
	      // the circle we are searching, not the rectangle enclosing it.
	      std::vector<long int> closeEnoughResults;
              
	      for (unsigned int ii = 0; ii < queryResults.size(); ii++) {
		PointAndValue<long int> * curResult = &(queryResults.at(ii));
		double resultRa = curResult->getPoint().at(0);
		double resultDec = curResult->getPoint().at(1);
		double properDistance =  angularDistanceRADec_deg(queryRA, 
								  queryDec, 
								  resultRa,
								  resultDec);
		if ((properDistance <= maxDistance) && (properDistance >= minDistance)) {
		  closeEnoughResults.push_back(curResult->getValue());
		}
	      }
	      
	      for (unsigned int ii = 0; ii < closeEnoughResults.size(); ii++) {
		// collect results for each query point's results for each MJD
		Tracklet newTracklet;
		newTracklet.indices.insert(curQuery->getID());               
		newTracklet.indices.insert(closeEnoughResults.at(ii));
		// copy results to local vector, avoid overhead of a
		// critical section till done searching
		localResults.push_back(newTracklet);
	      }
	      
	      queryResults.clear();
	      queryPt.clear();
	    }
	  }
      }

      std::cout << "Thread " << tid << " finished " << count << " items after " << std::fixed 
		<< std::setprecision(10) << timeElapsed(threadStart) << " sec." 
		<< "  Spent " << std::fixed << std::setprecision(10) 
		<< 1.0*queryTime / (1.0*CLOCKS_PER_SEC) << " sec on tree queries." << std::endl;
      

      // now copy results to output to the shared result vector.
#pragma omp critical(writeResults)
      {
	for (unsigned int i = 0; i < localResults.size(); i++) {
	  results.push_back(localResults[i]);
	}
      }

    } // end parallel section.



    double dif = lsst::mops::timeElapsed(start);
    std::cout << "Linking took " << std::fixed << std::setprecision(10)
	      << dif << " seconds." << std::endl;
}


}} // close lsst::mops