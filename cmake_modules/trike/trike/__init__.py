from dataclasses import dataclass
from pathlib import Path

import sphinx.util.logging
import docutils.nodes
import docutils.statemachine
import difflib

from clang.cindex import (
    Cursor,
    CursorKind,
    Index,
    SourceLocation,
    TokenKind,
    TranslationUnit,
)
from sphinx.application import Sphinx
from sphinx.environment import BuildEnvironment

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


@dataclass
class Comment:
    """The content of an individual ///"""

    file: Path
    line: int
    lines: list[str]
    clang_cursor_kind: str = ""


@dataclass
class DeclarationContext:
    directive: str
    namespace: str
    module: str

    def __hash__(self):
        return hash((self.directive, self.namespace, self.module))


@dataclass
class FileContent:
    """A container for all /// content in a file"""

    module: str
    comments: list[tuple[DeclarationContext | None, str, Comment]]
    diagnostics: list[str]
    mtime_when_parsed: float


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
            #   /// we only see "[[preconditions"
            #   [[preconditions{ this->foo == 3 }]] int Foo::get_three() const
            #
            # However this doesn't seem critical to support, particularly since
            # if these constructions are necessary it should be sufficient to
            # override the automatic declaration string.
            break

        if t.spelling in {"class", "struct", "export"}:
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


DOCUMENTATION_COMMENT = "///"


def comment_scan(file: Path, tu: TranslationUnit, contents: str) -> FileContent:
    module = ""  # TODO detect modules
    comments = []
    current_comment = []
    for t in tu.get_tokens(extent=tu.cursor.extent):
        if t.kind == TokenKind.COMMENT:
            line = t.spelling
            if not line.startswith(DOCUMENTATION_COMMENT):
                continue

            # TODO detect ///.. explicit:directive::

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
            context, declaration = None, ""
            clang_cursor_kind = ""

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
            clang_cursor_kind = str(t.cursor.kind).removeprefix("CursorKind.")
            if clang_cursor_kind == "MACRO_DEFINITION":
                directive = "c:macro"
            elif "STRUCT" in clang_cursor_kind or "CLASS" in clang_cursor_kind:
                # Classes and structs are stored together because libclang uses
                # CLASS_TEMPLATE for struct templates. We decide whether to use
                # cpp:struct or cpp:class based on the referencing directive.
                directive = "cpp:struct"
            elif "VAR" in clang_cursor_kind:
                directive = "cpp:var"
            else:
                logger.info(f"UNKNOWN decl kind {clang_cursor_kind}")
                directive = ""
            context = DeclarationContext(directive, namespace, module)

        comments.append(
            (
                context,
                declaration,
                Comment(
                    file,
                    line=current_comment_end.line + 1,
                    lines=current_comment,
                    clang_cursor_kind=clang_cursor_kind,
                ),
            )
        )
        current_comment = []

    return FileContent(
        module,
        comments,
        diagnostics=[str(d) for d in tu.diagnostics],
        mtime_when_parsed=file.stat().st_mtime,
    )


