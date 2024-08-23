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

struct Commands : std::vector<std::string> {
  Commands() = default;
  Commands(std::string name, std::vector<std::string> commands)
      : Commands::vector{commands}, name{name} {}

  std::string name;
  bool operator==(Commands const &) const = default;
  friend std::ostream &operator<<(std::ostream &os, Commands const &c) {
    os << c.name << ":";
    for (auto const &cmd : c) {
      os << "\n$ " << cmd;
    }
    return os;
  }
};

template <>
constexpr auto minyaml::Key<Commands> = &Commands::name;

using minyaml::Document;
using minyaml::Node;

static_assert(std::is_move_constructible_v<Document>);
static_assert(std::is_move_assignable_v<Document>);
static_assert(std::is_copy_constructible_v<Document>);
static_assert(std::is_copy_assignable_v<Document>);

TEST_(string) {
  Document doc(R"(hello
world)");
  std::string str;
  EXPECT_(doc.set(str));
  EXPECT_(str == "hello world");

  doc = Document::from(str);
  EXPECT_(doc.yaml() == "hello world\n");
}

TEST_(vector_of_string) {
  Document doc(R"(
- hello
- world
)");
  std::vector<std::string> vec;
  EXPECT_(doc.set(vec));
  EXPECT_(vec == std::vector<std::string>{"hello", "world"});

  doc = Document::from(vec);
  EXPECT_(doc.yaml() == "- hello\n- world\n");
}

TEST_(vector_of_int) {
  Document doc(R"(
- 0
- 1
- 2
)");
  std::vector<int> vec;
  EXPECT_(doc.set(vec));
  EXPECT_(vec == std::vector<int>{0, 1, 2});

  doc = Document::from(vec);
  EXPECT_(doc.yaml() == "- 0\n- 1\n- 2\n");
}

TEST_(record) {
  Document doc(R"(
bar: 1
baz: 2
)");
  Foo foo;
  EXPECT_(doc.set(foo));
  EXPECT_(foo == Foo{"1", "2"});

  doc = Document::from(foo);
  EXPECT_(doc.yaml() == R"(bar: 1
baz: 2
)");
}

TEST_(vector_of_record) {
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

  doc = Document::from(foos);
  EXPECT_(doc.yaml() == R"(- bar: 1
  baz: 2
- bar: 3
  baz: 4
)");
}

TEST_(mapping_of_record) {
  Document doc(R"(
init:
- echo initializing
- mkdir -p foo/bar/baz
run:
- echo running
- runner -d foo/bar/baz
)");

  std::vector<Commands> c;
  EXPECT_(doc.set(c));
  EXPECT_(c
          == std::vector<Commands>{
              {"init", {"echo initializing", "mkdir -p foo/bar/baz"}},
              {"run",  {"echo running", "runner -d foo/bar/baz"}    },
  });

  doc = Document::from(c);
  EXPECT_(doc.yaml() == R"(init:
  - echo initializing
  - mkdir -p foo/bar/baz
run:
  - echo running
  - runner -d foo/bar/baz
)");
}

TEST_(vector_of_node) {
  Document doc(R"(
---
bar: 1
baz: 2
---
bar: 3
baz: 4
...
)");
  std::vector<Node> nodes;
  EXPECT_(doc.set(nodes));
  EXPECT_(nodes.size() == 2);

  std::vector<std::string> nodes_json;
  for (auto &&node : nodes) {
    nodes_json.push_back(node.json());
  }
  EXPECT_(nodes_json
          == std::vector<std::string>{
              R"({"bar": 1,"baz": 2})",
              R"({"bar": 3,"baz": 4})",
          });
}
