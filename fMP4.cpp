#include "fMP4.h"
#include "fMP4.hpp"

fMP4Writer fMP4_CreateWriter(DataCallback cb)
{
    MP4Writer *fmp4_writer = MP4Writer::Create(cb);
    return fmp4_writer;
}

void fMP4_ReleaseWriter(fMP4Writer fmp4_writer)
{
    MP4Writer *writer = reinterpret_cast<MP4Writer *>(fmp4_writer);
    MP4Writer::Release(writer);
}

bool fMP4_WriteH264Sample(fMP4Writer fmp4_writer,
                          unsigned char *sample,
                          unsigned int sample_size,
                          bool is_key_frame,
                          unsigned long long int duration)
{
    MP4Writer *writer = reinterpret_cast<MP4Writer *>(fmp4_writer);
    return writer->WriteH264VideoSample(sample, sample_size, is_key_frame, duration);
}