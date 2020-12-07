#ifndef MULTI_TIMER_HPP__
#define MULTI_TIMER_HPP__

#include <Ticker.h>

class MultiTimer : public Ticker
{
public:
    using callback_with_arg_and_count_t = void (*)(void*, uint32_t);
    using cb2 = void (*)(AppController*, uint32_t);

    /** @brief This timer is repeated "counts" times on creation.
     * It can also be restarted up to UINT32_MAX times.
     * The callback not also receives the standard void* argument
     * but also the number of times it was called, starting at one (!).
     */
    //template <typename TArg>
    void attach_multiple_ms(const uint32_t milliseconds,
                            uint32_t repeat_count,
                            //void (*callback)(AppController*, uint32_t),
                            cb2 callback,
                            AppController* arg) {
        //static_assert(sizeof(TArg) <= sizeof(uint32_t),
        //              "attach_ms() callback argument size must be <= 4 bytes");
        _interval_ms = milliseconds;
        _repeat_count = repeat_count;
        _callback = reinterpret_cast<callback_with_arg_and_count_t>(callback);

        auto cb_lambda = [](void* _this){
            auto self = static_cast<MultiTimer*>(_this);
            static uint32_t curr_repeat{0};
            ++curr_repeat;
            if (curr_repeat < self->_repeat_count) {
                self->restart();
            }
            self->_callback(self->_orig_arg, curr_repeat);
            };
        _attach_ms(milliseconds, false, cb_lambda, (uint32_t)this);
    }

    void restart() {
        esp_timer_start_once(_timer, _interval_ms * 1000ULL);
    }

private:
    uint32_t _interval_ms;
    uint32_t _repeat_count;
    void *_orig_arg;
    callback_with_arg_and_count_t _callback;
};

#endif