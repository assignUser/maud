from clang.cindex import (
    Index,
    TranslationUnit,
    SourceRange,
    Cursor,
    CursorKind,
    TokenKind,
)

import json
import argparse

parser = argparse.ArgumentParser(
    description="Scan a source file for documentation comments.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)
parser.add_argument(
    "--source",
    default="-",
    type=argparse.FileType("r"),
    help="source file to scan",
)
parser.add_argument(
    "--output",
    default="-",
    type=argparse.FileType("w"),
    help="destination file for json comments database",
)
parser.add_argument(
    "--clang-args-file",
    type=argparse.FileType("r"),
    help="\\n-separated arguments, passed to libclang",
)
parser.add_argument(
    "--doc-patterns",
    nargs="+",
    default=["/// ", "/**\n"],
    metavar=("PATTERN", "PATTERNS"),
    help="patterns used to recognize documentation comments",
)


# Anything more complicated than getting the decl and getting the docstring
# is out of scope.
# https://www.sphinx-doc.org/en/master/usage/domains/cpp.html
# - class (with namespace, base classes, template)
# - function (with namespace/membership, template, trailings)

# Read a source file
# Parse with cindex
# For each doccomment
#     begins with a directive: goto directive comment
#     line comments: concatenate consecutive comments, strip pattern
#     block comments: strip pattern&comment delimiters&indent(up to first asterisk)
#   extract the tokens of the next decl
#   the decl may end early with ; or {
#   also get the namespace from the decl
# For each directive comment
#   not yet implemented
# Output as json


def can_be_documented(cursor_kind: CursorKind):
    if cursor_kind == CursorKind.MACRO_DEFINITION:
        return True

    return cursor_kind.is_declaration() and cursor_kind not in {
        CursorKind.NAMESPACE,
        CursorKind.INVALID_FILE,
        CursorKind.NAMESPACE_REF,
        CursorKind.TEMPLATE_REF,
        CursorKind.PREPROCESSING_DIRECTIVE,
        CursorKind.MACRO_INSTANTIATION,
    }


def get_ns(cursor):
    path = []
    parent = cursor.semantic_parent
    while parent is not None and parent.spelling != tu.spelling:
        path = [parent, *path]
        parent = parent.semantic_parent
    return path


def get_sphinx_decl_extent(cursor: Cursor):
    start = None
    end = None
    tokens = cursor.get_tokens()
    start = next(tokens).extent.start
    for t in tokens:
        if t.spelling in "{;":
            # FIXME these could occur in an attribute
            # or lambda expression
            break
        end = t.extent.end
    return SourceRange.from_locations(start, end)


def comment_scan(tu: TranslationUnit, doc_patterns: list[str], contents: str) -> dict:
    declarations = []
    current_comment = None
    current_comment_end = None
    for t in tu.get_tokens(extent=tu.cursor.extent):
        if t.kind == TokenKind.COMMENT:
            comment = t.spelling
            for pattern in doc_patterns:
                if not comment.startswith(pattern):
                    continue
                comment = comment.removeprefix(pattern)

                if pattern.startswith("//"):
                    if current_comment is not None:
                        current_comment = f"{current_comment}\n{comment}"
                    else:
                        current_comment = comment
                    current_comment_end = t.extent.end
                else:
                    raise NotImplementedError

        if current_comment is None:
            continue

        if not can_be_documented(t.cursor.kind):
            continue

        if t.cursor.extent.start.offset < current_comment_end.offset:
            continue

        e = get_sphinx_decl_extent(t.cursor)
        assert str(e.start.file) == str(e.end.file)
        decl_str = (
            contents.encode("utf-8")[e.start.offset : e.end.offset]
            .decode("utf-8")
            .replace("\n", " ")
        )
        declarations.append(
            {
                "declaration": decl_str,
                "ns": "::".join([segment.spelling for segment in get_ns(t.cursor)]),
                "location": {
                    "file": str(e.start.file),
                    "start": e.start.line,
                    "end": e.end.line,
                },
                "kind": str(t.cursor.kind).removeprefix("CursorKind."),
                "comment": current_comment.split("\n"),
            }
        )
        current_comment = None
        current_comment_end = None

    return {
        "diagnostics": [str(d) for d in tu.diagnostics],
        "declarations": declarations,
    }


if __name__ == "__main__":
    args = parser.parse_args()
    index = Index.create()
    source = args.source.read()
    tu = index.parse(
        args.source.name,
        args=args.clang_args_file,
        unsaved_files=[
            (args.source.name, source),
        ],
        options=(
            TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
            | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
            | TranslationUnit.PARSE_INCOMPLETE
        ),
    )

    json.dump(comment_scan(tu, args.doc_patterns, source), args.output, indent=2)
    args.output.write("\n")
