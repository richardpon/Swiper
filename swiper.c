#include "swiper.h"

main (int argc, char * argv[])
{
    int mean;
    int sample;
    int count;
    int i;
    int cur, prev;
    char inputFileName[STR_SIZE];
    int status;
    int noise;
    int maxLevel;
    int peakIndexList[MAX_NUM_PEAKS];    //holds index of each peak
    int peakDifferenceList[MAX_NUM_PEAKS]; //holds index difference of each peak pair
    int bitStream[MAX_NUM_DATA_BITS];
    char decodedStream[MAX_NUM_DATA_BITS];
    char cardNumber[NUM_DIGITS_CARD_NUMBER];
    char expDate[4]; //format: YYMM
    audioStats_t audioStats;
    FILE *inputFile;

    //init
    memset(inputFileName,'\0',STR_SIZE);
    for(i=0 ; i<MAX_NUM_DATA_BITS ; i++)
        bitStream[i]=-1;

    //defaults
    int threshold=-1;
    int numberOfChannels=1;
    int channel=0;

    //init command line options
    int optionPrintSamples=0;
    int optionStdin=1;  //input must be from stdin
    int optionStats=0;
    int optionBitStream=0;
    int optionApplyFilter=0;
    int optionDecodeMethod=0;

    //must have at least one argument
    if(argc==1){
        printHelp();
        return;
    }

    //Get command line arguments
    for (i = 1; i < argc; i++){
        if (strcmp (argv[i], "-t") == 0){
            i++;
            if(i<argc){
                threshold = atoi(argv[i]);
            }else{
                printf("Error parameter -t\n");
                return -1;
            }
        }else if (strcmp (argv[i], "-f") == 0){
            i++;
            if(i<argc){
                strcpy(inputFileName,argv[i]);
            }else{
                printf("Error parameter -f\n");
                return -1;
            }
        }else if ((strcmp (argv[i], "-h") == 0)||
                  (strcmp (argv[i], "--help") == 0)){
            printHelp();
            return;
        }else if ((strcmp (argv[i], "-p") == 0) ||
                  (strcmp (argv[i], "-print") == 0)){
            optionPrintSamples=1;
        }else if (strcmp (argv[i], "-stdin") == 0){
            optionStdin=1;
        }else if (strcmp (argv[i], "-c") == 0){
            i++;
            if(i<argc){
                channel = atoi(argv[i]);
            }else{
                printf("Error parameter -c\n");
                return -1;
            }
        }else if (strcmp (argv[i], "-n") == 0){
            i++;
            if(i<argc){
                numberOfChannels = atoi(argv[i]);
            }else{
                printf("Error parameter -n\n");
                return -1;
            }
        }else if (strcmp (argv[i], "-s") == 0){
            optionStats=1;
        }else if ((strcmp (argv[i], "-b") == 0) ||
                  (strcmp (argv[i], "--bitstream") == 0)){
            optionBitStream=1;
        }else if (strcmp (argv[i], "-filter") == 0){
            optionApplyFilter=1;
        }else if (strcmp (argv[i], "-m") == 0){
            i++;
            if(i<argc){
                if(strcmp(argv[i], "z")){
                    optionDecodeMethod=METHOD_NUMBER_ZEROS;
                }
            }else{
                printf("Error parameter -m\n");
                return -1;
            }

        }else{
            printHelp();
            return;
        }
    }

    //****************************************************************
    //****************************************************************
    //execute MAIN()
    if(optionStdin==1)
        inputFileName[0]='\0';

    if(optionPrintSamples==1){
        printAllSamples(inputFileName,channel, numberOfChannels, threshold, optionApplyFilter);
        return;
    }
    if(optionStats==1){
        if(printAllStatistics(inputFileName, channel, numberOfChannels) == -1){
            printf("error: printAllStatistics()\n");
        }
        return;
    }

    if(optionBitStream==1){


        //repeat as many times as necessary
        status=-1;
        while(status==-1){

            if(getBitStreamFromStreamWithMethodPeaks(NULL, numberOfChannels, channel, threshold, bitStream)==0){

                //decode bitStream
                if(decodeBitStreamTrack2(bitStream, decodedStream) == -1)
                    reverseBitStream(bitStream);
                if(decodeBitStreamTrack2(bitStream, decodedStream) == -1){
                    status=-1;
                }else{
                    status=0;
                }

                if (status==0){
                    status=getCardInfo(decodedStream, cardNumber, expDate);
                }

                //debug
                for(i=0 ; i<MAX_NUM_DATA_BITS ; i++){
                    if(decodedStream[i]!=-1)
                        printf("%x",decodedStream[i]);
                }
                printf("\n");



                if(status==0){
                    printf("Card number is ");
                    for(i=0 ; i<19 ; i++){
                        if(cardNumber[i] != -1){
                            printf("%x",cardNumber[i]);
                            if(i==3 || i==7 || i==11 || i==15)
                                printf(" ");
                        }
                    }
                    printf("\n");
                    printf("Expiration Date is %x%x/20%x%x\n",expDate[2],expDate[3],expDate[0],expDate[1]);



                }

            }


        }//while



    }
    return;

}




void printHelp(){
    printf("Usage: swiper [-t <threshold>] [-f <filename>] [-h|--help] [-p] [-stdin] \n");
    printf("              [-c <channel>] [-n <#channels>]\n");
    printf("\n");
    printf("INPUT options:\n");
    //printf("-f <filename>   :input file name\n");
    //printf("-stdin          :get input from stdin instead of input file\n");
    printf("                -stdin supercedes -f flag\n");
    printf("-t <threshold>  :min value to be data, (default=0)\n");
    printf("-c <channel>    :specify audio channel, (default=0)\n");
    printf("-n <# channels> :total number of audo channels, mono=1(default), stereo=2\n");
    printf("-filter         :apply filter on printed samples\n");
    printf("-h,--help       :print help\n");
    printf("-p,--print       :print samples\n");
    printf("-s,--stats      :print statistics\n");
    printf("-b,--bitstream  :print data bitstream\n");
    printf("-m <z>          :choose decoding method, z for number of zeros\n");

    return;
}







