import sphinx.util.docutils
import pygments.lexers.c_cpp
import sphinx.highlighting
extensions = [
    #'breathe',
]
templates_path = []
exclude_patterns = ['Thumbs.db', '.DS_Store']
html_static_path = []
source_suffix = {'.rst': 'restructuredtext'}

def setup_maud(app):
    app.add_directive('configuration', InlineConfigurationDirective)
    sphinx.highlighting.lexers['c++.in2'] = pygments.lexers.c_cpp.CppLexer()
    # TODO make a utility for building in2 lexers and embed cmake's syntax
    # https://pygments.org/docs/lexerdevelopment/#using-multiple-lexers

def setup(app):
    setup_maud(app)

class InlineConfigurationDirective(sphinx.util.docutils.SphinxDirective):
    has_content = True
    finished = False

    def run(self):
        if not InlineConfigurationDirective.finished:
            open(__file__, 'a').write('\n'.join(self.content) + '\n')
        return []

