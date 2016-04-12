#pragma once

#include <fMP4.h>

#ifdef __cplusplus
extern "C" {
#endif

fMP4Writer CNewMP4();
int GoMP4Callback(unsigned char *buf, int len);

#ifdef __cplusplus
}
#endif
