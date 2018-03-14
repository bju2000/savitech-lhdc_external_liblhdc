#ifdef _PROCESS_BLOCK_H_
#else
#define _PROCESS_BLOCK_H_


struct FFT_block_s;
typedef struct FFT_block_s FFT_BLOCK;

int LossyEncoderLoadQualitySetting(FFT_BLOCK *fb, char *file_name);
FFT_BLOCK *LossyEncoderNew(void);
int LossyEncoderDelete(FFT_BLOCK *fb);
void LossyEncoderInit(FFT_BLOCK *fb, int sample_rate, int bits_per_sample, int channels, int block_size, int sink_buf_len, int target_byte_rate, int fast_mode, int one_frame_per_channel);
int LossyEncoderProcessWav(FFT_BLOCK *fb, unsigned char *wav, int ns, int final, unsigned char *out, int out_len);
int LossyEncoderProcessPCM(FFT_BLOCK *fb, int *pcm0, int *pcm1, int ns, int final, unsigned char *out, int out_len);
void LossyEncoderSetTargetByteRate(FFT_BLOCK *fb, int target_byte_rate);
void LossyEncoderResetAlignmentBuf(FFT_BLOCK *fb);
#endif // _PROCESS_BLOCK_H_
