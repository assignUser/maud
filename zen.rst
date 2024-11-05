Zen
---

- Do not require enumeration of source files; use globs.
- Do not require specification of targets; infer these from exported modules.
- Do not require finding/linking to libraries; infer these from imported modules.
- Do not require install manifests; generate and install what's necessary.
- Do not lock authors into learning maud equivalents for what's already
  available in cmake; reverting to configuration is easy.
- Do not promise dependency management; there is no single best answer to this
  (and there are plenty of okay-ish answers, and a few people stuck with poor ones).

.. trike-put:: cpp:struct Parameter : c4::yml::ConstNodeRef
  :members:

  .. FIXME
    The problem: get_namespace only uses Cursor.spelling, which
    for this struct is "Parameter" (and not "Parameter : c4::yml::ConstNodeRef").
    This means looking up the members of "Parameter : c4::yml::ConstNodeRef" fails;
    there's nothing in that namespace.

    The fix is to build up ``dict[Cursor, str]`` mapping namespaces to *Sphinx's* spelling,
    and storing directive comment namespaces as ``Cursor`` in comment_scan().
    Then we finalize by replacing the Cursors with strings
