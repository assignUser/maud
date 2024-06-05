from sphinx.application import Sphinx
from sphinx.util.typing import ExtensionMetadata
from sphinx.util.docutils import SphinxDirective

class Configuration(SphinxDirective):
    has_content = True
    def run(self):
        f = open('_build/CONF.py', 'w')
        f.write('\n'.join(self.content) + '\n')
        return []

def setup(app: Sphinx) -> ExtensionMetadata:
    app.add_directive('configuration', Configuration)
    return {
        'version': '0.1',
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }

