// -*- LSST-C++ -*-
#include <stdlib.h>
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

#include "lsst/mops/common.h"
#include "lsst/mops/MopsDetection.h"
#include "lsst/mops/fileUtils.h"
#include "lsst/mops/daymops/findTracklets/findTracklets.h"



/*****************************************************************
 *The main program.
 *****************************************************************/
int main(int argc, char* argv[])
{
    time_t start = time(NULL);
    //list of all detections
    std::vector <lsst::mops::MopsDetection> myDets; 


    // parse command-line options
    // find name of dets file
    // get maximum velocity (and maybe minimum velocity?) from user
    std::string outFileName;
    std::string inFileName;


    double maxVelocity = 2.0;
    double minVelocity = 0.0;
    double maxDt = 0.0625;
    double minDt = 0.01;

    if(argc < 2){
        std::cout << "Usage: findTracklets -i <input file> -o <output file> [-v <max velocity>] [-m <min velocity>] [-e <max dt>] [-b <min dt>]" << std::endl;
        exit(1);
    }

    static const struct option longOpts[] = {
        { "inFile", required_argument, NULL, 'i' },
        { "outFile", required_argument, NULL, 'o' },
        { "maxVelocity", optional_argument, NULL, 'v' },
        { "minVelocity", optional_argument, NULL, 'm' },
        { "maxDt", optional_argument, NULL, 'e'},
        { "minDt", optional_argument, NULL, 'b'},
        { "help", no_argument, NULL, 'h' },
        { NULL, no_argument, NULL, 0 }
    };


    int longIndex = -1;
    const char *optString = "i:o:v:m:e:b:h";
    int opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
    while( opt != -1 ) {
        switch( opt ) {
        case 'i':
            inFileName = optarg; 
            break;
        case 'o':
            outFileName = optarg;
            break;
        case 'v':
            maxVelocity = atof(optarg);
            break;
        case 'm':
            minVelocity = atof(optarg);
            break;
        case 'e':
            maxDt = atof(optarg);
            break;
        case 'b':
            minDt = atof(optarg);
            break;
        case 'h':
            std::cout << "Usage: findTracklets -i <input file> -o <output file> [-v <max velocity>] [-m <min velocity>] [-e <max dt>] [-b <min dt>]" << std::endl;
            exit(0);
        default:
            break;
        }
        opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
    }

    std::ifstream detsFile(inFileName.c_str());
    
    populateDetVectorFromFile(detsFile, myDets);

    double dif = lsst::mops::timeElapsed(start);
    std::cout << "Reading input took " << std::fixed << std::setprecision(10) 
              <<  dif  << " seconds." <<std::endl;     


    lsst::mops::findTrackletsConfig config;
    config.maxV = maxVelocity;
    config.minV = minVelocity;
    config.maxDt = maxDt;
    config.minDt = minDt;
    config.outputMethod = lsst::mops::IDS_FILE_WITH_CACHE;
    config.outputFile = outFileName;
    // hold up to 1 GB before purging.
    config.outputBufferSize = 1073741824;
    
    // since we set up IDS_FILE_WITH_CACHE, output will be written automatically
    time_t linkingStart = time(NULL);
    lsst::mops::findTracklets(myDets, config);
    //double linkingDif = lsst::mops::timeElapsed(linkingStart);
    double linkingDif = (time(NULL) - linkingStart);
    std::cout << "Linking took " << std::fixed << std::setprecision(10)
	      << linkingDif << " seconds." << std::endl;
    
    dif = lsst::mops::timeElapsed(start);
    std::cout << "Completed after " << std::fixed << std::setprecision(10) 
              <<  dif  << " seconds." <<std::endl;     
    lsst::mops::printMemUse();
    return 0;
 }

