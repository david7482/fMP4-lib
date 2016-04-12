#include <thread>
#include <memory>
#include <cstring>

#include <glibmm-2.4/glibmm.h>
#include <giomm-2.4/giomm.h>
#include <mp4v2/mp4v2.h>
#include <netinet/in.h>

#include "ws-client.hpp"
#include "fMP4.h"

class OptionGroup : public Glib::OptionGroup
{
public:

    OptionGroup() : Glib::OptionGroup("", ""), repeat(false)
    {
        AddEntry('s', "server", "Set server address. Ex: echo.websocket.org:80", server);
        AddEntry('r', "repeat", "Enable repeat mode", repeat);
        AddEntryFileName('m', "mp4", "Set MP4 file path", mp4_file_path);
    }

    virtual ~OptionGroup() {}

    const std::string &GetServer() const { return server.raw(); }

    const std::string &GetMp4FilePath() const { return mp4_file_path; }

    const bool GetRepeatMode() const { return repeat; }

    void AddEntry(const char &short_name, const std::string &long_name, const std::string &description, Glib::ustring &arg)
    {
        Glib::OptionEntry entry;
        entry.set_short_name(short_name);
        entry.set_long_name(long_name);
        entry.set_description(description);
        add_entry(entry, arg);
    }

    void AddEntry(const char &short_name, const std::string &long_name, const std::string &description, bool &arg)
    {
        Glib::OptionEntry entry;
        entry.set_short_name(short_name);
        entry.set_long_name(long_name);
        entry.set_description(description);
        add_entry(entry, arg);
    }

    void AddEntryFileName(const char &short_name, const std::string &long_name, const std::string &description, std::string &arg)
    {
        Glib::OptionEntry entry;
        entry.set_short_name(short_name);
        entry.set_long_name(long_name);
        entry.set_description(description);
        add_entry_filename(entry, arg);
    }

private:

    Glib::ustring server;
    std::string mp4_file_path;
    bool repeat;
};

class MP4Reader
{
public:

    enum MP4ReadStatus
    {
        MP4_READ_OK,
        MP4_READ_EOS,
        MP4_READ_ERR
    };

    MP4Reader(const std::string &file_path)
            : time_scale(9 * MP4_MSECS_TIME_SCALE)
            , file_path(file_path)
            , handle(MP4_INVALID_FILE_HANDLE)
            , video_track_id(MP4_INVALID_TRACK_ID)
            , next_video_sample_idx(1)
            , video_sample(nullptr)
            , video_timescale(0)
            , video_sample_max_size(0)
            , video_sample_number(0)
            , video_duration(0)
            , pSeqHeaders(nullptr)
            , pSeqHeaderSize(nullptr)
            , pPictHeaders(nullptr)
            , pPictHeaderSize(nullptr)
    {
        handle = MP4Read(this->file_path.c_str());

        video_track_id = MP4FindTrackId(handle, 0, MP4_VIDEO_TRACK_TYPE);
        if (video_track_id != MP4_INVALID_TRACK_ID) {
            video_timescale = MP4GetTrackTimeScale(handle, video_track_id);
            video_sample_max_size = MP4GetTrackMaxSampleSize(handle, video_track_id) * 2;
            video_duration = MP4GetTrackDuration(handle, video_track_id);
            video_sample = new unsigned char[video_sample_max_size];
            video_sample_number = MP4GetTrackNumberOfSamples(handle, video_track_id);

            if (MP4GetTrackH264SeqPictHeaders(handle,
                                              video_track_id,
                                              &pSeqHeaders,
                                              &pSeqHeaderSize,
                                              &pPictHeaders,
                                              &pPictHeaderSize))
            {
                printf("Get SPS(%d) and PPS(%d)\n", *pSeqHeaderSize, *pPictHeaderSize);

                for(int i = 0; (pSeqHeaders[i] && pSeqHeaderSize[i]); i++) {
                    printf("SPS(%d): %02x %02x %02x %02x %02x\n", i,
                           pSeqHeaders[i][0], pSeqHeaders[i][1], pSeqHeaders[i][2],
                           pSeqHeaders[i][3], pSeqHeaders[i][4]);
                }
                for(int i = 0; (pPictHeaders[i] && pPictHeaderSize[i]); i++) {
                    printf("PPS(%d): %02x %02x %02x %02x %02x\n", i,
                           pPictHeaders[i][0], pPictHeaders[i][1], pPictHeaders[i][2],
                           pPictHeaders[i][3], pPictHeaders[i][4]);
                }
            }
        }
    }