//Grabs next sample
//return -1 when EOF in data stream reached
//
//inputs:
//      @fp   - input file, if NULL, then read from stdin
//      @stereoMonoFormat = MONO for mono, STEREO for stereo
//
//outputs:
//      @audoFrame-will contain the mono/stereo audio samples
//
int getNextSample(FILE *fp, int numberOfChannels, audioFrame_t *audioFrame){

    int lsb[MAX_NUM_CHANNELS];
    int msb[MAX_NUM_CHANNELS];
	int sample[MAX_NUM_CHANNELS];
    int i;

    //read from stdin if *fp==NULL
    if(fp==NULL)
        fp=stdin;

    //get sample for MONO/STEREO case:
	    lsb[0]=getc(fp);
	    if(lsb[0]==EOF)
            return -1;

	    msb[0]=getc(fp);
	    if(msb[0]==EOF)
            return -1;

    //get second sample for stereo case:
    if(numberOfChannels==2){
	    lsb[1]=getc(fp);
	    if(lsb[1]==EOF)
            return -1;

	    msb[1]=getc(fp);
	    if(msb[1]==EOF)
            return -1;
    }

    sample[0] = (msb[0] << 8) | lsb[0];
    sample[1] = (msb[1] << 8) | lsb[1];
    if(sample[0] >> 15 | 1 == 1){ //check if need to sign extend
        sample[0] = (sample[0] << 16) >> 16;
    }
    if(sample[1] >> 15 | 1 == 1){ //check if need to sign extend
        sample[1] = (sample[1] << 16) >> 16;
    }

    audioFrame->sample[0]=sample[0];
    if(numberOfChannels==2)
        audioFrame->sample[1]=sample[1];
    else
        audioFrame->sample[1]=0;


    //update delayed samples
    for(i=NUM_FILTER_TAPS ; i>0 ; i--){
        audioFrame->sampleDelay[0][i] = audioFrame->sampleDelay[0][i-1];
        audioFrame->sampleDelay[1][i] = audioFrame->sampleDelay[1][i-1];
    }
    audioFrame->sampleDelay[0][0] = audioFrame->sample[0];
    audioFrame->sampleDelay[1][0] = audioFrame->sample[1];

    //update filter output
    audioFrame->movingFilterOutput[0]=0;
    audioFrame->movingFilterOutput[1]=0;
    for(i=0; i < NUM_FILTER_TAPS ; i++){
        audioFrame->movingFilterOutput[0]+=audioFrame->sampleDelay[0][i];
        audioFrame->movingFilterOutput[1]+=audioFrame->sampleDelay[1][i];
    }
    audioFrame->movingFilterOutput[0] /= NUM_FILTER_TAPS;

    return 0;

}



//Calculates audio statistics from audio stream, file or stdin
//assumes no header
//
//NOTE:
//-Don't really need to do this over multiple channels, as always only deal with single audio channel,
//  just choose which one
//-Be careful with overflow, numberOfSamples shouldn't be too big(12000 is fine for 16bit pcm data)
//
//inputs:
//  @*fp- input file, if *fp==NULL, then input is stdin
//  @numberOfChannels-  specifies number of audio channels
//  @numberOfSamples-   number of samples on which to perform statistics(numberOfSamples <= STATS_SAMPLE_SIZE
//
//outputs:
//  @audioStats -holder for audio statistics, must be cleared outside this function
int calculateAudioStatistics(FILE *fp, int channel, int numberOfChannels, int numberOfSamples, audioStats_t *audioStats){

    int curChannel;
    int i;
    audioFrame_t audioFrame;
    int isFirstSample;  //indicates first sample, must update stats on first sample
    double runningTotal[MAX_NUM_CHANNELS];
    double runningTotalFiltered[MAX_NUM_CHANNELS];
    //int sampleBuf[STATS_SAMPLE_SIZE][MAX_NUM_CHANNELS]; //buffer samples in order to post process stddev
    int sigma[MAX_NUM_CHANNELS];

    //init
    for(i=0 ; i< MAX_NUM_CHANNELS ; i++){
        audioStats->mean[i]=0;
        audioStats->min[i]=0;
        audioStats->max[i]=0;
        audioStats->sigma[i]=0;

        audioStats->meanFiltered[i]=0;
        audioStats->minFiltered[i]=0;
        audioStats->maxFiltered[i]=0;
        audioStats->sigmaFiltered[i]=0;

        runningTotal[i]=0;
        runningTotalFiltered[i]=0;
    }

    //init filter/remove header
    for(i=0 ; i<50 ; i++){
        if(getNextSample(fp, numberOfChannels, &audioFrame) == -1){
            printf("calculateAudioStatistics: not enough samples\n");
            return -1;
        }
    }


    //calculate mean (and min/max) for first half of samples:
    isFirstSample=1;
    for(i=0 ; i<numberOfSamples/2 ; i++){
        if(getNextSample(fp, numberOfChannels, &audioFrame) == -1){
            printf("calculateAudioStatistics: not enough samples\n");
            return -1;
        }else{
            audioStats->mean[channel] += audioFrame.sample[channel];
            audioStats->meanFiltered[channel] += audioFrame.movingFilterOutput[channel];
            //find new max?
            if((audioFrame.sample[channel] > audioStats->max[channel]) || (isFirstSample==1))
                audioStats->max[channel] = audioFrame.sample[channel];
            if((audioFrame.movingFilterOutput[channel] > audioStats->maxFiltered[channel]) || (isFirstSample==1))
                audioStats->maxFiltered[channel] = audioFrame.movingFilterOutput[channel];

            //find new min?
            if((audioFrame.sample[channel] < audioStats->min[channel]) || (isFirstSample==1))
                audioStats->min[channel] = audioFrame.sample[channel];
            if((audioFrame.movingFilterOutput[channel] < audioStats->minFiltered[channel]) || (isFirstSample==1))
                audioStats->minFiltered[channel] = audioFrame.movingFilterOutput[channel];

            isFirstSample=0;
/*
    printf("*******%d\t%d\t%d\n",i,audioFrame.sample[0],audioFrame.sample[0]);
    printf("mean\t%d\t%d\n",audioStats->mean[0],audioStats->mean[1]);
    printf("min\t%d\t%d\n",audioStats->min[0],audioStats->min[1]);
    printf("max\t%d\t%d\n",audioStats->max[0],audioStats->max[1]);
*/

        }
        //printf("%d\tsameple\t%d\t%d\n",i,audioFrame.sample[0],audioFrame.sample[1]);
    }//for

    //calculate mean
    //for(curChannel=0; curChannel < MAX_NUM_CHANNELS ; curChannel++){
    audioStats->mean[channel] /= (numberOfSamples/2);
    audioStats->meanFiltered[channel] /= (numberOfSamples/2);
    //}


    //calculate sigma(and update min/max for second half of samples(N=numberOfSamples/2)
    for(i=0 ; i<numberOfSamples/2 ; i++){
        if(getNextSample(fp, numberOfChannels, &audioFrame) == -1){
            printf("calculateAudioStatistics: not enough samples\n");
            return -1;
        }else{

            //find new max?
            if(audioFrame.sample[channel] > audioStats->max[channel])
                audioStats->max[channel] = audioFrame.sample[channel];
            if(audioFrame.movingFilterOutput[channel] > audioStats->maxFiltered[channel])
                audioStats->maxFiltered[channel] = audioFrame.movingFilterOutput[channel];

            //find new min?
            if(audioFrame.sample[channel] < audioStats->min[channel])
                audioStats->min[channel] = audioFrame.sample[channel];
            if(audioFrame.movingFilterOutput[channel] < audioStats->minFiltered[channel])
                audioStats->minFiltered[channel] = audioFrame.movingFilterOutput[channel];

            //calc sigma
            runningTotal[channel] +=  pow(audioFrame.sample[channel]-audioStats->mean[channel],2);
            runningTotal[channel] +=  pow(audioFrame.movingFilterOutput[channel]-audioStats->meanFiltered[channel],2);

        }
        //printf("%d\tsameple\t%d\t%d\n",i,audioFrame.sample[0],audioFrame.sample[1]);
    }//for

    //finish sigma calculation
    runningTotal[channel] /= (int)(numberOfSamples/2);  //N = numberOfSamples/2
    runningTotalFiltered[channel] /= (int)(numberOfSamples/2);  //N = numberOfSamples/2

    runningTotal[channel] = (int)sqrt(runningTotal[channel]);
    runningTotalFiltered[channel] = (int)sqrt(runningTotalFiltered[channel]);

    audioStats->sigma[channel] = runningTotal[channel];
    audioStats->sigmaFiltered[channel] = runningTotalFiltered[channel];


    return 0;
}




