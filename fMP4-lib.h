#ifndef FMP4_FMP4_LIB_H
#define FMP4_FMP4_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* fMP4Writer;
typedef int (*DataCallback)(unsigned char*, int);

fMP4Writer CreatefMP4Writer(DataCallback cb);

void ReleasefMP4Writer(fMP4Writer);

bool WriteH264VideoSample(fMP4Writer,
                          unsigned char *sample,
                          unsigned int sample_size,
                          bool is_key_frame,
                          unsigned long long int duration);

#ifdef __cplusplus
}
#endif

#endif //FMP4_FMP4_LIB_H
