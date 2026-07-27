# mariab-plugin-oom-info

![mariabd-plugin-oom-info](logo/oom_info.png)

`OOM_INFO` is a read-only MariaDB `INFORMATION_SCHEMA` plugin. 
It reports the Linux OOM-killer values for the
MariaDB server process at query time.

```sql
INSTALL SONAME 'oom_info';
SELECT * FROM information_schema.OOM_INFO;
```

The table contains one row:

| Column | Meaning |
|---|---|
| `SCORE` | Current kernel OOM badness score (`0..1000`) |
| `ADJ` | Legacy OOM adjustment (`-17..15`), or `NULL` if unavailable |
| `SCORE_ADJ` | Current OOM adjustment (`-1000..1000`), or `NULL` if unavailable |

`SCORE` is the kernel's current assessment of how suitable the MariaDB process
is for termination when the system runs out of memory. A higher value makes
selection more likely relative to other processes.

`ADJ` is the legacy adjustment applied to the OOM score. Positive values make
the process more likely to be selected and negative values make it less likely.
`-17` disables OOM killing for the process. New software should use
`SCORE_ADJ`; `ADJ` is retained because some Linux systems still expose it.

`SCORE_ADJ` is the current adjustment. It ranges from `-1000` (protect the
process from the OOM killer) through `0` (no adjustment) to `1000` (make the
process maximally eligible for selection).

For example:

```text
    SCORE: 802
      ADJ: 3
SCORE_ADJ: 200
```

This reports a high current badness score. Both adjustment values are positive,
so MariaDB has been made more likely to be selected by the OOM killer than it
would be with neutral adjustments. `SCORE` is dynamic and may change as the
process's memory use and system conditions change.

On systems without Linux procfs, the columns are `NULL`.

## Build

Place or symlink this directory as `plugin/oom_info` in a MariaDB Server source
tree, configure MariaDB with `-DPLUGIN_OOM_INFO=DYNAMIC`, and build the
`oom_info` target. Install `oom_info.so` in MariaDB's plugin directory.

The reader accepts only complete, decimal, kernel-range values from fixed procfs
paths. It uses bounded buffers, handles interrupted system calls, does not
follow user-provided paths, and never throws C++ parsing exceptions.
