#pragma once
#include <chrono>
#include <coroutine>

#include <agents.h>
#include <pplawait.h>

namespace foo_scrobble
{

//! Creates a task that delays execution by the specified amount.
inline concurrency::task<void> create_delay(
    std::chrono::duration<int, std::milli> delay,
    concurrency::cancellation_token token = concurrency::cancellation_token::none())
{
    concurrency::task_completion_event<void> tce;
    auto timer = std::make_unique<concurrency::timer<int>>(delay.count(), 0, nullptr,
                                                           false);
    auto callback = std::make_unique<concurrency::call<int>>([tce](int) { tce.set(); });

    timer->link_target(callback.get());
    timer->start();
    co_await create_task(tce);
}

//! Delays execution by the specified amount.
auto operator co_await(std::chrono::system_clock::duration duration)
{
    class awaiter
    {
    public:
        explicit awaiter(std::chrono::system_clock::duration d)
            : duration(d)
        {}

        ~awaiter()
        {
            if (timer)
                CloseThreadpoolTimer(timer);
        }

        bool await_ready() const
        {
            return duration <= std::chrono::system_clock::duration::zero();
        }

        void await_suspend(std::coroutine_handle<> resume_cb)
        {
            timer = CreateThreadpoolTimer(TimerCallback, resume_cb.address(), nullptr);
            if (!timer)
                throw std::bad_alloc();
            int64_t relativeCount = -duration.count();
            SetThreadpoolTimer(timer, reinterpret_cast<PFILETIME>(&relativeCount), 0, 0);
        }

        void await_resume() {}

    private:
        static void CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE, void* context,
                                           PTP_TIMER)
        {
            std::coroutine_handle<>::from_address(context).resume();
        }

        PTP_TIMER timer = nullptr;
        std::chrono::system_clock::duration duration;
    };

    return awaiter{duration};
}

template<typename R>
auto with_scheduler(concurrency::task<R>&& task,
                    concurrency::scheduler_interface& scheduler)
{
    struct Awaiter
    {
        concurrency::scheduler_interface* scheduler;
        concurrency::task<R>&& input;
        concurrency::task<R> output;

        bool await_ready()
        {
            if (input.is_done()) {
                output = std::move(input);
                return true;
            }
            return false;
        }

        auto await_resume() { return output.get(); }

        void await_suspend(std::coroutine_handle<> coro)
        {
            auto callback = [this, coro](concurrency::task<R> t) {
                this->output = std::move(t);
                coro.resume();
            };

            if (scheduler)
                input.then(std::move(callback), concurrency::task_options(*scheduler));
            else
                input.then(std::move(callback));
        }
    };

    return Awaiter{&scheduler, std::forward<concurrency::task<R>&&>(task)};
}

} // namespace foo_scrobble
