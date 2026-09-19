# XML parser depth boundary regression

Standalone CMake/CTest target linking the production XML parser and tokenizer.
No SDL, game installation, campaign data, external framework, or network is needed.

```sh
cmake -S tests/xml_parser_depth -B build-xml-depth -DCMAKE_BUILD_TYPE=Debug
cmake --build build-xml-depth --config Debug
ctest --test-dir build-xml-depth -C Debug --output-on-failure -V
```

Repeat with Release and AddressSanitizer. The tests exercise both text append
and empty-element completion with a one-entry root schema, nesting to exactly
three entries, sibling text, text around children, reset/reuse, malformed-input
recovery, incomplete input, and repeated free/reinitialization. Comparisons remain
active in optimized builds and callbacks verify the current element's text.

The regression is an off-by-one in text storage: XML depth is one-based, whereas
the allocated text array (like the parent array) is zero-based. A root-only schema
with text and a three-entry schema nested three levels both reached one slot past
the allocation before the fix; an empty root also exposed the completion path.

For a before/after comparison without replacing production source, pass
`-DXML_PARSER_SOURCE=/absolute/path/to/previous/xml_parser.c` to a separate build.
The earlier parser is expected to fail under AddressSanitizer; the fixed parser
must pass all three CTest cases. On MSVC, use `/fsanitize=address` in
`CMAKE_C_FLAGS` and `/INCREMENTAL:NO` in `CMAKE_EXE_LINKER_FLAGS`.

The empty-root case uses an explicit opening/closing pair. It also records the
existing rejection of a sole self-closing root (tokenizer BUFFERDRY); the boundary
fix does not change that separate behavior.

Expected result: three CTest cases and 51 checks, with no failures. When running
MSVC AddressSanitizer tests, make its matching runtime DLL available to every
child process (for example, beside the test executable), not only the compiler
shell. A Windows loader error such as 0xc0000135 is not a parser test result.

The suite does not broaden the parser's supported XML schema, validate recursive
schemas, or measure allocations. Existing cleanup tests complement these boundary
tests. UI rendering, save compatibility, and full gameplay remain separate checks.
