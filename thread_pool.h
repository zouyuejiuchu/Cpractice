#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <iostream>
#include <algorithm>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <functional>

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
      have_task_cond(), // 通知有任务的条件变量
      queue_empty_cond(), // 任务队列中为空的条件变量
      add_mutex(), // 添加任务的锁
      queue_empty_mutex(),
      is_started_mutex()
    {}
    void start();
    void stop();
    template<typename Func_T, typename ...ARGS>
    void add_one_task(Func_T f, ARGS...args) {
        std::unique_lock<std::mutex> lock(add_mutex);
        __add_one_task(new Task(f, std::forward<ARGS>(args)...));
    }
    void stop_until_empty();
    ~thread_pool() { stop(); }

private:
    void thread_loop();
    Task *get_one_task();
    void __add_one_task(Task *);
    
    int thread_size;
    volatile bool is_started;
    std::vector<std::thread *> Threads;
    std::queue<Task *> Tasks;

    std::mutex is_started_mutex;
    std::mutex task_mutex;// m_mutex是为了配合have_task_cond来通知任务队列里有任务
    std::mutex queue_empty_mutex;// mutex2是为了锁住队列里没有任务的状态。
    std::mutex add_mutex; // add_mutex为了锁住任务队列，防止继续添加任务
    std::condition_variable have_task_cond; // 
    std::condition_variable queue_empty_cond; // 为了通知队列为空状态的出现
};

void thread_pool::start() {
    std::unique_lock<std::mutex> lock(is_started_mutex);
    is_started = true;
    for (int i = 0; i < thread_size; i++) {
        Threads.push_back(new std::thread(&thread_pool::thread_loop, this));
    }
}

void thread_pool::stop_until_empty() {
    std::unique_lock<std::mutex> lock1(queue_empty_mutex); // 首先需要获得任务队列没有任务的状态
    std::unique_lock<std::mutex> lock2(add_mutex); // 获得任务队列的状态，防止继续添加任务
    if (!Tasks.empty()) { // 如果任务队列不空，那么就需要等待，直到信号量queue_empty_cond可用
        queue_empty_cond.wait(lock1); // 需要释放queue_empty_mutex
    }
    stop();
    return ;
}

void thread_pool::stop() {
    {
        std::unique_lock<std::mutex> lock(is_started_mutex);
        is_started = false;
        have_task_cond.notify_all();
    }
    for (int i = 0; i < Threads.size(); i++) {
        Threads[i]->join();
        delete Threads[i];
    }
    Threads.clear();
    return ;
}

void thread_pool::thread_loop() {
    while (is_started) {
        Task *t = get_one_task();
        if (t != nullptr) {
            //std::cout << "thread_loop tid : " << std::this_thread::get_id() << std::endl;
            t->run();
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
    if (!Tasks.empty() && is_started) {
        t = Tasks.front();
        Tasks.pop();
        if (Tasks.empty()) {
            std::unique_lock<std::mutex> lock2(queue_empty_mutex);
            queue_empty_cond.notify_all();
        }
    }
    return t;
}

void thread_pool::__add_one_task(Task *t) {
    std::unique_lock<std::mutex> lock(task_mutex);
    Tasks.push(t);
    have_task_cond.notify_one();
    return ;
}

}

#endif
