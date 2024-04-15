#include <any>
#include <cassert>
#include <compare>
#include <functional>
#include <iostream>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

#include "zip-polyfill-rassafrassa.hxx"

template <typename T> std::type_index type = typeid(T);

// Wrapper around std::any which adds guaranteed equality comparability
// and extensible conversion.
struct Object;

// Type-erased function type
using Fn = std::function<Object(std::vector<Object>)>;

// Storage for all the equality comparison function pointers
std::unordered_map<std::type_index, bool (*)(Object, Object)> eq;

// Storage for object of child type -> object of parent type function pointers
struct ConversionKey {
  std::type_index from, to;
  bool operator==(ConversionKey const &) const = default;
  struct Hash {
    std::size_t operator()(const ConversionKey &key) const {
      return key.from.hash_code() ^ (key.to.hash_code() << 1);
    }
  };
};
std::unordered_map<ConversionKey, Object (*)(Object), ConversionKey::Hash>
    convert;

struct Object : std::any {
  template <typename T> Object(T object) : std::any{object} {
    eq.emplace(::type<T>,
               [](Object l, Object r) { return l.as<T>() == r.as<T>(); });
  }

  bool operator==(Object other) const {
    if (type() != other.type())
      return false;
    return eq[type()](*this, other);
  }

  Object convert_to(std::type_index type) {
    if (std::type_index{this->type()} == type) return *this;

    auto it = convert.find({.from = this->type(), .to = type});
    if (it == convert.end()) throw "can't convert";

    return it->second(*this);
  }


  template <typename T> T const &as() const {
    return std::any_cast<T &>(*this);
  }

  template <typename T> T &as() { return std::any_cast<T &>(*this); }
};

// Storage for the graph of parent type -> derived types
std::unordered_multimap<std::type_index, std::type_index>
    types_to_directly_derived_types;

// add a derive association
template <typename Child, typename Parent> void derive() {
  types_to_directly_derived_types.emplace(type<Parent>, type<Child>);
  convert.emplace(
      ConversionKey{type<Child>, type<Parent>}, +[](Object child) -> Object {
        return static_cast<Parent>(child.as<Child>());
      });
}

bool isa(Object lhs, Object rhs) { return lhs == rhs; }

bool isa(std::type_index child, std::type_index parent) {
  if (child == parent) return true;

  for (auto [it, end] = types_to_directly_derived_types.equal_range(parent);
       it != end; ++it) {
    auto [_, child_of_parent] = *it;
    if (child_of_parent == child) return true;
    // FIXME not checking for an acyclic graph
    if (isa(child, child_of_parent)) return true;
  }
  return false;
}

bool isa(Object object, std::type_index type) {
  return isa(std::type_index{object.type()}, type);
}

struct Pattern {
  std::variant<std::type_index, Object> type_or_exact_object;

  bool is_exact_value() const {
    return std::get_if<Object>(&type_or_exact_object);
  }

  bool matches(Object object) const {
    return is_exact_value()
               ? isa(object, std::get<Object>(type_or_exact_object))
               : isa(object, std::get<std::type_index>(type_or_exact_object));
  }

  //int match_score(Object object) const { }
};

// A little black magic to go from a strongly typed function pointer to a
// type-erased Fn
template <size_t N, typename V> auto spread(V visitor) {
  return [&]<size_t... I>(std::index_sequence<I...>) {
    return visitor(std::integral_constant<size_t, I>{}...);
  }(std::make_index_sequence<N>{});
}
template <typename R, typename... A> Fn make_any_fn(R fn(A...)) {
  return [fn](std::vector<Object> args) {
    return spread<sizeof...(A)>(
        [&](auto... I) -> Object { return fn(args[I].convert_to(type<A>).template as<A>()...); });
  };
}

struct MultiMethod {
  // FIXME more intelligent lookup could be arranged here and I'm not checking
  // for ambiguity. Just sort the patterns by exact value count so they'll be
  // checked first.
  struct Method {
    Method(std::vector<Pattern> arg_patterns, Fn fn)
        : arg_patterns(std::move(arg_patterns)), fn(std::move(fn)) {
      for (auto &&pattern : this->arg_patterns) {
        exact_value_count += pattern.is_exact_value();
      }
    }

    std::vector<Pattern> arg_patterns;
    Fn fn;
    int exact_value_count = 0;

    bool operator<(Method const &other) const {
      return exact_value_count > other.exact_value_count;
    }

    bool matches(std::vector<Object> const &args) const {
      if (args.size() != arg_patterns.size())
        return false;

      for (auto [arg, pattern] : zip(args, arg_patterns)) {
        if (!pattern.matches(arg))
          return false;
      }
      return true;
    }
  };

  std::vector<Method> methods;

  void defmethod(Method method) {
    // binary search for insert position
    auto it = std::upper_bound(methods.begin(), methods.end(), method, [](auto& l, auto& r) {
      return l.exact_value_count > r.exact_value_count;
    });
    methods.insert(it, std::move(method));
  }

  void defmethod(std::vector<Pattern> arg_patterns, auto fn) {
    defmethod({std::move(arg_patterns), make_any_fn(+fn)});
  }

  Object operator()(std::vector<Object> args) const {
    for (auto const &method : methods) {
      if (method.matches(args)) {
        return method.fn(std::move(args));
      }
    }
    throw "no matching function";
  }

  Object operator()(auto... args) const {
    return operator()({Object(args)...});
  }
};

MultiMethod defmulti() { return {}; }

int main() {
  derive<int, double>();
  assert(isa(3, 3));
  assert(isa(3, type<int>));
  assert(isa(3, type<double>));

  auto add = defmulti();

  try {
    add.defmethod({{type<int>}, {type<int>}}, [](int l, int r) { return l + r; });

    add.defmethod({{type<double>}, {type<double>}}, [](double l, double r) {
      return l + r + 1.25; // it's FPE or something don't look at me
    });

    add.defmethod({{999}, {999}}, [](int l, int r) {
      return 0; // An obscure SQL feature means we must return zero in this case
    });

    assert(add(1, 2) == Object(3));
    assert(add(1, 2.0) == Object(4.25));
    assert(add(999, 999) == Object(0));
  } catch (char const *msg) {
    std::cout << msg << std::endl;
  }
}
