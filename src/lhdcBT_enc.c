
#include <stdio.h>
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
    int err;

    FILE * recFile;
    FFT_BLOCK * fft_blk;
};




typedef struct _lhdc_control_block * lhdcBT;
#ifndef LIMITED_MAX_BITRATE
static int bitrate_array[] = {370, 380, 410, 460, 580, 900};
#warning "Max target bit rate = 900"
#else
static int bitrate_array[] = {370, 380, 390, 410, 450, 600};
#warning "Max target bit rate = 600"
#endif
#define BITRATE_ELEMENTS_SIZE   (sizeof(bitrate_array) / sizeof(int))
#define BITRATE_AT_INDEX(x) bitrate_array[x]
#define QUEUE_LENGTH_LEVEL   8


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
static int indexOfBitrate(int bitrate){
    for (size_t i = 0; i < BITRATE_ELEMENTS_SIZE; i++) {
        if (bitrate_array[i] == bitrate) {
            return i;
        }
    }
    return 0;
}
static void reset_queue_length(HANDLE_LHDC_BT handle){
    handle->avg_cnt = 0;
    handle->avgValue = 0;
}

HANDLE_LHDC_BT lhdcBT_get_handle(){
    lhdcBT handle = malloc(sizeof(struct _lhdc_control_block));
    memset(handle, 0, sizeof(struct _lhdc_control_block));

    handle->fft_blk = LossyEncoderNew();
    if (handle->fft_blk == NULL) {
        lhdcBT_free_handle(handle);
        return NULL;
    }
    handle->qualityStatus = LHDCBT_QUALITY_MID;
    handle->lastBitrate = 560;
    handle->sampleRate = 96000;
    handle->bitPerSample = 24;
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
            switch ((LHDCBT_QUALITY_T)bitrate_inx) {
                default:
                case LHDCBT_QUALITY_LOW:
                handle->qualityStatus = LHDCBT_QUALITY_LOW;
                handle->lastBitrate = 400;
                break;
                case LHDCBT_QUALITY_MID:
                handle->qualityStatus = LHDCBT_QUALITY_MID;
                handle->lastBitrate = 500;
                if (handle->bitPerSample == 24) {
                    handle->lastBitrate = 560;
                }
                break;
                case LHDCBT_QUALITY_HIGH:
                handle->qualityStatus = LHDCBT_QUALITY_HIGH;
#ifndef LIMITED_MAX_BITRATE
                handle->lastBitrate = 900;
#else
                handle->lastBitrate = 600;
#endif
                break;
                case LHDCBT_QUALITY_AUTO:
                handle->qualityStatus = LHDCBT_QUALITY_AUTO;
                if (!handle->lastBitrate) {
                    handle->lastBitrate = 400;
                }
                //handle->changeBRCnt = 0;
                //reset_queue_length(handle);
                resetDnBRParam(handle);
                resetUpBRParam(handle);
                break;
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
            ALOGD("%s:[Down BiTrAtE] queuSumTmp(%d) / queuCntTmp(%d) = queueLength(%d)",  __func__, queuSumTmp, queuCntTmp, queueLength);
            ALOGD("%s:[Down BiTrAtE] current bitrate(%d) bitrate_array[%d] = %d",  __func__, handle->lastBitrate, newBitrateInx, bitrate_array[newBitrateInx]);
            if (bitrate_array[newBitrateInx] < handle->lastBitrate) {
                handle->lastBitrate = bitrate_array[newBitrateInx];
                LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
                ALOGD("%s:[Down BiTrAtE] Update bitrate(%d), queue length(%zu)",  __func__, handle->lastBitrate, queueLength);
                resetUpBRParam(handle);

            }   /*else if (bitrate_array[newBitrateInx] > handle->lastBitrate && queuSumTmp > 0){
                size_t currentInx = indexOfBitrate(handle->lastBitrate);
                if (currentInx > 0) {
                    currentInx--;
                    handle->lastBitrate = bitrate_array[currentInx];
                    LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
                    ALOGD("%s:[Down BiTrAtE] Update bitrate(%d), queuSumTmp(%d)",  __func__, handle->lastBitrate, queuSumTmp);
                    resetUpBRParam(handle);
                }
            }*/
        }

        if (handle->upBitrateCnt >= 400) {
            //clear down bitrate parameters...
            size_t queueLength = handle->upBitrateSum / handle->upBitrateCnt;
            uint32_t queuSumTmp = handle->upBitrateSum;
            uint32_t queuCntTmp = handle->upBitrateCnt;
            handle->upBitrateSum = 0;
            handle->upBitrateCnt = 0;
            int newBitrateInx = bitrateIndexFrom(queueLength);
            ALOGD("%s:[Up BiTrAtE] queuSumTmp(%d) / queuCntTmp(%d) = queueLength(%d)",  __func__, queuSumTmp, queuCntTmp, queueLength);
            ALOGD("%s:[Up BiTrAtE] current bitrate(%d) bitrate_array[%d] = %d",  __func__, handle->lastBitrate, newBitrateInx, bitrate_array[newBitrateInx]);
            if (bitrate_array[newBitrateInx] > handle->lastBitrate && queuSumTmp == 0) {
                size_t currentInx = indexOfBitrate(handle->lastBitrate);
                if (currentInx < (BITRATE_ELEMENTS_SIZE - 1)) {
                    currentInx++;
                }
                handle->lastBitrate = bitrate_array[currentInx];
                LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
                ALOGD("%s:[Up BiTrAtE] Update bitrate(%d), queue length(%zu)",  __func__, handle->lastBitrate, queueLength);
                resetDnBRParam(handle);
            }
        }


        handle->upBitrateSum += queueLen;
        handle->dnBitrateSum += queueLen;

        handle->upBitrateCnt++;
        handle->dnBitrateCnt++;


/*      Old method
        if (handle->avg_cnt < QUEUE_LENGTH_AVG_TIMES) {
            handle->avgValue += queueLen;
            handle->avg_cnt++;
            return 0;
        }
        size_t queueLength = handle->avgValue / handle->avg_cnt;
        ALOGD("%s: Auto bitrate enabled(queue length = %zu)",  __func__, queueLength);
        reset_queue_length(handle);
        int newBitrateInx = bitrateIndexFrom(queueLen);

        if (bitrate_array[newBitrateInx] > handle->lastBitrate && queueLength == 0) {
            if (handle->changeBRCnt < UP_QUALITY_INDEX_TIMES) {
                handle->changeBRCnt
                return 0;
            }
            handle->changeBRCnt = 0;
            size_t currentInx = indexOfBitrate(handle->lastBitrate);
            if (currentInx < (BITRATE_ELEMENTS_SIZE - 1)) {
                currentInx++;
            }
            handle->lastBitrate = bitrate_array[currentInx];
            LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
            ALOGD("%s: Update bitrate(%d), queue length(%zu)",  __func__, handle->lastBitrate, queueLength);
        }else if (bitrate_array[newBitrateInx] < handle->lastBitrate) {
            handle->changeBRCnt = 0;
            handle->lastBitrate = bitrate_array[newBitrateInx];
            LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->lastBitrate * 1000) / 8);
            ALOGD("%s: Update bitrate(%d), queue length(%zu)",  __func__, handle->lastBitrate, queueLength);
        }else {
            handle->changeBRCnt = 0;

        }


        if (handle->recFile == NULL) {
            handle->recFile = fopen("/sdcard/Download/recData.txt", "wb+");
        }

        if (handle->recFile != NULL) {
            char test[1024];

            sprintf(test, "%d, %zu\n", handle->lastBitrate, queueLength);
            ALOGD("%s: Write data\"%s\"",  __func__, test);
            //fprintf(handle->recFile, "%d, %zu\n", handle->lastBitrate, queueLength);
            fwrite(test, sizeof(char),strlen(test), handle->recFile );
        }
        */
        return 0;
    }
    ALOGE("%s: Handle error!(%p)",  __func__, handle);
    return -1;
}

