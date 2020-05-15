// https://stackoverflow.com/a/12805690

#pragma once

#include <mutex>
#include <condition_variable>
#include <deque>
#include <iostream>

template <typename T>
class BlockingQueue
{
private:
    size_t max;
    std::mutex _mutex;
    std::condition_variable _condition;
    std::deque<T> _queue;
    bool closed;
    T closedObject;

public:
    BlockingQueue(size_t max = 30 * 5, T closedObject = NULL)
    {
        this->max = max;
        this->closed = false;
        this->closedObject = closedObject;
    }

    bool try_push(T const &value)
    {
        if (closed)
            return false;

        {
            std::unique_lock<std::mutex> lock(this->_mutex);

            if (this->max > 0)
                this->_condition.wait(lock, [=] { return this->_queue.size() < max; });
                
            this->_queue.push_front(value);
        }

        this->_condition.notify_all();

        return true;
     }

    T pop()
    {
        std::unique_lock<std::mutex> lock(this->_mutex);
        this->_condition.wait(lock, [=] { return closed || !this->_queue.empty(); });

        if (closed && this->_queue.empty()) {
            return this->closedObject;
        }

        T rc(std::move(this->_queue.back()));
        this->_queue.pop_back();
        this->_condition.notify_all();
        return rc;
    }

    bool tryPop(T &out)
    {
        std::unique_lock<std::mutex> lock(this->_mutex);

        if (this->_queue.empty())
            return false;

        T value(std::move(this->_queue.back()));
        this->_queue.pop_back();

        return true;
    }

    void close()
    {
        std::cout << "Closed blocking queue!\n";
        this->closed = true;
        this->_condition.notify_all();
    }

    void waitShutdown()
    {
        std::unique_lock<std::mutex> lock(this->_mutex);
        this->_condition.wait(lock, [=] { return closed && this->_queue.empty(); });
    }
};