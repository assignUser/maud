import argparse
import json

from clang.cindex import (
    Cursor,
    CursorKind,
    Index,
    SourceRange,
    TokenKind,
    TranslationUnit,
)

parser = argparse.ArgumentParser(
    description="Scan a source file for documentation comments.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)
parser.add_argument(
    "--source",
    required=True,
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
    tokens = cursor.get_tokens()
    first = next(tokens)
    start = first.extent.start
    end = start
    if cursor.kind == CursorKind.MACRO_DEFINITION:
        assert first.kind == TokenKind.IDENTIFIER
        end = first.extent.end
        if second := next(tokens, None):
            if second.spelling == "(":
                if second.extent.start == first.extent.end:
                    # function macro, find the end of the parameters
                    for t in tokens:
                        if t.spelling == ")":
                            end = t.extent.end
                            break
    else:
        for t in tokens:
            if t.spelling in "{;":
                # FIXME these could occur in an attribute
                # or lambda expression
                break
            end = t.extent.end
    return SourceRange.from_locations(start, end)


DOCUMENTATION_COMMENT_PREFIX = "/// "


def comment_scan(tu: TranslationUnit, contents: str) -> dict:
    declarations = []
    current_comment = []
    current_comment_end = None
    for t in tu.get_tokens(extent=tu.cursor.extent):
        if t.kind == TokenKind.COMMENT:
            comment = t.spelling
            if comment == DOCUMENTATION_COMMENT_PREFIX[:-1]:
                current_comment.append("")
                continue

            if not comment.startswith(DOCUMENTATION_COMMENT_PREFIX):
                continue
            current_comment.append(comment.removeprefix(DOCUMENTATION_COMMENT_PREFIX))
            current_comment_end = t.extent.end
            continue

        if current_comment == []:
            continue

        if not can_be_documented(t.cursor.kind):
            continue

        if t.cursor.extent.start.offset < current_comment_end.offset:
            continue

        e = get_sphinx_decl_extent(t.cursor)
        assert str(e.start.file) == str(e.end.file) == tu.spelling

        decl_str = (
            contents.encode("utf-8")[e.start.offset : e.end.offset]
            .decode("utf-8")
            .replace("\n", " ")
        )
        declarations.append({
            "declaration": decl_str,
            "ns": "::".join([segment.spelling for segment in get_ns(t.cursor)]),
            "line": e.start.line,
            "kind": str(t.cursor.kind).removeprefix("CursorKind."),
            "comment": current_comment,
        })
        current_comment = []
        current_comment_end = None

    return {
        "file": str(tu.spelling),
        "diagnostics": [str(d) for d in tu.diagnostics],
        "declarations": declarations,
    }


if __name__ == "__main__":
    args = parser.parse_args()

    if args.clang_args_file is not None:
        clang_args = args.clang_args_file.read().splitlines()
    else:
        clang_args = []

    index = Index.create()
    source = args.source.read()
    tu = index.parse(
        args.source.name,
        args=clang_args,
        unsaved_files=[
            (args.source.name, source),
        ],
        options=(
            TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
            | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
            | TranslationUnit.PARSE_INCOMPLETE
        ),
    )

    json.dump(comment_scan(tu, source), args.output, indent=2)
    args.output.write("\n")
