# Earned triumphal arch regression (Augustus #1827)

The production build menu must expose an earned, unplaced triumphal arch even
when the scenario's arch permission is false. This restores the reward rule
used by Julius and repairs the menu in existing affected saves without rewriting
them. Scenario permissions for other buildings and the number of earned arches
remain unchanged. An explicit arch ban also cannot hide an already earned reward;
without an unplaced reward the arch remains hidden.

The 12 assertions exercise the actual `building/menu.c`, `city/buildings.c`,
`scenario/allowed_building.c`, and resource/buffer implementations. Rendering,
unrelated building services, tutorial state, and resource availability are test
boundaries; unrelated service calls abort. The tests cover no reward, permission
on/off, award, placement, demolition, a second reward, exhaustion, other building
restrictions, tutorial restrictions, and unavailable trade resources.

From the repository root (CMake with a C compiler):

```text
cmake -S tests/triumphal_arch -B build/triumphal-arch
cmake --build build/triumphal-arch --config Debug
ctest --test-dir build/triumphal-arch -C Debug --output-on-failure
cmake --build build/triumphal-arch --config Release
ctest --test-dir build/triumphal-arch -C Release --output-on-failure
```

On Windows, use a short build path if MSBuild reaches its path limit. The default
suite does not need game assets, saves, SDL, PowerShell, or a player profile.

## Optional read-only save probe

Configure with `-DARCH_SAVE_PROBE=ON` (PowerShell required), then pass the path of
a **local save copy** to `arch_test`. `-DSOURCE_ROOT=...` selects another source
tree for before/after comparisons. The probe reads the save with complete,
unmodified functions extracted from the production file and city decoders;
it does not duplicate binary offsets. All extracted functions are regenerated
at CMake configure time. Reconfigure when changing the production decoders.

It loads the actual permission array and city reward counters, then exercises the
real menu update. It does not load a full city world, draw a window, play audio,
place a building, or write saves/preferences. Successful results prove decoding
and menu eligibility, not complete gameplay or audiovisual validation.

Evidence from 2026-09-21: the public save attached to issue #1827 (save version
180) and four private Mediolanum saves (version 189) each had arch permission 0,
one earned arch, zero placed, and one distant-battle victory. All five failed
menu eligibility before the fix and passed afterward in Debug and Release.
The synthetic suite failed three assertions before and passed all 12 afterward.

Private saves and downloaded campaign maps are not included in this suite or in
upstream contributions. For the public reproduction, see
https://github.com/Keriew/augustus/issues/1827. Retain the downloaded ZIP hash:
`14206402CDF0E91AB64525A31A30A13D19D0059D3E238E362781E897F72D2A67`.

Keep these tests and the optional probe in Claudius. The upstream contribution
contains only the change in `src/building/menu.c`, following the workspace policy.
