#include <cstdint>
#include <iostream>
#include <vector>
#include <future>
#include "thread_pool.h"
using namespace std;

ThreadPool t_pool(8);

int main(){
    auto square=[](const uint64_t x){
        return x*x;
    };
    const uint64_t tasks = 32;
    vector<future<uint64_t>> futures;

    for (uint64_t task=0; task<tasks; ++task){
        auto future = t_pool.enqueue(square, task);
        futures.emplace_back(std::move(future));   // emplace_back: 直接在容器尾部创建这个元素，省去了移动或拷贝的过程
    }
    
    for (auto& future : futures){
        cout << future.get() << endl;
    }
}