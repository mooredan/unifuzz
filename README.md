# unifuzz — SQLite Extension for RootsMagic Collations and Functions

`unifuzz` is a SQLite extension that emulates the Windows Unicode collation and string comparison behaviors required by RootsMagic 8/9/10 `.rmtree` genealogy databases.

RootsMagic uses SQLite with custom Unicode collation sequences (notably `RMNOCASE`) and SQL functions that mirror Windows behavior. This extension enables full compatibility on non-Windows platforms by mimicking the expected string handling, case folding, and accent insensitivity.

## ✨ Features

- **RMNOCASE collation**: Emulates Windows behavior (via Wine or native logic).
- Optional override of SQLite's built-in `NOCASE` collation.
- Additional collations: `NAMES`, `UNACCENTED`, `NUMERICS`.
- SQL functions for string normalization, case folding, unaccenting, character handling, and more:
  - `ascii()`, `case()`, `flip()`, `unaccent()`, `proper()`
  - `chrw()`, `space()`, `stripdiacritics()`
- Support for UTF-8 and UTF-16 SQL text encodings.
- Cross-platform: tested on Linux (x86_64), macOS (arm64), and Windows (planned).
- Reindexing support: `REINDEX RMNOCASE;` after loading the extension ensures proper use.

## 🛠️ Building

This project uses a flexible `Makefile` to support:

- Native builds on Linux and macOS.
- Cross-compilation on Linux for macOS (arm64).
- Organized output under `dist/$(PLATFORM_TAG)/`.

### Requirements

- **SQLite development headers**
- **Clang or GCC**
- **Make**
- On Linux, local Wine source code must be present (pared down, see `wine/` directory)

### Example Build Commands

```bash
# Native build (on Linux or macOS)
make

# Cross-compile for macOS arm64 from Linux
make PLATFORM=macos_arm64

# Run tests
make testall

# Package prebuilt libraries for distribution
make publish
```

## 🧪 Testing

The Makefile includes robust tests:

- `make test`: runs unit tests using in-memory SQLite and the `unifuzz` extension.
- `make testdb`: runs integration tests against a sanitized RootsMagic `.rmtree` file (`testdata.rmtree`).
- `make test_chrw`: exercises Unicode character output (via `chrw()`).
- `make testall`: runs all the above.

To test against an actual RootsMagic database:

```bash
cp your_sanitized_file.rmtree testdata.rmtree
make testdb
```

> ⚠️ Warning: Running against real data should be read-only. This extension does not modify the database.

## 📦 Distribution Layout

Prebuilt extensions are placed under `dist/`:

```
dist/
├── linux_x86_64/
│   └── unifuzz.so
├── macos_arm64/
│   └── unifuzz.dylib
```

## 📁 Directory Structure

```
Makefile               # Build and test targets
README.md              # This file
unifuzz.c              # Main extension code
test.sql               # SQL test harness
testdata.rmtree        # Sanitized RM10 test database
wine/                  # Minimal Wine source for collation support
```

## 🧑‍💻 Acknowledgments

- Wine Project — for Unicode string collation logic
- RootsMagic — for using SQLite in a creative, platform-specific way
- Tom Holden — for identifying the need for `RMNOCASE` collation

## 🔒 License

This project is licensed under the same terms as the SQLite Public Domain or the Wine LGPL components where applicable. See `LICENSE` and `COPYING.LIB`.

---

For more information, see comments in `unifuzz.c` and the build targets in `Makefile`.
