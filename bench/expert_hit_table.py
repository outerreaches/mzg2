#!/usr/bin/env python3
"""Per-request expert cache hit-rate table from moonshine log."""
import re, sys

if len(sys.argv) < 2:
    raise SystemExit(f"usage: {sys.argv[0]} <moonshine-server log file>")
path = sys.argv[1]
kv = lambda s: dict(re.findall(r'(\w+)=(".*?"|\S+)', s))

print(f"{'req':>4} {'ctx':>7} {'reuse%':>7} {'gen':>5} {'decode_t/s':>10} {'read_GiB':>9} {'expert_hit%':>11} {'hits/total':>22} {'GiB/gen_tok':>11}")
tot_h = tot_t = final_h = final_t = 0
for ln in open(path, errors='replace'):
    if 'request.complete' not in ln:
        continue
    f = kv(ln)
    rid = f.get('id', '?')
    n = rid.rsplit('-', 1)[-1]
    prompt = int(f.get('prompt', 0)); reused = int(f.get('reused', 0))
    gen = int(f.get('generated', 0))
    rate = f.get('rate', '?')
    read = f.get('read', '?')
    cache = f.get('cache', '')
    cumulative = f.get('cache_total', '')
    hit_pct = ''
    if '/' in cache:
        h, t = map(int, cache.split('/'))
        tot_h += h; tot_t += t
        hit_pct = f"{100*h/t:.1f}"
    if '/' in cumulative:
        final_h, final_t = map(int, cumulative.split('/'))
    gb_per_tok = ''
    if read.endswith('_GiB') and gen:
        gb_per_tok = f"{float(read[:-4])/gen:.3f}"
    reuse = f"{100*reused/max(prompt,1):.1f}"
    print(f"{n:>4} {prompt:>7} {reuse:>7} {gen:>5} {rate.replace('_tok/s',''):>10} {read.replace('_GiB',''):>9} {hit_pct:>11} {cache:>22} {gb_per_tok:>11}")

if tot_t:
    if final_t and (tot_h, tot_t) != (final_h, final_t):
        raise SystemExit(
            f"cache delta/cumulative mismatch: {tot_h}/{tot_t} != "
            f"{final_h}/{final_t}")
    print(f"\nTOTAL expert cache: {tot_h}/{tot_t} = {100*tot_h/tot_t:.2f}% hit")
