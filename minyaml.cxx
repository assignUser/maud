module;
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#define RYML_ID_TYPE int
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "rapidyaml.hxx"

// export module minyaml;
#define export
module test_;

export class Document {
 public:
  explicit Document(std::string yaml, std::string_view filename = "");

  template <typename T>
  bool set(T &object) const;

  template <typename T>
  static Document from(T const &object);

  std::string yaml() const;
  std::string json() const;

  struct Storage;

 private:
  Document();
  std::shared_ptr<Storage> _storage;
};

export class Node {
 public:
  std::string yaml() const;
  std::string json() const;

  template <typename T>
  bool set(T &object) const;

 private:
  std::shared_ptr<Document::Storage> _storage;
  int _id;
  friend bool _set(void *tree, int id, Node &node);
};

void *_tree(Document::Storage &);

export template <typename T>
constexpr auto Fields = nullptr;

export template <typename T>
constexpr auto Fields<T &> = Fields<T>;

export template <typename T>
constexpr auto Fields<T const> = Fields<T>;

template <typename R>
concept Record = not std::is_same_v<decltype(Fields<R>), decltype(Fields<void>)>;

template <typename R>
concept ResizableRange = requires(R &range) {
  { range.resize(16) };
  { range.begin() != range.end() };
};

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

  constexpr bool IS_STRING = std::is_same_v<T, std::string>;
  static_assert(IS_STRING or ResizableRange<T> or Record<T>);
  // TODO add FromString/ToString etc

  if constexpr (IS_STRING) {
    assert(_is_scalar(tree, id));
    object = std::string{_scalar(tree, id)};
    return true;
  }

  if constexpr (ResizableRange<T> and not IS_STRING) {
    assert(_is_sequence(tree, id) or _is_mapping(tree, id));

    object.resize(_count(tree, id));

    if (_is_sequence(tree, id)) {
      // range sequence
      for (int i = 0; auto &element : object) {
        if (not _set(tree, _child(tree, id, i++), element)) return false;
      }
      return true;
    }

    for (int i = 0; auto &element : object) {
      int child = _child(tree, id, i++);

      if (not _set(tree, child, element)) return false;
      // TODO add special fields
      // _set_field<KEY>(element, [&](auto &value) {
      //   // set the KEY field if there is one
      //   // TODO FromString trait or something
      //   value = std::string(_key(tree, child));
      //   return std::true_type{};
      // });
    }
    return true;
  }

  if constexpr (Record<T>) {
    if (_is_mapping(tree, id)) {
      return Fields<T>(object, [&](std::string_view name, auto &value) {
        // record mapping
        if (int child = _child_by_key(tree, id, name); child != c4::yml::NONE) {
          return _set(tree, child, value);
        }
        return true;
      });
    }
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
bool Document::set(T &object) const {
  return _set(_tree(*_storage), 0, object);
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

// module:private;

int _count(void *tree, int id) {
  return static_cast<c4::yml::Tree const *>(tree)->num_children(id);
}

int _child_by_key(void *tree, int id, std::string_view key) {
  return static_cast<c4::yml::Tree const *>(tree)->find_child(id,
                                                              {key.data(), key.size()});
}
int _child(void *tree, int id, int i) {
  return static_cast<c4::yml::Tree const *>(tree)->child(id, i);
}

std::string_view _key(void *tree, int id) {
  auto key = static_cast<c4::yml::Tree const *>(tree)->key(id);
  return {key.data(), key.size()};
}
std::string_view _scalar(void *tree, int id) {
  auto scalar = static_cast<c4::yml::Tree const *>(tree)->val(id);
  return {scalar.data(), scalar.size()};
}

bool _is_scalar(void *tree, int id) {
  return static_cast<c4::yml::Tree const *>(tree)->has_val(id);
}
bool _is_sequence(void *tree, int id) {
  return static_cast<c4::yml::Tree const *>(tree)->is_seq(id);
}
bool _is_mapping(void *tree, int id) {
  return static_cast<c4::yml::Tree const *>(tree)->is_map(id);
}

void _set_scalar(void *tree, int id, std::string_view value) {
  static_cast<c4::yml::Tree *>(tree)->to_val(id, {value.data(), value.size()});
}
void _set_sequence(void *tree, int id) {
  if (id == 0) {
    // TODO if it's root, make a stream instead of a sequence
    // return static_cast<c4::yml::Tree *>(tree)->set_root_as_stream();
  }
  static_cast<c4::yml::Tree *>(tree)->to_seq(id);
}
void _set_mapping(void *tree, int id) { static_cast<c4::yml::Tree *>(tree)->to_map(id); }

void _set_scalar(void *tree, int id, std::string_view value, std::string_view key) {
  static_cast<c4::yml::Tree *>(tree)->to_keyval(id, {key.data(), key.size()},
                                                {value.data(), value.size()});
}
void _set_sequence(void *tree, int id, std::string_view key) {
  static_cast<c4::yml::Tree *>(tree)->to_seq(id, {key.data(), key.size()});
}
void _set_mapping(void *tree, int id, std::string_view key) {
  static_cast<c4::yml::Tree *>(tree)->to_map(id, {key.data(), key.size()});
}

int _append_child(void *tree, int id) {
  return static_cast<c4::yml::Tree *>(tree)->append_child(id);
}

struct Document::Storage {
  c4::yml::Tree tree;
  std::weak_ptr<Storage> weak_this;
};

void *_tree(Document::Storage &storage) { return &storage.tree; }

static_assert(sizeof(Document::Storage) >= sizeof(std::string));

Document::Document(std::string yaml, std::string_view filename) {
  // Minor hackery: the string *probably* already has extra capacity,
  // so save ourselves a small allocation and store the Tree past all the
  // chars. This also guarantees that the string is on the heap instead of
  // short/inline so moving it won't invalidate pointers to its data.
  size_t size = alignof(Storage) + sizeof(Storage);
  yaml.append(size, '\0');
  c4::substr data{yaml.data(), yaml.size() - size};

  void *ptr = yaml.data() + yaml.size() - size;
  void *aligned = std::align(alignof(Storage), sizeof(Storage), ptr, size);

  _storage = {
      new (ptr) Storage,
      [yaml = std::move(yaml)](void *storage) {
        // No need to free; that'll be handled when the closure is destroyed.
        static_cast<Storage *>(storage)->~Storage();
      },
  };
  c4::yml::parse_in_place({filename.data(), filename.size()}, data, &_storage->tree);
  _storage->weak_this = _storage;
}

Document::Document() {
  _storage = {
      new Storage,
      [](void *storage) { delete static_cast<Storage *>(storage); },
  };
  _storage->weak_this = _storage;
  // Make sure the root id is initialized
  int root_id = _storage->tree.root_id();
  assert(root_id == 0);
}

bool _set(void *tree, int id, Node &node) {
  node._storage =
      reinterpret_cast<Document::Storage *>(static_cast<c4::yml::Tree *>(tree))
          ->weak_this.lock();
  node._id = id;
  return true;
}

std::string Document::yaml() const {
  return (std::stringstream{} << c4::yml::ConstNodeRef{&_storage->tree, 0}).str();
}

std::string Document::json() const {
  return (std::stringstream{} << c4::yml::as_json(
              c4::yml::ConstNodeRef{&_storage->tree, 0}))
      .str();
}

std::string Node::yaml() const {
  return (std::stringstream{} << c4::yml::ConstNodeRef{&_storage->tree, _id}).str();
}

std::string Node::json() const {
  return (std::stringstream{} << c4::yml::as_json(
              c4::yml::ConstNodeRef{&_storage->tree, _id}))
      .str();
}

struct Foo {
  std::string bar, baz;
  bool operator==(Foo const &) const = default;
};

template <>
constexpr auto Fields<Foo> = [](auto &foo, auto field) {
  return field("bar", foo.bar)  //
     and field("baz", foo.baz);
};

static_assert(std::is_move_constructible_v<Document>);
static_assert(std::is_move_assignable_v<Document>);
static_assert(std::is_copy_constructible_v<Document>);
static_assert(std::is_copy_assignable_v<Document>);

TEST_(usage) {
  Document doc(R"(
bar: 1
baz: 2
)");

  Foo foo;
  EXPECT_(doc.set(foo));
  EXPECT_(foo == Foo{"1", "2"});
}

TEST_(usage2) {
  Document doc(R"(
- hello
- world
)");

  std::vector<std::string> vec;
  assert(doc.set(vec));
  EXPECT_(vec == std::vector<std::string>{"hello", "world"});
}

TEST_(usage3) {
  Document doc(R"(hello world)");

  std::string str;
  assert(doc.set(str));
  EXPECT_(str == "hello world");
}

TEST_(usage4) {
  Document doc(R"(
---
bar: 1
baz: 2
---
bar: 3
baz: 4
...
)");

  std::vector<Foo> foos;
  EXPECT_(doc.set(foos));
  EXPECT_(foos
          == std::vector<Foo>{
              {"1", "2"},
              {"3", "4"},
  });
}

TEST_(from_string) {
  std::string s = "hello";
  auto doc = Document::from(s);
  EXPECT_(doc.yaml() == "hello\n");
}

TEST_(from_vector) {
  std::vector<std::string> vec{"hello", "world"};
  auto doc = Document::from(vec);
  EXPECT_(doc.yaml() == R"(- hello
- world
)");
}

TEST_(from_foo) {
  Foo foo{"1", "2"};
  auto doc = Document::from(foo);
  EXPECT_(doc.yaml() == R"(bar: 1
baz: 2
)");
}
