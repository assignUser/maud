module;
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
export module minyaml:interface;

namespace minyaml {
struct Storage;

export class Node {
 public:
  std::string yaml() const;
  std::string json() const;

  template <typename T>
  bool set(T &object) const;

 protected:
  std::shared_ptr<Storage> _storage;
  int _id;
  friend bool _set(void *tree, int id, Node &node);
};

export class Document : public Node {
 public:
  explicit Document(std::string yaml, std::string_view filename = "");

  template <typename T>
  static Document from(T const &object);

 private:
  Document();
};

export template <typename T>
constexpr auto Key = nullptr;

export template <typename T>
constexpr auto Fields = nullptr;

export template <typename T>
constexpr auto FromString = nullptr;

export template <typename T>
constexpr auto ToString = nullptr;

template <auto const &Trait>
constexpr bool is_defined_v = not std::is_same_v<decltype(Trait), std::nullptr_t const &>;

void *_tree(Storage &);
int _count(void *tree, int id);

int _child_by_key(void *tree, int id, std::string_view key);
int _child(void *tree, int id, int i);

std::string_view _key(void *tree, int id);
std::string_view _scalar(void *tree, int id);

bool _is_scalar(void *tree, int id);
bool _is_sequence(void *tree, int id);
bool _is_mapping(void *tree, int id);

void _set_scalar(void *tree, int id, std::string_view value);
void _set_sequence(void *tree, int id);
void _set_mapping(void *tree, int id);
void _set_scalar(void *tree, int id, std::string_view value, std::string_view key);
void _set_sequence(void *tree, int id, std::string_view key);
void _set_mapping(void *tree, int id, std::string_view key);
int _append_child(void *tree, int id);

template <typename T>
  requires(not std::is_const_v<T>)
bool _set(void *tree, int id, T &object) {
  constexpr bool IS_SCALAR = is_defined_v<FromString<T>>;

  constexpr bool IS_NODE = std::is_same_v<T, Node>;

  constexpr bool IS_RESIZABLE_RANGE = requires(decltype(object.begin()) it) {
    { it != object.end() } -> std::same_as<bool>;
    { *it++ };
    { object.resize(object.size()) };
  };

  static_assert(IS_SCALAR or IS_RESIZABLE_RANGE or is_defined_v<Fields<T>> or IS_NODE);

  if (_is_scalar(tree, id)) {
    if constexpr (IS_SCALAR) {
      return FromString<T>(object, _scalar(tree, id));
    }
    return false;
  }

  if (_is_sequence(tree, id)) {
    if constexpr (IS_RESIZABLE_RANGE and not IS_SCALAR) {
      object.resize(_count(tree, id));
      for (int i = 0; auto &element : object) {
        if (not _set(tree, _child(tree, id, i++), element)) return false;
      }
      return true;
    }
    return false;
  }

  if (not _is_mapping(tree, id)) return false;

  if constexpr (IS_RESIZABLE_RANGE and not IS_SCALAR) {
    object.resize(_count(tree, id));
    for (int i = 0; auto &element : object) {
      int child = _child(tree, id, i++);
      using Element = std::decay_t<decltype(element)>;
      if constexpr (is_defined_v<Key<Element>>) {
        element.*Key<Element> = _key(tree, child);
      }
      if (not _set(tree, child, element)) return false;
    }
    return true;
  }

  if constexpr (is_defined_v<Fields<T>>) {
    return Fields<T>(object, [&](std::string_view name, auto &value) {
      if (int child = _child_by_key(tree, id, name); child != -1) {
        return _set(tree, child, value);
      }
      return true;
    });
  }

  return false;
}

template <typename T>
void _from(void *tree, int id, T const &object, auto... key) {
  constexpr bool IS_SCALAR = is_defined_v<ToString<T>>;

  constexpr bool IS_RANGE = requires(decltype(object.begin()) it) {
    { it != object.end() } -> std::same_as<bool>;
    { *it++ };
  };

  static_assert(IS_SCALAR or IS_RANGE or is_defined_v<Fields<T>>);

  if constexpr (IS_SCALAR) {
    std::string scalar;
    ToString<T>(object, scalar);
    return _set_scalar(tree, id, scalar, key...);
  }

  if constexpr (IS_RANGE and not IS_SCALAR) {
    using Element = std::decay_t<decltype(*object.begin())>;
    if constexpr (is_defined_v<Key<Element>>) {
      _set_mapping(tree, id, key...);
    } else {
      _set_sequence(tree, id, key...);
    }

    for (auto const &element : object) {
      if constexpr (is_defined_v<Key<Element>>) {
        _from(tree, _append_child(tree, id), element, element.*Key<Element>);
      } else {
        _from(tree, _append_child(tree, id), element);
      }
    }
    return;
  }

  if constexpr (is_defined_v<Fields<T>>) {
    _set_mapping(tree, id, key...);
    Fields<T>(object, [&](std::string_view name, auto const &value) {
      int child = _append_child(tree, id);
      _from(tree, child, value, name);
      return std::true_type{};
    });
  }
}

template <typename T>
bool Node::set(T &object) const {
  return _set(_tree(*_storage), _id, object);
}

template <typename T>
Document Document::from(T const &object) {
  Document doc;
  _from(_tree(*doc._storage), 0, object);
  return doc;
}

export template <typename T>
  requires(std::is_assignable_v<T, std::string_view>
           and std::is_constructible_v<std::string_view, T const &>)
constexpr auto FromString<T> = [](T &object, std::string_view string) {
  object = string;
  return std::true_type{};
};

export template <typename T>
  requires(std::is_assignable_v<T, std::string_view>
           and std::is_constructible_v<std::string_view, T const &>)
constexpr auto ToString<T> =
    [](T const &object, std::string &string) { string = std::string_view{object}; };

bool _double_from_string(double&, std::string_view);
void _double_to_string(double, std::string&);

export template <>
constexpr auto FromString<double> = [](double &f, std::string_view string) {
  return _double_from_string(f, string);
};
export template <>
constexpr auto ToString<double> = [](double f, std::string &string) {
  return _double_to_string(f, string);
};

bool _int_from_string(int&, std::string_view);
void _int_to_string(int, std::string&);

export template <>
constexpr auto FromString<int> = [](int &f, std::string_view string) {
  return _int_from_string(f, string);
};
export template <>
constexpr auto ToString<int> = [](int f, std::string &string) {
  return _int_to_string(f, string);
};

}  // namespace minyaml
