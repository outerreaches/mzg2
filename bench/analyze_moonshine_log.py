#!/usr/bin/env python3
"""Analyze moonshine-server output.log: per-request prefill/decode/reuse summary."""
import re, sys
from collections import defaultdict

if len(sys.argv) < 2:
    raise SystemExit(f"usage: {sys.argv[0]} <moonshine-server log file>")
path = sys.argv[1]
lines = open(path, errors='replace').read().splitlines()
print(f'total lines: {len(lines)}')

kv = lambda s: dict(re.findall(r'(\w+)=(".*?"|\S+)', s))

reqs = defaultdict(dict)
warns, infos = [], []
for ln in lines:
    m = re.match(r'(\S+)\s+(INFO|WARN|ERROR)\s+(\S+)\s+(.*)', ln)
    if not m:
        continue
    ts, lvl, ev, rest = m.groups()
    f = kv(rest)
    rid = f.get('id')
    if lvl in ('WARN', 'ERROR'):
        warns.append((ts, ev, rest[:180]))
    if rid:
        r = reqs[rid]
        if ev == 'request.start':
            r['start'] = ts; r['msgs'] = f.get('messages'); r['max_out'] = f.get('max_output')
            r['stream'] = f.get('stream'); r['reasoning'] = f.get('reasoning'); r['tools'] = f.get('tools')
        elif ev == 'request.prefill.start':
            r['prompt'] = int(f.get('prompt', 0)); r['reused'] = int(f.get('reused', 0))
            r['reuse'] = f.get('reuse'); r['pstart'] = ts; r['strategy'] = f.get('strategy')
        elif ev == 'request.prefill.progress':
            r['plast'] = ts; r['pdone'] = int(f.get('completed', 0)); r['pelapsed'] = float(f.get('elapsed', '0').rstrip('s'))
        elif ev.startswith('request.prefill') and ev != 'request.prefill.progress':
            r.setdefault('p_events', []).append((ts, ev, rest[:160]))
        elif ev.startswith('request.decode') or 'decode' in ev:
            r.setdefault('d_events', []).append((ts, ev, rest[:200]))
        else:
            r.setdefault('events', []).append((ts, ev, rest[:200]))

print(f'requests: {len(reqs)}')
for i, (rid, r) in enumerate(sorted(reqs.items(), key=lambda x: x[1].get('start', ''))):
    print(f"\n--- [{i+1}] {rid}  start={r.get('start')}")
    print(f"    msgs={r.get('msgs')} max_out={r.get('max_out')} stream={r.get('stream')} reasoning={r.get('reasoning')} tools={r.get('tools')}")
    if 'prompt' in r:
        p, reused = r['prompt'], r.get('reused', 0)
        eff = p - reused
        print(f"    prompt={p} reused={reused} ({100*reused/max(p,1):.1f}%) new={eff} reuse={r.get('reuse')} strategy={r.get('strategy')}")
        if 'pelapsed' in r:
            rate = r['pdone'] / r['pelapsed'] if r['pelapsed'] else 0
            print(f"    prefill progress: {r['pdone']}/{p} in {r['pelapsed']:.0f}s ({rate:.1f} tok/s)")
    for e in r.get('p_events', []): print(f"    {e[0]} {e[1]}: {e[2]}")
    for e in r.get('d_events', [])[:6]: print(f"    {e[0]} {e[1]}: {e[2]}")
    if len(r.get('d_events', [])) > 6: print(f"    ... +{len(r['d_events'])-6} more decode events")
    for e in r.get('events', [])[:10]: print(f"    {e[0]} {e[1]}: {e[2]}")

print(f"\n=== WARN/ERROR ({len(warns)}) ===")
for ts, ev, rest in warns:
    print(f'{ts} {ev}: {rest}')

print('\n=== event type counts ===')
counts = defaultdict(int)
for ln in lines:
    m = re.match(r'\S+\s+(?:INFO|WARN|ERROR)\s+(\S+)', ln)
    if m: counts[m.group(1)] += 1
for ev, c in sorted(counts.items()):
    print(f'{c:5d}  {ev}')
