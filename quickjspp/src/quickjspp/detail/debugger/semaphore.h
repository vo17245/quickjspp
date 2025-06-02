#include <mutex>
#include <condition_variable>

class Semaphore {
public:
    explicit Semaphore(int count = 0) : m_Count(count) {}

    // Wait for the signal
    void Wait() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Condition.wait(lock, [this]() { return m_Count > 0; });
        --m_Count;
    }

    // Signal to release one waiting thread
    void Signal() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        ++m_Count;
        m_Condition.notify_one();
    }

private:
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
    int m_Count; // Counter for the semaphore
};