 /** @file multi_timer.hpp
  * @brief Ticker timer derivative allowing for a fixed number of repeated calls.
  * 
  * License: GPL v.3 
  * U. Lukas 2020-12-07
  */
#ifndef MULTI_TIMER_HPP__
#define MULTI_TIMER_HPP__

#include <Ticker.h>

// Allows calling an ordinary class member just like that..
#define TICKER_MEMBER_CALL(NON_STATIC_MEMBER) [](decltype(this) self){self->NON_STATIC_MEMBER();}, this

/** @brief Ticker timer derivative allowing for a fixed number of repeated calls.
 * 
 * This also allows unlimited on-demand restarting of the already attached
 * callback without deleting the existing timer first.
 * 
 * The Ticker base class is inherited privately because we only have one
 * restart() method implementation calling esp_timer_start_periodic in any case,
 * i.e. this does not know if this was a one-shot or periodic timer without
 * the "repeats" parameter specification and always starts a periodic timer.
 * On the other hand, if we set "repeats" to 1, we still have the one-shot timer.
 */
class MultiTimer : private Ticker
{
public:
    using callback_with_arg_and_count_t = void (*)(void*, uint32_t);

    /** @brief This timer is repeated "repeats" times after creation.
     * It can be stopped without detaching the callback by calling stop().
     * It can also be restarted (resetting the timeout) by calling restart().
     * 
     * The callback must receive one or two arguments:
     * - For one argument, this is the current number of repeats (uint32_t).
     * - In case of two arguments, the first one is a static fixed value
     *   (which can be a typed pointer to a class instance, a void* or a number)
     *   and the second argument again is the current number of repeated calls.
     * 
     * The counter wraps around after UINT32_MAX.
     */
    template <typename TArg>
    void attach_multitimer_ms(const uint32_t milliseconds,
                              const uint32_t repeats,
                              void (*callback)(type_identity_t<TArg>, uint32_t),
                              TArg arg) {
        static_assert(sizeof(TArg) <= sizeof(uint32_t),
                      "attach_ms() callback argument size must be <= 4 bytes");
        _interval_ms = milliseconds;
        _repeat_count_requested = repeats;
        _callback = reinterpret_cast<callback_t>(callback);
        _orig_arg = (uint32_t)arg;
        auto cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            if (++self->_repeat_count == self->_repeat_count_requested) {
                self->stop();
            }
            auto cb = reinterpret_cast<callback_with_arg_and_count_t>(self->_callback);
            TArg arg = (TArg)(self->_orig_arg);
            cb(arg, self->_repeat_count);
            };
        _attach_ms(milliseconds, cb_lambda, (uint32_t)this);
    }

    /** @brief This timer is repeated "repeats" times after creation.
     * It can be stopped without detaching the callback by calling stop().
     * It can also be restarted (resetting the timeout) by calling restart().
     * 
     * The callback must receive one or two arguments:
     * - For one argument, this is the current number of repeats (uint32_t).
     * - In case of two arguments, the first one is a static fixed value
     *   (which can be a typed pointer to a class instance, a void* or a number)
     *   and the second argument again is the current number of repeated calls.
     * 
     * The counter wraps around after UINT32_MAX.
     */
    template <typename TArg>
    void attach_multitimer_ms(const uint32_t milliseconds,
                              const uint32_t repeats,
                              void (*callback)(type_identity_t<TArg>),
                              TArg arg) {
        static_assert(sizeof(TArg) <= sizeof(uint32_t),
                      "attach_ms() callback argument size must be <= 4 bytes");
        _interval_ms = milliseconds;
        _repeat_count_requested = repeats;
        _callback = reinterpret_cast<callback_t>(callback);
        _orig_arg = (uint32_t)arg;
        auto cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            if (++self->_repeat_count == self->_repeat_count_requested) {
                self->stop();
            }
            auto cb = reinterpret_cast<callback_with_arg_t>(self->_callback);
            TArg arg = (TArg)(self->_orig_arg);
            cb(arg);
            };
        _attach_ms(milliseconds, cb_lambda, (uint32_t)this);
    }

    void start() {
        esp_timer_start_periodic(_timer, _interval_ms * 1000ULL);
    }

    void stop() {
        esp_timer_stop(_timer);
    }

    void reset() {
        esp_timer_stop(_timer);
        _repeat_count = 0;
    }

    // The only other functions we make available again in this class
    using Ticker::detach;
    using Ticker::active;

private:
    uint32_t _interval_ms;
    uint32_t _repeat_count_requested;
    uint32_t _repeat_count{0};
    uint32_t _orig_arg;
    callback_t _callback;

    void _attach_ms(uint32_t milliseconds, callback_with_arg_t callback, uint32_t arg) {
        esp_timer_create_args_t _timerConfig;
        _timerConfig.arg = reinterpret_cast<void*>(arg);
        _timerConfig.callback = callback;
        _timerConfig.dispatch_method = ESP_TIMER_TASK;
        _timerConfig.name = "MultiTimer";
        if (_timer) {
            esp_timer_stop(_timer);
            esp_timer_delete(_timer);
        }
        esp_timer_create(&_timerConfig, &_timer);
    }
};


#endif