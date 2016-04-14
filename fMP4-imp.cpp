#include "fMP4-imp.hpp"

#include <netinet/in.h>

MP4Writer* MP4Writer::Create(DataCallback cb)
{
    return new MP4WriterImp(cb);
}

void MP4Writer::Release(MP4Writer *writer)
{
    delete writer;
}

MP4WriterImp::MP4WriterImp(DataCallback cb)
        : MP4Writer(cb)
        , file_duration(0)
        , format_context(nullptr)
        , video_stream_id(0)
        , avio_buffer_size(1024 * 1024)
        , h264_parser(nullptr)
        , data_callback(cb)
{
    av_register_all();

    h264_parser = gst_h264_nal_parser_new();
}

MP4WriterImp::~MP4WriterImp()
{
    if (format_context && av_write_trailer(format_context) < 0) {
        printf("Fail to write trailer\n");
    }

    if (format_context && format_context->streams[video_stream_id]) {
        if (format_context->streams[video_stream_id]->codec) {
            avcodec_close(format_context->streams[video_stream_id]->codec);
        }
    }

    if (format_context && !(format_context->oformat->flags & AVFMT_NOFILE)) {
        // Need to free the buffer that we allocate to our custom AVIOContext.
        if (format_context->pb->buffer)
            av_free(format_context->pb->buffer);

        // Free custom AVIOContext.
        if (format_context->pb)
            av_free(format_context->pb);
    }

    if (format_context)
        avformat_free_context(format_context);

    if (h264_parser)
        gst_h264_nal_parser_free(h264_parser);
}

// Performs a write operation using the signature required for avio.
int MP4WriterImp::Write(void* opaque, uint8_t* buf, int buf_size)
{
    //static int i = 0;
    //printf("#%d Write: buf: %p(%02x%02x%02x%02x %c%c%c%c), size: %d\n", i++, buf, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf_size);

    // Don't need to write mfra box
    if (buf[4] == 'm' && buf[5] == 'f' && buf[6] == 'r' && buf[7] == 'a') {
        return buf_size;
    }

    DataCallback cb = reinterpret_cast<MP4WriterImp*>(opaque)->data_callback;
    return ((cb != nullptr) ? cb(buf, buf_size) : buf_size);
}

bool MP4WriterImp::WriteH264VideoSample(unsigned char *sample,
                                        unsigned int sample_size,
                                        bool is_key_frame,
                                        unsigned long long int duration)
{
    // Parse the sample into NALUs
    std::vector<GstH264NalUnit> nalus = ParseH264NALU(sample, sample_size);

    // To compatible with AVC1 format, we need to add SPS/PPS into mp4 header. (In avcC box)
    if (!format_context) {
        if (is_key_frame) {
            GstH264NalUnit nal_sps = {0}, nal_pps = {0};
            for (auto nalu : nalus) {
                if (nalu.type == GST_H264_NAL_SPS) nal_sps = nalu;
                else if (nalu.type == GST_H264_NAL_PPS) nal_pps = nalu;
            }

            if (!AddH264VideoTrack(nal_sps, nal_pps)) {
                printf("Fail to add H264 video track\n");
                return false;
            }
        } else {
            printf("Drop current frame because it is not a key frame. Need key frame for initialization\n");
            return true;
        }
    }

    // To compatible with AVC1 format, we could not put SPS/PPS in the sample.
    // So, we need to parse the data and only write video frame NALU into mp4.
    for (auto nalu : nalus) {
        if (nalu.type == GST_H264_NAL_SLICE_IDR || nalu.type == GST_H264_NAL_SLICE) {

            // Convert AnnexB format to AVC1
            unsigned int *p = (unsigned int *) (nalu.data + nalu.offset - 4);
            *p = htonl(nalu.size);

            AVPacket packet = { 0 };
            av_init_packet(&packet);

            packet.stream_index = video_stream_id;
            packet.data         = (unsigned char *)(p);
            packet.size         = nalu.size + 4;
            packet.pos          = -1;

            packet.dts = packet.pts = static_cast<int64_t>(file_duration);
            packet.duration = static_cast<int>(duration);
            av_packet_rescale_ts(&packet, (AVRational){1, 1000}, format_context->streams[video_stream_id]->time_base);

            if (is_key_frame) {
                packet.flags |= AV_PKT_FLAG_KEY;
            }

            if (av_interleaved_write_frame(format_context, &packet) < 0) {
                printf("Fail to write frame\n");
                return false;
            }

            file_duration += duration;
        }
    }

    return true;
}

