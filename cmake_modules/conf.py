#project = 'try doc'
#copyright = '2024, ben'
#author = 'ben'
import os
import sys
sys.path.append(os.path.abspath("."))

# from sphinx.application import Sphinx
# from sphinx.util.typing import ExtensionMetadata
# from sphinx.util.docutils import SphinxDirective
#
# class Configuration(SphinxDirective):
#     has_content = True
#     def run(self):
#         open('_build/CONF', 'w').write('\n'.join(self.content) + '\n')
#         return []
#
# class Extension:
#     @staticmethod
#     def setup(app: Sphinx) -> ExtensionMetadata:
#         app.add_directive('configuration', Configuration)
#         return {
#             'version': '0.1',
#             'parallel_read_safe': True,
#             'parallel_write_safe': True,
#         }
# inline_configuration = Extension()
extensions = ['inline_configuration']
templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
html_static_path = ['_static']
source_suffix = { '.rst': 'restructuredtext' }

def setup(app):
    pass
