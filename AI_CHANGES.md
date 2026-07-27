# AI Changes

Changes made by an AI assistant during a code review pass.

- **Date:** 2026-07-28
- **Model:** Claude Sonnet 5 (`claude-sonnet-5`)

## Summary

Reviewed the plugin's source (`oom_info.cc`), build file (`CMakeLists.txt`),
README, and test suite, and cross-checked the `Show::Column`/`ST_FIELD_INFO`
API and plugin-declaration macros against the MariaDB server source tree.
Two issues were found and fixed.

## Changes

### 1. `CMakeLists.txt` — guard the build against non-Linux platforms

The plugin unconditionally includes `<fcntl.h>` and `<unistd.h>` and calls
POSIX `open()`/`read()`/`close()` to read `/proc/self/oom_score*`. Since
`MYSQL_ADD_PLUGIN` defaults to building a plugin (`DYNAMIC`) on every
platform unless it explicitly opts out, this would break a Windows build of
MariaDB if this plugin directory were present under `plugin/`. Added an
`IF(WIN32) RETURN() ENDIF()` guard, matching the pattern used by other
POSIX-only in-tree plugins (e.g. `plugin/auth_socket`).

### 2. `oom_info.cc` — stop misusing `Show::Column`'s legacy-name parameter

Each column was declared as
`Show::Column("SCORE", Show::SLonglong(4), NULLABLE, "Current Linux OOM badness score")`.
The 4th `const char *` parameter of `Column` is bound to
`ST_FIELD_INFO::old_name`, the legacy `SHOW`-command-compatible column
alias (e.g. `"Field"`/`"Null"`/`"Type"` for `SHOW COLUMNS`) — there is no
free-text comment/description field anywhere in this API. As a result, the
descriptive sentences never reached a user anywhere (not in `DESCRIBE`, not
in `INFORMATION_SCHEMA.COLUMNS.COLUMN_COMMENT`), and it had the unintended
side effect of causing `finalize_schema_table()` to mark the table as
needing legacy-`SHOW` compatibility handling it doesn't need (harmless in
practice only because `OOM_INFO` isn't wired into any built-in legacy `SHOW`
statement).

Removed the 4th argument from all three `Show::Column` calls and moved the
descriptive text into a `//` comment above each field instead.
