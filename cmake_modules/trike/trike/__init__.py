from dataclasses import dataclass
from collections import defaultdict
from contextlib import contextmanager
from pathlib import Path

import sphinx.util.logging
import difflib

from clang.cindex import (
    Cursor,
    CursorKind,
    Index,
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
from typing import Self, Sequence

logger = sphinx.util.logging.getLogger(__name__)


PARSE_FLAGS = (
    TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
    | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
    | TranslationUnit.PARSE_INCOMPLETE
)


class Tokens:
    def __init__(self, tu: TranslationUnit):
        self.tokens = iter(tu.get_tokens(extent=tu.cursor.extent))
        self._next = None

    def __iter__(self):
        return self

    def __next__(self):
        t, self._next = self._next, None
        return t or next(self.tokens)

    def unget(self, t):
        self._next = t


type Namespace = tuple[str, ...]
type ModuleName = str


@dataclass
class Comment:
    """The content of an individual ///"""

    PREFIX = "///"

    file: Path
    next_line: int
    text: list[str]
    clang_cursor_kind: str = ""

    @staticmethod
    def read_from_tokens(file: Path, tokens: Tokens) -> Self | None:
        for t in tokens:
            if t.kind == TokenKind.COMMENT and t.spelling.startswith(Comment.PREFIX):
                break
        else:
            return None

        comment = Comment(file, t.extent.end.line + 1, [t.spelling])
        for t in tokens:
            if t.kind != TokenKind.COMMENT or t.extent.start.line > comment.next_line:
                tokens.unget(t)
                break
            comment.next_line = t.extent.end.line + 1

            if t.spelling.startswith(Comment.PREFIX):
                comment.text.append(t.spelling)

        return comment

    @property
    def stripped_text(self) -> list[str]:
        return [line[len(Comment.PREFIX) + 1 :] for line in self.text]

    def get_explicit_directive(self):
        if self.text[0].startswith("///.. "):
            directive, argument = self.text.pop(0).removeprefix("///.. ").split("::", 1)
            return directive.strip(), argument.strip()

    @dataclass
    class Context:
        directive: str
        namespace: Namespace = ()
        module: ModuleName = ""

        def __hash__(self):
            return hash((self.directive, self.namespace, self.module))


@dataclass
class FileContent:
    """A container for all /// content in a file"""

    module: ModuleName
    directive_comments: list[tuple[str, str, Namespace, Comment]]
    floating_comments: list[Comment]
    clang_diagnostics: list[str]
    mtime_when_parsed: float


def get_namespace(cursor: Cursor) -> tuple[str, ...]:
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
    return tuple(path)


def is_documentable(kind: CursorKind):
    # TODO this should instead return the directive which we use
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


def contiguous(first: Token | None, second: Token | None) -> bool:
    if first is None or second is None:
        return True
    return first.extent.end == second.extent.start


def join_tokens(tokens: Sequence[Token]) -> str:
    tokens_it = iter(tokens)
    previous = next(tokens_it)
    joined = previous.spelling
    for t in tokens_it:
        if not contiguous(previous, t):
            joined += " "
        joined += t.spelling
        previous = t
    return joined


def get_documentable_declaration(
    tokens: Tokens,
) -> tuple[str, str, Namespace, str] | None:
    """
    Get a documentable declaration from a token stream, with
    whitespace canonicalized to a single " "

    :return: declaration_string, clang_cursor_kind, directive_name, namespace
    """
    if t := next(tokens, None):
        min_offset = t.extent.start.offset
        tokens.unget(t)
    else:
        return None

    for t in tokens:
        if t.cursor.extent.start.offset < min_offset:
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

        if is_documentable(t.cursor.kind):
            cursor = t.cursor
            break
    else:
        return None

    namespace = get_namespace(cursor)

    clang_cursor_kind = str(cursor.kind).removeprefix("CursorKind.")
    if clang_cursor_kind == "MACRO_DEFINITION":
        directive = "c:macro"
    elif clang_cursor_kind == "FIELD_DECL":
        directive = "cpp:member"
    elif "TYPE_ALIAS" in clang_cursor_kind:
        directive = "cpp:type"
    elif "STRUCT" in clang_cursor_kind or "CLASS" in clang_cursor_kind:
        # Classes and structs are stored together because libclang uses
        # CLASS_TEMPLATE for struct templates. We decide whether to use
        # cpp:struct or cpp:class based on the referencing directive.
        directive = "cpp:struct"
    elif "FUNCTION" in clang_cursor_kind or clang_cursor_kind == "CXX_METHOD":
        directive = "cpp:function"
    elif "VAR" in clang_cursor_kind:
        directive = "cpp:var"
    else:
        logger.error(f"UNKNOWN decl kind {clang_cursor_kind}")
        directive = ""

    cursor_tokens = cursor.get_tokens()
    declaration = []

    if cursor.kind == CursorKind.MACRO_DEFINITION:
        name = next(cursor_tokens)
        declaration.append(name)
        if maybe_open_paren := next(cursor_tokens, None):
            if maybe_open_paren.spelling == "(" and contiguous(name, maybe_open_paren):
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

            if t.spelling in {"class", "struct", "export", "union", "using", "typedef"}:
                # sphinx decls do not include these; skip them
                #
                # Again, TECHNICALLY these could appear in a template argument *and*
                # be syntactically necessary. Again, simplicity here seems preferable.
                continue

            declaration.append(t)

    # Advance tokens past what we've consumed here
    next_line = declaration[-1].extent.end.line + 1
    for t in tokens:
        if t.extent.end.line >= next_line:
            tokens.unget(t)
            break

    return directive, join_tokens(declaration), namespace, clang_cursor_kind


def comment_scan(path: Path, clang_args: list[str]) -> FileContent:
    tu = Index.create().parse(str(path), args=clang_args, options=PARSE_FLAGS)
    module = ""  # TODO detect modules
    directive_comments = []
    floating_comments = []
    tokens = Tokens(tu)

    while True:
        comment = Comment.read_from_tokens(path, tokens)
        if comment is None:
            break

        t = next(tokens, None)
        explicitly_floating = t is None or t.extent.start.line > comment.next_line
        tokens.unget(t)

        directive, argument, clang_cursor_kind = "", "", ""
        namespace = ()

        if not explicitly_floating:
            if d := get_documentable_declaration(tokens):
                directive, argument, namespace, clang_cursor_kind = d

        if d := comment.get_explicit_directive():
            # explicit directives override those inferred from decls
            directive, argument = d

        if directive:
            comment.clang_cursor_kind = clang_cursor_kind
            directive_comments.append((directive, argument, namespace, comment))
        elif explicitly_floating:
            floating_comments.append(comment)
        else:
            logger.error(
                f"Could not infer directive from {str(path)}:{comment.next_line}"
            )

    return FileContent(
        module,
        directive_comments,
        floating_comments,
        clang_diagnostics=[str(d) for d in tu.diagnostics],
        mtime_when_parsed=path.stat().st_mtime,
    )


@dataclass
class State:
    """A container for all /// content in a project plus tracking metadata"""

    files: dict[Path, FileContent]
    directive_comments: dict[str, dict[str, dict[ModuleName, dict[str, Comment]]]]
    references: dict[str, set[Path]]
    members: dict[tuple[str, ModuleName], dict[tuple[str, str], Comment]]

    @staticmethod
    def empty():
        return State({}, {}, {}, {})

    def __reduce__(self):
        logger.info(f"reducing... {self.references}")
        return State, (self.files, self.directive_comments, self.references)

    def add(self, path: Path, file_content: FileContent):
        self.files[path] = file_content

        for directive, argument, namespace, comment in file_content.directive_comments:
            ns = "::".join(namespace)
            stored = (
                self.directive_comments.setdefault(directive, {})
                .setdefault(ns, {})
                .setdefault(file_content.module, {})
                .setdefault(argument, comment)
            )
            if stored is not comment:
                raise RuntimeError(
                    f"Duplicate /// detected:\n{stored}\n\n  vs\n\n{comment}"
                )
            self.members.setdefault((ns, file_content.module), {})[directive, argument] = comment

    def remove(self, path: Path, file_content: FileContent):
        invalidated = set()
        # Every doc which references this /// source is invalidated
        for docname, referenced_files in self.references.items():
            if path in referenced_files:
                invalidated.add(docname)

        # purge this file's ///s
        module = file_content.module
        for directive, argument, namespace, _ in file_content.directive_comments:
            ns = "::".join(namespace)
            del self.directive_comments[directive][ns][module][argument]
            del self.members[ns, module][directive, argument]
        del self.files[path]
        return invalidated


    def get_comment(
        self,
        directive: str,
        argument: str,
        namespace: str = "",
        module: ModuleName = "",
    ) -> tuple[Comment | None, dict[str, Comment]]:
        comments = (
            self.directive_comments.get(
                directive if directive != "cpp:class" else "cpp:struct", {}
            )
            .get(namespace, {})
            .get(module, {})
        )

        if comment := comments.get(argument, None):
            return comment, {}

        return None, {
            m: comments[m] for m in difflib.get_close_matches(argument, comments.keys())
        }

    def check_for_updates(
        self, paths: list[Path], clang_args: defaultdict[Path, list[str]]
    ) -> set[str]:
        logger.info("trike.State checking for updates")
        invalidated = set()

        for path, file_content in list(self.files.items()):
            if path in paths:
                mtime = path.stat().st_mtime
                if file_content.mtime_when_parsed == mtime:
                    continue
            invalidated |= self.remove(path, file_content)

        for path in paths:
            if path in self.files:
                # Anything outdated has already been purged
                assert self.files[path].mtime_when_parsed == path.stat().st_mtime
                continue
            self.add(path, comment_scan(path, clang_args[path]))
        return invalidated


def _env_get_outdated(
    app: Sphinx,
    env: BuildEnvironment,
    _added: set[str],
    _changed: set[str],
    _removed: set[str],
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
    return env.trike_state.check_for_updates(
        app.config.trike_files,
        defaultdict(
            lambda: app.config.trike_default_clang_args,
            app.config.trike_clang_args.items(),
        ),
    )


def _env_purge_doc(
    _: Sphinx,
    env: BuildEnvironment,
    docname: str,
):
    if docname in env.trike_state.references:
        del env.trike_state.references[docname]


def _env_merge_info(
    _: Sphinx,
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

    # TODO add :members: option

    @contextmanager
    def default_cpp(self):
        tmp = {}
        tmp["highlight_language"] = self.env.temp_data.get("highlight_language", None)
        self.env.temp_data["highlight_language"] = "cpp"
        tmp["default_domain"] = self.env.temp_data.get("default_domain", None)
        self.env.temp_data["default_domain"] = self.env.domains.get("cpp")
        try:
            yield
        finally:
            for key, value in tmp.items():
                if value is None:
                    del self.env.temp_data[key]
                else:
                    self.env.temp_data[key] = value

    def run(self) -> list[Node]:
        directive = self.arguments[0]
        argument = " ".join(filter(lambda arg: arg != "\\", self.arguments[1:]))
        namespace = self.env.temp_data.get("cpp:namespace_stack", [""])[-1]
        module = ""  # TODO get module

        comment, close_matches = self.env.trike_state.get_comment(
            directive, argument, namespace, module
        )
        if comment is not None:
            self.env.trike_state.references.setdefault(self.env.docname, set()).add(
                comment.file
            )
            logger.debug(f"{comment.file} referenced by {self.env.docname}")
            text = StringList(
                [
                    f".. {directive}:: {argument}",
                    # TODO add a link to the decl on GitHub
                    "",
                    *(f"  {line}" for line in self.content),
                    "",
                    *(f"  {line}" for line in comment.stripped_text),
                ]
            )
            with self.default_cpp():
                return self.parse_text_to_nodes(text)

        message = f"found no declaration matching `{argument}`\n"
        message += f"{directive=} {namespace=} {module=}"
        for argument, comment in close_matches.items():
            message += (
                f"\n  close match: `{argument}` {str(comment.file)}:{comment.next_line}"
            )
        logger.error(message)
        return []


def setup(app: Sphinx) -> ExtensionMetadata:
    app.add_config_value(
        "trike_files",
        [],
        "env",
        description="All files which will be scanned for ///",
    )

    app.add_config_value(
        "trike_default_clang_args",
        [],
        "env",
        description="Arguments which will be passed to clang",
    )
    app.add_config_value(
        "trike_clang_args",
        {},
        "env",
        description="Per-file overrides of arguments which will be passed to clang",
    )
    app.connect("env-get-outdated", _env_get_outdated)
    app.connect("env-merge-info", _env_merge_info)
    app.connect("env-purge-doc", _env_purge_doc)

    app.add_directive("trike-put", PutDirective)
    # TODO trike-function etc as a shortcut for trike-put:: cpp:function

    logger.info("trike setup")
    return {
        "version": "0.1",
        "env_version": 1,
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
