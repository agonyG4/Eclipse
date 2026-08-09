#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
temporary_dir=$(mktemp -d)
trap 'rm -rf -- "$temporary_dir"' EXIT

unit_dir="$temporary_dir/units"
log_file="$temporary_dir/systemctl.log"
mkdir -p -- "$unit_dir"
touch -- \
    "$unit_dir/astrea-dock.service" \
    "$unit_dir/astrea-alt-tabd.service" \
    "$unit_dir/astrea-spotlightd.service" \
    "$unit_dir/astrea-shell.service"

run_migration() {
    ASTREA_SYSTEMCTL="$script_dir/test-systemd-fake-systemctl" \
    ASTREA_TEST_SYSTEMCTL_LOG="$log_file" \
    ASTREA_USER_UNIT_DIR="$unit_dir" \
    "$script_dir/astrea-migrate-shell-systemd"
}

run_migration
for unit in astrea-dock.service astrea-alt-tabd.service astrea-spotlightd.service; do
    if [[ -e "$unit_dir/$unit" ]]; then
        echo "legacy unit was not retired: $unit" >&2
        exit 1
    fi
done
test -e "$unit_dir/astrea-shell.service"
grep -Fx -- "--user daemon-reload" "$log_file" >/dev/null
grep -Fx -- "--user enable --now astrea-shell.service" "$log_file" >/dev/null

run_migration
[[ $(grep -Fc -- "--user enable --now astrea-shell.service" "$log_file") -eq 2 ]]
[[ $(grep -Fc -- "--user disable --now astrea-dock.service" "$log_file") -eq 2 ]]
[[ $(grep -Fc -- "--user disable --now astrea-alt-tabd.service" "$log_file") -eq 2 ]]
[[ $(grep -Fc -- "--user disable --now astrea-spotlightd.service" "$log_file") -eq 2 ]]

echo "systemd migration: fresh, upgrade, and idempotent reruns passed"
