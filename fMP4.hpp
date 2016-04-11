#pragma once

#include "fMP4.h"

class MP4Writer
{
public:

    static MP4Writer *Create(DataCallback cb);

    static void Release(MP4Writer *writer);

    MP4Writer(DataCallback cb) {};

    virtual bool WriteH264VideoSample(unsigned char *sample,
                                      unsigned int sample_size,
                                      bool is_key_frame,
                                      unsigned long long int duration) = 0;

protected:

    virtual ~MP4Writer() {};

};