int lhdcBT_init_handle_encode(HANDLE_LHDC_BT handle,int sampling_freq, int bitPerSample, int bitrate_inx){


    handle->sampleRate = sampling_freq;
    handle->bitPerSample = bitPerSample;

    //encHandle->targetBitrate = bitrate;
    switch ((LHDCBT_QUALITY_T)bitrate_inx) {
        default:
        case LHDCBT_QUALITY_LOW:
        handle->qualityStatus = LHDCBT_QUALITY_LOW;
        handle->lastBitrate = 400;
        break;
        case LHDCBT_QUALITY_MID:
        handle->qualityStatus = LHDCBT_QUALITY_MID;
        handle->lastBitrate = 500;
        if (bitPerSample == 24) {
            handle->lastBitrate = 560;
        }
        break;
        case LHDCBT_QUALITY_HIGH:
        handle->qualityStatus = LHDCBT_QUALITY_HIGH;
#ifndef LIMITED_MAX_BITRATE
        handle->lastBitrate = 900;
#else
        handle->lastBitrate = 600;
#endif
        break;
        case LHDCBT_QUALITY_AUTO:
        handle->qualityStatus = LHDCBT_QUALITY_AUTO;
        if (!handle->lastBitrate) {
            handle->lastBitrate = 400;
        }
        //handle->changeBRCnt = 0;
        //reset_queue_length(handle);
        resetDnBRParam(handle);
        resetUpBRParam(handle);
        break;
    }

    ALOGD("%s: Init Encoder(%p) sampleRate = %d, bitrate = %d, bit per sample = %d",
     __func__, handle, handle->sampleRate, handle->lastBitrate, handle->bitPerSample);
    LossyEncoderInit(handle->fft_blk, handle->sampleRate, handle->bitPerSample, 2, LHDC_ENC_BLOCK_SIZE, 10 * 1024,
                 (handle->lastBitrate * 1000) / 8, 0, 0);
    return 0;
}

int lhdcBT_get_error_code(HANDLE_LHDC_BT handle){
    return handle->err;
}
