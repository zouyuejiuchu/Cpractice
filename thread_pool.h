#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <iostream>
#include <algorithm>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <atomic>
#include <unistd.h>

namespace zhang {

class Task {
public :
    template<typename Func_T, typename ...ARGS>
    Task(Func_T f, ARGS ...args) {
        func = std::bind(f, std::forward<ARGS>(args)...);
    }
    void run() {
        func();
    }
private:
    std::function<void()> func;
};

class thread_pool {
public:        
    thread_pool(int thread_size = 5) 
    : thread_size(thread_size),
      is_started(false),
      task_mutex(), // 任务锁。
      thread_mutex(), // 操作线程的互斥锁。
      have_task_cond(), // 通知有任务的条件变量
      queue_empty_cond(), // 任务队列中为空的条件变量
      add_mutex(), // 添加任务的锁
      queue_empty_mutex(),
      minus_mutex(),
      garbage_collection_mutex(),
      garbage_collection_cond()
    {}
    void start();
    void stop();
    template<typename Func_T, typename ...ARGS>
    void add_one_task(int priority, Func_T f, ARGS...args) {
        std::unique_lock<std::mutex> lock(add_mutex);
        __add_one_task(priority, new Task(f, std::forward<ARGS>(args)...));
    }
    void stop_until_empty();
    void log();
    void add_threads(unsigned);
    void minus_threads(unsigned);
    ~thread_pool() { stop(); }

private:
    void garbage_collection();
    void thread_loop();
    Task *get_one_task();
    void __add_one_task(int, Task *);
    
    std::atomic<bool>is_started{false};
    std::atomic<unsigned>thread_size{0};
    std::atomic<unsigned>using_size{0};
    std::atomic<unsigned>minus_num{0};
    
    std::vector<std::thread *> Threads;
    std::priority_queue<std::pair<int, Task *> > Tasks;

    std::mutex task_mutex;// m_mutex是为了配合have_task_cond来通知任务队列里有任务
    std::mutex queue_empty_mutex;// mutex2是为了锁住队列里没有任务的状态。
    std::mutex add_mutex; // add_mutex为了锁住任务队列，防止继续添加任务
    std::mutex thread_mutex;
    std::mutex minus_mutex;
    std::mutex garbage_collection_mutex;

    std::condition_variable have_task_cond; // 通知有任务的到来。
    std::condition_variable queue_empty_cond; // 为了通知队列为空状态的出现
    std::condition_variable garbage_collection_cond;
};

void thread_pool::garbage_collection() {
    while (is_started) {
        std::unique_lock<std::mutex>lock(garbage_collection_mutex);
        garbage_collection_cond.wait(lock);
        if (is_started == false) break;
        std::unique_lock<std::mutex>lock1(thread_mutex);
        for (int i = 1; i < Threads.size(); ++ i) {
            if (Threads[i] == nullptr) {
                Threads[i]->join();
                delete Threads[i];
                thread_size --;
            }
        }
    }
    return ;
}

void thread_pool::start() {
    std::unique_lock<std::mutex>lock(task_mutex);
    std::unique_lock<std::mutex>lock2(thread_mutex);
    is_started = true;
    Threads.push_back(new std::thread(&thread_pool::garbage_collection, this));
    for (int i = 0; i < thread_size; i++) {
        Threads.push_back(new std::thread(&thread_pool::thread_loop, this));
    }
}

void thread_pool::stop_until_empty() {
    std::unique_lock<std::mutex> lock1(queue_empty_mutex); // 获得锁
    std::unique_lock<std::mutex> lock2(add_mutex); // 获得任务队列的状态，防止继续添加任务
    if (!Tasks.empty()) { // 如果任务队列不空，那么就需要等待，直到信号量queue_empty_cond可用
        queue_empty_cond.wait(lock1); // wait的同时会释放queue_empty_mutex
    }
    stop();
    return ;
}

void thread_pool::stop() {
    {
        std::unique_lock<std::mutex> lock(task_mutex); 
        is_started = false;
        garbage_collection_cond.notify_all();
        have_task_cond.notify_all();
    }
    std::unique_lock<std::mutex> lock(thread_mutex);
    for (int i = 1; i < Threads.size(); i++) {
        if (Threads[i] && Threads[i]->joinable()){
            Threads[i]->join();
            delete Threads[i];
        }
    }
    Threads.clear();
    return ;
}

void thread_pool::thread_loop() {
    while (is_started) {
        Task *t = get_one_task();
        if (t != nullptr) {
            using_size ++;
            t->run(); 
            using_size --;
        }
        std::unique_lock<std::mutex>lock(minus_mutex);
        if (minus_num > 0) {
            if (-- minus_num == 0) {
                std::unique_lock<std::mutex>lock(garbage_collection_mutex);
                garbage_collection_cond.notify_one();
            }
            break;
        }
    }
    return ;
}

Task* thread_pool::get_one_task() {
    std::unique_lock<std::mutex> lock(task_mutex);
    if (Tasks.empty() && is_started) {
        have_task_cond.wait(lock);
    }
    Task *t = nullptr;
    bool is_get_task = false;
    if (!Tasks.empty() && is_started) {
        is_get_task = true;
        auto p = Tasks.top();
        t = p.second;
        if (Tasks.size() == 1) {
            std::unique_lock<std::mutex> lock1(queue_empty_mutex); // 这里一定要获取这个锁，否则队列为空的这个状态可能会在queue_empty_cond wait之前就notify，这样就会导致信号量一直在等待，却等不到。
            queue_empty_cond.notify_all();
        }
    }
    if (is_get_task) Tasks.pop();
    return t;
}

void thread_pool::__add_one_task(int priority, Task *t) {
    std::unique_lock<std::mutex> lock(task_mutex);
    Tasks.push(std::make_pair(priority, t));
    have_task_cond.notify_one();
    return ;
}

void thread_pool::log() {
    std::unique_lock<std::mutex>lock(task_mutex);
    std::string message = "thread_size ";
    message += to_string(thread_size);
    message.push_back('\n');
    message += "Tasks_size：";
    message += std::to_string(Tasks.size());
    message.push_back('\n');
    message += "using_size：";
    message += std::to_string(using_size);
    std::cout << message << std::endl;
    return ;
}

void thread_pool::add_threads(unsigned num) {
    std::unique_lock<std::mutex>lock(thread_mutex);
    for (int i = 0; i < num; i ++) {
        Threads.push_back(new std::thread(&thread_pool::thread_loop, this));
    }
    thread_size += num;
}

void thread_pool::minus_threads(unsigned num) {
    std::unique_lock<std::mutex>lock(thread_mutex);
    if (num > thread_size) {
        std::cout << "too big argument!" << std::endl;
        return ;
    }

    {
        std::unique_lock<std::mutex>lock(minus_mutex);
        minus_num += num;
    }
    
    {
        std::unique_lock<std::mutex>lock(task_mutex);
        have_task_cond.notify_all();
    }
    return ;
}

}

#endif