    ~MP4Reader()
    {
        if (pSeqHeaders || pSeqHeaderSize || pPictHeaders || pPictHeaderSize) {
            MP4FreeH264SeqPictHeaders(pSeqHeaders, pSeqHeaderSize, pPictHeaders, pPictHeaderSize);
        }

        if (handle != MP4_INVALID_FILE_HANDLE) MP4Close(handle);
        if (video_sample) delete[] video_sample;
    }

    std::string GetFilePath() const
    {
        return file_path;
    }

    MP4ReadStatus GetNextH264VideoSample(unsigned char **sample,
                                         unsigned int &sample_size,
                                         unsigned long long int &duration,
                                         bool &is_key_frame)
    {
        if (next_video_sample_idx > video_sample_number) {
            return MP4_READ_EOS;
        }

        unsigned int video_sample_offset = 0;
        if(MP4GetSampleSync(handle, video_track_id, next_video_sample_idx)) {
            /*
             * If current sample has key frame, we need to put SPS/PPS in front of key frame.
             */
            if (pSeqHeaders && pSeqHeaderSize) {
                for(int i = 0; (pSeqHeaders[i] && pSeqHeaderSize[i]); i++) {
                    (*(unsigned int *)(video_sample + video_sample_offset)) = htonl(1);
                    video_sample_offset += 4;
                    memcpy(video_sample + video_sample_offset, pSeqHeaders[i], pSeqHeaderSize[i]);
                    video_sample_offset += pSeqHeaderSize[i];
                }
            }
            if (pPictHeaders && pPictHeaderSize) {
                for(int i = 0; (pPictHeaders[i] && pPictHeaderSize[i]); i++) {
                    (*(unsigned int *)(video_sample + video_sample_offset)) = htonl(1);
                    video_sample_offset += 4;
                    memcpy(video_sample + video_sample_offset, pPictHeaders[i], pPictHeaderSize[i]);
                    video_sample_offset += pPictHeaderSize[i];
                }
            }
        }

        MP4Duration mp4_duration = 0;
        unsigned char *video_sample_start_addr = video_sample + video_sample_offset;
        sample_size = video_sample_max_size - video_sample_offset;
        if (!MP4ReadSample(handle, video_track_id, next_video_sample_idx,
                           &video_sample_start_addr, &sample_size,
                           NULL,
                           &mp4_duration,
                           NULL,
                           &is_key_frame)) {
            printf("Fail to read video sample (%d)\n", next_video_sample_idx);
            return MP4_READ_ERR;
        }

        // Convert AVC1 format to AnnexB
        if (sample_size >= 4) {
            unsigned int *p = (unsigned int *) video_sample_start_addr;
            *p = htonl(1);
        }

        *sample = video_sample;
        sample_size += video_sample_offset;
        duration = (1000 * mp4_duration) / time_scale;
        next_video_sample_idx++;
        return MP4_READ_OK;
    }

private:

    const unsigned int time_scale;

    std::string file_path;
    MP4FileHandle handle;
    MP4TrackId video_track_id;
    unsigned int next_video_sample_idx;
    unsigned char *video_sample;

    unsigned int video_timescale;
    unsigned int video_sample_max_size;
    unsigned int video_sample_number;
    unsigned long long int video_duration;
    unsigned char **pSeqHeaders;
    unsigned int *pSeqHeaderSize;
    unsigned char **pPictHeaders;
    unsigned int *pPictHeaderSize;
};

Glib::RefPtr<Glib::MainLoop> mainloop;
std::shared_ptr<MP4Reader> mp4_reader;
std::shared_ptr<WebSocketClient> websocket_client;
std::chrono::steady_clock::time_point wait_timepoint = std::chrono::steady_clock::now();
unsigned char sample_buffer[1024 * 1024];
OptionGroup option_group;

fMP4Writer fmp4_writer = nullptr;
FILE *fptr = nullptr;

static bool OnExit(gpointer data)
{
    if (fmp4_writer) {
        fMP4_ReleaseWriter(fmp4_writer);
        fmp4_writer = nullptr;
    }

    if (fptr) {
        fclose(fptr);
        fptr = nullptr;
    }

    mainloop->quit();

    return G_SOURCE_REMOVE;
}