@dataclass
class State:
    """A container for all /// content in a project plus tracking metadata"""

    files: dict[Path, FileContent]
    declaration_comments: dict[DeclarationContext, dict[str, Comment]]
    updated_builders: list[str]

    @staticmethod
    def empty():
        """
        We *don't* want to `note_dependency("foo.h")` because that would
        rebuild for any change to foo.h (including a change which didn't
        alter /// content)... On the other hand, most changes will at least
        modify line numbers.

        We *do* want to store the mtime of foo.h because that will enable
        us to skip parsing it if it hasn't been updated... but the stored
        mtime should be allowed to change without triggering rebuild.
        """
        return State({}, {}, [])

    def purge(self, outdated: FileContent):
        for context, declaration, _ in outdated.comments:
            if context is None:
                continue

            del self.declaration_comments[context][declaration]
            if not self.declaration_comments[context]:
                del self.declaration_comments[context]

    def check_for_updates(self, app: Sphinx) -> int:
        logger.info("trike.State checking for updates")

        mtimes = {
            path: path.stat().st_mtime for path in map(Path, app.config.trike_files)
        }
        old_mtimes = {path: file.mtime_when_parsed for path, file in self.files.items()}

        needs_purge = [path for path in old_mtimes.keys() if path not in mtimes]
        needs_parse = [
            path
            for path, mtime in mtimes.items()
            if mtime > old_mtimes.get(path, mtime - 1)
        ]

        update_count = len(needs_purge)
        for path in needs_purge:
            self.purge(self.files[path])
            del self.files[path]

        # TODO parse in parallel
        for path in needs_parse:
            index = Index.create()
            source = path.read_text()
            tu = index.parse(
                path.name,
                args=app.config.trike_clang_args.get(
                    path, app.config.trike_default_clang_args
                ),
                unsaved_files=[
                    (path.name, source),
                ],
                options=(
                    TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
                    | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
                    | TranslationUnit.PARSE_INCOMPLETE
                ),
            )
            file_content = comment_scan(path, tu, source)
            if old_file_content := self.files.get(path, None):
                if file_content == old_file_content:
                    logger.info(f"trike.State {path}'s /// content was unchanged")
                    continue
                self.purge(old_file_content)
            self.files[path] = file_content

            logger.info(f"trike.State {path}'s /// content differed")
            update_count += 1

            for context, declaration, comment in file_content.comments:
                if context is None:
                    continue

                stored = self.declaration_comments.setdefault(context, {}).setdefault(
                    declaration, comment
                )
                if stored is not comment:
                    raise RuntimeError(
                        f"Duplicate /// detected:\n{stored}\n\n  vs\n\n{comment}"
                    )

        return update_count


def _env_get_outdated(
    app: Sphinx,
    env: BuildEnvironment,
    added: set[str],
    changed: set[str],
    removed: set[str],
) -> list[str]:
    logger.info("trike.State handled in env-get-outdated")

    if not hasattr(env, "trike_state"):
        env.trike_state = State.empty()

    update_count = env.trike_state.check_for_updates(app)

    if update_count > 0:
        env.trike_state.updated_builders = [app.builder.name]
    elif app.builder.name not in env.trike_state.updated_builders:
        env.trike_state.updated_builders.append(app.builder.name)
    else:
        return []

    # Either the /// content has been updated during this build or
    # it has been updated since this builder last ran. In either
    # case all files which might've referenced /// content are outdated.
    #
    # TODO we could be more fine-grained here; if we maintain a list of
    # files which never use trike's directives then we can skip regenerating
    # their outputs. We might even be able to avoid regenerating sources
    # if the /// content which is actually referenced didn't change.
    return list(env.all_docs.keys())


class PutDirective(sphinx.util.docutils.SphinxDirective):
    has_content = True
    required_arguments = 2
    optional_arguments = 1000

    def run(self) -> list[docutils.nodes.Node]:
        # TODO add a link to the decl on GitHub
        comments = self.env.trike_state.declaration_comments
        namespace = self.env.temp_data.get("cpp:namespace_stack", [""])[-1]
        # TODO get module
        module = ""

        self.arguments = list(filter(lambda arg: arg != "\\", self.arguments))
        directive = self.arguments[0]
        declaration = " ".join(self.arguments[1:])
        context = DeclarationContext(
            directive if directive != "cpp:class" else "cpp:struct",
            namespace,
            module,
        )
        declarations = comments.get(context, {})
        if comment := declarations.get(declaration, None):
            text = docutils.statemachine.StringList(
                [
                    f".. {directive}:: {declaration}",
                    "",
                    *(f"  {line}" for line in self.content),
                    "",
                    *(f"  {line}" for line in comment.lines),
                ]
            )
            return self.parse_text_to_nodes(text)

        message = f"found no declaration matching `{declaration}`\n  {context=}"
        for m in difflib.get_close_matches(declaration, declarations.keys()):
            message += f"\n  close match: `{m}`"
        raise ValueError(message)


def setup(app: Sphinx):
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
