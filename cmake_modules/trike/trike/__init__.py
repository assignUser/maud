from dataclasses import dataclass
from pathlib import Path

import sphinx.util.logging
import difflib

from clang.cindex import (
    Cursor,
    CursorKind,
    Index,
    SourceLocation,
    Token,
    TokenKind,
    TranslationUnit,
)
from sphinx.application import Sphinx
from sphinx.environment import BuildEnvironment
from sphinx.util.typing import ExtensionMetadata
from sphinx.util.docutils import SphinxDirective
from docutils.nodes import Node
from docutils.statemachine import StringList
from typing import Sequence, Self

logger = sphinx.util.logging.getLogger(__name__)

NOT_DOCUMENTABLE = {
    CursorKind.NAMESPACE,
    CursorKind.INVALID_FILE,
    CursorKind.NAMESPACE_REF,
    CursorKind.TEMPLATE_REF,
    CursorKind.PREPROCESSING_DIRECTIVE,
    CursorKind.MACRO_INSTANTIATION,
    CursorKind.UNEXPOSED_DECL,
}

PARSE_FLAGS = (
    TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
    | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
    | TranslationUnit.PARSE_INCOMPLETE
)


@dataclass
class Comment:
    """The content of an individual ///"""

    PREFIX = "///"

    file: Path
    next_line: int
    text: list[str]
    clang_cursor_kind: str = ""

    @staticmethod
    def read_from_tokens(file: Path, tokens: Sequence[Token]) -> Self | None:
        comment = Comment(file, 0, [])

        for t in tokens:
            if t.kind != TokenKind.COMMENT:
                if comment.text:
                    break
                continue
            if t.spelling.startswith(Comment.PREFIX):
                comment.text.append(t.spelling)
                comment.next_line = t.extent.end.line + 1

        if comment.text:
            return comment

    @property
    def stripped_text(self) -> list[str]:
        return [line[len(Comment.PREFIX) + 1:] for line in self.text]


@dataclass
class DeclarationContext:
    directive: str
    namespace: str = ""
    #: For lookup purposes, DeclarationContext.module does not include a partition
    module: str = ""

    def __hash__(self):
        return hash((self.directive, self.namespace, self.module))


@dataclass
class FileContent:
    """A container for all /// content in a file"""

    module: str
    contexted_comments: list[tuple[DeclarationContext, str, Comment]]
    floating_comments: list[Comment]
    clang_diagnostics: list[str]
    mtime_when_parsed: float


def get_namespace(cursor):
    # FIXME this will probably need to be much more sophisticated;
    # since we're including classes in the namespace we may need to
    # memoize the canonicalized spelling of the class (semantic_parent.spelling
    # probably doesn't include template parameters, for example)
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
            #   /// we only see "[[preconditions"
            #   [[preconditions{ this->foo == 3 }]] int Foo::get_three() const
            #
            # However this doesn't seem critical to support, particularly since
            # if these constructions are necessary it should be sufficient to
            # override the automatic declaration string.
            break

        if t.spelling in {"class", "struct", "export", "union"}:
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

    # FIXME do a better job of canonicalizing all whitespace to " "
    return decl.strip()


def is_documentable(kind: CursorKind):
    return (
        kind == CursorKind.MACRO_DEFINITION
        or kind.is_declaration()
        and kind
        not in {
            CursorKind.NAMESPACE,
            CursorKind.INVALID_FILE,
            CursorKind.NAMESPACE_REF,
            CursorKind.TEMPLATE_REF,
            CursorKind.PREPROCESSING_DIRECTIVE,
            CursorKind.MACRO_INSTANTIATION,
            CursorKind.UNEXPOSED_DECL,
        }
    )


def spaced(first: Token, second: Token):
    return first.extent.end != second.extent.start


@dataclass
class TokenCanon:
    spelling: str = ""
    last_token: Token | None = None

    def append(self, token: Token):
        if self.last_token is not None and spaced(self.last_token, token):
            self.spelling += " "
        self.spelling += token.spelling
        self.last_token = token


