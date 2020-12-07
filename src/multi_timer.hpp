 /** @file multi_timer.hpp
  * @brief Ticker timer derivative allowing for a fixed number of repeated calls.
  * 
  * License: GPL v.3 
  * U. Lukas 2020-12-07
  */
#ifndef MULTI_TIMER_HPP__
#define MULTI_TIMER_HPP__

//#include <type_traits>
#include <Ticker.h>


/** @brief Ticker timer derivative allowing for a fixed number of repeated calls.
 * 
 * This also allows unlimited on-demand restarting of the already attached
 * callback without deleting the existing timer first.
 */
class MultiTimer : public Ticker
{
public:
    template<typename T>
    struct type_identity
    {
        using type = T;
    };
    template<class T>
    using type_identity_t = typename type_identity<T>::type;

    using callback_with_arg_and_count_t = void (*)(void*, uint32_t);

    /** @brief This timer is repeated "repeat_count" times after creation.
     * It can also be restarted at will by calling MultiTimer::restart().
     * 
     * The callback not only receives the standard void* argument
     * but also the number of times it was called, starting at one (!).
     * 
     * The counter wraps around after UINT32_MAX.
     */
    template <typename TArg>
    void attach_multitimer_ms(const uint32_t milliseconds,
                              const uint32_t repeat_count,
                              void (*callback)(type_identity_t<TArg>, uint32_t),
                              TArg arg) {
        static_assert(sizeof(TArg) <= sizeof(uint32_t),
                      "attach_ms() callback argument size must be <= 4 bytes");
        _interval_ms = milliseconds;
        _repeat_count = repeat_count;
        _callback = reinterpret_cast<callback_with_arg_and_count_t>(callback);

        auto cb_lambda = [](void *_this){
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