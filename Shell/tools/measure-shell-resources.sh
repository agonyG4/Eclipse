#!/usr/bin/env bash
set -euo pipefail

pids=()
interval=1
repeat=1
typhon_connections=0

usage() {
    echo "usage: $0 --pid PID [--interval SECONDS] [--repeat COUNT] [--typhon-connections N]" >&2
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    --pid)
        [[ $# -ge 2 ]] || usage
        pids+=("$2")
        shift 2
        ;;
    --interval)
        [[ $# -ge 2 ]] || usage
        interval=$2
        shift 2
        ;;
    --repeat)
        [[ $# -ge 2 ]] || usage
        repeat=$2
        shift 2
        ;;
    --typhon-connections)
        [[ $# -ge 2 ]] || usage
        typhon_connections=$2
        shift 2
        ;;
    -h|--help)
        usage
        ;;
    *)
        usage
        ;;
    esac
done

[[ "${#pids[@]}" -gt 0 ]] || usage
[[ "$repeat" =~ ^[1-9][0-9]*$ ]] || usage
[[ "$typhon_connections" =~ ^[0-9]+$ ]] || usage
[[ "$interval" =~ ^[0-9]+([.][0-9]+)?$ ]] || usage
for pid in "${pids[@]}"; do
    [[ "$pid" =~ ^[0-9]+$ && "$pid" -gt 0 ]] || usage
    [[ -r "/proc/$pid/status" ]] || { echo "process does not exist: $pid" >&2; exit 1; }
done

sample_cpu_ticks() {
    local sample_pid=$1 stat_line rest
    read -r stat_line < "/proc/$sample_pid/stat"
    rest=${stat_line##*) }
    awk '{print $12 + $13}' <<< "$rest"
}

sample_uptime() {
    awk '{print $1}' /proc/uptime
}

read_metric() {
    local file=$1 pattern=$2
    awk -v pattern="$pattern" '$0 ~ pattern { print $2; found=1; exit } END { if (!found) print 0 }' "$file"
}

echo "process_count rss_kib pss_kib private_clean_kib private_dirty_kib private_kib threads fds cpu_percent typhon_shell_connections"
for ((sample = 1; sample <= repeat; ++sample)); do
    rss=0
    pss=0
    private_clean=0
    private_dirty=0
    threads=0
    fds=0
    start_ticks=0
    declare -A start_ticks_by_pid=()
    for pid in "${pids[@]}"; do
        status_file="/proc/$pid/status"
        rollup_file="/proc/$pid/smaps_rollup"
        [[ -r "$status_file" ]] || { echo "process exited during measurement: $pid" >&2; exit 1; }
        rss=$((rss + $(read_metric "$status_file" '^VmRSS:')))
        threads=$((threads + $(read_metric "$status_file" '^Threads:')))
        pss=$((pss + $(read_metric "$rollup_file" '^Pss:')))
        private_clean=$((private_clean + $(read_metric "$rollup_file" '^Private_Clean:')))
        private_dirty=$((private_dirty + $(read_metric "$rollup_file" '^Private_Dirty:')))
        fds=$((fds + $(find "/proc/$pid/fd" -mindepth 1 -maxdepth 1 -type l 2>/dev/null | wc -l)))
        start_ticks_by_pid[$pid]=$(sample_cpu_ticks "$pid")
    done
    start_uptime=$(sample_uptime)
    sleep "$interval"
    end_ticks=0
    for pid in "${pids[@]}"; do
        end_ticks=$((end_ticks + $(sample_cpu_ticks "$pid")))
        start_ticks=$((start_ticks + start_ticks_by_pid[$pid]))
    done
    end_uptime=$(sample_uptime)
    cpu_percent=$(awk -v ticks="$((end_ticks - start_ticks))" \
        -v start="$start_uptime" -v end="$end_uptime" \
        -v hz="$(getconf CLK_TCK)" \
        'BEGIN { seconds=end-start; if (seconds <= 0) seconds=0.000001; printf "%.2f", ticks / hz / seconds * 100 }')

    private=$((private_clean + private_dirty))
    printf '%s %s %s %s %s %s %s %s %s %s\n' \
        "${#pids[@]}" "$rss" "$pss" "$private_clean" "$private_dirty" "$private" \
        "$threads" "$fds" "$cpu_percent" "$typhon_connections"
done
