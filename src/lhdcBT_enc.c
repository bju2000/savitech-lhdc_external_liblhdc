
#include <stdio.h>
#include <stdlib.h>
#include "lhdc_process.h"
#include "lhdcBT.h"
#include "lhdc_enc_config.h"



#define LOG_NDEBUG 0
#define LOG_TAG "lhdcBT_enc"
#include <cutils/log.h>
struct _lhdc_control_block {

    int targetBitrate;
    int sampleRate;
    int bitPerSample;
    int err;

    FILE * recFile;
    FFT_BLOCK * fft_blk;
};



typedef struct _lhdc_control_block * lhdcBT;

//lhdcBT encHandle = NULL;

HANDLE_LHDC_BT lhdcBT_get_handle(){
    lhdcBT handle = malloc(sizeof(struct _lhdc_control_block));
    memset(handle, 0, sizeof(struct _lhdc_control_block));

    handle->fft_blk = LossyEncoderNew();
    if (handle->fft_blk == NULL) {
        lhdcBT_free_handle(handle);
        return NULL;
    }
    handle->targetBitrate = 560;
    handle->sampleRate = 96000;
    handle->bitPerSample = 24;
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
         __func__, handle, handle->sampleRate, handle->targetBitrate, handle->bitPerSample);

        if (handle->recFile) {
            /* code */
            fwrite(p_stream, sizeof(uint8_t),encodedSize, handle->recFile );
        }
        return encodedSize;
    }
    ALOGE("%s: Handle error!(%p)",  __func__, handle);
    return 0;
}

int lhdcBT_get_bitrate(HANDLE_LHDC_BT handle) {
    if (handle) {
        return handle->targetBitrate;
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
        switch ((LHDCBT_QUALITY_T)bitrate_inx) {
            default:
            case LHDCBT_QUALITY_LOW:
            handle->targetBitrate = 400;
            break;
            case LHDCBT_QUALITY_MID:
            handle->targetBitrate = 500;
            if (handle->bitPerSample == 24) {
                handle->targetBitrate = 560;
            }
            break;
            case LHDCBT_QUALITY_HIGH:
            handle->targetBitrate = 900;
            break;
            case LHDCBT_QUALITY_AUTO:
            handle->targetBitrate = 400;
            break;
        }
        LossyEncoderSetTargetByteRate(handle->fft_blk, (handle->targetBitrate * 1000) / 8);
        ALOGD("%s: Update bitrate(%p)",  __func__, handle->targetBitrate);
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
        handle->targetBitrate = 400;
        break;
        case LHDCBT_QUALITY_MID:
        handle->targetBitrate = 500;
        if (bitPerSample == 24) {
            handle->targetBitrate = 560;
        }
        break;
        case LHDCBT_QUALITY_HIGH:
        handle->targetBitrate = 900;
        break;
        case LHDCBT_QUALITY_AUTO:
        handle->targetBitrate = 400;
        break;
    }

    ALOGD("%s: Init Encoder(%p) sampleRate = %d, bitrate = %d, bit per sample = %d",
     __func__, handle, handle->sampleRate, handle->targetBitrate, handle->bitPerSample);
    LossyEncoderInit(handle->fft_blk, handle->sampleRate, handle->bitPerSample, 2, LHDC_ENC_BLOCK_SIZE, 10 * 1024,
                 (handle->targetBitrate * 1000) / 8, 0, 0);
    return 0;
}

int lhdcBT_get_error_code(HANDLE_LHDC_BT handle){
    return handle->err;
}
