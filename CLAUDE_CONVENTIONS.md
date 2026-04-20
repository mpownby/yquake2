# CLAUDE_CONVENTIONS.md

Coding conventions and preferences for new code in this repo. Read this
before adding any new files.

## Engine code (existing C in `src/`)

The engine is plain C (mostly C99) with the original Quake 2 / Yamagi
style: `snake_case_functions`, lowercase filenames, headers in `header/`
subdirectories. Don't refactor for taste — only change what the task
requires. Don't introduce dependencies on C++ from engine code; the
renderer DLLs and the dedicated server build must stay pure-C reachable.

Additions to the engine should be **gated by a cvar** so they're inert
when not in use. The MCP server, for example, does nothing unless
`mcp_server_port` is non-zero. This keeps the upstream-merge story
simple: a fresh checkout of yquake2 still works as plain yquake2.

## New utilities (`tools/*`)

New standalone tools (the bsp2rbx converter is the first) are written in
**C++17** with **GoogleTest + GoogleMock** for testing. No vendoring —
GoogleTest is pulled in via CMake `FetchContent`.

Why C++ for tools and not the engine: tooling has different ergonomics
needs (containers, file I/O, RAII) and doesn't have to clear the C ABI
hurdles the engine does. The engine stays C; tools that don't link with
the engine can use whatever fits.

## Dependency Inversion is a first-class concern

For all new C++ code in this repo, **interfaces injected into constructors**
is the default architecture, not the exception.

1. **Interface per collaborator.** If class `A`'s method calls class `B`'s
   non-trivial method, declare `IB` (pure virtual abstract class), have
   `A`'s constructor accept `std::shared_ptr<IB>`, and have a concrete
   `B : public IB` for production wiring.
2. **Constructor injection only.** No service locators, no globals, no
   `new B()` inside `A`. Every dependency is passed in.
3. **Wire in `main()`.** `main()` parses CLI args and constructs the
   concrete graph, then hands off. `main()` is the only function exempt
   from unit testing.
4. **Almost every method has its own unit test.** If a test grows long or
   has to reason about a collaborator's behavior, that's a signal the
   collaborator should have been extracted behind an interface.

**Why:** When a large method calls another large method inside the same
class, every unit test for the outer method is forced to reason about the
inner method's behavior too — the result is long, brittle tests that drift
into integration territory. "Large" here is a complexity judgment, not a
line-count threshold: the bar is whether the two methods together make it
effectively impossible to write short, simple, concise unit tests.

## Mocking with GoogleMock

Each interface has a matching `Mock<Name>` class in `tests/mocks/` using
`MOCK_METHOD`. Tests use `EXPECT_CALL` to assert specific call patterns.

**Avoid wildcard matchers.** Don't use `testing::_` or other
"match anything" matchers in `EXPECT_CALL` arguments. Specify the exact
expected value with `Eq(x)`, `Pointee(...)`, `Field(&S::f, 3.0f)`, etc.

**The one exception:** `_` is acceptable for arguments that are lambdas or
callbacks, since those have no meaningful equality.

## Comments

Default to writing no comments. Add one only when the *why* is non-obvious:
a hidden constraint, a subtle invariant, a workaround for a specific bug,
behavior that would surprise a reader. Don't explain *what* the code does
— well-named identifiers do that. Don't reference the current task, fix,
or callers ("used by X", "added for the Y flow", "handles the case from
issue #123") — those belong in the PR description and rot as the codebase
evolves.

## Cross-platform / Windows specifics

This machine builds with MSVC + the bundled CMake 3.31 from VS 2022. The
system CMake is too old (3.20.2). Windows-specific code uses `winsock2.h`
guarded by `#ifdef _WIN32`; POSIX paths use `<sys/socket.h>` etc. Look at
[src/common/mcp_server.c](src/common/mcp_server.c) for the established
pattern (small per-platform abstraction at the top of the file rather
than separate per-platform sources for tiny chunks).

## Process & approvals

- Don't make commits unless explicitly asked.
- Don't push, force-push, or create PRs unless explicitly asked.
- Don't add features beyond what was requested. A bug fix doesn't need
  surrounding cleanup; a one-shot operation doesn't need a helper.
- For UI-visible changes, manually verify in a browser / running app
  before reporting "done".