bool MP4WriterImp::AddH264VideoTrack(GstH264NalUnit &nal_sps, GstH264NalUnit &nal_pps)
{
    // Parse SPS to get necessary params.
    unsigned char profile_idc = 0;
    unsigned char profile_compatibility = 0;
    unsigned char level_idc = 0;
    int width  = 0, height = 0;
    {
        GstH264SPS sps = {0};
        gst_h264_parser_parse_sps(h264_parser, &nal_sps, &sps, false);

        profile_idc = sps.profile_idc;
        level_idc = sps.level_idc;
        width  = sps.frame_cropping_flag ? sps.crop_rect_width : sps.width;
        height = sps.frame_cropping_flag ? sps.crop_rect_height : sps.height;

        profile_compatibility = 0x00;
        //profile_compatibility = (sps.constraint_set0_flag << 7) | (sps.constraint_set1_flag << 6) |
        //                        (sps.constraint_set2_flag << 5) | (sps.constraint_set3_flag << 4);

        printf("Profile: %d, Compatibility: %d, Level: %d\n", profile_idc, profile_compatibility, level_idc);
        printf("Width: %d, Height: %d\n", width, height);
    }

    avformat_alloc_output_context2(&format_context, nullptr, "mp4", nullptr);
    if (!format_context) {
        printf("Fail to create output context\n");
        return false;
    }

    AVStream *out_stream = avformat_new_stream(format_context, nullptr);
    if (!out_stream) {
        printf("Fail to allocate output stream\n");
        return false;
    }

    out_stream->id = video_stream_id = format_context->nb_streams - 1;
    out_stream->codec->codec_id   = AV_CODEC_ID_H264;
    out_stream->codec->profile    = profile_idc;
    out_stream->codec->level      = level_idc;
    out_stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    out_stream->codec->width      = width;
    out_stream->codec->height     = height;
    out_stream->codec->pix_fmt    = AV_PIX_FMT_YUV420P;
    out_stream->codec->codec_tag  = 0;

    // Fill extra data for AVCC format
    unsigned int extradata_offset = 0;
    out_stream->codec->extradata_size = nal_sps.size + nal_pps.size + 11;
    out_stream->codec->extradata = (uint8_t *)av_mallocz(out_stream->codec->extradata_size);
    out_stream->codec->extradata[extradata_offset++] = 0x01;                     // configurationVersion
    out_stream->codec->extradata[extradata_offset++] = profile_idc;              // AVCProfileIndication
    out_stream->codec->extradata[extradata_offset++] = profile_compatibility;    // profile_compatibility
    out_stream->codec->extradata[extradata_offset++] = level_idc;                // AVCLevelIndication, level: 4.0
    out_stream->codec->extradata[extradata_offset++] = 0xff;                     // 6 bits reserved (111111) + 2 bits nal size length - 1 (11)
    out_stream->codec->extradata[extradata_offset++] = 0xe1;                     // 3 bits reserved (111) + 5 bits number of sps (00001)

    unsigned short sps_size = static_cast<unsigned short>(nal_sps.size);
    *(unsigned short *)(out_stream->codec->extradata + extradata_offset) = htons(sps_size);
    extradata_offset += 2;
    memcpy(out_stream->codec->extradata + extradata_offset, nal_sps.data + nal_sps.offset, nal_sps.size);
    extradata_offset += nal_sps.size;

    out_stream->codec->extradata[extradata_offset++] = 0x01;                     // 8 bits number of pps (00000000)
    unsigned short pps_size = static_cast<unsigned short>(nal_pps.size);
    *(unsigned short *)(out_stream->codec->extradata + extradata_offset) = htons(pps_size);
    extradata_offset += 2;
    memcpy(out_stream->codec->extradata + extradata_offset, nal_pps.data + nal_pps.offset, nal_pps.size);
    extradata_offset += nal_pps.size;

    if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
        out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

    av_dump_format(format_context, 0, "CustomAVIO", 1);

    /*
     * Open output file
     */
    {
        if (!(format_context->oformat->flags & AVFMT_NOFILE)) {
            // Allocate our custom AVIO context
            AVIOContext *avio_out = avio_alloc_context(static_cast<unsigned char *>(av_malloc(avio_buffer_size)),
                                                       avio_buffer_size,
                                                       1,
                                                       this,
                                                       nullptr,
                                                       &Write,
                                                       nullptr
            );
            if(!avio_out) {
                printf("Fail to create avio context\n");
                return false;
            }

            format_context->pb = avio_out;
            format_context->flags = AVFMT_FLAG_CUSTOM_IO;
        }
    }

    /*
     * Write file header
     */
    {
        AVDictionary *movflags = nullptr;

        const bool low_delay = true;
        if (low_delay) {
            // In case of low delay, set fragment duration to 200 ms.
            av_dict_set(&movflags, "movflags", "empty_moov+default_base_moof", 0);
            av_dict_set_int(&movflags, "frag_duration", 200 * 1000, 0);
        } else {
            // Only produce fragment until we have next key frame.
            av_dict_set(&movflags, "movflags", "empty_moov+default_base_moof+frag_keyframe", 0);
        }

        if (avformat_write_header(format_context, &movflags) < 0) {
            printf("Error occurred when opening output file\n");
            return false;
        }
        av_dict_free(&movflags);
    }

    return true;
}

std::vector<GstH264NalUnit> MP4WriterImp::ParseH264NALU(unsigned char *data, unsigned int length)
{
    std::vector<GstH264NalUnit> nalus;

    GstH264NalUnit nalu = {0};
    GstH264ParserResult result;
    unsigned int offset = 0;
    while ((result = gst_h264_parser_identify_nalu(h264_parser, data, offset, length, &nalu)) == GST_H264_PARSER_OK)
    {
        // Update the offset
        gst_h264_parser_parse_nal(h264_parser, &nalu);
        offset = nalu.size + nalu.offset;

        nalus.push_back(nalu);
    }

    // Handle the last NALU because there is no other start_code followed it.
    if (gst_h264_parser_identify_nalu_unchecked(h264_parser, data, offset, length, &nalu) == GST_H264_PARSER_OK) {

        gst_h264_parser_parse_nal(h264_parser, &nalu);

        nalus.push_back(nalu);
    }

    return nalus;
}