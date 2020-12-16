 /** @file multi_timer.hpp
  * @brief Ticker timer derivative allowing for a fixed number of repeated calls.
  * 
  * License: GPL v.3 
  * U. Lukas 2020-12-10
  */
#ifndef MULTI_TIMER_HPP__
#define MULTI_TIMER_HPP__

//#include <functional> // Has std::invoke but is only available from C++17..
//#include <type_traits> // C++20 version has the std::type_identity_t built in
#include <Ticker.h>

// Allows calling a ordinary (non-static) class member function by inserting a
// lambda function taking the instance pointer as an argument..
// .. which because of creating a bound function should even perform better
// than indirecting a member function pointer call?
#define TICKER_MEMBER_CALL(NON_STATIC_MEMBER_FN) \
    [](decltype(this) self){self->NON_STATIC_MEMBER_FN();}, this
#define TICKER_MEMBER_CALL_WITH_COUNT(NON_STATIC_MEMBER_FN) \
    [](decltype(this) self, uint32_t i){self->NON_STATIC_MEMBER_FN(i);}, this

/** @brief Ticker timer derivative allowing for a fixed number of repeated calls.
 * 
 * This also allows unlimited on-demand restarting of the already attached
 * callback without deleting the existing timer first.
 * 
 * Like for the original Ticker.h version, all callbacks are invoked from
 * "esp_timer" task, which is a high-priority task. For this reason, the
 * callbacks should only perform a minimum amount of work and refer to
 * other tasks via message passing to do any blocking action.
 */
class MultiTimer : private Ticker
{
public:
    ////////////////////// OBSOLETE when xtensa32 toolchain has C++20 by default:
    // std::type_identity_t is part of C++20 but current toolchain uses C++11.
    // The following block is backporting the feature and will be obsolete soon.
    // When this is the case, include <type_traits> and use std::type_indentity_t
    template<typename T>
    struct type_identity {using type = T;};
    template<class T>
    using type_identity_t = typename type_identity<T>::type;
    ////////////////////// End soon to be obsolete part

    using callback_with_arg_and_count_t = void (*)(void*, uint32_t);

    /** @brief Attach a free function, static member function
     * or a non-capturing lambda function to the timer.
     * 
     * This timer is created without activating it.
     * - It is activated by calling start().
     * - It is stopped without detaching the callback by calling stop().
     *   When started again, it continues until total repeat count is reached.
     * - Calling reset() resets the number of repeats to its original value.
     * 
     * The callback can receive zero, one or two arguments:
     * - For no arguments, it is just called the preset number of times.
     * - For one argument, this is a fixed value for all calls, typically the
     *   pointer to the object of a user class for accessing any member of it.
     * - In case of two arguments, the second is an additional uint32_t value
     *   containing the current number of times the callback was called.
     *   (This wraps around after UINT32_MAX)
     * 
     * The macros TICKER_MEMBER_CALL() and TICKER_MEMBER_CALL_WITH_COUNT()
     * are a short-cut for calling non-static member functions from a calling
     * class object by inserting a lambda in place of the member function name:
     * 
     * class PrintFoo {
     * public:
     *     MultiTimer timer;
     *     const char* foo_str = "This is Foo Number: %d";
     * 
     *     void print_foo(uint32_t i) {
     *         printf(foo_str, i);
     *     }
     * 
     *     PrintFoo(uint32_t millisecs, uint32_t reps) {
     *         timer.attach_static_ms(millisecs,
     *                                reps,
     *                                TICKER_MEMBER_CALL_WITH_COUNT(print_foo)
     *                                );
     *     }
     * };
     * 
     * PrintFoo print_6x_foo{500, 6};
     * print_6x_foo.timer.start();
     * 
     */
    template <typename TArg>
    esp_err_t attach_static_ms(const uint32_t milliseconds,
                               const uint32_t total_repeat_count,
                               void (*callback)(type_identity_t<TArg>, uint32_t),
                               TArg arg) {
        static_assert(sizeof(TArg) <= sizeof(uint32_t),
                      "attach_ms() callback argument size must be <= 4 bytes");
        _interval_ms = milliseconds;
        _repeat_count_requested = total_repeat_count;
        _callback = reinterpret_cast<callback_t>(callback);
        _orig_arg = (uint32_t)arg;
        auto cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            if (++self->_repeat_count == self->_repeat_count_requested) {
                self->stop();
            }
            auto cb = reinterpret_cast<callback_with_arg_and_count_t>(self->_callback);
            auto arg = (TArg)(self->_orig_arg);
            cb(arg, self->_repeat_count);
            };
        return _attach_ms(milliseconds, cb_lambda, (uint32_t)this);
    }

    template <typename TArg>
    esp_err_t attach_static_ms(const uint32_t milliseconds,
                               const uint32_t total_repeat_count,
                               void (*callback)(type_identity_t<TArg>),
                               TArg arg) {
        static_assert(sizeof(TArg) <= sizeof(uint32_t),
                      "attach_ms() callback argument size must be <= 4 bytes");
        _interval_ms = milliseconds;
        _repeat_count_requested = total_repeat_count;
        _callback = reinterpret_cast<callback_t>(callback);
        _orig_arg = (uint32_t)arg;
        auto cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            if (++self->_repeat_count == self->_repeat_count_requested) {
                self->stop();
            }
            auto cb = reinterpret_cast<callback_with_arg_t>(self->_callback);
            auto arg = (TArg)(self->_orig_arg);
            cb(arg);
            };
        return _attach_ms(milliseconds, cb_lambda, (uint32_t)this);
    }

    esp_err_t attach_static_ms(const uint32_t milliseconds,
                               const uint32_t total_repeat_count,
                               callback_t callback) {
        _interval_ms = milliseconds;
        _repeat_count_requested = total_repeat_count;
        _callback = reinterpret_cast<callback_t>(callback);
        auto cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            if (++self->_repeat_count == self->_repeat_count_requested) {
                self->stop();
            }
            self->_callback();
            };
        return _attach_ms(milliseconds, cb_lambda, (uint32_t)this);
    }

    /** Simple version without return code for use as a callback
     */
    void start() {
        esp_timer_start_periodic(_timer, _interval_ms * 1000ULL);
    }
    void start(uint32_t interval_ms) {
        _interval_ms = interval_ms;
        esp_timer_start_periodic(_timer, _interval_ms * 1000ULL);
    }
    /** Version returning errors from API call
     */
    esp_err_t start_return_errors() {
        return esp_timer_start_periodic(_timer, _interval_ms * 1000ULL);
    }
    esp_err_t start_return_errors(uint32_t interval_ms) {
        _interval_ms = interval_ms;
        return esp_timer_start_periodic(_timer, _interval_ms * 1000ULL);
    }

    void stop() {
        esp_timer_stop(_timer);
        _repeat_count = 0;
    }
    esp_err_t stop_return_errors() {
        auto errors = esp_timer_stop(_timer);
        _repeat_count = 0;
        return errors;
    }

    void pause() {
        esp_timer_stop(_timer);
    }
    esp_err_t pause_return_errors() {
        return esp_timer_stop(_timer);
    }

    // The only other functions we make available again in this class
    using Ticker::detach;
    using Ticker::active;

