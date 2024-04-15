#include <iterator>
#include <tuple>
#include <cstddef>

template <typename Ranges, typename Indices>
struct zip;

template <typename... Ranges>
zip(Ranges&&...) -> zip<std::tuple<Ranges...>, std::index_sequence_for<Ranges...>>;

template <typename... Ranges, size_t... I>
struct zip<std::tuple<Ranges...>, std::index_sequence<I...>> {
  explicit zip(Ranges... ranges) : ranges_(std::forward<Ranges>(ranges)...) {}

  std::tuple<Ranges...> ranges_;

  using sentinel = std::tuple<decltype(std::end(std::get<I>(ranges_)))...>;
  constexpr sentinel end() { return {std::end(std::get<I>(ranges_))...}; }

  struct iterator : std::tuple<decltype(std::begin(std::get<I>(ranges_)))...> {
    using std::tuple<decltype(std::begin(std::get<I>(ranges_)))...>::tuple;

    constexpr auto operator*() {
      return std::tuple<decltype(*std::get<I>(*this))...>{*std::get<I>(*this)...};
    }

    constexpr iterator& operator++() {
      (++std::get<I>(*this), ...);
      return *this;
    }

    constexpr bool operator!=(const sentinel& s) const {
      bool all_iterators_valid = (... && (std::get<I>(*this) != std::get<I>(s)));
      return all_iterators_valid;
    }
  };
  constexpr iterator begin() { return {std::begin(std::get<I>(ranges_))...}; }
};

