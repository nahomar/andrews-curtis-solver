"""Stage 2: turn the component runs (data/classify.json) into Sigma-AC classes of the open MS instances.

Two open instances are put in one class iff an explicit replayed path joins one to a Sigma-image of the
other (union-find over all watch hits of all runs).  For each class we report the certified cap: for
every member Y, the Sigma-quotient component of Y in G_cap was exhausted and does not contain (x, y).
Writes data/classes.json and data/classes.md.
"""
import collections, json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ms

def main():
    db = json.load(open(os.path.join(ROOT, 'data', 'classify.json')))
    rows = {('ms%04d' % r['seq']): r for r in ms.load()}
    open_ids = [k for k, r in rows.items() if r['status'] == 'open']
    parent = {k: k for k in list(rows) + ['AK3', 'BM_P1', 'TRIV']}
    def find(a):
        while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
        return a
    cap_of = {}; alarms = []
    for rid, run in db['runs'].items():
        if run['status'].startswith('TRIVIAL'): alarms.append(('TRIVIAL', rid))
        for h in run['hits']:
            parent[find(h['id'])] = find(rid)
            cap_of[h['id']] = max(cap_of.get(h['id'], 0), run['cap'])
            if h['id'] in rows and rows[h['id']]['status'] != 'open':
                alarms.append((rows[h['id']]['status'], rid, h['id']))
    classes = collections.defaultdict(list)
    for k in open_ids: classes[find(k)].append(k)
    out = []
    for root, mem in classes.items():
        extra = sorted(k for k in parent if k not in rows or rows[k]['status'] != 'open' if find(k) == root)
        labels = collections.Counter(rows[m]['cls'] for m in mem)
        caps = [cap_of.get(m) for m in mem]
        out.append(dict(members=sorted(mem, key=lambda m: (rows[m]['L'], m)), size=len(mem),
                        min_len=min(rows[m]['L'] for m in mem),
                        cert_cap=min(c for c in caps) if None not in caps else None,
                        also_contains=extra, reported_labels=dict(labels)))
    out.sort(key=lambda c: (c['min_len'], -c['size']))
    # label consistency: how are Two-Hump labels distributed over our classes?
    lab = collections.defaultdict(set)
    for i, c in enumerate(out):
        for m in c['members']: lab[rows[m]['cls']].add(i)
    split = {l: sorted(v) for l, v in lab.items() if len(v) > 1}
    summary = dict(open_instances=len(open_ids), classes=len(out),
                   uncovered=[k for k in open_ids if k not in cap_of],
                   reported_labels=len(lab), labels_split_across_our_classes=split,
                   alarms=alarms,
                   cap_histogram=dict(collections.Counter(c['cert_cap'] for c in out)))
    json.dump(dict(summary=summary, classes=out), open(os.path.join(ROOT, 'data', 'classes.json'), 'w'), indent=1)
    with open(os.path.join(ROOT, 'data', 'classes.md'), 'w') as f:
        f.write('| # | size | min L | cert. cap | reported labels | members (first 8) | also contains |\n|---|---|---|---|---|---|---|\n')
        for i, c in enumerate(out):
            f.write(f"| {i} | {c['size']} | {c['min_len']} | {c['cert_cap']} | {', '.join(f'{k}×{v}' for k, v in sorted(c['reported_labels'].items()))} | "
                    f"{' '.join(c['members'][:8])}{' …' if c['size'] > 8 else ''} | {' '.join(c['also_contains'])} |\n")
    print(json.dumps(summary, indent=1)[:3000])

if __name__ == '__main__':
    main()
