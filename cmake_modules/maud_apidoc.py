import argparse
import json

from clang.cindex import (
    Cursor,
    CursorKind,
    Index,
    SourceLocation,
    TokenKind,
    TranslationUnit,
)

argparser = argparse.ArgumentParser(
    description="Scan a source file for documentation comments.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)
argparser.add_argument(
    "--source",
    required=True,
    type=argparse.FileType("r"),
    help="source file to scan",
)
argparser.add_argument(
    "--output",
    default="-",
    type=argparse.FileType("w"),
    help="destination file for json comments database",
)
argparser.add_argument(
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
#   line comments: concatenate consecutive comments, strip pattern
#   extract the tokens of the next decl
#   the decl may end early with ; or {
#   also get the namespace from the decl
# Output as json


NOT_DOCUMENTABLE = {
    CursorKind.NAMESPACE,
    CursorKind.INVALID_FILE,
    CursorKind.NAMESPACE_REF,
    CursorKind.TEMPLATE_REF,
    CursorKind.PREPROCESSING_DIRECTIVE,
    CursorKind.MACRO_INSTANTIATION,
}


def get_namespace(cursor):
    path = []
    parent = cursor.semantic_parent
    tu = cursor.translation_unit.cursor
    while parent is not None and parent != tu:
        path = [parent.spelling, *path]
        parent = parent.semantic_parent
    return "::".join(path)


def byte_slice(s: str, begin: SourceLocation, end: SourceLocation):
    # source location offsets are byte offsets, whereas str[] indexes by code point
    return s.encode("utf-8")[begin.offset : end.offset].decode("utf-8")


def get_sphinx_decl(cursor: Cursor, contents: str):
    tokens = cursor.get_tokens()

    if cursor.kind == CursorKind.MACRO_DEFINITION:
        first = next(tokens)
        assert first.kind == TokenKind.IDENTIFIER
        start = first.extent.start
        end = first.extent.end
        if second := next(tokens, None):
            if second.spelling == "(":
                if second.extent.start == first.extent.end:
                    # function macro; find the end of the parameters
                    for t in tokens:
                        if t.spelling == ")":
                            end = t.extent.end
                            break
        return byte_slice(contents, start, end)

    if not cursor.kind.is_declaration() or cursor.kind in NOT_DOCUMENTABLE:
        return None

    decl = ""
    previous_token = None
    for t in tokens:
        if t.spelling in "{;":
            # TECHNICALLY these could occur in an attribute or lambda expression:
            #
            # .. code-block:: c++
            #
            #   /// we only see "IDENTITY = [](auto self)"
            #   auto IDENTITY = [](auto self) { return self; };
            #
            # .. code-block:: c++
            #
            #   /// we only see "[[preconditions"
            #   [[preconditions{ this->foo == 3 }]] int Foo::get_three() const
            #
            # However this doesn't seem critical to support, particularly since
            # if these constructions are necessary it should be sufficient to
            # override the automatic declaration string.
            break

        if t.spelling in {"class", "struct"}:
            # sphinx decls do not include these; skip them
            #
            # Again, TECHNICALLY these could appear in a template argument *and*
            # be syntactically necessary. Again, simplicity here seems preferable.
            previous_token = t
            continue

        if previous_token is not None:
            # include any whitespace between this token and the previous
            start = previous_token.extent.end
        else:
            start = t.extent.start

        decl += byte_slice(contents, start, t.extent.end)
        previous_token = t

    return decl


DOCUMENTATION_COMMENT = "///"


def comment_scan(tu: TranslationUnit, contents: str) -> dict:
    comments = []
    current_comment = []
    for t in tu.get_tokens(extent=tu.cursor.extent):
        if t.kind == TokenKind.COMMENT:
            line = t.spelling
            if not line.startswith(DOCUMENTATION_COMMENT):
                continue

            current_comment.append(line[len(DOCUMENTATION_COMMENT) + 1 :])
            current_comment_end = t.extent.end
            previous_token = t
            continue

        if current_comment == []:
            continue

        # At this point, we have collected a doccomment and we are looking
        # for a declaration to which it should be attached. Clang associates tokens
        # with cursors (pointers into the AST). So we can scan through the tokens
        # following this comment, looking for the first which is associated with
        # a documentable declaration.

        if t.extent.start.line >= previous_token.extent.end.line + 2:
            # There is at least one blank line after the previous token. At this point
            # we consider this doccomment orphaned; we could not find a declaration
            # with which it is obviously associated. This could be a failure of
            # liblclang or our usage of it or could be intentional. In either case,
            # we store the orphaned comment and let Sphinx sort it out.
            declaration = ""
            namespace = ""

        else:
            previous_token = t
            if t.cursor.extent.start.offset < current_comment_end.offset:
                # Some of the tokens captured by Clang might be associated with a declaration
                # which *encloses* the decl of interest. For example:
                #
                # .. code-block:: c++
                #
                #   struct Foo {
                #     enum Color { R, G, B };
                #     /// The first token after this doccomment is the return type,
                #     /// for which t.cursor corresponds to Foo rather than get_color.
                #     Color get_color();
                #   };
                continue

            declaration = get_sphinx_decl(t.cursor, contents)
            if declaration is None:
                continue

            namespace = get_namespace(t.cursor)

        comments.append(
            {
                "declaration": declaration,
                "namespace": namespace,
                "line": current_comment_end.line + 1,
                "kind": str(t.cursor.kind).removeprefix("CursorKind."),
                "comment": current_comment,
            }
        )
        current_comment = []

    return {
        "file": str(tu.spelling),
        "diagnostics": [str(d) for d in tu.diagnostics],
        "comments": comments,
    }


if __name__ == "__main__":
    args = argparser.parse_args()

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
