import sphinx.util.docutils
import docutils.nodes
import pygments.lexers.c_cpp
import sphinx.highlighting
import pathlib
import textwrap
import json

# add to the sphinx prelude code which loads apidoc jsons in preparation for .. cpp:autofunction::

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
    "declarations": [],  # all declarations
}
for apidoc in STAGE_DIR.parent.glob("apidoc/**/*.json"):
    apidoc = json.load(apidoc.open())
    f = apidoc["file"]
    for d in apidoc["declarations"]:
        d["file"] = f
        APIDOC["declarations"].append(d)
    APIDOC["diagnostics"][f] = apidoc["diagnostics"]

for conf in STAGE_DIR.parent.glob("configuration/**/*.py"):
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
            nodes = []
            term = ' '.join(self.arguments)
            print(f"searching for declaration matching {term}")
            for d in APIDOC["declarations"]:
                if self.arguments[0] in d["declaration"]:
                    print(
                        f"found declaration matching {term} {d['kind']=}"
                    )
                    if d["kind"] == "MACRO_DEFINITION":
                        text = f".. c:macro:: {d['declaration']}\n\n"
                    elif "CLASS" in d["kind"]:
                        text = f".. cpp:class:: {d['declaration']}\n\n"
                    else:
                        continue
                    text += textwrap.indent("\n".join(d["comment"]), "  ")
                    nodes = [*nodes, *self.parse_text_to_nodes(text)]
            return nodes

    app.add_directive("configuration", Configuration)
    app.add_directive("apidoc", ApiDoc)
    sphinx.highlighting.lexers["c++.in2"] = pygments.lexers.c_cpp.CppLexer()
    # TODO make a utility for building in2 lexers and embed cmake's syntax
    # https://pygments.org/docs/lexerdevelopment/#using-multiple-lexers

    for setup in setups:
        setup(app)
