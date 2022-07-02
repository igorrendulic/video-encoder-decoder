#include <iostream>
#include <chrono>
#include <memory>
#include <csignal>
#include <future>
#include <fstream>
#include <stdio.h>

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

namespace
{

    // std::future<void> can be used to the thread, and it should exit when value in future is available.
    std::promise<void> exitSignal;

    // global definitions for easier cleanup after termination signal
    itvsl::media::LibItvsl *vsl = new LibItvsl();
    StreamingContext *decoder = vsl->getDecodingContext();

    // archive inits
    ItvslArchive ca;
    Queue<AVPacket> archiveQueue;

    // mp4 cleanup inits
    Mp4FileCleanup mp4Cleanup;

    AVPacket *input_packet = av_packet_alloc();
}

void signalHandler(int signum)
{

    logging("exiting ITVS with signum %d", signum);

    exitSignal.set_value();

    AVPacket *pkt = av_packet_alloc();
    pkt->stream_index = 123456789;
    archiveQueue.push(*pkt);

    if (input_packet != NULL)
    {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }

    delete vsl;
    google::protobuf::ShutdownProtobufLibrary();

    exit(signum);
}

int main()
{

    logging("***************************************************************************************");
    logging("*        Tool : Igor Tech Media Streams                                               *");
    logging("*      Author : Igor Rendulic (https://igor.techology)                                *");
    logging("*   Used Libs : FFmpeg/libav                                                          *");
    logging("***************************************************************************************");
    logging("---------------------------------------------------------------------------------------");
    logging("");
    logging("---------------------------------------------------------------------------------------");

    signal(SIGINT, signalHandler);

    av_log_set_level(AV_LOG_DEBUG);

    // // Desktop capture input formats
    // #ifdef Q_OS_LINUX
    //     idesktopFormat = av_find_input_format("x11grab");
    // #endif
    // #ifdef Q_OS_WIN
    //     idesktopFormat = av_find_input_format("gdigrab");
    // #endif

    //     // Webcam input formats
    // #ifdef Q_OS_LINUX
    //     if ((iformat = av_find_input_format("v4l2")))
    //         return true;
    // #endif

    // #ifdef Q_OS_WIN
    //     if ((iformat = av_find_input_format("dshow")))
    //         return true;
    //     if ((iformat = av_find_input_format("vfwcap")))
    // #endif

    // #ifdef Q_OS_OSX
    //     if ((iformat = av_find_input_format("avfoundation")))
    //         return true;
    //     if ((iformat = av_find_input_format("qtkit")))
    //         return true;
    // #endif

    if (vsl->openInput(V4L2, "/dev/video0", &decoder->pFormatContext) < 0)
    {
        logging("failed to open input stream");
        return -1;
    };

    if (vsl->prepareDecoder(decoder))
    {
        logging("failed to prepare decoder");
        return -1;
    }

    if (!input_packet)
    {
        logging("failed to allocated memory for AVPacket");
        return -1;
    }

    print_timing(decoder->pFormatContext, decoder->video_codec_context, decoder->video_stream);

    // Enable non-blocking mode
    // fmt_ctx->flags |= AVFMT_FLAG_NONBLOCK;

    bool first_keyframe = true;

    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame)
    {
        logging("failed to allocated memory for AVFrame");
        return -1;
    }

    int ret = 0;

    AVPixelFormat outputPixFormat = AV_PIX_FMT_BGR24;

    AVFrame *pFrameBGR = av_frame_alloc();
    pFrameBGR->width = decoder->video_codec_context->width;
    pFrameBGR->height = decoder->video_codec_context->height;
    pFrameBGR->format = outputPixFormat;

    int alloRet = av_image_alloc(pFrameBGR->data, pFrameBGR->linesize, decoder->video_codec_context->width, decoder->video_codec_context->height, outputPixFormat, 1);
    if (alloRet < 0)
    {
        logging("failed to allocate image");
        return -1;
    }

    struct SwsContext *sws_ctx = NULL;

    sws_ctx = sws_getContext(decoder->video_codec_context->coded_width,
                             decoder->video_codec_context->coded_height,
                             decoder->video_codec_context->pix_fmt,
                             decoder->video_codec_context->coded_width,
                             decoder->video_codec_context->coded_height,
                             outputPixFormat,
                             SWS_DIRECT_BGR,
                             NULL,
                             NULL,
                             NULL);

    if (!sws_ctx)
    {
        logging("failed to create scale context for the conversion");
        return -1;
    }

    int cnt = 0;
    while (1)
    {
        int rfRet = av_read_frame(decoder->pFormatContext, input_packet);

        if (rfRet >= 0)
        {

            if (input_packet->stream_index == decoder->video_index)
            {
                // logging("AVPacket->pts %" PRId64, input_packet->pts);

                if (input_packet->flags & AV_PKT_FLAG_KEY)
                {
                    // AV_PKT_FLAG_CORRUPT
                }

                int response = vsl->decode(decoder->video_codec_context, pFrame, input_packet, sws_ctx, pFrameBGR);
                if (response < 0)
                {
                    break;
                }

                av_packet_unref(input_packet);
            }
        }
    }

    if (input_packet != NULL)
    {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }
    if (pFrame != NULL)
    {
        av_frame_free(&pFrame);
    }
    // if (pFrameBGR != NULL) {
    //     av_frame_free(&pFrameBGR);
    // }

    delete vsl;

    google::protobuf::ShutdownProtobufLibrary();

    // register all formats and codecs
    // av_register_all();

    // ifmt = av_find_input_format("video4linux2");
    // if (!ifmt) {
    //     av_log(0, AV_LOG_ERROR, "Cannot find input format\n");
    //     exit(1);
    // }

    sws_freeContext(sws_ctx);
    // av_freep(&outBuffer[0]);
    delete vsl;

    return ret;
}