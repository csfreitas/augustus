# Empire city localization tests

Standalone CMake/CTest suite using production empire/campaign/message overlay
parsers, campaign lifecycle, locale resolver, folder/ZIP loading and text encoding.
No game assets, installation, profiles, saves or external test framework are needed.

From the repository root in a C compiler environment:

```sh
cmake -S tests/empire_localization -B build-empire-localization -DCMAKE_BUILD_TYPE=Debug
cmake --build build-empire-localization --config Debug
ctest --test-dir build-empire-localization -C Debug --output-on-failure -V
```

Repeat with Release. For MSVC AddressSanitizer, use RelWithDebInfo with
`-DCMAKE_C_FLAGS=/fsanitize=address -DCMAKE_EXE_LINKER_FLAGS=/INCREMENTAL:NO`.

Fixtures are generated in a new directory under the build working directory on
each run, both as unpacked campaigns and real ZIP `.campaign` files. Expected
result: zero failures.

The existing XML tokenizer requires explicit opening/closing root tags even for
an empty overlay; a self-closing root is rejected and falls back to source names.

Coverage includes exact object ID/source guards, fallback pointer identity,
invalid XML, duplicate cities/names (including empty names), decimal ID limits
and overflow on 32-bit long platforms, long localized names, missing overlays,
locale aliases/defaults/reload, multiple scenario extensions, and inactive,
original, editor and suspended-campaign behavior. French, Russian, Greek and
Japanese strings use the real encoding converters. Interleaving campaign,
message and empire overlay loading checks that their stored display text remains
independent and canonical campaign state is unchanged.

Adapters provide language settings, filesystem roots, editor/scenario selection,
message UID/count, emperor rank, progression storage, original-campaign setup and
scenario execution. Original missions and the editor itself are not loaded.
The suite does not link the complete empire city model or game-file/UI hooks;
it tests the localization API and explicit lifecycle calls. Actual city/save
serialization, UI ordering/rendering, glyphs, audio, full gameplay and translation
quality still require game-level validation.
