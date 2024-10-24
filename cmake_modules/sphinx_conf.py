import sphinx.util.docutils
import docutils.nodes
import docutils.statemachine
import pygments.lexers.c_cpp
import sphinx.highlighting
import pathlib
import json

extensions = []

templates_path = []
exclude_patterns = ["CMAKE_SOURCE_DIR", "Thumbs.db", ".DS_Store"]
html_static_path = []
source_suffix = {
    ".rst": "restructuredtext",
}

setups = set()

STAGE_DIR = pathlib.Path(__file__).parent

APIDOC = {
    "diagnostics": {},  # mapping from file to diagnostics
    "comments": [],  # all comments
}
for apidoc in STAGE_DIR.parent.glob("apidoc/**/*.json"):
    apidoc = json.load(apidoc.open())
    f = apidoc["file"]
    for d in apidoc["comments"]:
        d["file"] = f
        APIDOC["comments"].append(d)
    APIDOC["diagnostics"][f] = apidoc["diagnostics"]

for conf in STAGE_DIR.glob("**/*.conf.py"):
    # There must be a better way to do this.
    exec(conf.read_text())


def setup(app):
    class Configuration(sphinx.util.docutils.SphinxDirective):
        has_content = True

        def run(self):
            return []

    class ApiDoc(sphinx.util.docutils.SphinxDirective):
        has_content = True
        required_arguments = 1
        optional_arguments = 1000

        def run(self) -> list[docutils.nodes.Node]:
            # TODO add a link to the decl on GitHub
            nodes = []
            term = " ".join(self.arguments)
            print(f"searching for declaration matching {term}")
            for d in APIDOC["comments"]:
                if self.arguments[0] in d["declaration"]:
                    print(f"found declaration matching {term} {d['kind']=}")
                    text = []
                    # TODO add a .. cpp:namespace:: here if appropriate
                    if d["kind"] == "MACRO_DEFINITION":
                        text.append(f".. c:macro:: {d['declaration']}")
                    elif "STRUCT" in d["kind"]:
                        text.append(f".. cpp:struct:: {d['declaration']}")
                    elif "CLASS" in d["kind"]:
                        # TODO libclang uses CLASS_TEMPLATE for struct
                        # templates, which looks odd.
                        text.append(f".. cpp:class:: {d['declaration']}")
                    else:
                        continue
                    blank = [""]
                    for line in (*blank, *self.content, *blank, *d["comment"]):
                        text.append(f"  {line}")
                    text = docutils.statemachine.StringList(text)
                    nodes.extend(self.parse_text_to_nodes(text))
            return nodes

    app.add_directive("configuration", Configuration)
    app.add_directive("apidoc", ApiDoc)
    app.add_config_value("APIDOC", {}, "env")
    sphinx.highlighting.lexers["c++.in2"] = pygments.lexers.c_cpp.CppLexer()
    # TODO make a utility for building in2 lexers and embed cmake's syntax
    # https://pygments.org/docs/lexerdevelopment/#using-multiple-lexers

    for setup in setups:
        setup(app)
