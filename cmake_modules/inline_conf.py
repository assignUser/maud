from sphinx.application import Sphinx
from sphinx.util.docutils import SphinxDirective
extensions = []
templates_path = []
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
html_static_path = []
source_suffix = {'.rst': 'restructuredtext'}

def setup(app: Sphinx):
    app.add_directive('configuration', Configuration)
    Configuration.write(
        '',
        overwrite=True
    )

class Configuration(SphinxDirective):
    has_content = True

    def run(self):
        Configuration.write(*self.content)
        return []

    @staticmethod
    def write(*lines, overwrite=False):
        m = 'w' if overwrite else 'a'
        # TODO don't rely on the working dir being _build
        open('conf.py', m).write('\n'.join(lines) + '\n')
