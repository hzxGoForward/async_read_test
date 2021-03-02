#pragma once
#ifndef _THREAD_SAFE_QUEUE_H
#define _THREAD_SAFE_QUEUE_H

#include <condition_variable>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <numeric>

template <typename T>
class CThreadSafeQueue
{
#define GUARD_LOCK std::unique_lock<std::mutex> lg(m_queueMtx)
    using value_type = T;
    using size_type = typename std::list<T>::size_type;

public:
    explicit CThreadSafeQueue(size_type maxSz = std::numeric_limits<size_type>::max())
        : m_maxSize(maxSz), m_writeEnd(false)
    {
    }

    ~CThreadSafeQueue()
    {
        // std::cout << "CThreadSafeQueue deconstruct end\n";
    }

    size_type size() const
    {
        GUARD_LOCK;
        return m_queue.size();
    }

    void clear()
    {
        GUARD_LOCK;
        m_queue.clear();
    }

    size_type maxSize() const
    {
        GUARD_LOCK;
        return m_maxSize;
    }

    bool empty() const
    {
        GUARD_LOCK;
        return m_queue.empty();
    }

    bool full() const
    {
        GUARD_LOCK;
        return m_queue.size() >= m_maxSize;
    }

    bool push(const T &val)
    {
        bool notify_notEmpty = false;
        {
            std::unique_lock<std::mutex> ul(m_queueMtx);
            while (m_queue.size() >= m_maxSize)
            {
                m_notFull_condVar.wait(ul, [&] { return m_queue.size() < m_maxSize; });
            }
            // push 之前是空载状态，发出notEmpty信号
            if (m_queue.empty())
                notify_notEmpty = true;
            m_queue.push_back(val);
        }
        if (notify_notEmpty)
            m_notEmpty_condVar.notify_one();
        return true;
    }

    bool pop(T &val)
    {
        bool pop_success = false;
        bool notify_notFull = false;
        bool notify_all = false;
        // std::cout << "pop...\n";
        {
            std::unique_lock<std::mutex> ul(m_queueMtx);
            while (m_queue.empty() && !m_writeEnd)
            {
                // std::cout << "等待队列不为空的信号....\n";
                m_notEmpty_condVar.wait(ul, [&]() { return !m_queue.empty() || (m_queue.empty() && m_writeEnd); });
            }
            if (!m_queue.empty())
            {
                // std::cout << "队列弹出元素\n";
                // pop之前是满载状态，发出notFull信号
                if (m_queue.size() == m_maxSize)
                    notify_notFull = true;

                // pop
                val = m_queue.front();
                m_queue.pop_front();
                pop_success = true;

                // 队列为空，且不再增加元素，则告知其他线程结束等待
                if (m_queue.empty() && m_writeEnd)
                    notify_all = true;
            }
        }
        // std::cout << "pop...finish\n";
        if (notify_notFull)
            m_notFull_condVar.notify_one();
        if (notify_all)
            m_notEmpty_condVar.notify_all();

        return pop_success;
    }

    bool front(T &t)
    {
        GUARD_LOCK;
        if (m_queue.empty())
            return false;
        t = m_queue.front();
        return true;
    }

    void set_end()
    {
        {
            GUARD_LOCK;
            m_writeEnd = true;
        }
        m_notEmpty_condVar.notify_all();
        m_notFull_condVar.notify_all();
    }

    bool is_end()
    {
        GUARD_LOCK;
        return m_writeEnd;
    }

    std::mutex &mutex() { return m_queueMtx; }
    std::list<T> &content() { return m_queue; }

protected:
    mutable std::mutex m_queueMtx;
    std::list<T> m_queue;
    size_type m_maxSize;
    bool m_writeEnd;
    // two cv to notify not empty and not full.
    std::condition_variable m_notEmpty_condVar;
    std::condition_variable m_notFull_condVar;

#undef GUARD_LOCK
};

typedef std::shared_ptr<char> char_ptr_t;

template <typename T>
std::shared_ptr<T> make_shared_array(size_t size)
{
    return std::shared_ptr<T>(new T[size], std::default_delete<T[]>());
    // return std::shared_ptr<T>(new T[size]);
}

struct CDataPkg
{
    int64_t length;
    char_ptr_t data;
    CDataPkg(std::streamsize sz = 0) : length(sz)
    {
        data = make_shared_array<char>(length);
    }
};


typedef std::shared_ptr<CDataPkg> CDataPkg_ptr_t;


#endif
