"""Minimal SAIR ACC API client. The key is read from $SAIR_API_KEY or ~/.sair_key (never stored in the repo).

  python3 sair.py me                      team eligibility
  python3 sair.py spec                    submission spec (limits, batch size)
  python3 sair.py snapshot                download both official snapshots -> snapshots/, rebuild records.json
  python3 sair.py submit FILE [--yes]     dry run by default; --yes actually submits (uses 1 unit of daily quota)
  python3 sair.py status SUBMISSION_ID    poll one submission until complete/failed
  python3 sair.py mine                    team submission history
  python3 sair.py leaderboard [ac|stable_ac]
"""
import json, os, sys, time, urllib.request, urllib.error
BASE = 'https://api.sair.foundation/api/public/v1/competitions/acc'

def key():
    k = os.environ.get('SAIR_API_KEY') or (open(os.path.expanduser('~/.sair_key')).read().strip() if os.path.exists(os.path.expanduser('~/.sair_key')) else '')
    if not k: sys.exit('no API key: set SAIR_API_KEY or create ~/.sair_key')
    return k

def call(path, method='GET', body=None):
    req = urllib.request.Request(BASE + path, method=method, data=json.dumps(body).encode() if body is not None else None,
                                 headers={'Authorization': f'Bearer {key()}', 'Content-Type': 'application/json', 'Accept': 'application/json', 'User-Agent': 'acsolver/1.0 (+team tooling)'})
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return r.status, json.loads(r.read() or b'{}'), dict(r.headers)
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read() or b'{}'), dict(e.headers)

def show(x): print(json.dumps(x, indent=1)[:4000])

cmd = sys.argv[1] if len(sys.argv) > 1 else ''
if cmd == 'me': show(call('/me')[1])
elif cmd == 'spec': show(call('/submission-spec')[1])
elif cmd == 'mine': show(call('/submissions/mine')[1])
elif cmd == 'leaderboard':
    show(call(f'/leaderboard?problem={sys.argv[2] if len(sys.argv) > 2 else "ac"}')[1])
elif cmd == 'snapshot':
    os.makedirs('snapshots', exist_ok=True); stamp = time.strftime('%Y%m%d-%H%M')
    snaps = {}
    for prob in ('ac', 'stable_ac'):
        code, data, _ = call(f'/discoveries/snapshot?problem={prob}')
        if code != 200: show(data); sys.exit(f'snapshot {prob} failed: HTTP {code}')
        json.dump(data, open(f'snapshots/{prob}_{stamp}.json', 'w'))
        items = data.get('data', {}).get('items', [])
        print(prob, len(items), 'items; example item:', json.dumps(items[0])[:400] if items else None)
        snaps[prob] = items
    # records.json: 'ac-NNNNN' -> [ac_best, ac_k, sac_best, sac_k]. Field names checked on first run.
    def fields(it):
        L = it.get('currentBestLength')
        k = it.get('kTeams', it.get('k'))
        return L, k
    rec = {}
    for it in snaps['ac']:
        L, k = fields(it); rec[it['challengeId']] = [L, k or 0, None, 0]
    for it in snaps['stable_ac']:
        L, k = fields(it); i = 'ac-' + it['challengeId'].split('-')[1]
        rec.setdefault(i, [None, 0, None, 0])[2:] = [L, k or 0]
    if not any(v[0] for v in rec.values()): sys.exit('could not find the length field; inspect snapshots/ and adjust fields()')
    json.dump(rec, open('records.json', 'w')); print('records.json rebuilt from official snapshot', stamp)
elif cmd == 'submit':
    f = sys.argv[2]; text = open(f, encoding='utf-8').read()
    n = sum(1 for l in text.splitlines() if l.strip() and not l.lstrip().startswith('#'))
    print(f'{f}: {n} solutions, {len(text.encode())} bytes')
    if '--yes' not in sys.argv: sys.exit('dry run only; add --yes to submit (uses 1 unit of the daily quota)')
    code, data, h = call('/submissions', 'POST', {'payload': {'text': text}, 'meta': {'description': 'acsolver'}})
    show(data)
    if code == 202:
        sid = data['data']['submissionId']; open('submissions.log', 'a').write(f'{time.ctime()} {f} {sid}\n'); print('submission id', sid)
elif cmd == 'status':
    sid = sys.argv[2]
    while True:
        code, data, h = call(f'/submissions/{sid}')
        st = data.get('data', {}).get('status'); print('status', st)
        if st in ('complete', 'failed') or code != 200: show(data); break
        time.sleep(int(h.get('Retry-After', 15)))
else: print(__doc__)
