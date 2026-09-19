# Claudius integration

Claudius is an independently maintained integration branch of
[Augustus](https://github.com/Keriew/augustus), itself based on Julius.
The project retains upstream attribution and the license in `LICENSE.txt`.
It requires the user's Caesar III game data; this branch does not bundle
proprietary game files, campaign packages, or translated audio assets.

## Initial composition — 2026-09-11

Base: Augustus `95e120d80babd55e93a6e5d755a4971ddbb4b2a5`.

| Feature | Source | Integration |
| --- | --- | --- |
| Custom campaign message localization | PR #1893, `cbb7b795` | Inherited through the media branch |
| Localized campaign media and safe fanfare-to-speech cancellation | `feature/custom-campaign-localized-media-overlays`, `c14f509b` | Integration base |
| SDL2/SDL3 audio buffer ownership | PR #1908, `b05dacdb` | Ownership fix inherited from media; missing SDL3 channel cleanup on shutdown added to match the full PR |
| Brazilian Portuguese font asset layout | PR #1906, `366e29b7` | Applied on top of media |
| Content-aware UI layout | PR #1907, `c5e1c625` | Applied on top of media and fonts |

Campaign localization and responsive layout remain language-agnostic.
Font asset detection supports the Brazilian game assets without requiring them
for other installations. Text and media retain their canonical fallbacks.
See [campaign localization](custom_campaign_localization.md) for package details.

## Campaign metadata integration — 2026-09-19

The local continuation of Augustus PR #1893 adds display-only campaign names
and descriptions, mission titles, and scenario names/descriptions through
`localization/<locale>/campaign.xml`. The campaign picker also displays localized
names in its left list without changing the active campaign or saved progress.
The original filenames and campaign identity remain canonical.

This integration preserves Claudius media overlays, font handling and responsive
dialogs. Text and metadata share locale resolution; media companions stay bound
to the exact locale that provided the valid message overlay. No campaign
translations or audio assets are bundled by this change.

The accepted upstream XML cleanup fix from PR #1921 (`277fabe7`) is included.
It fixes pending text cleanup on parser reset/free; it is independent of the
metadata feature. The existing SDL3 music fix from PR #1920 remains unchanged.

Standalone folder/ZIP suites cover metadata and previews (106 cases, 824 checks)
and message/media lookup (110 cases, 1148 checks). See the
[metadata test guide](../tests/campaign_localization/README.md) and
[message/media test guide](../tests/custom_messages_localization/README.md).
These tests do not establish visual quality, audio playback, save serialization
or full gameplay compatibility. Integrated campaign content, empire city-name
localization, package/installer work and final mission QA remain separate gates.

## Branch policy

- `master` tracks Augustus upstream without Claudius-specific changes.
- Feature/PR branches stay independent and reviewable upstream.
- `claudius` contains the integrated feature stack and Claudius documentation/CI.
- Diagnostic and historical QA branches are not release sources.
- Approval of an upstream PR is not required to integrate a validated feature
  into Claudius. Continue responding to upstream reviews separately.

Before updating, inspect local modifications and remote branch changes, fetch
upstream, and preserve a local backup reference of the current integration.
Rebase `claudius` onto the current upstream `master`; do not use Sync fork or
create merge commits on this branch. Resolve overlaps by preserving the intended
feature behavior. When a feature lands upstream, compare implementations before
dropping duplicate patches, including features that upstream squash-merges.
Use an explicit `--force-with-lease` expectation when publishing rewritten history.

Check the resulting patch series, build results, and affected runtime behavior
after each update. A clean rebase is not proof of functional compatibility.

## Validation and delivery gates

1. Build the integrated branch using the inherited platform matrix, including
   SDL2 and SDL3, and run CodeQL on `claudius`.
2. Validate campaign text/media fallback, missing or invalid companions,
   closing messages during fanfares, glyph rendering, and long dialog text.
   Cover directory and `.campaign` layouts and `.mapx`/`.svx` scenarios.
3. Validate campaign metadata in the integrated candidate and prepare its
   package/installer. Validate Steam, GOG, and CD-ROM installations actually
   available for testing; do not claim untested distribution coverage.
4. Deliver one playable candidate before requesting the user's detailed
   translation QA across the 20 Reconquered missions.
5. Apply final fixes and publish a stable release only after those gates pass.

The initial branch creation is not a stable release or a completed runtime QA.
The first integrated build is tracked in
[GitHub Actions run 34562410079](https://github.com/csfreitas/augustus/actions/runs/34562410079).
CodeQL has been enabled for this fork and is configured to analyze pushes to
`claudius`. Check completed run results before distributing a candidate.
CI artifacts are development builds. They retain upstream binary names and
settings paths for now; isolate the user directory when testing.
Do not point Claudius users to upstream downloads as if they contained these changes.

Future releases should use immutable `claudius-v...` tags and clearly identify
the upstream base, included patches, and tested platforms. Never rewrite a
published release tag when rebasing the development branch. Keep localized
campaign content separately packaged and document its compatible engine version.
