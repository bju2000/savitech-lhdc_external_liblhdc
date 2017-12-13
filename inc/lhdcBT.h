/*
 * Copyright (C) 2013 - 2016 Sony Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LHDCBT_H_
#define _LHDCBT_H_
#ifdef __cplusplus
extern "C" {
#endif
#ifndef LHDCBT_API
#define LHDCBT_API
#endif /* LHDCBT_API  */

struct _lhdc_control_block;
typedef struct _lhdc_control_block * HANDLE_LHDC_BT;


typedef enum {
    LHDCBT_SMPL_FMT_S16 = 16,
    LHDCBT_SMPL_FMT_S24 = 24,
} LHDCBT_SMPL_FMT_T;

typedef enum name {
    LHDCBT_QUALITY_LOW = 0,
    LHDCBT_QUALITY_MID,
    LHDCBT_QUALITY_HIGH,
    LHDCBT_QUALITY_AUTO,
    LHDCBT_QUALITY_MAX
} LHDCBT_QUALITY_T;

#define LHDCBT_MAX_BLOCK_SIZE 512

#define LHDCBT_ENC_BLOCK_SIZE  512

HANDLE_LHDC_BT lhdcBT_get_handle();


void lhdcBT_free_handle(HANDLE_LHDC_BT handle);

//static const char* LHDC_ENCODE_NAME = "lhdcBT_encode";
int lhdcBT_encode(HANDLE_LHDC_BT hLhdcParam, void* p_pcm, unsigned char* p_stream);

int lhdcBT_get_bitrate(HANDLE_LHDC_BT hLhdcParam);

int lhdcBT_set_bitrate(HANDLE_LHDC_BT handle, int bitrate_inx);

int lhdcBT_get_sampling_freq(HANDLE_LHDC_BT handle);

int lhdcBT_init_handle_encode(HANDLE_LHDC_BT hLhdcParam,int sampling_freq, int bitPerSample, int bitrate_inx);

int lhdcBT_get_error_code(HANDLE_LHDC_BT handle);

int lhdcBT_adjust_bitrate(HANDLE_LHDC_BT handle, size_t queueLength) ;

void lhdcBT_setLimitBitRateEnabled(HANDLE_LHDC_BT handle, int enabled);

#ifdef __cplusplus
}
#endif
#endif /* _LHDCBT_H_ */
