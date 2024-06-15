import sphinx.util.docutils
import pygments.lexers.c_cpp
import sphinx.highlighting
import pathlib

# TODO delete this when breathe no longer triggers the warning
import warnings

warnings.filterwarnings(
    "ignore",
    category=DeprecationWarning,
    message=".*will drop support for representing paths as strings.*",
)
extensions = ["breathe"]
breathe_projects = {project: pathlib.Path(__file__).parent.parent / "xml"}
breathe_default_project = project

templates_path = []
exclude_patterns = ["CMAKE_SOURCE_DIR", "Thumbs.db", ".DS_Store"]
html_static_path = []
source_suffix = {".rst": "restructuredtext"}

setups = set()


def setup(app):
    class Configuration(sphinx.util.docutils.SphinxDirective):
        has_content = True

        def run(self):
            return []

    app.add_directive("configuration", Configuration)
    sphinx.highlighting.lexers["c++.in2"] = pygments.lexers.c_cpp.CppLexer()
    # TODO make a utility for building in2 lexers and embed cmake's syntax
    # https://pygments.org/docs/lexerdevelopment/#using-multiple-lexers

    for setup in setups:
        setup(app)