protected:
    uint32_t _interval_ms;
    uint32_t _repeat_count_requested;
    uint32_t _repeat_count{0};
    uint32_t _orig_arg{0};
    callback_t _callback;

    esp_err_t _attach_ms(uint32_t milliseconds, callback_with_arg_t callback, uint32_t arg) {
        esp_timer_create_args_t _timerConfig;
        _timerConfig.arg = reinterpret_cast<void*>(arg);
        _timerConfig.callback = callback;
        _timerConfig.dispatch_method = ESP_TIMER_TASK;
        _timerConfig.name = "MultiTimer";
        if (_timer) {
            esp_timer_stop(_timer);
            esp_timer_delete(_timer);
        }
        return esp_timer_create(&_timerConfig, &_timer);
    }
};


/** @brief Same as MultiTimer but allows a pointer to a non-static
 * member function as a callback.
 * 
 * For this to work without the overhead of a std::function object,
 * the MultiTimerNonStatic object must be created with an additional
 * template argument specifying the type of the object from which to call
 * the non-static member function:
 * 
 * MultiTimerNonStatic<AppClass> timer;
 * timer.attach_mem_func_ptr_ms(10, 1, &AppClass::member_func, this);
 * timer.start();
 * 
 */
template<typename TClass>
class MultiTimerNonStatic : public MultiTimer
{
public:
    using mem_func_ptr_t = void (TClass::*)();
    using mem_func_ptr_with_count_t = void (TClass::*)(uint32_t);

    /** @brief Same as attach_static_ms() but taking a pointer to a
     * non-static member function as a callback, see class descrition.
     * 
     * Like for the static version, the member function can have an additional
     * argument containing the current number of times the callback was called.
     */
    esp_err_t attach_mem_func_ptr_ms(const uint32_t milliseconds,
                                     const uint32_t repeats,
                                     mem_func_ptr_t mem_func_ptr,
                                     TClass *inst) {
        _interval_ms = milliseconds;
        _repeat_count_requested = repeats;
        _mem_func_ptr = mem_func_ptr;
        _orig_arg = reinterpret_cast<uint32_t>(inst);
        auto cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            if (++self->_repeat_count == self->_repeat_count_requested) {
                self->stop();
            }
            auto inst = reinterpret_cast<TClass*>(self->_orig_arg);
            //std::invoke(self->_mem_func_ptr, *inst);
            auto mfp = self->_mem_func_ptr;
            (inst->*mfp)();
            };
        return _attach_ms(milliseconds, cb_lambda, (uint32_t)this);
    }

    esp_err_t attach_mem_func_ptr_ms(const uint32_t milliseconds,
                                     const uint32_t repeats,
                                     mem_func_ptr_with_count_t mem_func_ptr,
                                     TClass *inst) {
        _interval_ms = milliseconds;
        _repeat_count_requested = repeats;
        _mem_func_ptr = mem_func_ptr;
        _orig_arg = reinterpret_cast<uint32_t>(inst);
        auto cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            if (++self->_repeat_count == self->_repeat_count_requested) {
                self->stop();
            }
            auto inst = reinterpret_cast<TClass*>(self->_orig_arg);
            //std::invoke(self->_mem_func_ptr, *inst, self->_repeat_count);
            auto mfp = reinterpret_cast<mem_func_ptr_with_count_t>(self->_mem_func_ptr);
            (inst->*mfp)(self->_repeat_count);
            };
        return _attach_ms(milliseconds, cb_lambda, (uint32_t)this);
    }

protected:
    mem_func_ptr_t _mem_func_ptr;
};


#endif
