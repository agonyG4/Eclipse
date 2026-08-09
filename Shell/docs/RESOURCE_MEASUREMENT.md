# Unified shell resource measurement

`Shell/tools/measure-shell-resources.sh` records a reproducible process-level
sample for the unified shell. It reports RSS, PSS, private clean/dirty memory,
thread count, file-descriptor count, CPU percentage over the requested sample
interval, and the observed number of shared Typhon shell connections.

Run it against a live shell process, for example:

```sh
Shell/tools/measure-shell-resources.sh --pid "$ASTREA_SHELL_PID" \
    --interval 1 --repeat 5 --typhon-connections 1
```

The Typhon connection count is supplied from the runtime observation because a
generic Wayland file descriptor cannot distinguish the shared shell transport
from unrelated display connections. A valid M7-D run has one shared shell
connection, not one connection per Dock, Alt-Tab, or Spotlight surface.

Record the command, build profile, compositor setup, and all output when
comparing a future shell build. Historical resident-daemon measurements are
not available in this checkout, so this harness does not invent a baseline.