//DEPRICATED
//Calculates audio statistics from File only
/*
int calculateAudioStatisticsFromFile(char* inputFileName,
                                    int numberOfChannels,
                                    int numberOfSamples,
                                    audioStats_t *audioStats){
    FILE *inputFile;
    audioFrame_t audioFrame;
    int i;

    inputFile = fopen(inputFileName, "r");
    if (inputFile == NULL){
        printf("calculateAudioStatisticsFromFile: Error opening %s\n", inputFileName);
        return -1;
    }

    //throw away first 22 samples(can include 44 byte header)
    for(i=0 ; i < 22 ; i++){
        if(getNextSample(inputFile, numberOfChannels, &audioFrame) == -1){
            printf("error getting sample in calculateAudioStatisticsFromFile\n");
        }
    }

    if(calculateAudioStatistics(inputFile, 0 ,numberOfChannels, numberOfSamples, audioStats) == -1){
        printf("error: calculateAudioStatisticsFromFile: getting audio statistic\n");
        return -1;
    }

    fclose(inputFile);
    return 0;
}
*/

//DEPRECATED
//Calculates audio statistics for entire stream/file
//assumes header
//
//inputs:
//  @*fp- input file, if *fp==NULL, then input is stdin
//  @numberOfChannels-  specifies number of audio channels
//  @numberOfSamples-   number of samples on which to perform statistics
//
//outputs:
//  @audioStats -holder for audio statistics, must be cleared outside this function
/*
int calculateAllAudioStatistics(FILE *fp, int numberOfChannels, audioStats_t *audioStats){

    int curChannel;
    int i;
    audioFrame_t audioFrame;
    int isFirstSample;  //indicates first sample, must update stats on first sample
    int count;

    for(i=0 ; i< MAX_NUM_CHANNELS ; i++){
        audioStats->mean[i]=0;
        audioStats->min[i]=0;
        audioStats->max[i]=0;
    }

    count=0;
    isFirstSample=1;
    while(getNextSample(fp, numberOfChannels, &audioFrame) != -1){
        for(curChannel=0 ; curChannel < MAX_NUM_CHANNELS ; curChannel++){
            audioStats->mean[curChannel] += audioFrame.sample[curChannel];
            //find new max?
            if((audioFrame.sample[curChannel] > audioStats->max[curChannel]) ||
                (isFirstSample==1))
                audioStats->max[curChannel] = audioFrame.sample[curChannel];
            //find new min?
            if((audioFrame.sample[curChannel] < audioStats->min[curChannel]) ||
                (isFirstSample==1))
                audioStats->min[curChannel] = audioFrame.sample[curChannel];
                isFirstSample=0;
        }
        count++;
    }

        //printf("%d\tsameple\t%d\t%d\n",i,audioFrame.sample[0],audioFrame.sample[1]);

    //calculate mean
    for(curChannel=0; curChannel < MAX_NUM_CHANNELS ; curChannel++){
        audioStats->mean[curChannel] /= count;
    }
    return 0;
}
*/


//Applies filtering on frame:
// remove dc offset(mean)
// set to zero if below threshold
//
//
//
int applyFilterOnFrame(int threshold, audioStats_t audioStats, audioFrame_t *audioFrame){
    int channel;
    int thresholdFiltered;

    for(channel=0; channel<MAX_NUM_CHANNELS ; channel++){

        //Calculate threshold(threshold is magnitude)
        if(threshold==-1){
                threshold = (THRESHOLD_PERCENT)*max(audioStats.max[channel]-audioStats.mean[channel],
                                                abs(audioStats.min[channel]-audioStats.mean[channel]));
                thresholdFiltered = (THRESHOLD_PERCENT)*max(audioStats.maxFiltered[channel]-audioStats.meanFiltered[channel],
                                                abs(audioStats.minFiltered[channel]-audioStats.meanFiltered[channel]));
        }



        //don't get magnitude
        //Remove DC offset
        audioFrame->sample[channel] = audioFrame->sample[channel]- audioStats.mean[channel];
        audioFrame->movingFilterOutput[channel] = audioFrame->movingFilterOutput[channel]
                                                - audioStats.meanFiltered[channel];


        //remove if below threshold
        if(abs(audioFrame->sample[channel]) < threshold){
            audioFrame->sample[channel] = 0;
        }
        if(abs(audioFrame->movingFilterOutput[channel]) < thresholdFiltered){
            audioFrame->movingFilterOutput[channel] = 0;
        }
    }

    return 0;
}


