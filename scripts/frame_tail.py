#!/usr/bin/env python3
"""Per-retrace wall time from a BLUEWAKE_FRAME_TIMING log, and its tail.

Why this exists. NFR-001 gates *rendered* frame time at p95 <= 36.7 ms and p99 <=
50 ms, and every frame-time number in the ledger so far has been a mean, a median or
a p90: the tail a player actually feels has never been quoted, let alone attributed.
The host already stamps CLOCK_MONOTONIC at each VI retrace delivery, so a retrace's
cost is the gap between two consecutive stamps - this reads them back, reports the
distribution against the two gates, and names the retraces that own the tail, which
is the input the next instrument needs to say what makes them slow.

A gap is only counted when the two stamps are consecutive retraces: a batch delivery
or a dropped line would otherwise read as one enormous frame.

Usage: scripts/frame_tail.py LOG [--window LOW:HIGH] [--worst N]
"""

import re
import sys

STAMP = re.compile(r"^\[frame-timing\] retrace=(\d+) us=(\d+)$")


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    path = args[0]
    window = (0, 1 << 40)
    worst_n = 10
    i = 1
    while i < len(args):
        if args[i] == "--window" and i + 1 < len(args):
            low, high = args[i + 1].split(":")
            window = (int(low), int(high))
            i += 2
        elif args[i] == "--worst" and i + 1 < len(args):
            worst_n = int(args[i + 1])
            i += 2
        else:
            print(__doc__)
            return 2

    stamps = []
    try:
        handle = open(path, errors="replace")
    except OSError as exc:
        sys.exit("frame_tail: %s" % exc)
    for line in handle:
        match = STAMP.match(line.rstrip("\n"))
        if match:
            stamps.append((int(match.group(1)), int(match.group(2))))
    handle.close()

    deltas = []
    for (first_retrace, first_us), (second_retrace, second_us) in zip(stamps, stamps[1:]):
        if second_retrace - first_retrace != 1:
            continue
        if not window[0] <= second_retrace <= window[1]:
            continue
        deltas.append((second_us - first_us, second_retrace))
    if not deltas:
        sys.exit("frame_tail: no consecutive frame stamps in window %d:%d" % window)

    times = sorted(delta for delta, _ in deltas)
    count = len(times)

    def percentile(percent):
        return times[min(count - 1, int(round(percent / 100.0 * (count - 1))))]

    mean = sum(times) / count
    print("%s: %d retraces in %d:%d" % (path, count, window[0], window[1]))
    print("  mean %.2f ms   median %.2f   p90 %.2f   p95 %.2f   p99 %.2f   max %.2f"
          % (mean / 1000.0, percentile(50) / 1000.0, percentile(90) / 1000.0,
             percentile(95) / 1000.0, percentile(99) / 1000.0, times[-1] / 1000.0))
    p95 = percentile(95)
    p99 = percentile(99)
    print("  NFR-001 rendered gates p95 <= 36.7 ms and p99 <= 50 ms: p95 %s, p99 %s"
          % ("ok" if p95 <= 36700 else "over by %.2f ms" % ((p95 - 36700) / 1000.0),
             "ok" if p99 <= 50000 else "over by %.2f ms" % ((p99 - 50000) / 1000.0)))
    print("  worst %d:" % worst_n)
    for delta, retrace in sorted(deltas, reverse=True)[:worst_n]:
        print("    retrace %d  %.2f ms" % (retrace, delta / 1000.0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

