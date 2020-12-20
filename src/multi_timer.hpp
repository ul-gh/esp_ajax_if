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
     * - It is stopped without detaching the callback by calling stop(),
     *   which also resets the number of repeats to its original value.
     * - It can be paused by calling pause(), which does not reset the repeats.
     * - After calling resume(), continues until total repeat count is reached.
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
     *     const char *foo_str = "This is Foo Number: %d";
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
     * ==> Please note: This software timer is only relatively accurate.
     *     That menas, for each repeat, the timer is stopped and started again
     *     immediately if the total number of repeats is not yet reached.
     *     This means that for multiple repeats, each small timing error
     *     will sum up to a larger value.
     *     If you need accurate timing for a large (or infinite) number of
     *     repeats, please use the Ticker class which features the periodic
     *     attach_ms() which has better long-term accuracy.
     * 
     * @param milliseconds: Timer interval in milliseconds
     * @param total_repeat_count: Timer is stopped after this many repeats
     * @param callback: Callback function to register into this timer
     * @param arg: Numeric arg or pointer to object, e.g. calling class instance
     * @param first_tick_nodelay: If set to true, call callback immediately when
     *                            the start() function is invoked, the first
     *                            tick counts as a normal repeat and is repeated
     *                            until total repeat count is reached
     */
    template <typename TArg>
    esp_err_t attach_static_ms(uint32_t milliseconds,
                               uint32_t total_repeat_count,
                               void (*callback)(type_identity_t<TArg>, uint32_t),
                               TArg arg,
                               bool first_tick_nodelay=false) {
        static_assert(sizeof(TArg) <= sizeof(uint32_t),
                      "attach_ms() callback argument size must be <= 4 bytes");
        _interval_ms = milliseconds;
        _repeat_count_requested = total_repeat_count;
        _callback = reinterpret_cast<callback_t>(callback);
        _orig_arg = (uint32_t)arg;
        _first_tick_nodelay = first_tick_nodelay;
        _cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            auto repeat_count = ++self->_repeat_count;
            if (repeat_count < self->_repeat_count_requested) {
                esp_timer_start_once(self->_timer, self->_interval_ms*1000ull);
            } else {
                self->_repeat_count = 0;
            }
            auto cb = reinterpret_cast<callback_with_arg_and_count_t>(self->_callback);
            auto arg = (TArg)(self->_orig_arg);
            cb(arg, repeat_count);
            };
        return _attach_ms((uint32_t)this);
    }

    template <typename TArg>
    esp_err_t attach_static_ms(uint32_t milliseconds,
                               uint32_t total_repeat_count,
                               void (*callback)(type_identity_t<TArg>),
                               TArg arg,
                               bool first_tick_nodelay=false) {
        static_assert(sizeof(TArg) <= sizeof(uint32_t),
                      "attach_ms() callback argument size must be <= 4 bytes");
        _interval_ms = milliseconds;
        _repeat_count_requested = total_repeat_count;
        _callback = reinterpret_cast<callback_t>(callback);
        _orig_arg = (uint32_t)arg;
        _first_tick_nodelay = first_tick_nodelay;
        _cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            auto repeat_count = ++self->_repeat_count;
            if (repeat_count < self->_repeat_count_requested) {
                esp_timer_start_once(self->_timer, self->_interval_ms*1000ull);
            } else {
                self->_repeat_count = 0;
            }
            auto cb = reinterpret_cast<callback_with_arg_t>(self->_callback);
            auto arg = (TArg)(self->_orig_arg);
            cb(arg);
            };
        return _attach_ms((uint32_t)this);
    }

    esp_err_t attach_static_ms(uint32_t milliseconds,
                               uint32_t total_repeat_count,
                               callback_t callback,
                               bool first_tick_nodelay=false) {
        _interval_ms = milliseconds;
        _repeat_count_requested = total_repeat_count;
        _callback = reinterpret_cast<callback_t>(callback);
        _first_tick_nodelay = first_tick_nodelay;
        _cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            auto repeat_count = ++self->_repeat_count;
            if (repeat_count < self->_repeat_count_requested) {
                esp_timer_start_once(self->_timer, self->_interval_ms*1000ull);
            } else {
                self->_repeat_count = 0;
            }
            self->_callback();
            };
        return _attach_ms((uint32_t)this);
    }

    /** Without return value, we don't get this out of the lambda without
     * another class member.. 
     */
    void start() {
        if (_first_tick_nodelay && _cb_lambda) {
            _cb_lambda(this);
        } else {
            esp_timer_start_once(_timer, _interval_ms*1000ull);
        }
    }
    void start(uint32_t interval_ms) {
        _interval_ms = interval_ms;
        if (_first_tick_nodelay && _cb_lambda) {
            _cb_lambda(this);
        } else {
            esp_timer_start_once(_timer, _interval_ms*1000ull);
        }
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

    void resume() {
        esp_timer_start_once(_timer, _interval_ms*1000ull);
    }
    esp_err_t resume_return_errors() {
        return esp_timer_start_once(_timer, _interval_ms*1000ull);
    }

    // The only other functions we make available again in this class
    using Ticker::detach;
    using Ticker::active;

protected:
    using Ticker::_timer;
    uint32_t _interval_ms;
    uint32_t _repeat_count_requested{1};
    uint32_t _repeat_count{0};
    // Original callback cast into a callback_t
    callback_t _callback;
    uint32_t _orig_arg{0};
    // Callback encapsulated into lambda function with repeat counter etc.
    callback_with_arg_t _cb_lambda{nullptr};
    bool _first_tick_nodelay{false};

    esp_err_t _attach_ms(uint32_t arg) {
        esp_timer_create_args_t _timerConfig;
        _timerConfig.callback = _cb_lambda;
        _timerConfig.arg = reinterpret_cast<void*>(arg);
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
 */
template<typename TClass>
class MultiTimerNonStatic : public MultiTimer
{
public:
    using mem_func_ptr_t = void (TClass::*)();
    using mem_func_ptr_with_count_t = void (TClass::*)(uint32_t);

    /** @brief Same as attach_static_ms() but taking a pointer to a
     * non-static member function as a callback, see class description.
     * 
     * Like for the static version, the member function can have an additional
     * argument containing the current number of times the callback was called.
     * 
     * @param milliseconds: Timer interval in milliseconds
     * @param total_repeat_count: Timer is stopped after this many repeats
     * @param mem_func_ptr: Pointer to a bound, non-static member function
     * @param inst: Pointer to the class instance the mem_func_ptr is bound to
     * @param first_tick_nodelay: If set to true, call callback immediately when
     *                            the start() function is invoked, the first
     *                            tick counts as a normal repeat and is repeated
     *                            until total repeat count is reached
     */
    esp_err_t attach_mem_func_ptr_ms(uint32_t milliseconds,
                                     uint32_t total_repeat_count,
                                     mem_func_ptr_t mem_func_ptr,
                                     TClass *inst,
                                     bool first_tick_nodelay=false) {
        _interval_ms = milliseconds;
        _repeat_count_requested = total_repeat_count;
        _mem_func_ptr = mem_func_ptr;
        _orig_arg = reinterpret_cast<uint32_t>(inst);
        _first_tick_nodelay = first_tick_nodelay;
        _cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            auto repeat_count = ++self->_repeat_count;
            if (repeat_count < self->_repeat_count_requested) {
                esp_timer_start_once(self->_timer, self->_interval_ms*1000ull);
            } else {
                self->_repeat_count = 0;
            }
            auto inst = reinterpret_cast<TClass*>(self->_orig_arg);
            //std::invoke(self->_mem_func_ptr, *inst);
            auto mfp = self->_mem_func_ptr;
            (inst->*mfp)();
            };
        return _attach_ms((uint32_t)this);
    }

    esp_err_t attach_mem_func_ptr_ms(uint32_t milliseconds,
                                     uint32_t total_repeat_count,
                                     mem_func_ptr_with_count_t mem_func_ptr,
                                     TClass *inst,
                                     bool first_tick_nodelay=false) {                   
        _interval_ms = milliseconds;
        _repeat_count_requested = total_repeat_count;
        _mem_func_ptr = reinterpret_cast<mem_func_ptr_t>(mem_func_ptr);
        //_mem_func_ptr_with_count = mem_func_ptr;
        _orig_arg = reinterpret_cast<uint32_t>(inst);
        _first_tick_nodelay = first_tick_nodelay;
        _cb_lambda = [](void *_this){
            auto self = static_cast<decltype(this)>(_this);
            auto repeat_count = ++self->_repeat_count;
            if (repeat_count < self->_repeat_count_requested) {
                esp_timer_start_once(self->_timer, self->_interval_ms*1000ull);
            } else {
                self->_repeat_count = 0;
            }
            auto inst = reinterpret_cast<TClass*>(self->_orig_arg);
            //std::invoke(self->_mem_func_ptr, *inst, self->_repeat_count);
            auto mfp = reinterpret_cast<mem_func_ptr_with_count_t>(self->_mem_func_ptr);
            //auto mfp = self->_mem_func_ptr_with_count;
            (inst->*mfp)(repeat_count);
            };
        return _attach_ms((uint32_t)this);
    }

protected:
    mem_func_ptr_t _mem_func_ptr;
    //mem_func_ptr_with_count_t _mem_func_ptr_with_count;
};


#endif
