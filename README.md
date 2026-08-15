# kopio

An app for manga archival workflows.
kopio scans a manga library, finds the files that only waste space, and lets
you triage them in a thumbnail grid before moving them to the trash:

- **Scanlation credit pages** — the same group banner/splash repeated in every
  chapter of a series (found by exact hashing, perceptual hashing, and a
  frequency-across-chapters rule).
- **Mihon leftovers** — `.nomedia` markers and `ComicInfo.xml` metadata files
  that serve no purpose after transferring chapters to a server.
- **Blank and padding pages** — near-empty pages, title-only pages, wide
  banners, square logos.

Pages that *look* suspicious but are often worth keeping — translation notes,
joke/context pages, anything that looks like a cover or color spread — are
never auto-flagged for deletion. They land in a **Needs review** list instead,
labeled with the series and chapter they belong to.

## Usage

Launch `kopio`, pick your library folder, and confirm. After the scan you get
a grid of flagged files:

- **Click** a thumbnail to check/uncheck it.
- **Ctrl+click** to enlarge it in a preview with its own action buttons.
- Filter chips (`All flagged · Junk files · Exact dupes · Junk pages · Needs
  review · Allowed · Clean`) switch between categories; sort by file contents
  (duplicate clusters end up adjacent), name, or size.
- Actions: **Select all / Deselect all**, **Allow** (keep the file and stop
  flagging it), **To review**, and **Move to Trash…** (always the system
  trash, never a hard delete).

Tips:

- `kopio /path/to/library` starts scanning right away.
- `KOPIO_DRY_RUN=1 kopio` logs what would be trashed without touching files.
- `kopio-scan --classify /path/to/library` is a headless CLI that prints the
  full analysis as TSV — handy for inspecting verdicts or tuning thresholds.

## How it decides

All image analysis is plain C (no Python, no OpenCV, no AI): header sniffers,
XXH3-128 content hashes, a 64-bit dHash, and cheap pixel metrics (whiteness,
ink, edge density, unique colors, chroma). The strongest signal is
*frequency*: a near-identical image appearing across many chapters of the same
series is a credit page, no matter how much it looks like a cover. Color pages
with normal manga proportions are explicitly protected from the heuristics to
avoid flagging covers and color spreads.

## Installing

Grab a build from the GitHub releases page: `kopio-x86_64.AppImage` for any
Linux distro (`chmod +x` and run), or `kopio-windows-x64.zip` for Windows
(unzip and run `kopio.exe`). Releases are built automatically when a `v*` tag
is pushed. To install from source: build as below, then `cmake --install
build` (binaries, desktop entry, and icon).

## Building

Requires CMake ≥ 3.21, a C11/C++17 compiler, and Qt 6 (Widgets, Gui,
Concurrent; the `qwebp`/`qgif` image format plugins for webp/gif support).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/kopio
```

Run the tests with `ctest --test-dir build`. `tests/make-fixture.sh` builds a
throwaway `Title/Chapter` tree in `/tmp/kopio-fixture` from the sample corpus
for end-to-end testing — use it (not your real library) when trying the
delete flow.

## Portability

Linux/KDE Plasma is the primary target (the UI follows Breeze/Gwenview
conventions), but nothing is Plasma-specific: the analysis core is pure C11
with no POSIX or path handling, all platform surface (directory walking, file
dialogs, trash, image decoding) goes through Qt, and trashing uses
`QFile::moveToTrash` (FreeDesktop trash on Linux, Recycle Bin on Windows).
For a Windows build, ship the `imageformats/qwebp.dll` and `qgif.dll` plugins
with `windeployqt`.

## License

MIT — see [LICENSE](LICENSE).
