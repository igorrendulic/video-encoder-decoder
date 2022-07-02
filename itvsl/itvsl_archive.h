#ifndef ITVSL_ARCHIVER_H
#define ITVSL_ARCHIVER_H

#include <future>

#include "itvsl_queue.h"
#include "itvslprotocol.pb.h"
#include "itvsl.h"

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <inttypes.h>
#ifdef __cplusplus
}
#endif

using namespace itvsl::queue;
using namespace itvsl::media;
using namespace itvsl::protocol::v1beta1;

namespace itvsl
{

    namespace archive
    {
        class ItvslArchive
        {
            std::mutex m_mutex;
            std::condition_variable m_decodePackets;
            bool hasFirstKeyFrame;
            bool hasFirstFilename;
            int64_t last_pts;

        public:
            ItvslArchive()
            {
                hasFirstKeyFrame = false;
                hasFirstFilename = false;
                last_pts = 0;
            }
            ~ItvslArchive()
            {
            }

            void processMedia(Queue<AVPacket> &q, LibItvsl &itvsl, std::future<void> future);
        };
    }; // namespace archive
};     // namespace itvsl

#endif