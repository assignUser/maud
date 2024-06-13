import sphinx.util.docutils
import pygments.lexers.c_cpp
import sphinx.highlighting

extensions = [
    "breathe",
]
templates_path = []
exclude_patterns = ["CMAKE_SOURCE_DIR", "Thumbs.db", ".DS_Store"]
html_static_path = []
source_suffix = {".rst": "restructuredtext"}


class Setup:
    def __call__(self, app):
        for setup in self.setups:
            setup(app)

    def __init__(self):
        self.setups = []

    def append(self, setup):
        self.setups.append(setup)
        return self


setup = Setup()


@setup.append
def setup(app):
    class Configuration(sphinx.util.docutils.SphinxDirective):
        has_content = True

        def run(self):
            return []

    app.add_directive("configuration", Configuration)
    sphinx.highlighting.lexers["c++.in2"] = pygments.lexers.c_cpp.CppLexer()
    # TODO make a utility for building in2 lexers and embed cmake's syntax
    # https://pygments.org/docs/lexerdevelopment/#using-multiple-lexers
