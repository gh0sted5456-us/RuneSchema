#pragma once
#include <atomic>
#include <cstddef>
#include <future>
#include <new>
#include <type_traits>
#include <utility>

namespace PS {
inline std::atomic_bool ProcessExitInProgress{false};

template<class T> class ProcessExitFuture : public std::future<T> {
    using Base=std::future<T>;
    alignas(Base) std::byte m_exitState[sizeof(Base)];
public:
    ProcessExitFuture() noexcept=default;
    ProcessExitFuture(Base&& value) noexcept:Base(std::move(value)){}
    ProcessExitFuture(ProcessExitFuture&& value) noexcept:Base(std::move(value)){}
    template<class U> requires std::is_same_v<U,Base>
    ProcessExitFuture& operator=(U&& value) noexcept {
        Base::operator=(std::move(value));return *this;
    }
    ProcessExitFuture& operator=(ProcessExitFuture&& value) noexcept {
        Base::operator=(std::move(value));return *this;
    }
    ~ProcessExitFuture(){
        // Windows has stopped worker threads before process DLL teardown; normal unload still waits.
        if(ProcessExitInProgress.load(std::memory_order_relaxed)&&Base::valid())
            ::new(static_cast<void*>(m_exitState)) Base(std::move(*this));
    }
};
}
