#pragma once
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
using namespace std;

class ThreadPool{
private:
    bool stop_bool;     // indicates whether the pool is stopped
    uint32_t active_threads;  // number of active threads
    const uint32_t capacity;   // max number of threads

    mutex mtx;
    condition_variable cv;

    vector<thread> threads;
    queue<function<void(void)>> tasks;     // first in first out

    template<
        typename Func,
        typename ... Args,
        typename Rtrn=typename result_of<Func(Args...)>::type>     // Rtrn: 函数返回值
    auto make_task(Func && func, Args&& ... args) -> packaged_task<Rtrn(void)> {
        auto aux = bind(forward<Func>(func), forward<Args>(args)...);
        return packaged_task<Rtrn(void)>(aux);
    }

    void before_task_hook(){
        active_threads++;
    }

    void after_task_hook(){
        active_threads--;
    }

public:
    explicit ThreadPool(uint64_t capacity_):stop_bool(false),active_threads(0),capacity(capacity_) {
        auto wait_loop = [this]{
            while (true){
    
                function<void(void)> task;

                {
                    unique_lock<mutex> unique_lock(mtx);
                    auto predicate = [this] () -> bool {
                        return (stop_bool) || (!tasks.empty());
                    };

                    cv.wait(unique_lock, predicate);

                    if (stop_bool && tasks.empty())
                        return ;
                    
                    task = std::move(tasks.front());
                    tasks.pop();
                    before_task_hook();
                }

                task();

                {
                    lock_guard<mutex> lock_guard(mtx);
                    after_task_hook();
                }

            }
        };

        for (uint32_t i = 0; i<capacity; i++)
           threads.emplace_back(thread(wait_loop)); 
    }

    ~ThreadPool() {
        {
            lock_guard<mutex> lock_guard(mtx);
            stop_bool = true;
        }
        cv.notify_all();
        for(auto& thread: threads)
            thread.join();
    }

    template<
        typename Func,
        typename ... Args,
        typename Rtrn=typename result_of<Func(Args...)>::type>
    auto enqueue(Func && func, Args&& ... args) -> future<Rtrn>{
        auto task = make_task(func, args...);
        auto future = task.get_future();
        auto task_ptr = make_shared<decltype(task)>(move(task));

        {
            lock_guard<mutex> lock_guard(mtx);
            if(stop_bool)
                throw runtime_error("enqueue on stopped ThreadPool!");
            auto payload = [task_ptr] ()->void{
                task_ptr->operator()();
            };
            tasks.emplace(payload);
        }
        cv.notify_one();

        return future;
    }

};