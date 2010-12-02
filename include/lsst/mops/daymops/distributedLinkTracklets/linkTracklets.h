// -*- LSST-C++ -*-
/* jonathan myers */

#ifndef LSST_LINKTRACKLETS_H_
#define LSST_LINKTRACKLETS_H_

#include <vector>

#include "lsst/mops/MopsDetection.h"
#include "lsst/mops/Tracklet.h"
#include "lsst/mops/Track.h"
#include "lsst/mops/TrackSet.h"




/*
 * linkTrackletsConfig is a simple class which holds the many configurable
 * parameters for running linkTracklets.  The default values should be 
 * fine for LSST.
 */

namespace lsst { namespace mops {

class linkTrackletsConfig {
public:

    linkTrackletsConfig() 
        {   
            maxRAAccel = .02; 
            maxDecAccel = .02; 
            detectionLocationErrorThresh = .002; 
            minEndpointTimeSeparation = 2; 
            minSupportToEndpointTimeSeparation = .5;
            minSupportTracklets = 1;
            quadraticFitErrorThresh = 0.;
            minDetectionsPerTrack = 6;

            restrictTrackStartTimes = false;
            latestFirstEndpointTime = -1;
            restrictTrackEndTimes = false;
            earliestLastEndpointTime = -1;

            velocityErrorThresh = .00025;//.5;  per Jon's email 5/20/10
	    //.00025 turned out to miss some potential tracks;  11/23/10 - setting back to .00025 to test out performance

            
        }

    /* acceleration terms are in degrees/(day^2)
       
       objects which accelerate faster than these maximum acceleration params
       are not reported (or considered)
     */
    double maxRAAccel;
    double maxDecAccel;


    /* 
       if you will perform repeated runs, it may be wise to look for tracks
       which start or end on a limited set of nights or times.

       If you'd like to restrict linkTracklets to finding only tracks which
       start at or before time X, set restrictTrackStarTimes to true and
       latestFirstEndpointTime to X.

       if you'd like to look only for tracks which *end* after time Y (that is,
       their final endpoint tracklet begins on time Y or later) then set
       restrictTrackEndTimes to true and set earliestLastEndpointTime to Y.
     */
    bool restrictTrackStartTimes;
    double latestFirstEndpointTime;

    bool restrictTrackEndTimes;
    double earliestLastEndpointTime;


    /* detection error thresh is the upper bound on observational error for
       Detections; this has repercussions for what Detections are included in a
       returned track as well as the behavior of the searching itself (we will
       prune off regions of space based on their location, so error threshold
       will factor in here.)
     */
    double detectionLocationErrorThresh;


    /* quadratic fit error thresh: 

       this is used to determine how much error we will accept from the
       quadratic fitting process.  If a detection is within
       detectionLocationErrorThresh + quadraticFitErrorThresh of the predicted
       location, it will be considered for addition to a track
     */
    double quadraticFitErrorThresh;


    /*
      minSupportToEndpointTimeSeparation: the minimum time between endpoints and
      support points.  normally, orbit fits need several days of observations in
      order to be useful, so we will ignore support points which happen right
      after the actual detection. This also speeds up the search a little.
    */
    double minSupportToEndpointTimeSeparation;
    /*
      min time between the first and last point of a tracklet in order to be
      considered for a track
     */
    double minEndpointTimeSeparation;
    /*
      minimum number of *support* tracklets required for a track.  note that the
      track will have two "endpoint" tracklets PLUS this many support tracklets.
     */
    double minSupportTracklets;
    
    /*
      the minimum number of detections per track
     */
    unsigned int minDetectionsPerTrack;

    /*
      velocity error thresh: this is used when calculating whether two regions
      could contain the same object.  this is the maximum believable discrepancy
      between *actual* velocity and best-fit velocity, measured in deg/day.
     */
    double velocityErrorThresh;




};








/* queryTracklets are non-const because we set their velocityRA and velocityDec fields. 
   otherwise queryTracklets will not be changed. */
void distributedLinkTracklets(const std::vector<MopsDetection> &allDetections,
                              std::vector<Tracklet> &queryTracklets,
                              linkTrackletsConfig searchConfig, int numProcs);

void waitForTask(int rank,
		 const std::vector<MopsDetection> &allDetections, //from MAIN
		 std::vector<Tracklet> &allTracklets, //from MAIN
		 linkTrackletsConfig searchConfig  /*from MAIN*/);
	



//only declared for unit testing
void getBestFitVelocityAndAcceleration(std::vector<double> positions, const std::vector<double>&times,
                                       double & velocity, double &acceleration, double &position0);

// note that position and velocity will be MODIFIED. 
void modifyWithAcceleration(double &position, double &velocity, 
                            double acceleration, double time);


}}; //close lsst::mops namespace
#endif 