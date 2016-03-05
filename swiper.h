#include "mymath.h"

#include   <math.h>
#include  <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_SIZE 64
#define MAX_FILE_SIZE 256
#define AUDIO_SAMPLE_RATE 48000

#define THRESHOLD_PERCENT   5.00 //noise floor as a percentage of the max noise level
#define MIN_POINTS_IN_ZERO  1   //minimum number of zeros between peaks
#define MIN_POINTS_IN_PEAK  1   //min number of points that defines a peak

#define DATA_BIT_SPEED_TOLERANCE    .25 //%the bit to bit index difference tolerance
#define MAX_NUM_PEAKS       1500   //maximum number of peaks
#define MAX_NUM_DATA_BITS   750    //max number of databits
#define MAX_NUM_CHANNELS 2  //maximum num audio channels
#define NUM_CLOCKING_ZEROS  10   //number of clocking zeros to calibrate zero length

//#define STATS_SAMPLE_SIZE 12000 //AUDIO_SAMPLE_RATE*.25 //number of samples to perform statistical analysis over
#define STATS_SAMPLE_SIZE 12000 //AUDIO_SAMPLE_RATE*.25 //number of samples to perform statistical analysis over

#define METHOD_NUMBER_ZEROS 2
#define NUM_FILTER_TAPS 10 //this is actually taps-1
#define NUM_DIGITS_CARD_NUMBER 19

//struct to contain an audio frame, currently up to two channels
typedef struct {
    int sample[MAX_NUM_CHANNELS];
    int sampleDelay[MAX_NUM_CHANNELS][NUM_FILTER_TAPS]; //contains samples with delay up to 10
    int movingFilterOutput[MAX_NUM_CHANNELS]; //output of moving filter
}audioFrame_t;

//holds audio statistical informationint getBitStreamFromFile(char* fileName, int numberOfChannels, int channel, int threshold, audioStats_t audioStats);
typedef struct {
    int mean[MAX_NUM_CHANNELS];
    int min[MAX_NUM_CHANNELS];
    int max[MAX_NUM_CHANNELS];
    int sigma[MAX_NUM_CHANNELS];

    //stats on filter output
    int meanFiltered[MAX_NUM_CHANNELS];
    int minFiltered[MAX_NUM_CHANNELS];
    int maxFiltered[MAX_NUM_CHANNELS];
    int sigmaFiltered[MAX_NUM_CHANNELS];
}audioStats_t;


void printHelp();

int getNextSample(FILE *fp, int numberOfChannels, audioFrame_t *audioFrame);

int calculateAudioStatistics(FILE *fp, int channel, int numberOfChannels, int numberOfSamples, audioStats_t *audioStats);
//int calculateAudioStatisticsFromFile(char* inputFileName, int numberOfChannels, int numberOfSamples, audioStats_t *audioStats);
//int calculateAllAudioStatistics(FILE *fp, int numberOfChannels, audioStats_t *audioStats);
int applyFilterOnFrame(int threshold, audioStats_t audioStats, audioFrame_t *audioFrame);



//post processing file only
int findAllPeaks(char* fileName, int numberOfChannels, int channel, int threshold, audioStats_t audioStats, int peakIndexList[]);
int findPeakDifferences(int *peakIndexList,int *peakDifferenceList);
int calculateBitStream(int *peakDifferenceList, int *bitSteam);
//int getBitStreamFromFile(char* inputFileName, int numberOfChannels, int channel, int threshold, int bitStream[]);

//process stream(file or stdin)
int getBitStreamFromStreamWithMethodNumberZeros(FILE *fp, int numberOfChannels, int channel, int threshold, int bitStream[]);
int getBitStreamFromStreamWithMethodPeaks(FILE *fp, int numberOfChannels, int channel, int threshold, int bitStream[]);

int getBitStreamFromStream(FILE *fp, int numberOfChannels, int channel, int threshold, int bitStream[]);
int getNextBitFromStream(int prevZeroCount, int zeroCount, int *zeroLength, int *curBit);

//decode
int decodeBitStreamTrack2(int bitStream[], char decodedStream[]);
int reverseBitStream(int bitStream[]);
int getCardInfo(char decodedStream[], char cardNumber[], char expDate[]);

//printing functions
int printAllSamples(char *fileName, int channel, int numberOfChannels, int threshold, int optionApplyFilter);
int printAllStatistics(char *fileName, int channel, int numberOfChannels);



