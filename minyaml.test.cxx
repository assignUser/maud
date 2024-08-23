module;
#include <cassert>
#include <string>
#include <string_view>
#include <vector>
module test_;
import minyaml;

struct Foo {
  std::string bar, baz;
  bool operator==(Foo const &) const = default;
  friend std::ostream &operator<<(std::ostream &os, Foo const &foo) {
    return os << "bar=" << foo.bar << ",baz=" << foo.baz;
  }
};

template <>
constexpr auto minyaml::Fields<Foo> = [](auto &foo, auto field) {
  return field("bar", foo.bar)  //
     and field("baz", foo.baz);
};

template <>
constexpr auto minyaml::Key<Foo> = &Foo::bar;

namespace minyaml {

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

TEST_(usage5) {
  Document doc(R"(
key1:
  baz: 1
key2:
  baz: 2
)");

  std::vector<Foo> foos;
  EXPECT_(doc.set(foos));
  EXPECT_(foos
          == std::vector<Foo>{
              {"key1", "1"},
              {"key2", "2"},
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
}  // namespace minyaml