static bool ReadSample()
{
    unsigned char *sample = nullptr;
    unsigned int sample_size = 0;
    unsigned long long int duration = 0;
    bool is_key_frame = false;

    MP4Reader::MP4ReadStatus status = mp4_reader->GetNextH264VideoSample(&sample, sample_size, duration, is_key_frame);
    if (status == MP4Reader::MP4_READ_ERR) {
        printf("Fail to get next H264 sample from MP4\n");
        return true;
    } else if (status == MP4Reader::MP4_READ_EOS) {
        // Already get the end of current MP4 file, we will loop from the beginning.
        std::string file_path = mp4_reader->GetFilePath();
        mp4_reader.reset();

        if (option_group.GetRepeatMode()) {
            mp4_reader = std::make_shared<MP4Reader>(file_path);
            status = mp4_reader->GetNextH264VideoSample(&sample, sample_size, duration, is_key_frame);
            if (status != MP4Reader::MP4_READ_OK) {
                printf("Fail to loop back to the first sample\n");
                return true;
            }
        } else {
            return false;
        }
    }
    if (sample_size == 0) {
        printf("Fail because sample size is zero\n");
        return true;
    }

    // Wait until the next timestamp then push out the frame.
    std::this_thread::sleep_until(wait_timepoint);

    // Send data to websocket
    if (websocket_client) {

        // Append meta-data to sample buffer then copy sample to sample buffer
        unsigned int sample_buffer_offset = 0;
        {
            sample_buffer[sample_buffer_offset++] = (is_key_frame) ? 0x01 : 0x00;

            *(unsigned int *)(&sample_buffer[sample_buffer_offset]) = static_cast<unsigned int>(duration);
            sample_buffer_offset += 4;

            *(unsigned int *)(&sample_buffer[sample_buffer_offset]) = sample_size;
            sample_buffer_offset += 4;

            memcpy(&sample_buffer[sample_buffer_offset], sample, sample_size);
            sample_buffer_offset += sample_size;
        }

        websocket_client->SendData(sample_buffer, sample_buffer_offset);
    }

    // Update the waiting time point
    wait_timepoint = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration);

    return true;
}

static int Write(unsigned char* buf, int buf_size)
{
    static int i = 0;
    printf("#%d Write: buf: %p(%02x%02x%02x%02x %c%c%c%c), size: %d\n", i++, buf, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf_size);
    return fwrite(buf, 1, buf_size, fptr);
}

static void OnWebsocketConnected(const std::string &file_path)
{
    printf("Websocket connected\n");

    mp4_reader = std::make_shared<MP4Reader>(file_path);
    Glib::signal_idle().connect(sigc::ptr_fun(&ReadSample));

    wait_timepoint = std::chrono::steady_clock::now();
}

static void OnWebsocketClosed()
{
    printf("Websocket closed\n");
    mainloop->quit();
}

static void OnWebsocketMessage(const std::string& msg)
{
    printf("WebsocketMessage: %s\n", msg.c_str());
}

static void OnWebsocketData(const unsigned char *data, unsigned int data_size)
{
    // If we test with echo server (echo.websocket.org),
    // we will get data back so that we could mux and dump those data into mp4.
    {
        if (!fptr)
            fptr = fopen("dump.mp4", "wb");
        if (!fmp4_writer)
            fmp4_writer = fMP4_CreateWriter(&Write);

        bool tmp_is_key_frame = (data[0] == 0x01);
        unsigned int tmp_duration = *(unsigned int *)(&data[1]);
        unsigned int tmp_size = *(unsigned int *)(&data[5]);
        unsigned char *tmp_sample = const_cast<unsigned char *>(&data[9]);
        fMP4_WriteH264Sample(fmp4_writer, tmp_sample, tmp_size, tmp_is_key_frame, tmp_duration);
    }
}

int main (int argc, char **argv)
{
    // Setup program command line context
    Glib::OptionContext option_context("- Websocket Client");
    option_context.set_main_group(option_group);
    try {
        option_context.parse(argc, argv);
    } catch (const Glib::Error &ex) {
        printf("Options parsing failure: %s\n", ex.what().c_str());
        return 1;
    }

    Gio::init();

    websocket_client = std::make_shared<WebSocketClient>();
    websocket_client->RegisterSignals(sigc::bind(sigc::ptr_fun(&OnWebsocketConnected), option_group.GetMp4FilePath()),
                                      sigc::ptr_fun(&OnWebsocketMessage),
                                      sigc::ptr_fun(&OnWebsocketData),
                                      sigc::ptr_fun(&OnWebsocketClosed),
                                      NotifyPingPongSlot());

    if (!websocket_client->Connect(option_group.GetServer())) {
        printf("Fail to connect websocket client to %s\n", option_group.GetServer().c_str());
        return 1;
    }

    printf("Start websocket client to %s\n", option_group.GetServer().c_str());

    g_unix_signal_add(SIGINT, (GSourceFunc)OnExit, NULL);
    g_unix_signal_add(SIGTERM, (GSourceFunc)OnExit, NULL);
    mainloop = Glib::MainLoop::create();
    mainloop->run();

    return 0;
}
