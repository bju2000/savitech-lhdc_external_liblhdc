
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lhdc_process.h"
#include "lhdcBT.h"
#include "lhdc_enc_config.h"



#define LOG_NDEBUG 0
#define LOG_TAG "lhdcBT_enc"
#include <cutils/log.h>
#define QUEUE_LENGTH_AVG_TIMES   8  //Time about QUEUE_LENGTH_AVG_TIMES * 20ms
#define UP_QUALITY_INDEX_TIMES   8  //Time about .... ex. UP_QUALITY_INDEX_TIMES * 160ms

struct _lhdc_control_block {

    LHDCBT_QUALITY_T qualityStatus;      //0xff == Auto bitrate
    int lastBitrate;     //Record lastbitrate for auto bitrate adjust.

    uint16_t avg_cnt;
    uint32_t avgValue;

    uint16_t changeBRCnt;
    uint32_t chageBRAvg;

    uint16_t upBitrateCnt;
    uint32_t upBitrateSum;

    uint16_t dnBitrateCnt;
    uint32_t dnBitrateSum;



    int sampleRate;
    int bitPerSample;
    int limitBitRateStatus;
    int err;

    FILE * recFile;
    FFT_BLOCK * fft_blk;
};




typedef struct _lhdc_control_block * lhdcBT;

static int bitRateTable[] = {64, 96, 128, 192, 256, 400, 500, 900, 1000/* auto */};

/*
#ifndef LIMITED_MAX_BITRATE
static int bitrate_array[] = {370, 380, 410, 460, 580, 900};
#warning "Max target bit rate = 900"
#else
static int bitrate_array[] = {370, 380, 390, 410, 450, 600};
#warning "Max target bit rate = 600"
#endif
*/
#define BITRATE_ELEMENTS_SIZE   8
#define QUEUE_LENGTH_LEVEL   8


static int bitrateFromIndex(HANDLE_LHDC_BT handle, int index){
	//int bitrate_array_org[] = {370, 380, 410, 460, 580, 900};
	//int bitrate_array_small[] = {370, 380, 390, 410, 450, 600};
    int bitrate_array_org[] = {64, 80, 100, 125, 180, 300, 500, 900};
	//int bitrate_array_small[] = {64, 80, 100, 125, 180, 260, 400, 600};

    int limit = bitRateTable[handle->limitBitRateStatus];

    int result = bitrate_array_org[index];
	return limit < result ? limit : result;

}

static int bitrateIndexFrom(size_t queueLength) {

    int newBitrateInx = 0;
    if (queueLength < QUEUE_LENGTH_LEVEL) {
        float queuePercenty = (1 - ((float)queueLength / QUEUE_LENGTH_LEVEL)) * (BITRATE_ELEMENTS_SIZE - 1);
        newBitrateInx = (int)queuePercenty;
    }
    return newBitrateInx;
}

static void resetUpBRParam(HANDLE_LHDC_BT h){
    h->upBitrateSum = 0;
    h->upBitrateCnt = 0;
}
static void resetDnBRParam(HANDLE_LHDC_BT h){
    h->dnBitrateSum = 0;
    h->dnBitrateCnt = 0;
}

//lhdcBT encHandle = NULL;
static int indexOfBitrate(HANDLE_LHDC_BT handle, int bitrate){
    for (size_t i = 0; i < BITRATE_ELEMENTS_SIZE; i++) {
        if (bitrateFromIndex(handle, i) == bitrate) {
            return i;
        }
    }
    return 0;
}
static void reset_queue_length(HANDLE_LHDC_BT handle){
    handle->avg_cnt = 0;
    handle->avgValue = 0;
}

void lhdcBT_setLimitBitRate(HANDLE_LHDC_BT handle, int max_rate_index) {
	if (handle == NULL || max_rate_index == LHDCBT_QUALITY_AUTO){
    	ALOGV("%s: Error LHDC instance(%p), max rate(%d)",  __func__, handle, max_rate_index);
		return;
	}
	if (max_rate_index != handle->limitBitRateStatus){

		handle->limitBitRateStatus = max_rate_index;

		if (handle->limitBitRateStatus != (LHDCBT_QUALITY_T)handle->qualityStatus ){
            if (handle->qualityStatus != LHDCBT_QUALITY_AUTO) {
                handle->qualityStatus = handle->limitBitRateStatus;
            }

            int newRate = bitRateTable[handle->limitBitRateStatus];

			if (handle->lastBitrate >= newRate)	{
                handle->lastBitrate = newRate;
        		LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
        		ALOGD("%s: Update bitrate(%d)",  __func__, handle->lastBitrate);
			}
		}
	}
}