//calculates a bitstream from a file only
//this post processes entire file
//
//inputs:   @inputFileName  - name of input file
//          @numberOfChannels - # of interleaved channels in inputstream
//          @channel    -specifies which channel to analyze
//          @threshold -specifies noise threshold,
//          @audioStats- must give audio statistics as input
//
//output:   @bitStream-holder for calculated bitstream
//
/*
int getBitStreamFromFile(char* inputFileName,
                        int numberOfChannels,
                        int channel,
                        int threshold,
                        int bitStream[]){
    int i;
    int peakIndexList[MAX_NUM_PEAKS];    //holds index of each peak
    int peakDifferenceList[MAX_NUM_PEAKS]; //holds index difference of each peak pair
    audioStats_t audioStats;
    FILE *inputFile;

    //Get Audio Statistics
    if(calculateAudioStatisticsFromFile(inputFileName, numberOfChannels, STATS_SAMPLE_SIZE, &audioStats)== -1){
        printf("error: getBitStreamFromFile: calculateAudioStatisticsFromFile\n");
        return -1;
    }


    //max threshold above first 100ms noise, but remove dc offset first
    if(threshold==-1)
        threshold = (THRESHOLD_PERCENT)*max(audioStats.max[channel]-audioStats.mean[channel],
                                            abs(audioStats.min[channel]-audioStats.mean[channel]));

    if(findAllPeaks(inputFileName, numberOfChannels, channel, threshold, audioStats, peakIndexList)==-1){
        printf("Error in getBitStreamFromFile: findAllPeaks\n");
        return;
    }

    if(findPeakDifferences(peakIndexList,peakDifferenceList) == -1){
        printf("error peak dif\n");
        return;
    }

    if(calculateBitStream(peakDifferenceList, bitStream)==-1){
        printf("error calculate bitstream\n");
    }


    for(i=0; i< MAX_NUM_DATA_BITS ; i++){
        if(bitStream[i] != -1)
            printf("%d", bitStream[i]);
    }
    printf("\n");

    return 0;
}
*/

//find all peaks of specified channel
//
//@fileName - input file name
//
//
/*
int findAllPeaks(char* fileName, int numberOfChannels, int channel, int threshold, audioStats_t audioStats, int peakIndexList[]){

    FILE *inputFile;
    int index,i;
    int curSample,prevSample;
    //int indexFirstZero, indexSecondZero, indexMax;
    int zeroCount;  //track num consecutive zero points
    int zeroCountReached;
    int peakCount;  //track num consecutive possible peak points
    int peakCountReached;
    int curPeakMax; //track local maximum as possible peak point;
    int curPeakIndex;//index of possible peak
    int peakIndex; //index of peaks into peakIndexList
    audioFrame_t audioFrame;

    inputFile = fopen(fileName, "r");
    if (inputFile == NULL){
        printf("findAllPeaks: Error opening %s\n", fileName);
        return -1;
    }

    //  Discard Header
    //first 44 bytes is header
	for (i = 0 ; i < 44 ; i++){
		if(getNextSample(inputFile, 1, &audioFrame) == -1){
			return -1;
		}
    }



    //initialize peakIndexList
    for(i = 0 ; i < MAX_NUM_PEAKS ; i++){
        peakIndexList[i] = -1;
    }

    //find each peak's index
    index=0;
    //indexFirstZero=-1;
    //indexSecondZero=-1;
    //indexMax=-1;
    prevSample=-1;
    zeroCount=0;
    peakCount=0;
    zeroCountReached=0;
    peakCountReached=0;
    peakIndex=0;
    while (getNextSample(inputFile, numberOfChannels, &audioFrame) != -1){

        //normalize for mean offset
        curSample = audioFrame.sample[channel] - audioStats.mean[channel];

        //want just magnitude
        curSample=abs(curSample);

        //printf("thresh\t%d\tsample\t%d\n",threshold,curSample);

        //remove if below threshold
        if(curSample < threshold)
            curSample = 0;




        //*******************************************************
        //four cases to analyze on sample stream:
        //     (prevSample,curSample) == (00,0N,N0,NN), N>0
        //ignore if N<0;

        //CASE (00)
        //increment zeroCount if Applicable
        if (curSample==0 && prevSample==0){
            zeroCount++;
            peakCount=0;
            if(zeroCount >= MIN_POINTS_IN_ZERO){
                zeroCountReached=1;
                //printf("zrto count reachS!\n");
            }
        }

        //CASE (NN)
        //increment peakCount if Applicable
        if (curSample>0 && prevSample>0){
            //zeroCount=0;
            peakCount++;
            if(peakCount >= MIN_POINTS_IN_PEAK)
                peakCountReached=1;

            //check if found new peak candidate
            if(curSample > curPeakMax){
                curPeakMax=curSample;   //record magnitude of peak candidate
                curPeakIndex=index;     //record index of peak candidate
            }
        }

        //CASE (0N)
        //found first peak point
        if ((curSample > 0 && prevSample == 0)&&
            zeroCountReached==1){
            peakCount=1;

            curPeakIndex=index;
            curPeakMax=curSample;

        }

        //CASE (N0)
        //determine if peak or noise found
        if(curSample==0 && prevSample>1){

            //check if found actual peak
            if((zeroCountReached==1) &&
               (peakCountReached==1)){
                peakIndexList[peakIndex]=curPeakIndex;

                //printf("%d\t%d\tpeak found!\n",peakIndex,curPeakIndex);

                peakIndex++;
            }

            //printf("zero is %d peak is %d\n", zeroCount, peakCount);

            //reset counters
            curPeakIndex=-1;
            zeroCount=1;
            zeroCountReached=0;
            peakCountReached=0;

        }
        prevSample=curSample;
        index++;
    }//end While

    fclose(inputFile);
    return 0;
}
*/


