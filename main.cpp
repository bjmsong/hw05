// 小彭老师作业05：假装是多线程 HTTP 服务器 - 富连网大厂面试官觉得很赞
#include <functional>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include "MTQueue.h"
#include "thread_pool.h"

struct User {
    std::string password;
    std::string school;
    std::string phone;
};

std::unordered_map<std::string, User> users;   // 不需要有序，unordered_map取代map，效率更高
std::shared_mutex user_mtx;

std::unordered_map<std::string, std::chrono::steady_clock::time_point> has_login;  // 换成 std::chrono::seconds 之类的
std::shared_mutex has_login_mtx;

// 作业要求1：把这些函数变成多线程安全的
// 提示：能正确利用 shared_mutex 加分，用 lock_guard 系列加分
std::string do_register(std::string username, std::string password, std::string school, std::string phone) {
    // 移动语义，减少不必要的拷贝
    User user = {std::move(password), std::move(school), std::move(phone)};
    
    std::unique_lock grd(user_mtx);  // 写锁
    if (users.emplace(std::move(username), user).second)
        return "注册成功\n";
    else
        return "用户名已被注册\n";
}

std::string do_login(std::string username, std::string password) {
    // 作业要求2：把这个登录计时器改成基于 chrono 的
    std::unique_lock grd(has_login_mtx);  // 写锁
    // long now = time(NULL);   // C 语言当前时间
    // if (has_login.find(username) != has_login.end()) {
    //     int sec = now - has_login.at(username);  // C 语言算时间差
    //     return std::to_string(sec) + "秒内登录过";
    // }
    // has_login[username] = now;

    auto now = std::chrono::steady_clock::now();
    if (has_login.find(username) != has_login.end()) {
        auto dt = now - has_login.at(username);
        return username + "在" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(dt).count()) + "us内登录过";
    }
    has_login[username] = now;
    grd.unlock();

    std::shared_lock qrd(user_mtx);   // 读锁
    if (users.find(username) == users.end())
        return "用户名错误\n";
    if (users.at(username).password != password)
        return "密码错误\n";
    return "登录成功\n";
}

std::string do_queryuser(std::string username) {
    std::shared_lock qrd(user_mtx);   // 读锁
    // 增加校验
    if(users.find(username) == users.end())
        return "用户不存在\n";
    auto &user = users.at(username);
    std::stringstream ss;
    ss << "用户名: " << username << std::endl;
    ss << "学校:" << user.school << std::endl;
    ss << "电话: " << user.phone << std::endl;
    return ss.str();
}


// struct ThreadPool {
//     std::vector<std::thread> m_pool;
//     void create(std::function<void()> start) {
//         // 作业要求3：如何让这个线程保持在后台执行不要退出？
//         // 提示：改成 async 和 future 且用法正确也可以加分
//         m_pool.push_back(std::thread(start));
//     }

//     ~ThreadPool() {
//     for (auto &thr : m_pool) {
//         thr.join();
//     }
// }
// };

// ThreadPool tpool;
ThreadPool tpool(4);


namespace test {  // 测试用例？出水用力！
std::string username[] = {"张心欣", "王鑫磊", "彭于斌", "胡原名"};
std::string password[] = {"hellojob", "anti-job42", "cihou233", "reCihou_!"};
std::string school[] = {"九百八十五大鞋", "浙江大鞋", "剑桥大鞋", "麻绳理工鞋院"};
std::string phone[] = {"110", "119", "120", "12315"};
}

int main() {
    // for (int i = 0; i < 262144; i++) {
    //     tpool.create([&] {
    //         std::cout << do_register(test::username[rand() % 4], test::password[rand() % 4], test::school[rand() % 4], test::phone[rand() % 4]) << std::endl;
    //     });
    //     tpool.create([&] {
    //         std::cout << do_login(test::username[rand() % 4], test::password[rand() % 4]) << std::endl;
    //     });
    //     tpool.create([&] {
    //         std::cout << do_queryuser(test::username[rand() % 4]) << std::endl;
    //     });
    // }

    const uint64_t tasks = 262144;
    vector<future<basic_string<char>>> futures;

    for (uint64_t task=0; task<tasks; ++task){
        auto future1 = tpool.enqueue(do_register, test::username[rand() % 4], test::password[rand() % 4],
                            test::school[rand() % 4],test::phone[rand() % 4]);                    
        futures.emplace_back(std::move(future1));

        auto future2 = tpool.enqueue(do_login, test::username[rand() % 4], test::password[rand() % 4]);
        futures.emplace_back(std::move(future2));

        auto future3 = tpool.enqueue(do_queryuser, test::username[rand() % 4]);
        futures.emplace_back(std::move(future3));
    }
    
    for (auto& future : futures){
        cout << future.get() << endl;
    }

    // 作业要求4：等待 tpool 中所有线程都结束后再退出
    return 0;
}
