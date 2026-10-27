#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <chrono>

class ThreadPool {
public:
    // 构造函数：创建指定数量的工作线程
    ThreadPool(size_t numThreads) : stop_(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            // 启动线程，每个线程都执行 worker_loop
            workers_.emplace_back([this]() {
                worker_loop();
            });
        }
    }

    // 析构函数：安全关闭线程池
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stop_ = true; // 告诉所有线程：准备下班了
        }
        condition_.notify_all(); // 唤醒所有正在睡觉的线程

        // 等待所有线程把手头的活干完并安全退出
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        std::cout << "[线程池] 已安全关闭，所有打工人下班。" << std::endl;
    }

    // 核心接口：提交任务（支持带任意参数的任务）
    template<typename F, typename... Args>
    void enqueue(F&& f, Args&&... args) {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_) {
                throw std::runtime_error("线程池已停止，禁止提交新任务！");
            }

            // 使用 std::bind 将函数和参数打包成一个无参的可调用对象
            // 完美转发保证参数的值类别（左值/右值）不丢失
            auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            
            // 放入队列（底层自动被转换为 std::function<void()>）
            tasks_.emplace(task);
        }
        // 唤醒一个正在等待任务的线程
        condition_.notify_one();
    }

private:
    // 工作线程的主循环
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                
                // 如果队列为空，且没有收到停止信号，就在这里睡觉
                // 使用 Lambda 配合 condition_variable 防止“虚假唤醒”
                condition_.wait(lock, [this]() {
                    return stop_ || !tasks_.empty();
                });

                // 如果收到停止信号，且队列已空，线程就可以退出了
                if (stop_ && tasks_.empty()) {
                    return;
                }

                // 从队列中取出一个任务
                task = std::move(tasks_.front());
                tasks_.pop();
            } // 锁的作用域结束，unique_lock 析构，自动解锁

            // 在锁外执行任务（耗时任务不阻塞其他线程抢任务）
            task();
        }
    }

    // 核心成员变量
    std::vector<std::thread> workers_;                  // 工作线程数组
    std::queue<std::function<void()>> tasks_;           // 任务队列
    std::mutex queueMutex_;                             // 保护任务队列的互斥锁
    std::condition_variable condition_;                 // 条件变量，用于唤醒和休眠线程
    std::atomic<bool> stop_;                            // 停止标志
};


// ================= 测试代码 =================

// 测试函数：模拟一个耗时任务
std::mutex printMutex; // 用于保护打印输出，避免多线程打印混乱

void print_task(int id, std::string msg) {
    std::lock_guard<std::mutex> lock(printMutex);
    std::cout << "线程 [" << std::this_thread::get_id() << "] 执行任务 " << id << ": " << msg << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟耗时
}

int main() {
    std::cout << "主线程：初始化线程池（雇佣 3 个打工人）..." << std::endl;
    ThreadPool pool(3);

    // 提交 10 个任务
    for (int i = 0; i < 10; ++i) {
        // 提交带有参数的任务
        pool.enqueue(print_task, i, "Hello 线程池");
        
        // 也可以提交 Lambda
        pool.enqueue([&](int x) {
            std::lock_guard<std::mutex> lock(printMutex);
            std::cout << "线程 [" << std::this_thread::get_id() << "] 执行 Lambda 任务: " << x * x << std::endl;
        }, i);
    }

    std::cout << "主线程：任务全部提交完毕，等待线程池处理完..." << std::endl;
    
    // 主线程休眠 2 秒，让线程池有时间处理完所有任务
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "主线程：准备退出，触发线程池析构..." << std::endl;
    return 0; // 退出 main 函数，触发 ThreadPool 的析构函数（安全关闭）
}