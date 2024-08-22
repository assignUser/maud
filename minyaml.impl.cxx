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

module minyaml:implementation;
import :interface;

namespace minyaml {

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

struct Storage {
  c4::yml::Tree tree;
  std::weak_ptr<Storage> weak_this;
};

void *_tree(Storage &storage) { return &storage.tree; }

static_assert(sizeof(Storage) >= sizeof(std::string));

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
  // TODO use the most explicit overload and make sure static Documents will be fine
  c4::yml::parse_in_place({filename.data(), filename.size()}, data, &_storage->tree);
  _id = _storage->tree.root_id();
  _storage->weak_this = _storage;
}

Document::Document() {
  _storage = {
      new Storage,
      [](void *storage) { delete static_cast<Storage *>(storage); },
  };
  _storage->weak_this = _storage;
  _id = _storage->tree.root_id();
  assert(_id == 0);
}

bool _set(void *tree, int id, Node &node) {
  node._storage =
      reinterpret_cast<Storage *>(static_cast<c4::yml::Tree *>(tree))->weak_this.lock();
  node._id = id;
  return true;
}

std::string Node::yaml() const {
  // TODO use a more efficient emitter, maybe we can even get an estimate of the output's size
  return (std::stringstream{} << c4::yml::ConstNodeRef{&_storage->tree, _id}).str();
}

std::string Node::json() const {
  return (std::stringstream{} << c4::yml::as_json(
              c4::yml::ConstNodeRef{&_storage->tree, _id}))
      .str();
}
}  // namespace minyaml
