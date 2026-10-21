#!/usr/bin/env python3
"""Bounded, serial libFuzzer smoke runs. Inputs are synthetic and private to build/."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs", type=int, default=2000)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    targets = ("block", "model", "roundtrip", "store")
    build = Path("build/fuzz").resolve()
    artifacts = build / "artifacts"
    artifacts.mkdir(exist_ok=True)
    # A fresh corpus avoids stale coverage masking whether the seeds still work.
    with tempfile.TemporaryDirectory(prefix="corpus-", dir=build) as tmp:
        corpus = Path(tmp)
        for target in targets:
            (corpus / target).mkdir()
        subprocess.run([str(build / "seeds"), str(corpus)], check=True)
        for target in targets:
            print(f"== fuzz_{target}: {args.runs} runs, at most 30 seconds", flush=True)
            subprocess.run([
                str(build / f"fuzz_{target}"), str(corpus / target),
                f"-runs={args.runs}", "-seed=20261020", "-max_len=65536",
                "-timeout=5", "-rss_limit_mb=256", "-max_total_time=30",
                "-print_final_stats=1", "-verbosity=0",
                f"-artifact_prefix={artifacts}/",
            ], check=True, timeout=60)


if __name__ == "__main__":
    main()
