module;
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
export module minyaml:interface;
// #define export
// module test_;

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
constexpr void *Key = nullptr;

export template <typename T>
constexpr void *Scalar = nullptr;

export template <typename T>
constexpr void *Sequence = nullptr;

export template <typename T>
constexpr void *Mapping = nullptr;

export template <typename T>
constexpr void *Fields = nullptr;

template <typename R>
concept Record = not std::is_same_v<decltype(Fields<R>), decltype(Fields<void>)>;

template <typename R>
concept ResizableRange = requires(R &range) {
  { range.resize(16) };
  { range.begin() != range.end() };
};

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

[[noreturn]] void _unreachable() { throw 0; }

template <typename T>
bool _set(void *tree, int id, T &object) {
  static_assert(not std::is_same_v<T, T const>);

  constexpr bool IS_STRING = std::is_assignable_v<T, std::string_view>;
  static_assert(IS_STRING or ResizableRange<T> or Record<T> or Scalar<T> or Sequence<T>
                or Mapping<T>);
  // TODO add FromString/ToString etc

  if constexpr (IS_STRING) {
    assert(_is_scalar(tree, id));
    object = _scalar(tree, id);
    return true;
  }

  // ... FIXME but how do we decide what to write?
  if constexpr (Scalar<T>) {
    if (_is_scalar(tree, id)) {
      return _set(tree, id, object.*Scalar<T>);
    }
  }

  if constexpr (Sequence<T>) {
    if (_is_sequence(tree, id)) {
      return _set(tree, id, object.*Sequence<T>);
    }
  }

  if constexpr (Mapping<T>) {
    if (_is_mapping(tree, id)) {
      return _set(tree, id, object.*Mapping<T>);
    }
  }

  if constexpr (ResizableRange<T> and not IS_STRING) {
    using Element = std::decay_t<decltype(*object.begin())>;
    assert(_is_sequence(tree, id) or _is_mapping(tree, id));

    object.resize(_count(tree, id));

    if (_is_sequence(tree, id) or not Key<Element>) {
      bool result = true;
      for (int i = 0; auto &element : object) {
        result &= _set(tree, _child(tree, id, i++), element);
      }
      return result;
    }

    if constexpr (Key<Element>) {
      bool result = true;
      for (int i = 0; auto &element : object) {
        int child = _child(tree, id, i++);
        element.*Key<Element> = _key(tree, child);
        result &= _set(tree, child, element);
      }
      return result;
    }

    return false;
  }

  if constexpr (Record<T>) {
    if (_is_mapping(tree, id)) {
      return Fields<T>(object, [&](std::string_view name, auto &value) {
        if (int child = _child_by_key(tree, id, name); child != -1) {
          return _set(tree, child, value);
        }
        return true;
      });
    }
    return false;
  }

  _unreachable();
}

template <typename T>
void _from(void *tree, int id, T const &object, auto... key) {
  constexpr bool IS_STRING =
      std::is_same_v<T, std::string> or std::is_same_v<T, std::string_view>;
  static_assert(IS_STRING or ResizableRange<T> or Record<T>);
  // TODO add FromString/ToString etc

  if constexpr (IS_STRING) {
    return _set_scalar(tree, id, object, key...);
  }

  if constexpr (ResizableRange<T> and not IS_STRING) {
    // range sequence
    _set_sequence(tree, id, key...);
    for (auto const &element : object) {
      _from(tree, _append_child(tree, id), element);
    }
    return;
  }

  if constexpr (Record<T>) {
    _set_mapping(tree, id, key...);
    Fields<T>(object, [&](std::string_view name, auto const &value) {
      // record mapping
      int child = _append_child(tree, id);
      _from(tree, child, value, name);
      return true;
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

}  // namespace minyaml
