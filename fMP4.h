#ifndef FMP4_FMP4_LIB_H
#define FMP4_FMP4_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* fMP4Writer;
typedef int (*DataCallback)(unsigned char*, int);

fMP4Writer fMP4_CreateWriter(DataCallback cb);

void fMP4_ReleaseWriter(fMP4Writer);

bool fMP4_WriteH264Sample(fMP4Writer,
                          unsigned char *sample,
                          unsigned int sample_size,
                          bool is_key_frame,
                          unsigned long long int duration);

#ifdef __cplusplus
}
#endif

#endif //FMP4_FMP4_LIB_H
