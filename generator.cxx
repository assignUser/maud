module;
#include <coroutine>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
module executable;

template <typename T>
struct Generator {
  struct promise_type {
    std::optional<T> value;
    std::exception_ptr exception;

    Generator get_return_object() {
      return {std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { exception = std::current_exception(); }

    template <std::convertible_to<T> From>
    std::suspend_always yield_value(From &&from) {
      value = std::forward<From>(from);
      return {};
    }
    void return_void() {}
  };

  struct sentinel {};

  struct iterator {
    std::coroutine_handle<promise_type> &handle;

    bool operator==(sentinel) const { return handle.done(); }

    iterator &operator++() {
      handle.resume();
      if (handle.promise().exception) {
        // Note that if we deferred this to operator* we might skip an exception
        // if we were only iterating and not reading the range.
        std::rethrow_exception(std::move(handle.promise().exception));
      }
      return *this;
    }

    T operator*() const {
      // Since this is an input range, we're not required to guarantee the
      // validity of `*it` independent of `*it++`.
      T value = std::move(*handle.promise().value);
      handle.promise().value.reset();
      return value;
    }
  };

  sentinel end() { return {}; }
  iterator begin() { return ++iterator{handle}; }

  std::coroutine_handle<promise_type> handle;
  ~Generator() { handle.destroy(); }
};

Generator<uint64_t> fibonacci_sequence(unsigned n) {
  if (n == 0) co_return;

  co_yield 0;
  if (n == 1) co_return;

  co_yield 1;
  if (n == 2) co_return;

  uint64_t a = 0, b = 1;
  for (unsigned i = 2; i < n; ++i) {
    uint64_t s = a + b;
    co_yield s;
    a = b;
    b = s;

    if (i > 7) throw "Too big Fibonacci sequence. Elements would overflow.";
  }
}

struct NoDefault {
  NoDefault() = delete;
};
Generator<NoDefault> wtf() { co_return; }

int main() {
  try {
    for (int i = 0; int j : fibonacci_sequence(10)) {
      std::cout << "fib(" << i++ << ")=" << j << '\n';
    }
    for (int i = 0; int j : [] () -> Generator<int> { co_yield 0; }()) {
      std::cout << "adhoc(" << i++ << ")=" << j << '\n';
    }
  } catch (char const *str) {
    std::cerr << "exception: " << str << std::endl;
  }
}
