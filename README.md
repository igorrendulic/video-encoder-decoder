# igor.technology Video Service and Library (ITVSL)

[![stability-experimental](https://img.shields.io/badge/stability-experimental-orange.svg)](https://github.com/mkenney/software-guides/blob/master/STABILITY-BADGES.md#experimental)
[![stability-wip](https://img.shields.io/badge/stability-wip-lightgrey.svg)](https://github.com/mkenney/software-guides/blob/master/STABILITY-BADGES.md#work-in-progress)
[![c++](https://img.shields.io/badge/C++-Solutions-blue.svg?style=flat&logo=c%2B%2B)](https://img.shields.io/badge/C++-Solutions-blue.svg?style=flat&logo=c%2B%2B)

_This is works in progress._

The idea behind the `ITVSL` (Igor Technology Video Service/Library) is to have a data format wrapped around `proto` where machine learning inference on a video is made once. The video may then be streamed live or on demand with inference information wrapped within it.

## Install Protobuf

sudo apt install protobuf-compiler

Current format:

```proto
syntax = "proto3";

package itvsl.protocol.v1beta1;


// This structure stores compressed data.
// For video, it should typically contain one compressed frame.
// For audio it may contain several compressed frames.
// Encoders are allowed to output empty packets, with no compressed data, containing only side data (e.g. to update some stream parameters at the end of encoding).
message CPacket {
    int64 pts  = 1; // Presentation timestamp in AVStream->time_base units; the time at which the decompressed packet will be presented to the user.
    int64 dts = 2; // Decompression timestamp in AVStream->time_base units; the time at which the packet is decompressed.
    bytes data = 3;
    int32 size = 4;
    int32 stream_id = 5;
    int32 flags = 6; // A combination of AV_PKT_FLAG values.
    bytes side_data = 7;
    int32 side_data_elems = 8;
    int64 duration = 9; // Duration of this packet in AVStream->time_base units, 0 if unknown.
    int64 pos = 10; // byte position of stream, -1 if unknown
    string meta = 11; // custom metadata if needed
    bytes raw_bgr24 = 12; // raw bgr image
}

// Video Streaming messages
message ShapeProto {
    message Dim {
        // Size of image in that dimension (-1 means unknown dimension)
        int64 size = 1;
        // optional name of image dimension
        string name = 2;
    }

    repeated Dim dim = 2;
}

message VideoFrame {
    int64 width = 1;
    int64 height = 2;
    bytes data = 3;
    int64 timestamp = 4;
    bool is_keyframe = 5;
    int64 pts = 6;
    int64 dts = 7;
    string frame_type = 8;
    bool is_corrupt = 9;
    double time_base = 10;
    ShapeProto shape = 11;
    string device_id = 12;
    int64 packet = 13;
    int64 keyframe = 14;
    bytes extradata = 15;
    string codec_name = 16;
    string pix_fmt = 17;
}

// VideoCodec information about the stream
message VideoCodec {
    string name = 1;
    int32 width = 2;
    int32 height = 3;
    string pix_fmt = 4;
    bytes extradata = 5;
    int32 extradata_size = 6;
    string long_name = 7;
}
```

## Compile Proto

First proto files must be compile and moved to `itvsl` root project. (/itvsl folder)

```
cd proto
cmake .
make
cp itvslprotocol.pb.* ../
```

## Debug with Valgrind

```
rm valgrind-out.txt
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./myapp
```

## Examples

### RTSP Camera with MP4 segments

```c
#include <iostream>
#include <chrono>
#include <memory>
#include <csignal>
#include <future>

#include <itvsl.h>
#include "itvsl/itvsl_archive.h"
#include "itvsl/itvsl_queue.h"
#include "itvsl/mp4_file_cleanup.h"

// using namespace std::placeholders;
// using namespace std::this_thread;     // sleep_for, sleep_until
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.
using namespace std::chrono;

using namespace itvsl::queue;
using namespace itvsl::archive;
using namespace itvsl::media;
using namespace itvsl::log;

// struct SwsContext *img_convert_ctx;
    // img_convert_ctx = sws_getCachedContext(NULL,
        // pFrame->width, pFrame->height, AV_PIX_FMT_YUV420P,
        // pFrame->width, pFrame->height, AV_PIX_FMT_BGR24,
        // SWS_BICUBIC, NULL, NULL, NULL);

// sws_scale(img_convert_ctx,
//         pFrame->data,
//         pFrame->linesize, 0, pFrame->height,
//         pFrameBGR->data,
//         pFrameBGR->linesize);
//     sws_freeContext(img_convert_ctx);

namespace {

    // std::future<void> can be used to the thread, and it should exit when value in future is available.
    std::promise<void> exitSignal;

    // global definitions for easier cleanup after termination signal
    itvsl::media::LibItvsl *cms = new LibItvsl();
    StreamingContext *decoder = cms->getDecodingContext();

    // archive inits
    ItvslArchive ca;
    Queue<AVPacket> archiveQueue;

    // mp4 cleanup inits
    Mp4FileCleanup mp4Cleanup;

    AVPacket *input_packet = av_packet_alloc();
}

void signalHandler( int signum ) {

    logging("exiting VSL with signum %d", signum);


    exitSignal.set_value();

    AVPacket *pkt = av_packet_alloc();
    pkt->stream_index = 123456789;
    archiveQueue.push(*pkt);

    if (input_packet != NULL) {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }

    delete cms;
    google::protobuf::ShutdownProtobufLibrary();

   exit(signum);
}

int main() {

    logging("**************************************************************************************");
    logging("*       Tool : ITVSL Media Streams                                                    *");
    logging("*     Author : Igor Rendulic (https://igor.technology)                                *");
    logging("*  Used Libs : FFmpeg/libav                                                           *");
    logging("**************************************************************************************");
    logging("--------------------------------------------------------------------------------------");
    logging("");
    logging("--------------------------------------------------------------------------------------");


    signal(SIGINT, signalHandler);

    av_log_set_level(AV_LOG_DEBUG);
    // av_log_set_level(AV_LOG_TRACE);

    StreamingParams params = {0};
    params.copy_audio = 0;
    params.copy_video = 1;

    char* video_codec = (char*)"h264";
    char* codec_priv_key = (char*)"h264-params";

    if (cms->openInput(RTSP, "rtsp://root:pass@10.0.1.26/axis-media/media.amp", &decoder->pFormatContext) < 0) {
        logging("failed to open input stream");
        return -1;
    };

    if (cms->prepareDecoder(decoder)) {
        logging("failed to prepare decoder");
        return -1;
    }

    // // optinally start archiving threads (storing mp4s and cleaning them up)
    // std::future<void> termFuture = exitSignal.get_future();

    // std::thread archiveThread(&ItvslArchive::processMedia, &ca, std::ref(archiveQueue), std::ref(*itvsl), std::move(termFuture));

    // std::thread mp4cleanupThread(&Mp4FileCleanup::processCleanup, &mp4Cleanup, ".", "10s");
    // mp4cleanupThread.detach();

    if (!input_packet) {logging("failed to allocated memory for AVPacket"); return -1;}

    print_timing("decoder", decoder->pFormatContext, decoder->video_codec_context, decoder->video_stream);

    bool first_keyframe = true;

    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame) {
        logging("failed to allocated memory for AVFrame");
        return -1;
    }

    while (av_read_frame(decoder->pFormatContext, input_packet) >= 0)
    {

        if (input_packet->stream_index == decoder->video_index) {
            // logging("AVPacket->pts %" PRId64, input_->pts);

            // if (input_packet->flags & AV_PKT_FLAG_KEY) {
            //     // AV_PKT_FLAG_CORRUPT
            // }

            int response = cms->decode(decoder->video_codec_context, pFrame, input_packet);
            if (response < 0) {
                break;
            }
        }

        // CPacket *cPacket = itvsl->itvslPacketize(input_packet);
        // std::string cPacketBinary;
        // cPacket->SerializeToString(&cPacketBinary);

        // CPacket testPacket;
        // const std::string test = cPacketBinary;
        // testPacket.ParseFromString(test);

        // AVPacket *archive_packet = av_packet_clone(input_packet);

        // archiveQueue.push(*archive_packet);

        av_packet_unref(input_packet);
    }

    // archiveThread.join();

    if (input_packet != NULL) {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }
    if (pFrame != NULL) {
        av_frame_free(&pFrame);
    }

    delete vsl;

    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}
```

## Resources

nice decoder here: [https://github.com/DaWelter/h264decoder/blob/master/src/h264decoder.cpp](https://github.com/DaWelter/h264decoder/blob/master/src/h264decoder.cpp)

ffmpeg configure: [https://github.com/FFmpeg/FFmpeg/blob/master/configure](https://github.com/FFmpeg/FFmpeg/blob/master/configure)

Livestreaming with libav: [https://blog.mi.hdm-stuttgart.de/index.php/2018/03/21/livestreaming-with-libav-tutorial-part-2/](https://blog.mi.hdm-stuttgart.de/index.php/2018/03/21/livestreaming-with-libav-tutorial-part-2/)

This project is based on: [https://github.com/leandromoreira/ffmpeg-libav-tutorial/blob/master/3_transcoding.c](https://github.com/leandromoreira/ffmpeg-libav-tutorial/blob/master/3_transcoding.c)

check this transcoding example for audio maybe: [https://ffmpeg.org/doxygen/2.4/transcoding_8c_source.html](https://ffmpeg.org/doxygen/2.4/transcoding_8c_source.html)

list of examples: [https://github.com/FFmpeg/FFmpeg/tree/master/doc/examples](https://github.com/FFmpeg/FFmpeg/tree/master/doc/examples)

extracting motion vectors: https://github.com/jishnujayakumar/MV-Tractus/blob/master/extract_mvs.c

compressed video action recognition: https://github.com/chaoyuaw/pytorch-coviar
