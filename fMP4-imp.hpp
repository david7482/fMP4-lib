#pragma once

#include "fMP4.hpp"

#include <vector>

extern "C" {
#include <libavformat/avformat.h>
};

#define GST_USE_UNSTABLE_API /* To avoid H264 parser warning */
#include <gst/codecparsers/gsth264parser.h>

class MP4WriterImp : public MP4Writer
{
public:

    MP4WriterImp(DataCallback cb);

    ~MP4WriterImp();

    virtual bool WriteH264VideoSample(unsigned char *sample,
                                      unsigned int sample_size,
                                      bool is_key_frame,
                                      unsigned long long int duration);

private:

    // Performs a write operation using the signature required for avio.
    static int Write(void* opaque, uint8_t* buf, int buf_size);

    bool AddH264VideoTrack(GstH264NalUnit &nal_sps, GstH264NalUnit &nal_pps);

    std::vector<GstH264NalUnit> ParseH264NALU(unsigned char *data, unsigned int length);

    unsigned long long int file_duration;
    AVFormatContext *format_context;
    unsigned int video_stream_id;
    unsigned int avio_buffer_size;
    GstH264NalParser *h264_parser;
    DataCallback data_callback;
};