#ifndef ITVSL_CONCURRENT_QUEUE_H
#define ITVSL_CONCURRENT_QUEUE_H

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace itvsl
{

    namespace queue
    {

        template <typename T>
        class Queue
        {
        public:
            T pop()
            {
                std::unique_lock<std::mutex> mlock(mutex_);
                while (queue_.empty())
                {
                    cond_.wait(mlock);
                }
                auto val = queue_.front();
                queue_.pop();
                return val;
            }

            int size()
            {
                return queue_.size();
            }

            void pop(T &item)
            {
                std::unique_lock<std::mutex> mlock(mutex_);
                while (queue_.empty())
                {
                    cond_.wait(mlock);
                }
                item = queue_.front();
                queue_.pop();
            }

            void push(const T &item)
            {
                std::unique_lock<std::mutex> mlock(mutex_);
                queue_.push(item);
                mlock.unlock();
                cond_.notify_one();
            }
            Queue() = default;
            Queue(const Queue &) = delete;            // disable copying
            Queue &operator=(const Queue &) = delete; // disable assignment

        private:
            std::queue<T> queue_;
            std::mutex mutex_;
            std::condition_variable cond_;
        };
    }; // namespace queue
};     // namespace chrys

#endif