//Creates list of index deltas between peaks
//length will be number of peaks - 1
//
//[peakIndexList]   -input, list of peak indices
//[peakDifferenceList] - output, lists peak index deltas
/*
int findPeakDifferences(int peakIndexList[],int peakDifferenceList[]){

    int i;

    //init peakDifferenceList
    for(i=0 ; i<MAX_NUM_PEAKS ; i++)
        peakDifferenceList[i]=-1;

    for(i=1 ; i<=MAX_NUM_PEAKS ; i++){

        //check for valid peaks
        if((peakIndexList[i-1] == -1) ||
           (peakIndexList[i] == -1)){
            return 0;
        }else{
            peakDifferenceList[i-1] = peakIndexList[i] - peakIndexList[i-1];
        }


    }//FOR

}
*/

//inputs:
//      @peakDifferenceList input-list of index deltas between peaks
//outputs:
//      @bitStream  - output - decoded bitStream
//
//return 0 on failure, 1 on success
//Note: always have leading and trailing zeros in both swipe directions
//  tracks have at least 22 leading/trailing clocking zeroes
/*
int calculateBitStream(int *peakDifferenceList, int *bitStream){
    int difIndex;   //index into peakDifferenceList
    int bitIndex;   //index into bitStream[]
    int curDif;     //current difference in peak differences
    int zeroLength; //length of zero bits
    int foundFirstHalfOne;  //flag to track if on first or second half of one
    int lowZeroThreshold;   //lower threshold for zero peak dif
    int highZeroThreshold;  //higher threshold for zero peak dif
    int numClockingZeros;

    //init bitStream
    for(bitIndex=0 ; bitIndex<MAX_NUM_DATA_BITS ; bitIndex++){
        bitStream[bitIndex]=-1;
    }

    //init
    foundFirstHalfOne=0;
    bitIndex=0;


    int i=0;
    while(peakDifferenceList[i] != -1){
        //printf("%d\t%d\n",i,peakDifferenceList[i]);
        i++;
    }

    //get first zeroLength, must find at least five consecutive zeros to calibrate zero length
    numClockingZeros=0;
    difIndex=0;
    zeroLength = peakDifferenceList[difIndex++];    //get first zeroLegth candidate
    while(numClockingZeros < NUM_CLOCKING_ZEROS){

        curDif = peakDifferenceList[difIndex];

        //calculate thresholds
        lowZeroThreshold = (int)(zeroLength*(1-DATA_BIT_SPEED_TOLERANCE));
        highZeroThreshold = (int)(zeroLength*(1+DATA_BIT_SPEED_TOLERANCE));

        //is zeroLength within thresholds:
        if( (curDif > lowZeroThreshold) &&
            (curDif < highZeroThreshold)){
            numClockingZeros++;
        }else{
            //not within threshold
            numClockingZeros=0;
        }
        zeroLength=curDif;
        difIndex++;
    }//done geting zero legnth

    //calculate bitstream
    while(peakDifferenceList[difIndex] != -1){

        //get current difference
        curDif=peakDifferenceList[difIndex];

        lowZeroThreshold = (int)(zeroLength*(1-DATA_BIT_SPEED_TOLERANCE));
        highZeroThreshold = (int)(zeroLength*(1+DATA_BIT_SPEED_TOLERANCE));

        //find a zero?
        if ((curDif > lowZeroThreshold)&&
            (curDif < highZeroThreshold)){
            bitStream[bitIndex++]=0; //found a 0
            zeroLength=curDif; //update zeroLength

            //printf("0\t%d\tcurdif=%d\n",difIndex,curDif);

            if(foundFirstHalfOne){
                printf("calculateBitStream: found zero immediately after first half of one!!!\n");
                printf("index\t%d\tcurDif\t%d\n",difIndex,curDif);
            }

        //find a ONE?
        //short delta for ones must come in pairs
        }else if (curDif < lowZeroThreshold){
            if(foundFirstHalfOne){
                bitStream[bitIndex++]=1;
                foundFirstHalfOne=0;
            }else{
                foundFirstHalfOne=1;
            }
            //printf("1\t%d\tcurdif=%d\n",difIndex,curDif);
            //printf("found 1?\t%d\n",difIndex);

        //is the zero threshold incorrect?
        }else if (curDif > highZeroThreshold){
            printf("found incorrect highZeroThreshold!!!\n");
            return -1;
        }

        difIndex++;
    }//while

    return 1;
}
*/







