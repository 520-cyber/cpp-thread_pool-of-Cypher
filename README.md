# C++11 高性能线程池 (Cpp-ThreadPool)

## 📖 项目简介
基于现代 C++ (C++11) 标准库实现的高并发线程池组件。采用生产者-消费者模型，适用于处理大量并发计算任务，有效降低线程频繁创建与销毁的开销。

## ✨ 核心特性
* **统一任务封装**：使用 `std::function` 与 `std::bind` 结合完美转发（`std::forward`），支持提交任意类型、任意参数的函数及 Lambda 表达式。
* **高效线程同步**：基于 `std::mutex` 与 `std::condition_variable` 实现任务的唤醒与休眠机制，避免 CPU 空转。
* **严谨的虚假唤醒保护**：使用带 Lambda 条件的 `wait` 函数，防止系统底层虚假唤醒导致的线程异常。
* **优雅退出机制**：基于 `std::atomic<bool>` 设置停止标志位，析构时保证队列内剩余任务执行完毕，防止内存泄漏与悬垂指针。

## 🛠️ 构建与运行
本项目采用 CMake 构建：

```bash
mkdir build && cd build
cmake ..
make
./thread_app
