#ifndef ITVSL_PACKET_READER_H
#define ITVSL_PACKET_READER_H

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <sstream>
#include <chrono>
#include <functional>

#include "queue.h"

using namespace itvsl::queue;

namespace itvsl
{

    namespace packet
    {

        class PacketReader
        {
            std::mutex m_mutex;
            std::condition_variable m_decodePackets;
            bool decodePackets;

        public:
            PacketReader()
            {
                decodePackets = false;
            }

            bool isDecodePackets()
            {
                return decodePackets;
            }

            void setDecodePackets(bool decode)
            {
                // Acquire the lock
                std::lock_guard<std::mutex> guard(m_mutex);

                decodePackets = decode;

                std::cout << "decode packets set to " << decodePackets << std::endl;

                m_decodePackets.notify_all();
            }

            bool putPacket(Queue<int> &q, int packet)
            {

                // push until next I frame
                if (decodePackets)
                {
                    std::cout << "pushing to queue packet " << std::to_string(packet) << std::endl;
                    q.push(packet);

                    return true;
                }
                return false;
            }

            void packetDecoder(Queue<int> &q)
            {
                std::cout << "Packet decoder started" << std::endl;

                std::vector<int> previous_group;
                std::vector<int> current_group;

                long long count = 0;
                while (true)
                {
                    std::cout << "iteration: " << std::to_string(count) << std::endl;
                    count++;
                    auto item = q.pop();
                    if (!item)
                    {
                        std::cout << "Waiting for lock to be acquired " << std::endl;
                        // Acquire the lock
                        std::unique_lock<std::mutex> mlock(m_mutex);
                        m_decodePackets.wait(mlock, std::bind(&PacketReader::isDecodePackets, this));

                        std::cout << " continue decoding packets" << std::endl;
                        continue;
                    }
                    std::cout << "Decoded packet -> " << std::to_string(item) << std::endl;
                }
            }
        };
    }; // namespace packet
};     // namespace itvsl

#endif