def get_documentable_declaration(
    tokens: Sequence[Token],
) -> tuple[str, str, str, str] | None:
    """
    Get a documentable declaration from a token stream, with
    whitespace canonicalized to a single " "

    :return: declaration_string, clang_cursor_kind, directive_name, namespace
    """
    for t in tokens:
        if is_documentable(t.cursor.kind):
            cursor = t.cursor
            break
    else:
        return None

    namespace = get_namespace(cursor)

    clang_cursor_kind = str(cursor.kind).removeprefix("CursorKind.")
    if clang_cursor_kind == "MACRO_DEFINITION":
        directive = "c:macro"
    elif "STRUCT" in clang_cursor_kind or "CLASS" in clang_cursor_kind:
        # Classes and structs are stored together because libclang uses
        # CLASS_TEMPLATE for struct templates. We decide whether to use
        # cpp:struct or cpp:class based on the referencing directive.
        directive = "cpp:struct"
    elif "FUNCTION" in clang_cursor_kind:
        directive = "cpp:function"
    elif "VAR" in clang_cursor_kind:
        directive = "cpp:var"
    else:
        logger.info(f"UNKNOWN decl kind {clang_cursor_kind}")
        directive = ""

    cursor_tokens = cursor.get_tokens()
    declaration = TokenCanon()

    if cursor.kind == CursorKind.MACRO_DEFINITION:
        identifier = next(cursor_tokens)
        declaration.append(identifier)
        if maybe_open_paren := next(cursor_tokens, None):
            if maybe_open_paren.spelling == "(" and not spaced(
                identifier, maybe_open_paren
            ):
                # function macro; include parameters
                declaration.append(maybe_open_paren)
                for t in cursor_tokens:
                    declaration.append(t)
                    if t.spelling == ")":
                        break

    else:
        for t in cursor_tokens:
            if t.spelling in "{;":
                # TECHNICALLY these could occur in an attribute or lambda expression:
                #
                # .. code-block:: c++
                #
                #   /// we only see "IDENTITY = [](auto self)"
                #   auto IDENTITY = [](auto self) { return self; };
                #
                #   /// we only see "[[preconditions"
                #   [[preconditions{ this->foo == 3 }]] int Foo::get_three() const
                #
                # However this doesn't seem critical to support, particularly since
                # if these constructions are necessary it should be sufficient to
                # override the automatic declaration string.
                break

            if t.spelling in {"class", "struct", "export", "union"}:
                # sphinx decls do not include these; skip them
                #
                # Again, TECHNICALLY these could appear in a template argument *and*
                # be syntactically necessary. Again, simplicity here seems preferable.
                continue

            declaration.append(t)

    # Advance tokens past what we've consumed here
    offset = declaration.last_token.extent.end.offset
    for t in tokens:
        if t.extent.end.offset == offset:
            break

    return declaration.spelling, clang_cursor_kind, directive, namespace


DOCUMENTATION_COMMENT = "///"


def comment_scan(path: Path, clang_args: list[str], contents: str) -> FileContent:
    index = Index.create()
    tu = index.parse(
        path.name,
        args=clang_args,
        unsaved_files=[(path.name, contents)],
        options=PARSE_FLAGS,
    )
    #   - if explicit directive
    #     - if attached to documentable declaration, override but steal namespace
    #     - otherwise use namespace = ''
    #     - append to contexted_comments
    #   - if explicitly floating, append to floating_comments
    #   - if attached to documentable declaration, append to contexted_comments
    #   - otherwise we are unattached and not explicitly floating, error
    module = ""  # TODO detect modules
    contexted_comments = []
    floating_comments = []
    current_comment_lines = []
    tokens = tu.get_tokens(extent=tu.cursor.extent)

    while True:
        comment = Comment.read_from_tokens(path, tokens)
        if comment is None:
            break

        if d := get_documentable_declaration(tokens):
            # TODO detect ///.. explicit:directive::
            declaration, clang_cursor_kind, directive, namespace = d
            comment.clang_cursor_kind = clang_cursor_kind
            context = DeclarationContext(directive, namespace, module)
            contexted_comments.append((context, declaration, comment))
        else:
            floating_comments.append(comment)

        # TODO if not explicitly floating, then error
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

    return FileContent(
        module,
        contexted_comments,
        floating_comments,
        clang_diagnostics=[str(d) for d in tu.diagnostics],
        mtime_when_parsed=path.stat().st_mtime,
    )