HANDLE_LHDC_BT lhdcBT_get_handle(){
    lhdcBT handle = malloc(sizeof(struct _lhdc_control_block));
    memset((void*)handle, 0, sizeof(struct _lhdc_control_block));

    handle->fft_blk = LossyEncoderNew();
    if (handle->fft_blk == NULL) {
        lhdcBT_free_handle(handle);
        return NULL;
    }
    handle->qualityStatus = LHDCBT_QUALITY_MID;
    handle->lastBitrate = 560;
    handle->sampleRate = 96000;
    handle->bitPerSample = 24;
    handle->limitBitRateStatus = LHDCBT_QUALITY_HIGH;
    reset_queue_length(handle);
    //handle->recFile = fopen("/sdcard/Download/record.lhdc", "wb+");
    handle->recFile = NULL;

    ALOGV("%s: get LHDC instance(%p)",  __func__, handle);
    return handle;
}

void lhdcBT_free_handle(HANDLE_LHDC_BT handle){
    if (handle) {
        ALOGV("%s: free LHDC instance(%p)",  __func__, handle);
        if (handle->fft_blk) {
            LossyEncoderDelete(handle->fft_blk);
            handle->fft_blk = NULL;
        }
        if (handle->recFile) {
            /* code */
            fclose(handle->recFile);
        }
        free(handle);
    }
}


int lhdcBT_encode(HANDLE_LHDC_BT handle, void* p_pcm, unsigned char* p_stream){
    if (handle) {
        if (p_pcm == NULL || p_stream == NULL) {
            ALOGE("%s: Buffer error! source(%p), output(%p)",  __func__, p_pcm, p_stream);
            return 0;
        }
        int bytesSizePerBlock = LHDCBT_ENC_BLOCK_SIZE * (handle->bitPerSample >> 3) * 2;

        int encodedSize = LossyEncoderProcessWav(handle->fft_blk, (unsigned char *)p_pcm, LHDCBT_ENC_BLOCK_SIZE, 0, p_stream, bytesSizePerBlock);
        ALOGD("%s: Encoded size = %d",  __func__, encodedSize);
        ALOGD("%s: Encoded(%p) sampleRate = %d, bitrate = %d, bit per sample = %d",
         __func__, handle, handle->sampleRate, handle->lastBitrate, handle->bitPerSample);

//        if (handle->recFile) {
            /* code */
//            fwrite(p_stream, sizeof(uint8_t),encodedSize, handle->recFile );
//        }
        return encodedSize;
    }
    ALOGE("%s: Handle error!(%p)",  __func__, handle);
    return 0;
}

int lhdcBT_get_bitrate(HANDLE_LHDC_BT handle) {
    if (handle) {
        return handle->lastBitrate;
    }
    ALOGE("%s: Handle error!(%p)",  __func__, handle);
    return -1;
}

int lhdcBT_get_sampling_freq(HANDLE_LHDC_BT handle){
    if (handle) {
        return handle->sampleRate;
    }
    ALOGE("%s: Handle error!(%p)",  __func__, handle);
    return -1;
}

int lhdcBT_set_bitrate(HANDLE_LHDC_BT handle, int bitrate_inx){
    if (handle) {

        if (bitrate_inx != handle->qualityStatus) {

            if (bitrate_inx != LHDCBT_QUALITY_AUTO) {
                handle->lastBitrate = bitRateTable[bitrate_inx];
            }else{
                handle->lastBitrate = 400;
                resetDnBRParam(handle);
                resetUpBRParam(handle);
            }

            handle->qualityStatus = bitrate_inx;

            if (handle->qualityStatus > (LHDCBT_QUALITY_T)handle->limitBitRateStatus &&
                handle->qualityStatus != LHDCBT_QUALITY_AUTO) {
                handle->lastBitrate = bitRateTable[handle->limitBitRateStatus];
                handle->qualityStatus = handle->limitBitRateStatus;
            }
        }
        LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
        ALOGD("%s: Update bitrate(%d)",  __func__, handle->lastBitrate);
        return 0;
    }
    ALOGE("%s: Handle error!(%p)",  __func__, handle);
    return -1;
}



