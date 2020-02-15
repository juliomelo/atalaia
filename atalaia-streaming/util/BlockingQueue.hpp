// https://stackoverflow.com/a/12805690

#pragma once

#include <mutex>
#include <condition_variable>
#include <deque>

template <typename T>
class BlockingQueue
{
private:
    size_t max;
    std::mutex d_mutex;
    std::condition_variable d_condition;
    std::deque<T> d_queue;

public:
    BlockingQueue(size_t max)
    {
        this->max = max;
    }

    bool try_push(T const &value)
    {
        {
            std::unique_lock<std::mutex> lock(this->d_mutex);


            if (this->d_queue.size() == max)
                return false;

            this->d_queue.push_front(value);
        }

        this->d_condition.notify_all();

        return true;
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(this->d_mutex);
        this->d_condition.wait(lock, [=] { return !this->d_queue.empty(); });
        T rc(std::move(this->d_queue.back()));
        this->d_queue.pop_back();
        return rc;
    }

    bool tryPop(T &out)
    {
        std::unique_lock<std::mutex> lock(this->d_mutex);

        if (this->d_queue.empty())
            return false;

        T value(std::move(this->d_queue.back()));
        this->d_queue.pop_back();

        return true;
    }
};