import trike
from pathlib import Path
from clang.cindex import (
    Cursor,
    CursorKind,
    Index,
    SourceLocation,
    Token,
    TokenKind,
    TranslationUnit,
)


def make_tu(tmp_path, source, clang_args=[]):
    path = tmp_path / "source.cxx"
    path.write_text(source)
    tu = Index.create().parse(str(path), args=clang_args, options=trike.PARSE_FLAGS)
    return tu, path


def test_basic(tmp_path):
    _, path = make_tu(
        tmp_path,
        """
        /// The entry point
        // clang-format on
        /// something clang-format would mangle
        // clang-format off
        int main() {
            return 0
        }
        """,
    )
    file_content = trike.comment_scan(path, clang_args=[])
    assert file_content.contexted_comments == [
        (
            trike.DeclarationContext(directive="cpp:function"),
            "int main()",
            trike.Comment(
                path,
                next_line=5,
                text=["/// The entry point", "/// something clang-format would mangle"],
                clang_cursor_kind="FUNCTION_DECL",
            ),
        )
    ]


def test_comment_from_tokens(tmp_path):
    tu, path = make_tu(
        tmp_path,
        """
        /// The entry point
        // clang-format off
        /// something clang-format would mangle
        // clang-format on
        int foo = 3;

        /// Foo
        // clang-format off
        /// Bar
        // clang-format on
        """,
    )

    tokens = trike.Tokens(tu)
    comment = trike.Comment.read_from_tokens(path, tokens)
    assert comment.next_line == 5
    assert comment.text == [
        "/// The entry point",
        "/// something clang-format would mangle",
    ]
    assert next(tokens).spelling == "int"

    comment = trike.Comment.read_from_tokens(path, tokens)
    assert comment.next_line == 11
    assert comment.text == [
        "/// Foo",
        "/// Bar",
    ]

    comment = trike.Comment.read_from_tokens(path, tokens)
    assert comment is None


def test_is_documentable():
    assert trike.is_documentable(CursorKind.MACRO_DEFINITION)
    assert trike.is_documentable(CursorKind.FUNCTION_DECL)
    assert trike.is_documentable(CursorKind.FUNCTION_TEMPLATE)
    assert trike.is_documentable(CursorKind.CLASS_TEMPLATE)
    assert trike.is_documentable(CursorKind.STRUCT_DECL)
    assert trike.is_documentable(CursorKind.ENUM_DECL)
    assert trike.is_documentable(CursorKind.VAR_DECL)
    assert not trike.is_documentable(CursorKind.PREPROCESSING_DIRECTIVE)
    assert not trike.is_documentable(CursorKind.UNEXPOSED_DECL)
    assert not trike.is_documentable(CursorKind.STRING_LITERAL)
    assert not trike.is_documentable(CursorKind.BLOCK_EXPR)
    assert not trike.is_documentable(CursorKind.CXX_BASE_SPECIFIER)


def test_documentable_declaration(tmp_path):
    tu, _ = make_tu(
        tmp_path,
        """
        /// The entry point
        // clang-format off
        /// something clang-format would mangle
        // clang-format on
        int foo = 3;

        /// Foo
        // clang-format off
        /// Bar
        // clang-format on
        """,
    )

    tokens = trike.Tokens(tu)
    assert trike.get_documentable_declaration(tokens) == (
        "int foo = 3",
        "VAR_DECL",
        "cpp:var",
        "",
    )
    assert next(tokens).spelling == "/// Foo"
    assert trike.get_documentable_declaration(tokens) is None


def test_whitespace(tmp_path):
    tu, _ = make_tu(tmp_path, """  int   foo  =3   ;   """)
    assert trike.join_tokens(trike.Tokens(tu)) == """int foo =3 ;"""