int lhdcBT_adjust_bitrate(HANDLE_LHDC_BT handle, size_t queueLen) {
    if (handle != NULL && handle->qualityStatus == LHDCBT_QUALITY_AUTO) {
        if (handle->dnBitrateCnt >= QUEUE_LENGTH_AVG_TIMES) {
            /* code */
            size_t queueLength = handle->dnBitrateSum / handle->dnBitrateCnt;
            uint32_t queuSumTmp = handle->dnBitrateSum;
            uint32_t queuCntTmp = handle->dnBitrateCnt;
            handle->dnBitrateSum = 0;
            handle->dnBitrateCnt = 0;
            int newBitrateInx = bitrateIndexFrom(queueLength);
            ALOGD("%s:[Down BiTrAtE] queuSumTmp(%d) / queuCntTmp(%d) = queueLength(%zu)",  __func__, queuSumTmp, queuCntTmp, queueLength);
            ALOGD("%s:[Down BiTrAtE] current bitrate(%d) bitrate_array[%d] = %d",  __func__, handle->lastBitrate, newBitrateInx, bitrateFromIndex(handle, newBitrateInx));
            if (bitrateFromIndex(handle, newBitrateInx) < handle->lastBitrate) {
                handle->lastBitrate = bitrateFromIndex(handle, newBitrateInx);
                LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
                ALOGD("%s:[Down BiTrAtE] Update bitrate(%d), queue length(%zu)",  __func__, handle->lastBitrate, queueLength);
                resetUpBRParam(handle);
            }
        }

        if (handle->upBitrateCnt >= 400) {
            //clear down bitrate parameters...
            size_t queueLength = handle->upBitrateSum / handle->upBitrateCnt;
            uint32_t queuSumTmp = handle->upBitrateSum;
            uint32_t queuCntTmp = handle->upBitrateCnt;
            handle->upBitrateSum = 0;
            handle->upBitrateCnt = 0;
            int newBitrateInx = bitrateIndexFrom(queueLength);
            ALOGD("%s:[Up BiTrAtE] queuSumTmp(%d) / queuCntTmp(%d) = queueLength(%zu)",  __func__, queuSumTmp, queuCntTmp, queueLength);
            ALOGD("%s:[Up BiTrAtE] current bitrate(%d) bitrate_array[%d] = %d",  __func__, handle->lastBitrate, newBitrateInx, bitrateFromIndex(handle, newBitrateInx));
            if (bitrateFromIndex(handle, newBitrateInx) > handle->lastBitrate && queuSumTmp == 0) {
                size_t currentInx = indexOfBitrate(handle, handle->lastBitrate);
                if (currentInx < (BITRATE_ELEMENTS_SIZE - 1)) {
                    currentInx++;
                }
                handle->lastBitrate = bitrateFromIndex(handle, currentInx);

                LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
                ALOGD("%s:[Up BiTrAtE] Update bitrate(%d), queue length(%zu)",  __func__, handle->lastBitrate, queueLength);
                resetDnBRParam(handle);
            }
        }


        handle->upBitrateSum += queueLen;
        handle->dnBitrateSum += queueLen;

        handle->upBitrateCnt++;
        handle->dnBitrateCnt++;

        return 0;
    }
    ALOGE("%s: Handle error!(%p)",  __func__, handle);
    return -1;
}

int lhdcBT_init_handle_encode(HANDLE_LHDC_BT handle,int sampling_freq, int bitPerSample, int bitrate_inx, int dualChannel, int need_padding){


    handle->sampleRate = sampling_freq;
    handle->bitPerSample = bitPerSample;

    //encHandle->targetBitrate = bitrate;
    if (bitrate_inx != LHDCBT_QUALITY_AUTO) {
        handle->lastBitrate = bitRateTable[bitrate_inx];
    }else{
        handle->lastBitrate = 400;
        resetDnBRParam(handle);
        resetUpBRParam(handle);
    }

    handle->qualityStatus = bitrate_inx;

    if (handle->qualityStatus > (LHDCBT_QUALITY_T)handle->limitBitRateStatus &&
        handle->qualityStatus != LHDCBT_QUALITY_AUTO) {
        handle->lastBitrate = bitRateTable[handle->limitBitRateStatus];
        handle->qualityStatus = handle->limitBitRateStatus;
    }

    ALOGD("%s: Init Encoder(%p) sampleRate = %d, bitrate = %d, bit per sample = %d",
     __func__, handle, handle->sampleRate, handle->lastBitrate, handle->bitPerSample);
    LossyEncoderInit(handle->fft_blk, handle->sampleRate, handle->bitPerSample, 2, LHDCBT_ENC_BLOCK_SIZE, 10 * 1024,
                 (handle->lastBitrate * 1000) / 8, 0, dualChannel, need_padding);
    return 0;
}

int lhdcBT_get_error_code(HANDLE_LHDC_BT handle){
    return handle->err;
}
