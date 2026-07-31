#!/usr/bin/env python3
"""Drive a running planet simulator.

The simulator listens on a loopback port when started with -control <port>.
This sends it commands and prints the replies.

    planetsimulator.exe -control 8765
    python tools/planetctl.py status
    python tools/planetctl.py "find deepest-valley"
    python tools/planetctl.py --script inspect.txt

Most of what goes wrong in this simulation only goes wrong over time, so the
useful shape is almost always: put the camera somewhere definite, advance a
known amount of geological time, and measure the same thing again.

    python tools/planetctl.py --watch stats --advance 0.5 --steps 20

writes a CSV of one row per sample, which is the form a bug shows up in.
"""

import argparse
import json
import socket
import sys
import time


class Planet:
    def __init__(self, host="127.0.0.1", port=8765, timeout=600.0):
        self.sock = socket.create_connection((host, port), timeout=10.0)
        self.sock.settimeout(timeout)
        self.buffer = b""

    def send(self, command):
        """One command, one reply. Blocks until the simulator has answered,
        which for `sim advance` means until the geology has actually happened."""
        self.sock.sendall(command.strip().encode() + b"\n")
        while b"\n" not in self.buffer:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("simulator closed the connection")
            self.buffer += chunk
        line, _, self.buffer = self.buffer.partition(b"\n")
        text = line.decode().strip()
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return {"ok": False, "raw": text}

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def watch(planet, what, advance, steps, out):
    """Sample one command repeatedly with a known amount of time between.

    The advance is geological time, not wall clock, so the rows are evenly
    spaced in the only units that matter and the result does not depend on how
    fast the machine happened to be.
    """
    rows = []
    for step in range(steps + 1):
        if step > 0:
            reply = planet.send(f"sim advance {advance}")
            if not reply.get("ok", False):
                print(f"advance failed: {reply}", file=sys.stderr)
                break
        sample = planet.send(what)
        if not sample.get("ok", False):
            print(f"sample failed: {sample}", file=sys.stderr)
            break
        rows.append(sample)
        print(f"  {step}/{steps}  t={sample.get('simulationTime', 0):.3f} My",
              file=sys.stderr)

    if not rows:
        return

    columns = [k for k in rows[0].keys() if k != "ok"]
    handle = open(out, "w", encoding="utf-8") if out else sys.stdout
    try:
        handle.write(",".join(columns) + "\n")
        for row in rows:
            handle.write(",".join(str(row.get(c, "")) for c in columns) + "\n")
    finally:
        if out:
            handle.close()
            print(f"wrote {len(rows)} rows to {out}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", nargs="*", help="command to send")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--script", help="file of commands, one per line")
    parser.add_argument("--watch", help="command to sample repeatedly")
    parser.add_argument("--advance", type=float, default=0.5,
                        help="million years between samples")
    parser.add_argument("--steps", type=int, default=10)
    parser.add_argument("--out", help="CSV output path for --watch")
    args = parser.parse_args()

    try:
        planet = Planet(args.host, args.port)
    except OSError as error:
        print(f"cannot reach the simulator on {args.host}:{args.port} ({error})",
              file=sys.stderr)
        print("is it running with -control?", file=sys.stderr)
        return 1

    try:
        if args.watch:
            watch(planet, args.watch, args.advance, args.steps, args.out)
        elif args.script:
            with open(args.script, encoding="utf-8") as handle:
                for line in handle:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    print(f"> {line}", file=sys.stderr)
                    print(json.dumps(planet.send(line)))
        elif args.command:
            print(json.dumps(planet.send(" ".join(args.command)), indent=2))
        else:
            # Interactive, because the first thing anyone does with a tool like
            # this when it misbehaves is try it by hand.
            print("connected; blank line or ctrl-d to leave", file=sys.stderr)
            for line in sys.stdin:
                line = line.strip()
                if not line:
                    break
                print(json.dumps(planet.send(line)))
    finally:
        planet.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