//calculates a bitstream from a stream, stdin or file
//
//inputs:   @fp - FILE descriptor, or NULL if stdin input
//          @numberOfChannels - # of interleaved channels in inputstream
//          @channel    -specifies which channel to analyze
//          @threshold -specifies noise threshold,
//
//output:   @bitStream-holder for calculated bitstream
//
int getBitStreamFromStreamWithMethodPeaks(FILE *fp, int numberOfChannels, int channel, int threshold, int bitStream[]){

    int i;
    audioStats_t audioStats;
    audioFrame_t audioFrame;

    //find all peaks
    int curSample,prevSample;//current and previous samples
    int zeroCount;          //track num consecutive zero points
    int peakCount;          //track num consecutive possible peak points
    int curPeakDif,prevPeakDif;//holds difference between peaks
    int zeroLength; //holds duration between zeros
    int curBit;    //contains current, previous bits of output stream
    int foundFirstHalfOfOne; //tracks if found first half of a one
    int bitStreamIndex; //index into bitStream[];
    int clockingBitCounter; //track number of clocking bits seen
    int curPeakIndex;
    int prevPeakIndex;
    int curPeakAmplitude;
    int index;
    int peakPolarity; //is 1 or -1
    int bitCount;//track # bits so can break


    //init bitStream
    for(i=0 ; i < MAX_NUM_DATA_BITS ; i++)
        bitStream[i]=-1;

    //remove header and populate filter delays
    for(i=0 ; i<22+NUM_FILTER_TAPS+5 ; i++){
        if(getNextSample(fp, numberOfChannels, &audioFrame)==-1){
            printf("error: getBitStreamFromStream, removing header\n");
        }
    }

    //Get Audio Statistics
    if(calculateAudioStatistics(fp, channel, numberOfChannels, STATS_SAMPLE_SIZE, &audioStats)==-1){
        printf("error: getBitStreamFromStream: calculateAudioStatistics\n");
        return -1;
    }

    //Enumerate samples to find peaks
    //peaks are where magnitude hits zero.
    prevSample=-1;
    zeroCount=0;
    peakCount=0;
    zeroLength=-1;
    bitStreamIndex=0;
    clockingBitCounter=0;
    foundFirstHalfOfOne=0;
    curPeakIndex=0;
    curPeakAmplitude=0;
    peakPolarity=1; //look for positive peak first
    bitCount=0;

    index=0;
    while (getNextSample(fp, numberOfChannels, &audioFrame) != -1){

        //normalized for mean offset
        //remove if below threshold
        applyFilterOnFrame(threshold, audioStats, &audioFrame);
        curSample = audioFrame.movingFilterOutput[channel];


        //*******************************************************
        //four cases to analyze on sample stream:
        //     (prevSample,curSample) == (00,0N,N0,NN), N>0
        //ignore if N<0;

        //CASE (00)(both samples below threshold)
        //increment zeroCount if Applicable
        if (curSample==0 && prevSample==0){
            zeroCount++;
        }

        //CASE (NN) (and with correct polarity)
        //increment peakCount if Applicable
        if (curSample*peakPolarity>0 && prevSample*peakPolarity>0){
            peakCount++;

            //found new possible peak?
            if(abs(curSample)>abs(curPeakAmplitude)){
                curPeakAmplitude=curSample;
                curPeakIndex=index;
            }

        }

        //CASE (0N)
        //found first peak point
        if (curSample*peakPolarity>0 && prevSample == 0){
            peakCount=1;
            curPeakAmplitude=curSample;
            curPeakIndex=index;
        }

        //CASE (N0)
        //determine if peak or noise found
        if(curSample==0 && prevSample*peakPolarity>0){
            //check if found actual peak
            if((zeroCount >= MIN_POINTS_IN_ZERO) &&
               (peakCount >= MIN_POINTS_IN_PEAK)){
                curPeakDif=curPeakIndex - prevPeakIndex;

                //Get next data bit
                if(getNextBitFromStream(prevPeakDif, curPeakDif, &zeroLength, &curBit)==-1){
                    printf("Error in getNextBitFromStream()\n");
                    return -1;
                }

                //debug
                //printf("clk %d prevdif %d curdif %d zeroLen %d curBit %d Half1 %d polarity %d\n", clockingBitCounter,prevPeakDif,curPeakDif,zeroLength,curBit,foundFirstHalfOfOne, peakPolarity);
                //fflush(stdout);


                //latch data after first NUM_CLOCKING_ZEROS peaks
                if (clockingBitCounter >= NUM_CLOCKING_ZEROS){

                    //check if only single one bit peak difference found
                    if(curBit==0 && foundFirstHalfOfOne==1){
                        //debug
                        //printf("Error: getBitStreamFromStream, found only single one bit peak difference, need those in pairs!\n");

                        //curSample=1000; //debug value
                        //return -1;

                    //find first half one?
                    }else if(curBit==1 && foundFirstHalfOfOne==0){
                        foundFirstHalfOfOne=1;

                    //found data
                    }else if((curBit==1 && foundFirstHalfOfOne==1) ||
                             (curBit==0)){
                        bitStream[bitStreamIndex++]=curBit;
                        foundFirstHalfOfOne=0;

                        printf("%d",curBit);
                        fflush(stdout);
                        bitCount++;

                    }
                //still on clocking zeros
                }else{
                    zeroLength=curPeakDif;
                }

                clockingBitCounter++;
                prevPeakDif=curPeakDif;
                prevPeakIndex=curPeakIndex;
                peakPolarity*=-1;   //next peak must be opposite polarity

            }//end peak found

            //reset counters for peak or noise cases
            zeroCount=1;
        }//End Case (NO)

        //debug
        //printf("%d\t%d\t%d\n",index,curSample, zeroLength);


        prevSample=curSample;
        index++;

        //timeout (5 seconds...) or have data and see a lot of zeros
        if((bitCount > 40) && zeroCount> AUDIO_SAMPLE_RATE/10)
            break;
        else if (zeroCount > 5*AUDIO_SAMPLE_RATE)
            return -1;

    }//end While

    return 0;
}

