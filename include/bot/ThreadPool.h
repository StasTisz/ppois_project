#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <atomic>

namespace bot {

/**
 * @class ThreadPool
 * @brief Пул рабочих потоков для асинхронной неблокирующей обработки событий Telegram.
 *
 * Предотвращает блокировку long-polling цикла опроса, когда отправка постеров
 * или сетевые запросы требуют времени.
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t threads = 4) : stop_(false) {
        threads = std::max<size_t>(1, threads);
        workers_.reserve(threads);
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        cv_.wait(lock, [this]() {
                            return stop_.load() || !tasks_.empty();
                        });

                        if (stop_.load() && tasks_.empty()) {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    try {
                        task();
                    } catch (...) {
                        // Поглощаем исключения, чтобы рабочий поток не завершался аварийно
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    [[nodiscard]] size_t size() const {
        return workers_.size();
    }

    /**
     * @brief Добавляет задачу в очередь на исполнение в пуле потоков.
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_.load()) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return res;
    }

    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_.load()) return;
            stop_.store(true);
        }
        cv_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
};

} // namespace bot
