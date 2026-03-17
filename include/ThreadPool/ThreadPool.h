#pragma once

#include <future>
#include <mutex>
#include <condition_variable>
#include <queue>

class ThreadPool
{
    using Task = std::packaged_task<void()>;
public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    ThreadPool(unsigned int threadCount = std::thread::hardware_concurrency()) : m_thread_count(threadCount) 
    {

    }
    ~ThreadPool() 
    { 
        Stop(); 
    }
    template<class F, class... Args>
    auto CommitTask(F&& f, Args&&... args)
    {
        using ReturnType = std::invoke_result_t<F, Args...>;
        std::packaged_task<ReturnType()> packagedTask([f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)] () mutable {
            return std::apply(std::move(f), std::move(args));
        });
        std::future<ReturnType> taskReturn = packagedTask.get_future();
        {
            std::unique_lock<std::mutex> lock(m_task_mutex);
            m_tasks.emplace([task = std::move(packagedTask)]() mutable {
                task();
            });
            m_task_count++;
        }
        m_task_cv.notify_one();
        return taskReturn;
    }
    void Start()
    {
        std::lock_guard<std::mutex> lifecycleLock(m_lifecycle_mutex);
        if(m_threads.empty())
        {
            m_stop = false;
            for(unsigned int i = 0; i < m_thread_count; ++i)
            {
                std::string threadName = "WorkThread [" + std::to_string(i) + "]";
                std::thread thread([this, threadName = std::move(threadName)]() {
                    pthread_setname_np(pthread_self(), threadName.c_str());
                    while(true)
                    {
                        Task task;
                        {
                            std::unique_lock<std::mutex> lock(m_task_mutex);
                            m_waiting_thread_count++;
                            m_task_cv.wait(lock, [this]() {
                                return m_stop || (!m_pause && !m_tasks.empty());
                            });
                            m_waiting_thread_count--;
                            if(m_stop && m_tasks.empty())
                            {
                                break;
                            }
                            if(!m_tasks.empty())
                            { 
                                task = std::move(m_tasks.front());
                                m_tasks.pop();
                                m_task_count--;
                            }
                        }
                        if(task.valid())
                        {
                            m_running_thread_count++;
                            task();
                            m_running_thread_count--;
                        }
                    }
                });
                m_threads.emplace_back(std::move(thread));
            }
        }
    }
    void Stop()
    {
        std::lock_guard<std::mutex> lifecycleLock(m_lifecycle_mutex);
        if(!m_threads.empty())
        {
            {
                std::lock_guard<std::mutex> lock(m_task_mutex);
                m_stop = true;
                m_pause = false;
            }
            m_task_cv.notify_all();
            for(auto&& t : m_threads)
            {
                if(t.joinable())
                {
                    t.join();
                }
            }
            m_threads.clear();
            m_running_thread_count = 0;
            m_waiting_thread_count = 0;
            m_task_count = 0;
        }
       
    }
    void Pause()
    {
        std::lock_guard<std::mutex> lifecycleLock(m_lifecycle_mutex);
        {
            std::lock_guard<std::mutex> lock(m_task_mutex);
            m_pause = true;
        }
        m_task_cv.notify_all();
    }
    void Resume()
    {
        std::lock_guard<std::mutex> lifecycleLock(m_lifecycle_mutex);
        {
            std::lock_guard<std::mutex> lock(m_task_mutex);
            m_pause = false;
        }
        m_task_cv.notify_all();
    }

    unsigned int GetRunningThreadCount() const
    {
        return m_running_thread_count.load();
    }

    unsigned int GetWaitingThreadCount() const
    {
        return m_waiting_thread_count.load();
    }

    unsigned int GetTaskCount() const
    {
        return m_task_count.load();
    }
private:
    std::vector<std::thread>    m_threads;
    std::mutex                  m_lifecycle_mutex;
    std::mutex                  m_task_mutex;
    std::condition_variable     m_task_cv;
    std::queue<Task>            m_tasks;
    bool                        m_stop = false;
    bool                        m_pause = false;
    unsigned int                m_thread_count = 0;
    std::atomic<unsigned int>   m_running_thread_count = 0;
    std::atomic<unsigned int>   m_waiting_thread_count = 0;
    std::atomic<unsigned int>   m_task_count = 0;
};