/*backup
int getBitStreamFromStreamWithMethodPeaks(FILE *fp, int numberOfChannels, int channel, int threshold, int bitStream[]){

    int i;
    audioStats_t audioStats;
    audioFrame_t audioFrame;

    //find all peaks
    int curSample,prevSample;//current and previous samples
    int zeroCount;          //track num consecutive zero points
    int peakCount;          //track num consecutive possible peak points
    int curPeakDif,prevPeakDif;//holds difference between peaks
    //int peakDifCounter; //track difference betweek previou peak and upcoming peak
    int zeroLength; //holds duration between zeros
    int curBit;    //contains current, previous bits of output stream
    int foundFirstHalfOfOne; //tracks if found first half of a one
    int bitStreamIndex; //index into bitStream[];
    int clockingBitCounter; //track number of clocking bits seen
    int curPeakIndex;
    int prevPeakIndex;
    int curPeakAmplitude;
    int index;
    int lowLevel; //threshold for zeros
    int highLevel;


    //init bitStream
    for(i=0 ; i < MAX_NUM_DATA_BITS ; i++)
        bitStream[i]=-1;

    //remove header and populate filter delays
    for(i=0 ; i<22+NUM_FILTER_TAPS+5 ; i++){
        if(getNextSample(fp, numberOfChannels, &audioFrame)==-1){
            printf("error: getBitStreamFromStream, removing header\n");
        }
    }

    //Get Audio Statistics
    if(calculateAudioStatistics(fp, channel, numberOfChannels, STATS_SAMPLE_SIZE, &audioStats)==-1){
        printf("error: getBitStreamFromStream: calculateAudioStatistics\n");
        return -1;
    }

    //Enumerate samples to find peaks
    //peaks are where magnitude hits zero.
    prevSample=-1;
    zeroCount=0;
    peakCount=0;
    zeroLength=-1;
    bitStreamIndex=0;
    clockingBitCounter=0;
    foundFirstHalfOfOne=0;
    curPeakIndex=0;
    curPeakAmplitude=0;

    lowLevel=0;
    highLevel=0;

    index=0;
    while (getNextSample(fp, numberOfChannels, &audioFrame) != -1){

        //get magnitude normalized for mean offset
        //remove if below threshold
        applyFilterOnFrame(threshold, audioStats, &audioFrame);
        curSample = audioFrame.movingFilterOutput[channel];
        //curSample = audioFrame.sample[channel];

        //*******************************************************
        //four cases to analyze on sample stream:
        //     (prevSample,curSample) == (00,0N,N0,NN), N>0
        //ignore if N<0;

        //CASE (00) (LL)
        //increment zeroCount if Applicable
        if (curSample<=lowLevel && prevSample<=lowLevel){
            zeroCount++;
        }

        //CASE (NN) (HH)
        //increment peakCount if Applicable
        if (curSample>highLevel && prevSample>highLevel){
            peakCount++;

            //found new possible peak?
            if(curSample>curPeakAmplitude){
                curPeakAmplitude=curSample;
                curPeakIndex=index;
            }

        }

        //CASE (0N) (LH)
        //found first peak point
        if (curSample > highLevel && prevSample <= lowLevel){
            peakCount=1;
            curPeakAmplitude=curSample;
            curPeakIndex=index;
        }

        //CASE (N0) (HL)
        //determine if peak or noise found
        if(curSample<=lowLevel && prevSample>highLevel){
            //check if found actual peak
            if((zeroCount >= MIN_POINTS_IN_ZERO) &&
               (peakCount >= MIN_POINTS_IN_PEAK)){
                curPeakDif=curPeakIndex - prevPeakIndex;

                //update lowLevel/highLevel
                //lowLevel=(int).1*curPeakAmplitude;
                //highLevel=(int).4*curPeakAmplitude;

                //Get next data bit
                if(getNextBitFromStream(prevPeakDif, curPeakDif, &zeroLength, &curBit)==-1){
                    printf("Error in getNextBitFromStream()\n");
                    return -1;
                }

                //debug
                printf("clk %d prevpeakdif %d curpeakdif %d zeroLength %d curBit %d foundHalf1 %d\n", clockingBitCounter,prevPeakDif,curPeakDif,zeroLength,curBit,foundFirstHalfOfOne);



                //latch data after first NUM_CLOCKING_ZEROS peaks
                if (clockingBitCounter >= NUM_CLOCKING_ZEROS){

                    //check if only single one bit peak difference found
                    if(curBit==0 && foundFirstHalfOfOne==1){
                    //    printf("Error: getBitStreamFromStream, found only single one bit peak difference, need those in pairs!\n");
                        //debug
                        curSample=-50;
                        //return -1;

                    //find first half one?
                    }else if(curBit==1 && foundFirstHalfOfOne==0){
                        foundFirstHalfOfOne=1;

                    //found data
                    }else if((curBit==1 && foundFirstHalfOfOne==1) ||
                             (curBit==0)){
                        bitStream[bitStreamIndex++]=curBit;
                        foundFirstHalfOfOne=0;
                    }
                //still on clocking zeros
                }else{
                    zeroLength=curPeakDif;
                }

                clockingBitCounter++;
                prevPeakDif=curPeakDif;
                prevPeakIndex=curPeakIndex;

            }else{
                //do nothing, no peak, just noise found
            }

            //reset counters for peak or noise cases
            zeroCount=1;
        }//End Case (NO)

        //debug
        //printf("%d\t%d\n",index,curSample);


        prevSample=curSample;
        index++;

        //timeout (5 seconds...)
        if(zeroCount > 5*AUDIO_SAMPLE_RATE)
            return -1;

    }//end While

    return 0;
}
*/



//Returns 0 on success, -1 on error
//inputs:
//      @prevNumConsecZero    - previous duration of zeros
//      @curNumConsecZero     - current duration of zeros
//      @zeroLength     - duration between peaks of zero
//outputs:
//      @zeroLength     - this can be updated
//      @curBit        - outputs the bit based upon the zeroLength
//int getNextBitFromStream(int prevPeakDif, int curPeakDif, int *zeroLength, int *curBit){
int getNextBitFromStream(int prevZeroCount, int zeroCount, int *zeroLength, int *curBit){

    int lowZeroThreshold;

    //calculate thresholds
    lowZeroThreshold = (int)(*zeroLength*(1-DATA_BIT_SPEED_TOLERANCE));

    //first peakdif
    if(*zeroLength==-1){
        *zeroLength=zeroCount;
        *curBit=0;
        return 0;
    }

    //found a zero?
    if ((zeroCount >= lowZeroThreshold)){
        *curBit=0;

        //update zeroCount
        //if(zeroCount
        *zeroLength=zeroCount;



    //found half a one
    }else if(zeroCount < lowZeroThreshold){
        *curBit=1;
    }



    return 0;
}



//decodes bit stream for track 2
//checks LRC and parity bits
int decodeBitStreamTrack2(int bitStream[], char decodedStream[]){

    int curByte[5];  //4 data + 1 parity
    int lrc[4] = {0,0,0,0};;   //lrc
    int i;
    int bitStreamIndex;
    int parity;
    int byteCount;
    int data;

    //init decodedStream
    for(i=0 ; i<MAX_NUM_DATA_BITS ; i++){
        decodedStream[i]=-1;
    }

    //find first bit
    for(bitStreamIndex = 0 ; bitStreamIndex < MAX_NUM_DATA_BITS ; bitStreamIndex++){
        //found first bit?
        if (bitStream[bitStreamIndex]==1)
            break;
    }

    byteCount=0;
    //process each data byte
    while(1){

        //get data byte
        for(i=0 ; i < 5 ; i++){
            curByte[i] = bitStream[bitStreamIndex+i];
            if(curByte[i] == -1)
                break;
        }
        data=0;
        data |= ( ((curByte[0] & 1)<<0) |
                  ((curByte[1] & 1)<<1) |
                  ((curByte[2] & 1)<<2) |
                  ((curByte[3] & 1)<<3));

        decodedStream[byteCount]=data;

        //calculate parity
        parity=0;
        for(i=0 ; i < 5 ; i++){
            parity ^= curByte[i];
        }

        //check parity
        if(parity != 1){
            //printf("parity error!\n");
            return -1;
        }

        //update LRC
        for(i=0 ; i<4 ; i++){
            lrc[i] ^= curByte[i];
        }

        byteCount++;
        bitStreamIndex+=5;  //get next byte(5bits per byte)

        //check for end sentinel
        if(curByte[0] == 1 && curByte[1] == 1 && curByte[2] == 1 && curByte[3] == 1 && curByte[4] == 1){
            break;
        }
    }

    //get LRC byte
    //bitStreamIndex+=5;
    for(i=0 ; i < 4 ; i++){
        curByte[i] = bitStream[bitStreamIndex+i];
    }

    //check LRC
    for(i=0 ; i < 4 ; i++){
        if(curByte[i] != lrc[i]){
            //printf("LRC error\n");
            //printf("calculated LRC[%d]=%d\tactual LRC=%d\n",i,lrc[i],curByte[i]);
            return -1;
        }
    }

    return 0;
}



