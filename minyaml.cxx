module;
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#define RYML_ID_TYPE int
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "rapidyaml.hxx"
export module minyaml;

export decltype(auto) set_map(c4::yml::NodeRef node, auto key) {
  node[key] |= c4::yml::MAP;
  return node[key];
}

export template <typename T>
constexpr auto Fields = nullptr;

export template <typename T>
constexpr auto Fields<T &> = Fields<T>;

export template <typename T>
constexpr auto Fields<T const> = Fields<T>;

export template <typename R>
concept Record = requires(R &record) {
  {
    Fields<R>(record, [](auto field, auto &value) { return true; })
  } -> std::same_as<bool>;
};

template <typename F, typename T>
constexpr bool has(F field, T const &object) {
  if constexpr (Record<T>) {
    bool none = Fields<T>(object, [&](auto f, auto &) {
      if constexpr (std::is_same_v<F, decltype(f)>) {
        return f != field;
      } else {
        return std::true_type{};
      }
    });
    return not none;
  } else {
    return std::false_type{};
  }
}

export template <typename E>
constexpr auto Enumeration = nullptr;

export enum { SCALAR, SEQUENCE, MAPPING, KEY };

export template <typename R>
concept Range = requires(R &range) {
  { range.resize(16) };
  { range.begin() != range.end() };
};

template <typename T>
constexpr void set_string(T &object, std::string_view v) {
  object = T(v);
}

export struct Node {
  // enough to reconstruct a ConstNodeRef
  void *_tree;
  int _id;

  std::string yaml() const {
    return (std::stringstream{}
            << c4::yml::ConstNodeRef{static_cast<c4::yml::Tree const *>(_tree), _id})
        .str();
  }
  std::string json() const {
    return (std::stringstream{} << c4::yml::as_json(
                c4::yml::ConstNodeRef{static_cast<c4::yml::Tree const *>(_tree), _id}))
        .str();
  }

  template <typename T>
  constexpr bool set(T &object) const {
    constexpr bool STRING_OR_VIEW =
        std::is_same_v<T, std::string> or std::is_same_v<T, std::string_view>;
    // make this an overload set instead

    static_assert(std::is_same_v<T, Node> or STRING_OR_VIEW or Range<T> or Record<T>);

    if constexpr (std::is_same_v<T, Node>) {
      object = *this;
      return true;
    }

    if constexpr (STRING_OR_VIEW) {
      assert(_is_scalar());

      // string scalar
      set_string(object, _scalar());
      return true;
    }

    if constexpr (Range<T>) {
      assert(not _is_scalar());

      object.resize(_count());
      int i = 0;

      if (_is_sequence()) {
        // vector sequence
        for (auto &element : object) {
          if (not _child(i++).set(element)) return false;
        }
      }

      if (_is_mapping()) {
        // vector mapping
        for (auto &element : object) {
          if (not _child(i).set(element)) return false;

          Fields<T>(object, [&](auto field, auto &value) {
            if constexpr (field == KEY) {
              set_string(value, _child(i)._key());
            }
            return std::true_type{};
          });

          ++i;
        }
      }

      return true;
    }

    if constexpr (Record<T>) {
      if constexpr (has(SCALAR, object)) {
        if (_is_scalar()) {
          // record with scalar field
          return _set_field<SCALAR>(object);
        }
      }

      if constexpr (has(SEQUENCE, object)) {
        if (_is_sequence()) {
          // record with sequence field
          return _set_field<SEQUENCE>(object);
        }
      }

      assert(_is_mapping());
      if constexpr (has(MAPPING, object)) {
        // record with mapping field
        return _set_field<MAPPING>(object);
      }

      // record mapping
      return Fields<T>(object, [&](auto name, auto &value) {  //
        if constexpr (std::is_constructible_v<std::string_view, decltype(name)>) {
          return _child(name).set(value);
        } else {
          // ignore special fields
          return std::true_type{};
        }
      });
    }
  }

 private:
  template <auto FIELD, typename T>
  constexpr bool _set_field(T &object) const {
    return Fields<T>(object, [&](auto field, auto &value) {
      if constexpr (field == FIELD) {
        return set(value);
      }
      return true;
    });
  }

  Node _child(std::string_view key) const {
    return {_tree, static_cast<c4::yml::Tree const *>(_tree)->find_child(
                       _id, {key.data(), key.size()})};
  }
  Node _child(int i) const {
    return {_tree, static_cast<c4::yml::Tree const *>(_tree)->child(_id, i)};
  }
  int _count() const {
    return static_cast<c4::yml::Tree const *>(_tree)->num_children(_id);
  }
  std::string_view _key() const {
    auto key = static_cast<c4::yml::Tree const *>(_tree)->key(_id);
    return {key.data(), key.size()};
  }

  std::string_view _scalar() const {
    auto s = static_cast<c4::yml::Tree const *>(_tree)->val(_id);
    return {s.data(), s.size()};
  }

  bool _is_scalar() const {
    return static_cast<c4::yml::Tree const *>(_tree)->is_val(_id);
  }
  bool _is_sequence() const {
    return static_cast<c4::yml::Tree const *>(_tree)->is_seq(_id);
  }
  bool _is_mapping() const {
    return static_cast<c4::yml::Tree const *>(_tree)->is_map(_id);
  }
};

export struct Document : Node {
  Document(std::string yaml) : Node{nullptr, 0}, _data{std::move(yaml)} {
    auto size = _data.size();
    _data.append(sizeof(c4::yml::Tree) * 2, '\0');
    char *data = _data.data();
    _tree = data + size;
    size = sizeof(c4::yml::Tree) * 2;
    std::align(alignof(c4::yml::Tree), sizeof(c4::yml::Tree), _tree, size);
    new (_tree) c4::yml::Tree{c4::yml::parse_in_place(data)};
  }
  Document(Document &&) = default;
  Document &operator=(Document &&) = default;
  ~Document() { static_cast<c4::yml::Tree *>(_tree)->~Tree(); }

  std::string _data;
};