@dataclass
class State:
    """A container for all /// content in a project plus tracking metadata"""

    files: dict[Path, FileContent]
    contexted_comments: dict[DeclarationContext, dict[str, Comment]]
    references: dict[str, set[Path]]

    @staticmethod
    def empty():
        return State({}, {}, {})

    def __reduce__(self):
        logger.info(f"reducing... {self.references}")
        return State, (self.files, self.contexted_comments, self.references)

    def purge(self, outdated: FileContent):
        for context, declaration, _ in outdated.contexted_comments:
            del self.contexted_comments[context][declaration]
            if not self.contexted_comments[context]:
                del self.contexted_comments[context]

    def check_for_updates(self, app: Sphinx) -> set[str]:
        logger.info("trike.State checking for updates")
        invalidated = set()

        needs_purge = []
        for path, file_content in self.files.items():
            if path in app.config.trike_files:
                mtime = path.stat().st_mtime
                if file_content.mtime_when_parsed == mtime:
                    continue
            self.purge(file_content)
            for docname, referenced_files in self.references.items():
                if path in referenced_files:
                    invalidated.add(docname)
            needs_purge.append(path)

        for path in needs_purge:
            del self.files[path]

        for path in app.config.trike_files:
            if path in self.files:
                # Anything outdated has already been purged
                assert self.files[path].mtime_when_parsed == path.stat().st_mtime
                continue
            clang_args = app.config.trike_clang_args.get(
                path, app.config.trike_default_clang_args
            )

            file_content = comment_scan(path, clang_args, path.read_text())
            self.files[path] = file_content

            for context, declaration, comment in file_content.contexted_comments:
                stored = self.contexted_comments.setdefault(context, {}).setdefault(
                    declaration, comment
                )
                if stored is not comment:
                    raise RuntimeError(
                        f"Duplicate /// detected:\n{stored}\n\n  vs\n\n{comment}"
                    )

        return invalidated


def _env_get_outdated(
    app: Sphinx,
    env: BuildEnvironment,
    added: set[str],
    changed: set[str],
    removed: set[str],
) -> set[str]:
    logger.info("trike.State handled in env-get-outdated")

    if not hasattr(env, "trike_state"):
        env.trike_state = State.empty()

    # Even if foo.rst itself has not changed, if it referenced foo.hxx
    # which *did* change then we must consider it outdated.
    #
    # XXX what if:
    # - we update foo.hxx
    # - we rebuild with html
    # - foo.rst is correctly re-parsed to foo.doctree
    # - foo.doctree is correctly re-rendered to foo.html
    # - ... now we rebuild for manpages
    # - foo.doctree is newer than foo.1 so... it *does* get re-rendered, right?
    invalidated = env.trike_state.check_for_updates(app)
    return invalidated


def _env_purge_doc(
    _app: Sphinx,
    env: BuildEnvironment,
    docname: str,
):
    if docname in env.trike_state.references:
        del env.trike_state.references[docname]


def _env_merge_info(
    _app: Sphinx,
    env: BuildEnvironment,
    subprocess_docnames: list[str],
    subprocess_env: BuildEnvironment,
):
    for docname in subprocess_docnames:
        referenced_files = env.trike_state.references.setdefault(docname, set())
        referenced_files |= subprocess_env.trike_state.references.get(docname, set())


class PutDirective(SphinxDirective):
    has_content = True
    required_arguments = 2
    optional_arguments = 1000

    def run(self) -> list[Node]:
        directive = self.arguments[0]
        namespace = self.env.temp_data.get("cpp:namespace_stack", [""])[-1]
        # TODO get module
        module = ""

        context = DeclarationContext(
            directive if directive != "cpp:class" else "cpp:struct",
            namespace,
            module,
        )
        declarations = self.env.trike_state.contexted_comments.get(context, {})

        declaration = " ".join(filter(lambda arg: arg != "\\", self.arguments[1:]))
        if comment := declarations.get(declaration, None):
            self.env.trike_state.references.setdefault(self.env.docname, set()).add(
                comment.file
            )
            logger.info(f"{comment.file} referenced by {self.env.docname}")
            text = StringList(
                [
                    f".. {directive}:: {declaration}",
                    # TODO add a link to the decl on GitHub
                    "",
                    *(f"  {line}" for line in self.content),
                    "",
                    *(f"  {line}" for line in comment.stripped_text),
                ]
            )
            return self.parse_text_to_nodes(text)

        message = f"found no declaration matching `{declaration}`\n  {context=}"
        for m in difflib.get_close_matches(declaration, declarations.keys()):
            message += f"\n    close match: `{m}`"
        logger.error(message)
        return []


def setup(app: Sphinx) -> ExtensionMetadata:
    app.add_config_value(
        "trike_files",
        [],
        "env",
        #description="All files which will be scanned for ///",
    )

    app.add_config_value(
        "trike_default_clang_args",
        [],
        "env",
        #description="Arguments which will be passed to clang",
    )
    app.add_config_value(
        "trike_clang_args",
        {},
        "env",
        #description="Per-file overrides of arguments which will be passed to clang",
    )
    app.connect("env-get-outdated", _env_get_outdated)
    app.connect("env-merge-info", _env_merge_info)
    app.connect("env-purge-doc", _env_purge_doc)

    # XXX should trike be a Domain?
    app.add_directive("trike-put", PutDirective)
    # TODO trike-function etc as a shortcut for trike-put:: cpp:function

    logger.info("trike setup")
    return {
        "version": "0.1",
        "env_version": 1,
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