//reverses order of bitStream[]
int reverseBitStream(int bitStream[]){
    int i;
    int reverse[MAX_NUM_DATA_BITS];

    //reverse order into temp array
    for(i=0; i<MAX_NUM_DATA_BITS ; i++){
        reverse[i] = bitStream[MAX_NUM_DATA_BITS-1-i];
    }

    //copy back to bitStream[]
    for(i=0; i<MAX_NUM_DATA_BITS ; i++){
        bitStream[i]=reverse[i];
    }

    return 0;
}

//Get card info for TRACK 2
//Expiration Data format = YYMM
//return 0 on success, -1 on error
int getCardInfo(char decodedStream[], char cardNumber[], char expDate[]){
    int i;
    int cardIndex;
    int foundFS;

    //init cardNumber(may not be all 19 digits
    for(i=0 ; i<NUM_DIGITS_CARD_NUMBER ; i++){
        cardNumber[i]=-1;
    }



    //get card number(stored as HEX)
    cardIndex=0;
    for(i=0 ; i<20 ; i++){
        if(decodedStream[i] == -1){
            return -1;

        //start sentinel(0xB=11)
        }else if (decodedStream[i] == 11){
            //do nothing

        //Found Field Separator FS(0xD=13)
        }else if (decodedStream[i] == 13){
            break;

        //found card data
        }else{
            cardNumber[cardIndex] = decodedStream[i];
            cardIndex++;
        }
    }

    //get exp data(stored as HEX)
    cardIndex=0;
    foundFS=0;
    for(i=0 ; i<25 ; i++){
        if(decodedStream[i] == -1){
            return -1;

        //Found Field Separator FS(0xD=13)
        }else if (decodedStream[i] == 13){
            foundFS=1;


        }else if (foundFS==1){
            expDate[cardIndex] = decodedStream[i];
            cardIndex++;
            if(cardIndex==4)
                break;

        }
    }



    return 0;
}

//***************************************************
//Prints all samples to stdout
//
//
//int printAllSamples(char *fileName, int numberOfChannels){
int printAllSamples(char *fileName, int channel, int numberOfChannels, int threshold, int optionApplyFilter){
    audioFrame_t audioFrame;
    FILE *inputFile;
    int i;
    audioStats_t audioStats;
    i=0;

    if(fileName[0]=='\0'){
        inputFile=NULL;
    }else{
        inputFile = fopen(fileName, "r");
        if (inputFile == NULL){
            printf("printAllSamples: Error opening %s\n", fileName);
            return -1;
        }

    }

    //remove header
    for(i=0 ; i<22 ;i++)
        getNextSample(inputFile, numberOfChannels, &audioFrame);

    //get audioStats
    if(calculateAudioStatistics(inputFile, channel, numberOfChannels, STATS_SAMPLE_SIZE, &audioStats)== -1){
        printf("Error:printAllSamples:calculateAudioStatistics\n");
        return -1;
    }

/*
    printf("min=%d\n", audioStats.min[0]);
    printf("max=%d\n", audioStats.max[0]);
    printf("mean=%d\n", audioStats.mean[0]);
    printf("thresh=%d\n", threshold);

*/


    while(getNextSample(inputFile, numberOfChannels, &audioFrame) != -1){
        if(optionApplyFilter==1){
            applyFilterOnFrame(threshold, audioStats, &audioFrame);
        }


        if(i>10)
            printf("%d\t%d\t%d\n",i,audioFrame.sample[channel], audioFrame.movingFilterOutput[channel]);

        i++;


    }

    return 0;

}


//Prints all audio statistics of file/stream
// throw away first 22 samples, could be header
//
//
int printAllStatistics(char *fileName, int channel, int numberOfChannels){
    audioFrame_t audioFrame;
    audioStats_t audioStats;
    FILE *inputFile;
    int i;

    //set appropriate filename
    if(fileName[0]=='\0'){
        inputFile=NULL;
    }else{
        inputFile = fopen(fileName, "r");
        if (inputFile == NULL){
            printf("printAllSamples: Error opening %s\n", fileName);
            return -1;
        }

    }


    //throw away first 22 samples(can include header) + init filter
    for(i=0 ; i < 50 ; i++){
        if(getNextSample(inputFile, numberOfChannels, &audioFrame) == -1){
            printf("error getting sample in printAllStatistics\n");
        }
    }

    if(calculateAudioStatistics(inputFile, channel, numberOfChannels, STATS_SAMPLE_SIZE, &audioStats)){
    //if(calculateAllAudioStatistics(inputFile, numberOfChannels, &audioStats) == -1){
        printf("error: printAllStatistics: getting audio statistic\n");
        return -1;
    }

/*
    //print the channels
    for(i=0; i<numberOfChannels ; i++){
        printf("Channel %d min=%d\n",i,audioStats.min[i]);
        printf("Channel %d max=%d\n",i,audioStats.max[i]);
        printf("Channel %d mean=%d\n",i,audioStats.mean[i]);
    }
*/

    printf("stats on %d samples\n", STATS_SAMPLE_SIZE);
    printf("min\t%d\t%d\n",audioStats.min[0],audioStats.min[1]);
    printf("mean\t%d\t%d\n",audioStats.mean[0],audioStats.mean[1]);
    printf("max\t%d\t%d\n",audioStats.max[0],audioStats.max[1]);
    printf("sigma\t%d\t%d\n",audioStats.sigma[0],audioStats.sigma[1]);
    printf("Filtered:::\n");
    printf("min\t%d\t%d\n",audioStats.minFiltered[0],audioStats.minFiltered[1]);
    printf("mean\t%d\t%d\n",audioStats.meanFiltered[0],audioStats.meanFiltered[1]);
    printf("max\t%d\t%d\n",audioStats.maxFiltered[0],audioStats.maxFiltered[1]);
    printf("sigma\t%d\t%d\n",audioStats.sigmaFiltered[0],audioStats.sigmaFiltered[1]);

    return 0;

}

