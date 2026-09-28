// acs: Andrews-Curtis solver for the SAIR ACC Discovery track (rank-2 AC and Stable AC).
//
//   acs ball    --cap C [--stable] --out FILE          exact distance table for all states with total length <= C
//   acs solve   --problems F --ball FILE --out F.jsonl beam search into the ball, then shortcut post-optimisation
//   acs shorten --problems F --paths F.jsonl --out F.jsonl   post-optimise existing paths only
//
// Letters 0=x 1=X 2=y 3=Y (inverse = ^1). Move ids 0-13 follow the official ac-r2-v1 spec.
// Problem file lines: `id a b c | d e f` with generators 1=x 2=y, negative = inverse.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std;
constexpr int MAXT = 160;
static const int INV_MOVE[14] = {0, 1, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12};

struct St {
    uint8_t n0 = 0, n1 = 0;
    uint8_t c[MAXT];
    int tot() const { return n0 + n1; }
    const uint8_t* r0() const { return c; }
    const uint8_t* r1() const { return c + n0; }
    bool operator==(const St& o) const { return n0 == o.n0 && n1 == o.n1 && !memcmp(c, o.c, tot()); }
};

// ---------------------------------------------------------------- moves
static inline int inv_word(const uint8_t* a, int n, uint8_t* out) {
    for (int i = 0; i < n; i++) out[i] = a[n - 1 - i] ^ 1;
    return n;
}
static inline int concat(const uint8_t* a, int na, const uint8_t* b, int nb, uint8_t* out) {
    int i = na, j = 0;
    while (i > 0 && j < nb && (a[i - 1] ^ 1) == b[j]) i--, j++;
    int n = i + nb - j;
    if (n > MAXT) return -1;
    memcpy(out, a, i); memcpy(out + i, b + j, nb - j);
    return n;
}
static inline int conj(const uint8_t* w, int n, int c, uint8_t* out) {  // c w c^-1, freely reduced
    if (n == 0) return 0;
    int k = 0;
    if (w[0] == (c ^ 1)) { memcpy(out, w + 1, n - 1); k = n - 1; }
    else { if (n + 1 > MAXT) return -1; out[0] = c; memcpy(out + 1, w, n); k = n + 1; }
    if (k && out[k - 1] == c) return k - 1;
    if (k + 1 > MAXT) return -1;
    out[k] = c ^ 1;
    return k + 1;
}
// Applies move m. Returns false if the result has an empty relator or exceeds `cap` letters in total.
static bool apply(const St& s, int m, St& o, int cap) {
    uint8_t w[MAXT + 2], t[MAXT];
    int n;
    const uint8_t *r0 = s.r0(), *r1 = s.r1();
    int a = s.n0, b = s.n1;
    bool first = (m == 0 || m == 2 || m == 3 || (m >= 6 && m <= 9));
    switch (m) {
        case 0: n = inv_word(r0, a, w); break;
        case 1: n = inv_word(r1, b, w); break;
        case 2: n = concat(r0, a, r1, b, w); break;
        case 3: inv_word(r1, b, t); n = concat(r0, a, t, b, w); break;
        case 4: n = concat(r1, b, r0, a, w); break;
        case 5: inv_word(r0, a, t); n = concat(r1, b, t, a, w); break;
        default: n = m < 10 ? conj(r0, a, m - 6, w) : conj(r1, b, m - 10, w);
    }
    if (n <= 0) return false;
    int other = first ? b : a;
    if (n + other > cap || n + other > MAXT) return false;
    if (first) { o.n0 = n; o.n1 = b; memcpy(o.c, w, n); memcpy(o.c + n, r1, b); }
    else { o.n0 = a; o.n1 = n; memcpy(o.c, r0, a); memcpy(o.c + a, w, n); }
    return true;
}
static inline int cyc_len(const uint8_t* w, int n) {
    int i = 0, j = n - 1;
    while (i < j && (w[i] ^ 1) == w[j]) i++, j--;
    return j - i + 1;
}
static inline uint64_t mix(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 27; x *= 0x94d049bb133111ebULL; return x ^ (x >> 31);
}
static uint64_t hst(const St& s) {
    uint64_t h = mix(s.n0 * 1000003ULL + s.n1);
    int n = s.tot();
    for (int i = 0; i < n; i += 8) {
        uint64_t v = 0; memcpy(&v, s.c + i, min(8, n - i));
        h = mix(h ^ v);
    }
    return h;
}
static St make(const vector<int>& a, const vector<int>& b) {
    St s; s.n0 = a.size(); s.n1 = b.size();
    for (size_t i = 0; i < a.size(); i++) s.c[i] = (abs(a[i]) - 1) * 2 + (a[i] < 0);
    for (size_t i = 0; i < b.size(); i++) s.c[a.size() + i] = (abs(b[i]) - 1) * 2 + (b[i] < 0);
    return s;
}

// Open-addressing set of 64-bit hashes (0 is reserved as empty; hashes are forced nonzero).
struct FlatSet {
    vector<uint64_t> t; size_t n = 0;
    FlatSet(size_t cap = 1 << 16) { size_t c = 16; while (c < cap * 2) c <<= 1; t.assign(c, 0); }
    bool insert(uint64_t h) {   // true if new
        h |= 1;
        if ((n + 1) * 2 > t.size()) { vector<uint64_t> o; o.swap(t); t.assign(o.size() * 2, 0); n = 0; for (uint64_t v : o) if (v) insert(v); }
        size_t m = t.size() - 1, i = mix(h) & m;
        while (t[i]) { if (t[i] == h) return false; i = (i + 1) & m; }
        t[i] = h; n++; return true;
    }
    bool contains(uint64_t h) const {
        h |= 1; size_t m = t.size() - 1, i = mix(h) & m;
        while (t[i]) { if (t[i] == h) return true; i = (i + 1) & m; }
        return false;
    }
};

// ---------------------------------------------------------------- ball
// Key: n0 (5 bits) | n1 (5 bits) | 2 bits per letter. Valid for total length <= 27.
static inline uint64_t pack(const St& s) {
    uint64_t k = s.n0 | (uint64_t)s.n1 << 5;
    for (int i = 0; i < s.tot(); i++) k |= (uint64_t)s.c[i] << (10 + 2 * i);
    return k;
}
static inline St unpack(uint64_t k) {
    St s; s.n0 = k & 31; s.n1 = (k >> 5) & 31;
    for (int i = 0; i < s.tot(); i++) s.c[i] = (k >> (10 + 2 * i)) & 3;
    return s;
}
struct Ball {
    int cap = 0; bool stable = false;
    vector<uint64_t> keys; vector<uint8_t> dist; uint64_t mask = 0, count = 0;
    void init(uint64_t capacity) { keys.assign(capacity, 0); dist.assign(capacity, 0); mask = capacity - 1; count = 0; }
    inline uint64_t slot(uint64_t k) const {
        uint64_t i = mix(k) & mask;
        while (keys[i] && keys[i] != k) i = (i + 1) & mask;
        return i;
    }
    bool insert(uint64_t k, uint8_t d) {   // false if present
        if ((count + 1) * 10 > keys.size() * 7) grow();
        uint64_t i = slot(k);
        if (keys[i]) return false;
        keys[i] = k; dist[i] = d; count++;
        return true;
    }
    void grow() {
        vector<uint64_t> ok; vector<uint8_t> od; ok.swap(keys); od.swap(dist);
        init(ok.size() * 2);
        for (size_t i = 0; i < ok.size(); i++) if (ok[i]) { uint64_t j = slot(ok[i]); keys[j] = ok[i]; dist[j] = od[i]; count++; }
    }
    int get(const St& s) const {
        if (s.tot() > cap) return -1;
        uint64_t i = slot(pack(s));
        return keys[i] ? dist[i] : -1;
    }
    void save(const string& f) {
        FILE* fp = fopen(f.c_str(), "wb");
        uint64_t hdr[4] = {(uint64_t)cap, (uint64_t)stable, keys.size(), count};
        fwrite(hdr, 8, 4, fp); fwrite(keys.data(), 8, keys.size(), fp); fwrite(dist.data(), 1, dist.size(), fp); fclose(fp);
    }
    bool load(const string& f) {
        FILE* fp = fopen(f.c_str(), "rb"); if (!fp) return false;
        uint64_t hdr[4]; if (fread(hdr, 8, 4, fp) != 4) return false;
        cap = hdr[0]; stable = hdr[1]; keys.resize(hdr[2]); dist.resize(hdr[2]); mask = hdr[2] - 1; count = hdr[3];
        bool ok = fread(keys.data(), 8, keys.size(), fp) == keys.size() && fread(dist.data(), 1, dist.size(), fp) == dist.size();
        fclose(fp); return ok;
    }
};

// Endpoints. AC: (x, y). Stable AC: any signed permutation of (x, y); the official finish is
// "invert negative relators, then destabilise from the highest index": [0?][1?] 16 15, cost 2 + #negative.
struct Source { St s; vector<int> suffix; };
static vector<Source> sources(bool stable) {
    vector<Source> out;
    for (int swap_ = 0; swap_ < (stable ? 2 : 1); swap_++)
        for (int n0 = 0; n0 < (stable ? 2 : 1); n0++)
            for (int n1 = 0; n1 < (stable ? 2 : 1); n1++) {
                int a = swap_ ? 2 : 1, b = swap_ ? 1 : 2;
                Source src{make({n0 ? -a : a}, {n1 ? -b : b}), {}};
                if (stable) { if (n0) src.suffix.push_back(0); if (n1) src.suffix.push_back(1); src.suffix.push_back(16); src.suffix.push_back(15); }
                out.push_back(src);
            }
    return out;
}

static void build_ball(int cap, bool stable, const string& out) {
    if (cap > 27) { fprintf(stderr, "cap must be <= 27\n"); exit(1); }
    auto t0 = chrono::steady_clock::now();
    Ball B; B.cap = cap; B.stable = stable; B.init(1 << 20);
    map<int, vector<uint64_t>> src;
    for (auto& s : sources(stable)) src[s.suffix.size()].push_back(pack(s.s));
    vector<uint64_t> fr, nx;
    for (int d = 0; d < 255; d++) {
        if (src.count(d)) for (uint64_t k : src[d]) if (B.insert(k, d)) fr.push_back(k);
        if (fr.empty() && src.upper_bound(d) == src.end()) break;
        nx.clear();
        for (uint64_t k : fr) {
            St s = unpack(k), t;
            for (int m = 0; m < 14; m++)
                if (apply(s, m, t, cap)) { uint64_t q = pack(t); if (B.insert(q, d + 1)) nx.push_back(q); }
        }
        fr.swap(nx);
        double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        fprintf(stderr, "  d=%d frontier=%zu total=%llu %.0fs\n", d + 1, fr.size(), (unsigned long long)B.count, el);
        if (fr.empty() && src.upper_bound(d) == src.end()) break;
    }
    B.save(out);
    fprintf(stderr, "ball cap=%d stable=%d states=%llu slots=%zu -> %s\n", cap, stable, (unsigned long long)B.count, B.keys.size(), out.c_str());
}

// Moves from a ball state to the endpoint, following exact distances.
static bool descend(const Ball& B, const vector<Source>& srcs, St s, vector<int>& moves) {
    int d = B.get(s);
    while (d >= 0) {
        for (auto& src : srcs) if (src.s == s && (int)src.suffix.size() == d) { moves.insert(moves.end(), src.suffix.begin(), src.suffix.end()); return true; }
        if (d == 0) return false;
        bool step = false; St t;
        for (int m = 0; m < 14 && !step; m++)
            if (apply(s, m, t, B.cap) && B.get(t) == d - 1) { moves.push_back(m); s = t; d--; step = true; }
        if (!step) return false;
    }
    return false;
}

// ---------------------------------------------------------------- replay / verification
// Replays the AC part (moves 0-13) and checks the endpoint; Stable AC suffixes are checked against the sources.
static bool verify(const St& start, const vector<int>& moves, bool stable, vector<St>* states = nullptr) {
    St s = start, t;
    size_t k = 0;
    if (states) { states->clear(); states->push_back(s); }
    for (; k < moves.size() && moves[k] < 14; k++) {
        // The stable finish begins with inversions (0/1) of a signed permutation; stop there if the rest is the suffix.
        if (stable) {
            bool is_suffix = false;
            for (auto& src : sources(true)) if (src.s == s && vector<int>(moves.begin() + k, moves.end()) == src.suffix) is_suffix = true;
            if (is_suffix) break;
        }
        if (!apply(s, moves[k], t, MAXT)) return false;
        s = t;
        if (states) states->push_back(s);
    }
    vector<int> rest(moves.begin() + k, moves.end());
    for (auto& src : sources(stable)) if (src.s == s && rest == src.suffix) return true;
    return false;
}

// ---------------------------------------------------------------- shortcut post-optimisation
// Meet in the middle between path states: radius-r BFS balls around every s_j register the best (j - d2)
// per state; a radius-r BFS from s_i finding u with (j - d2) - (i + d1) > 0 is a strict shortcut.
static void bfs_hashes(const St& s, int r, int cap, vector<pair<uint64_t, int>>& out, const Ball* B = nullptr, int* best_ball = nullptr) {
    vector<St> cur{s}, nxt; FlatSet seen(4096); seen.insert(hst(s));
    out.push_back({hst(s), 0});
    if (B && best_ball) { int d = B->get(s); if (d >= 0) *best_ball = min(*best_ball, d); }
    for (int dep = 1; dep <= r; dep++) {
        nxt.clear(); St t;
        for (auto& u : cur) for (int m = 0; m < 14; m++) if (apply(u, m, t, cap)) {
            uint64_t h = hst(t);
            if (!seen.insert(h)) continue;
            out.push_back({h, dep}); nxt.push_back(t);
            if (B && best_ball) { int d = B->get(t); if (d >= 0) *best_ball = min(*best_ball, dep + d); }
        }
        cur.swap(nxt);
    }
}
// Moves from s to the state with hash `target` within radius r (BFS with parents).
static vector<int> bfs_path(const St& s, int r, int cap, uint64_t target) {
    vector<St> nodes{s}; vector<int> par{-1}, dep{0}; vector<int8_t> mv{-1}; FlatSet seen(4096); seen.insert(hst(s));
    int hit = hst(s) == target ? 0 : -1;
    for (size_t q = 0; q < nodes.size() && hit < 0; q++) {
        if (dep[q] == r) continue;
        St t;
        for (int m = 0; m < 14 && hit < 0; m++) if (apply(nodes[q], m, t, cap)) {
            uint64_t h = hst(t);
            if (!seen.insert(h)) continue;
            nodes.push_back(t); par.push_back(q); dep.push_back(dep[q] + 1); mv.push_back(m);
            if (h == target) hit = nodes.size() - 1;
        }
    }
    vector<int> p; if (hit < 0) return p;
    for (int q = hit; par[q] >= 0; q = par[q]) p.push_back(mv[q]);
    reverse(p.begin(), p.end()); return p;
}
// Moves from s into the ball and down to the endpoint, total <= budget, via radius-r BFS.
static vector<int> bfs_to_ball(const St& s, int r, int cap, const Ball& B, const vector<Source>& srcs, int want) {
    vector<St> nodes{s}; vector<int> par{-1}, dep{0}; vector<int8_t> mv{-1}; FlatSet seen(4096); seen.insert(hst(s));
    for (size_t q = 0; q < nodes.size(); q++) {
        int d = B.get(nodes[q]);
        if (d >= 0 && dep[q] + d == want) {
            vector<int> p; for (int k = q; par[k] >= 0; k = par[k]) p.push_back(mv[k]);
            reverse(p.begin(), p.end());
            vector<int> fin; if (descend(B, srcs, nodes[q], fin)) { p.insert(p.end(), fin.begin(), fin.end()); return p; }
        }
        if (dep[q] == r) continue;
        St t;
        for (int m = 0; m < 14; m++) if (apply(nodes[q], m, t, cap) && seen.insert(hst(t))) {
            nodes.push_back(t); par.push_back(q); dep.push_back(dep[q] + 1); mv.push_back(m);
        }
    }
    return {};
}
// Shortcut post-optimisation. Each round finds the single best strict improvement among
//  (a) meet in the middle between path states s_i -> u <- s_j (radius r on both sides), and
//  (b) s_i -> u with u in the exact ball, then the exact finish,
// applies it, re-verifies, and repeats until nothing improves.
static vector<int> shorten(const St& start, vector<int> moves, bool stable, int r, int slack, const Ball* B = nullptr, const vector<Source>* srcs = nullptr) {
    for (int round = 0; round < 500; round++) {
        vector<St> st;
        if (!verify(start, moves, stable, &st)) return moves;
        int n = st.size() - 1;
        vector<int> suffix(moves.begin() + n, moves.end());
        int total_len = moves.size();
        int cap = 0; for (auto& x : st) cap = max(cap, x.tot()); cap = min(MAXT, cap + slack);
        vector<pair<uint64_t, int>> fw;               // (hash, j - d2), kept at max per hash after sort
        {
            vector<pair<uint64_t, int>> tmp;
            for (int j = 0; j <= n; j++) {
                size_t k0 = tmp.size(); bfs_hashes(st[j], r, cap, tmp);
                for (size_t k = k0; k < tmp.size(); k++) tmp[k].second = j * 256 + (255 - tmp[k].second);   // encode j and d2
            }
            sort(tmp.begin(), tmp.end(), [](auto& x, auto& y) { return x.first != y.first ? x.first < y.first : (x.second / 256 - (255 - x.second % 256)) > (y.second / 256 - (255 - y.second % 256)); });
            for (auto& e : tmp) if (fw.empty() || fw.back().first != e.first) fw.push_back(e);
        }
        int gain = 0, bi = -1, bj = -1, ball_i = -1, ball_len = 0; uint64_t bh = 0;
        vector<pair<uint64_t, int>> tmp;
        for (int i = 0; i < n; i++) {
            tmp.clear(); int bb = INT32_MAX;
            bfs_hashes(st[i], r, cap, tmp, B, &bb);
            if (B && bb < INT32_MAX) {                  // finish via the ball: i + bb + suffix-equivalent (descend includes suffix)
                int g = (total_len) - (i + bb);
                if (g > gain) { gain = g; ball_i = i; ball_len = bb; bi = -1; }
            }
            for (auto& [h, d1] : tmp) {
                auto it = lower_bound(fw.begin(), fw.end(), make_pair(h, INT32_MIN));
                if (it == fw.end() || it->first != h) continue;
                int j = it->second / 256, d2 = 255 - it->second % 256;
                if (j <= i) continue;
                int g = (j - i) - (d1 + d2);
                if (g > gain) { gain = g; bi = i; bj = j; bh = h; ball_i = -1; }
            }
        }
        if (gain <= 0) return moves;
        vector<int> nm;
        if (ball_i >= 0) {
            vector<int> tail = bfs_to_ball(st[ball_i], r, cap, *B, *srcs, ball_len);
            if (tail.empty()) return moves;
            nm.assign(moves.begin(), moves.begin() + ball_i); nm.insert(nm.end(), tail.begin(), tail.end());
        } else {
            vector<int> p1 = bfs_path(st[bi], r, cap, bh), p2 = bfs_path(st[bj], r, cap, bh);
            nm.assign(moves.begin(), moves.begin() + bi);
            nm.insert(nm.end(), p1.begin(), p1.end());
            for (int k = p2.size() - 1; k >= 0; k--) nm.push_back(INV_MOVE[p2[k]]);
            nm.insert(nm.end(), moves.begin() + bj, moves.begin() + n);
            nm.insert(nm.end(), suffix.begin(), suffix.end());
        }
        if (nm.size() >= moves.size() || !verify(start, nm, stable)) return moves;
        moves.swap(nm);
    }
    return moves;
}


// ---------------------------------------------------------------- cross-path splicing
// All known paths of one instance become one graph: path steps are edges of cost 1, and states of any two
// paths that meet within radius r of a common state u get an edge of cost d1 + d2. Nodes inside the ball
// get an edge to the endpoint at their exact distance. Dijkstra from the start gives the best splice.
static vector<int> splice(const St& start, const vector<vector<int>>& paths, bool stable, int r, int slack, const Ball* B, const vector<Source>& srcs) {
    vector<St> node; unordered_map<uint64_t, int> id;
    struct E { int to, w, kind; int a; uint64_t via; };   // kind 0: path move a; 1: shortcut via hash; 2: ball finish
    vector<vector<E>> adj;
    auto get = [&](const St& x) { uint64_t h = hst(x); auto it = id.find(h); if (it != id.end()) return it->second;
        id[h] = node.size(); node.push_back(x); adj.emplace_back(); return (int)node.size() - 1; };
    int s0 = get(start); int cap = 0; int best_single = INT32_MAX;
    vector<int> ends;  // node index where the AC part ends, plus suffix length
    vector<int> end_extra;
    for (auto& p : paths) {
        vector<St> st;
        if (!verify(start, p, stable, &st)) continue;
        best_single = min(best_single, (int)p.size());
        int prev = s0; for (auto& x : st) cap = max(cap, x.tot());
        for (size_t k = 1; k < st.size(); k++) { int v = get(st[k]); adj[prev].push_back({v, 1, 0, p[k - 1], 0}); prev = v; }
        ends.push_back(prev); end_extra.push_back(p.size() - (st.size() - 1));
    }
    if (ends.empty()) return {};
    cap = min(MAXT, cap + slack);
    int T = node.size(); adj.emplace_back();   // virtual target
    for (size_t k = 0; k < ends.size(); k++) adj[ends[k]].push_back({T, end_extra[k], 3, (int)k, 0});
    // shortcut edges
    vector<pair<uint64_t, int>> reach;         // (hash of u, node*256 + depth)
    for (int v = 0; v < T; v++) {
        vector<pair<uint64_t, int>> tmp; int bb = INT32_MAX;
        bfs_hashes(node[v], r, cap, tmp, B, &bb);
        if (B && bb < INT32_MAX) adj[v].push_back({T, bb, 2, bb, 0});
        for (auto& [h, d] : tmp) reach.push_back({h, v * 256 + d});
    }
    sort(reach.begin(), reach.end());
    for (size_t a = 0; a < reach.size();) {
        size_t b = a; while (b < reach.size() && reach[b].first == reach[a].first) b++;
        if (b - a > 1 && b - a <= 64)
            for (size_t x = a; x < b; x++) for (size_t y = a; y < b; y++) if (x != y) {
                int vx = reach[x].second / 256, dx = reach[x].second % 256, vy = reach[y].second / 256, dy = reach[y].second % 256;
                if (vx != vy) adj[vx].push_back({vy, dx + dy, 1, dx, reach[x].first});
            }
        a = b;
    }
    // Dijkstra
    vector<int> dist(T + 1, INT32_MAX); vector<pair<int, int>> from(T + 1, {-1, -1});
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<>> pq; dist[s0] = 0; pq.push({0, s0});
    while (!pq.empty()) {
        auto [d, v] = pq.top(); pq.pop();
        if (d > dist[v] || v == T) continue;
        for (size_t k = 0; k < adj[v].size(); k++) { auto& e = adj[v][k];
            if (d + e.w < dist[e.to]) { dist[e.to] = d + e.w; from[e.to] = {v, (int)k}; pq.push({dist[e.to], e.to}); } }
    }
    if (dist[T] >= best_single) return {};
    vector<pair<int, int>> chain; for (int v = T; v != s0; v = from[v].first) chain.push_back(from[v]);
    reverse(chain.begin(), chain.end());
    vector<int> out;
    for (auto [v, k] : chain) {
        auto& e = adj[v][k];
        if (e.kind == 0) out.push_back(e.a);
        else if (e.kind == 1) {
            vector<int> p1 = bfs_path(node[v], r, cap, e.via), p2 = bfs_path(node[e.to], r, cap, e.via);
            out.insert(out.end(), p1.begin(), p1.end());
            for (int q = p2.size() - 1; q >= 0; q--) out.push_back(INV_MOVE[p2[q]]);
        } else if (e.kind == 2) {
            vector<int> tail = bfs_to_ball(node[v], r, cap, *B, srcs, e.a); out.insert(out.end(), tail.begin(), tail.end());
        } else {   // original suffix of path e.a
            auto& p = paths[e.a]; out.insert(out.end(), p.end() - e.w, p.end());
        }
    }
    if (!verify(start, out, stable)) return {};
    return out;
}


// ---------------------------------------------------------------- window shortcuts (bidirectional similarity beam)
// For path states s_i and s_j (j - i = w, 20..40), run two elementary beams: forward from s_i scored by similarity
// to s_j, and from s_j (moves are invertible) scored by similarity to s_i. Similarity = L1 distance of the cyclic
// bigram histograms of corresponding relators + length difference. If the two frontiers meet in d < w moves,
// the detour is replaced. Repeats until no window improves.
static void bigram(const uint8_t* w, int n, int* h) {
    for (int k = 0; k < 16; k++) h[k] = 0;
    for (int k = 0; k < n; k++) h[w[k] * 4 + w[(k + 1) % n]]++;
}
static int simdist(const St& x, const int* g0, const int* g1, int gl0, int gl1) {
    int h0[16], h1[16]; bigram(x.r0(), x.n0, h0); bigram(x.r1(), x.n1, h1);
    int d = abs(x.n0 - gl0) + abs(x.n1 - gl1);
    for (int k = 0; k < 16; k++) d += abs(h0[k] - g0[k]) + abs(h1[k] - g1[k]);
    return d;
}
struct WSide { vector<vector<St>> lv; vector<vector<pair<int, int8_t>>> par; unordered_map<uint64_t, pair<int, int>> at; };
static vector<int> window_search(const St& a, const St& b, int maxd, int width, int cap, uint64_t seed) {
    WSide F, B;
    auto init = [&](WSide& S, const St& s) { S.lv.assign(1, {s}); S.par.assign(1, {{-1, -1}}); S.at[hst(s)] = {0, 0}; };
    init(F, a); init(B, b);
    int ga0[16], ga1[16], gb0[16], gb1[16];
    bigram(a.r0(), a.n0, ga0); bigram(a.r1(), a.n1, ga1); bigram(b.r0(), b.n0, gb0); bigram(b.r1(), b.n1, gb1);
    auto chain = [](WSide& S, int l, int i) { vector<int> m; while (l > 0) { m.push_back(S.par[l][i].second); i = S.par[l][i].first; l--; } reverse(m.begin(), m.end()); return m; };
    for (int d = 1; d < maxd; d++) {
        WSide& S = (d % 2) ? F : B; WSide& O = (d % 2) ? B : F;
        const int* g0 = (d % 2) ? gb0 : ga0; const int* g1 = (d % 2) ? gb1 : ga1; int gl0 = (d % 2) ? b.n0 : a.n0, gl1 = (d % 2) ? b.n1 : a.n1;
        struct C { St s; int p; int8_t m; uint64_t sc; };
        vector<C> cs; const vector<St>& cur = S.lv.back(); int L = S.lv.size();
        for (int i = 0; i < (int)cur.size(); i++) {
            St t;
            for (int m = 0; m < 14; m++) {
                if (!apply(cur[i], m, t, cap)) continue;
                uint64_t h = hst(t);
                if (S.at.count(h)) continue;
                auto it = O.at.find(h);
                if (it != O.at.end()) {                          // frontiers meet
                    int dl = L + it->second.first;
                    if (dl < maxd) {
                        vector<int> p1 = chain(S, L - 1, i); p1.push_back(m);
                        vector<int> p2 = chain(O, it->second.first, it->second.second);
                        vector<int> fw, bw;
                        if (&S == &F) { fw = p1; bw = p2; } else { fw = p2; bw = p1; }
                        vector<int> out = fw; for (int k = bw.size() - 1; k >= 0; k--) out.push_back(INV_MOVE[bw[k]]);
                        return out;
                    }
                }
                uint64_t sc = (uint64_t)simdist(t, g0, g1, gl0, gl1) << 32 | (mix(h ^ seed) & 0xffffffff);
                cs.push_back({t, i, (int8_t)m, sc});
            }
        }
        if (cs.empty()) break;
        if ((int)cs.size() > width) { nth_element(cs.begin(), cs.begin() + width, cs.end(), [](const C& x, const C& y) { return x.sc < y.sc; }); cs.resize(width); }
        vector<St> nx; vector<pair<int, int8_t>> np;
        for (auto& c : cs) { uint64_t h = hst(c.s); if (S.at.count(h)) continue; S.at[h] = {L, (int)nx.size()}; nx.push_back(c.s); np.push_back({c.p, c.m}); }
        S.lv.push_back(std::move(nx)); S.par.push_back(std::move(np));
    }
    return {};
}
static vector<int> WSIZES = {40, 30, 20};
static vector<int> window_opt(const St& start, vector<int> moves, bool stable, int width, uint64_t seed, int* gained) {
    *gained = 0;
    for (int round = 0; round < 50; round++) {
        vector<St> st; if (!verify(start, moves, stable, &st)) return moves;
        int n = st.size() - 1; bool improved = false;
        int cap = 0; for (auto& x : st) cap = max(cap, x.tot()); cap = min(MAXT, cap + 6);
        for (int w : WSIZES) {
            for (int i = 0; i + w <= n && !improved; i += 5) {
                vector<int> seg = window_search(st[i], st[i + w], w, width, cap, seed + i * 131 + w);
                if (seg.empty() || (int)seg.size() >= w) continue;
                vector<int> nm(moves.begin(), moves.begin() + i); nm.insert(nm.end(), seg.begin(), seg.end()); nm.insert(nm.end(), moves.begin() + i + w, moves.end());
                if (verify(start, nm, stable)) { *gained += moves.size() - nm.size(); moves.swap(nm); improved = true; }
            }
            if (improved) break;
        }
        if (!improved) return moves;
    }
    return moves;
}

// ---------------------------------------------------------------- beam search
struct Opt {
    int width = 20000, maxlen = 64, extra = 6, radius = 3, slack = 4, heur = 1, max_depth = 2000, maxcost = 30, maxgrow = 1000, class_dedup = 1;
    double time_limit = 600; int verbose = 0, junk = 4, gw = 8, reps = 3, look = 0, look_pen = 16, power = 8, minw = 0, noise = 0, bmult = 8, gsw = 8, gspen = 64, guidew = 8, guidek = 1, lincap = 0, linper = 8;
    vector<vector<int>> macros;
    vector<double> lin;   // learned score: cyc, junk, |c0-c1|, min(c0,c1), best_child, const
    string mode = "bucket";
    uint64_t seed = 1; bool stable = false;
};
static vector<int> beam(const St& start, const Ball& B, const vector<Source>& srcs, const Opt& o, long long* expanded) {
    struct Cand { St s; int parent; int8_t move; uint64_t score; };
    vector<vector<pair<int, int8_t>>> hist;           // per level: (parent index, move)
    vector<St> cur{start};
    hist.push_back({{-1, -1}});
    FlatSet seen(1 << 16); seen.insert(hst(start));
    vector<int> best; int best_len = INT32_MAX;
    auto chain = [&](int level, int idx) { vector<int> p; for (int l = level; l > 0; l--) { p.push_back(hist[l][idx].second); idx = hist[l][idx].first; } reverse(p.begin(), p.end()); return p; };
    {   vector<int> fin; if (B.get(start) >= 0 && descend(B, srcs, start, fin)) { best = fin; best_len = fin.size(); } }
    int found_at = -1;
    for (int depth = 1; depth <= o.max_depth; depth++) {
        if (found_at >= 0 && (depth - found_at > o.extra || depth >= best_len)) break;
        vector<Cand> ch; ch.reserve(cur.size() * 14);
        FlatSet level(cur.size() * 16);
        for (int i = 0; i < (int)cur.size(); i++) {
            St t;
            for (int m = 0; m < 14; m++) {
                if (!apply(cur[i], m, t, o.maxlen)) continue;
                uint64_t h = hst(t);
                if (seen.contains(h) || !level.insert(h)) continue;
                int d = t.tot() <= B.cap ? B.get(t) : -1;
                if (d >= 0 && depth + d < best_len) {
                    vector<int> fin;
                    if (descend(B, srcs, t, fin)) {
                        vector<int> p = chain(depth - 1, i);
                        p.push_back(m); p.insert(p.end(), fin.begin(), fin.end());
                        best = p; best_len = p.size(); if (found_at < 0) found_at = depth;
                    }
                }
                int c = cyc_len(t.r0(), t.n0) + cyc_len(t.r1(), t.n1);
                uint64_t sc = o.heur == 0 ? 2 * t.tot() : t.tot() + c;
                if (d >= 0) sc = min<uint64_t>(sc, d);   // inside the ball the exact distance wins
                ch.push_back({t, i, (int8_t)m, (sc << 32) | (mix(h ^ o.seed) & 0xffffffff)});
            }
        }
        *expanded += ch.size();
        if (ch.empty()) break;
        if ((int)ch.size() > o.width) {
            nth_element(ch.begin(), ch.begin() + o.width, ch.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; });
            ch.resize(o.width);
        }
        vector<St> nxt; nxt.reserve(ch.size());
        vector<pair<int, int8_t>> h; h.reserve(ch.size());
        for (auto& c : ch) { nxt.push_back(c.s); h.push_back({c.parent, c.move}); seen.insert(hst(c.s)); }
        hist.push_back(std::move(h)); cur.swap(nxt);
    }
    return best;
}


// ---------------------------------------------------------------- macro beam
// A macro = rotate r_i and r_j by conjugations (cost = number of conjugations), then r_i <- r_i r_j^{+-1}.
// Only macros whose product cancels at least one letter pair are kept; the 14 elementary moves are always
// kept too (so growth is always possible). Beam levels count macros; the score is
//   8 * (cyclically reduced total length, or the exact ball distance when inside the ball) + moves so far.
struct Seq { int8_t m[48]; uint8_t n = 0;
    void push_back(int8_t x) { m[n++] = x; } size_t size() const { return n; } bool empty() const { return n == 0; }
    const int8_t* begin() const { return m; } const int8_t* end() const { return m + n; } };
struct Rot { uint8_t w[MAXT]; int n; Seq seq; };
static void rotations(const uint8_t* w0, int n0, int rel, vector<Rot>& out) {
    out.clear();
    Rot r; memcpy(r.w, w0, n0); r.n = n0; out.push_back(r);
    int base = rel == 0 ? 6 : 10;
    for (int dir = 0; dir < 2; dir++) {            // 0: left (conjugate by inverse of first letter), 1: right (by last letter)
        Rot cur = out[0];
        for (int k = 1; k < n0 && k <= 24; k++) {
            int c = dir == 0 ? (cur.w[0] ^ 1) : cur.w[cur.n - 1];
            Rot nx; nx.n = conj(cur.w, cur.n, c, nx.w);
            if (nx.n <= 0) break;
            nx.seq = cur.seq; nx.seq.push_back(base + c);
            bool dup = false;
            for (auto& o : out) if (o.n == nx.n && !memcmp(o.w, nx.w, nx.n)) { dup = true; if (o.seq.size() > nx.seq.size()) o.seq = nx.seq; break; }
            if (!dup) out.push_back(nx);
            cur = nx;
        }
    }
}
static uint64_t class_hash(const uint8_t* w, int n) {   // cyclic reduction + minimal rotation hash
    int i = 0, j = n - 1;
    while (i < j && (w[i] ^ 1) == w[j]) i++, j--;
    int m = j - i + 1; const uint8_t* c = w + i;
    uint64_t best = ~0ULL;
    for (int r = 0; r < m; r++) { uint64_t h = 1469598103934665603ULL; for (int k = 0; k < m; k++) h = (h ^ c[(r + k) % m]) * 1099511628211ULL; best = min(best, h); }
    return mix(best + m);
}
struct MNode { St s; int g; };
static vector<int> macro_beam(const St& start, const Ball& B, const vector<Source>& srcs, const Opt& o, long long* expanded) {
    struct Cand { St s; int parent; int g; uint64_t score; uint64_t cls; vector<int8_t> seq; };
    struct Info { int parent; vector<int8_t> seq; };
    vector<vector<Info>> hist; hist.push_back({{-1, {}}});
    vector<MNode> cur{{start, 0}};
    FlatSet seen(1 << 16); seen.insert(hst(start)); FlatSet seen_cls(1 << 16);
    seen_cls.insert(mix(class_hash(start.r0(), start.n0) * 31 + class_hash(start.r1(), start.n1)));
    vector<int> best; int best_len = INT32_MAX;
    auto chain = [&](int level, int idx) { vector<vector<int8_t>> parts; for (int l = level; l > 0; l--) { parts.push_back(hist[l][idx].seq); idx = hist[l][idx].parent; }
        vector<int> p; for (int k = parts.size() - 1; k >= 0; k--) p.insert(p.end(), parts[k].begin(), parts[k].end()); return p; };
    { vector<int> fin; if (B.get(start) >= 0 && descend(B, srcs, start, fin)) { best = fin; best_len = fin.size(); } }
    vector<Rot> ri, rj; int found_at = -1; auto t0 = chrono::steady_clock::now();
    for (int level = 1; level <= o.max_depth; level++) {
        if (found_at >= 0 && level - found_at > o.extra) break;
        if (chrono::duration<double>(chrono::steady_clock::now() - t0).count() > o.time_limit) break;
        vector<Cand> ch;
        auto add = [&](int i, const St& t, vector<int8_t>&& seq) {
            int g = cur[i].g + seq.size();
            if (g >= best_len) return;
            uint64_t h = hst(t);
            if (seen.contains(h)) return;
            uint64_t cl = mix(class_hash(t.r0(), t.n0) * 31 + class_hash(t.r1(), t.n1));
            if (o.class_dedup == 2 && seen_cls.contains(cl)) return;
            int d = t.tot() <= B.cap ? B.get(t) : -1;
            if (d >= 0 && g + d < best_len) {
                vector<int> fin;
                if (descend(B, srcs, t, fin)) {
                    vector<int> p = chain(level - 1, i); p.insert(p.end(), seq.begin(), seq.end()); p.insert(p.end(), fin.begin(), fin.end());
                    best = p; best_len = p.size(); if (found_at < 0) found_at = level;
                }
            }
            int c = cyc_len(t.r0(), t.n0) + cyc_len(t.r1(), t.n1);
            uint64_t hv = d >= 0 ? min(c * 8, d * 8) : c * 8 + (t.tot() - c) * o.junk;
            uint64_t sc = (hv + (uint64_t)g * o.gw / 8) << 24 | (mix(h ^ o.seed) & 0xffffff);
            ch.push_back({t, i, g, sc, cl, std::move(seq)});
        };
        for (int i = 0; i < (int)cur.size(); i++) {
            const St& s = cur[i].s; St t;
            for (int m = 0; m < 14; m++) if (apply(s, m, t, o.maxlen)) add(i, t, {(int8_t)m});
            for (int rel = 0; rel < 2; rel++) {
                const uint8_t* wi = rel == 0 ? s.r0() : s.r1(); int ni = rel == 0 ? s.n0 : s.n1;
                const uint8_t* wj = rel == 0 ? s.r1() : s.r0(); int nj = rel == 0 ? s.n1 : s.n0;
                rotations(wi, ni, rel, ri); rotations(wj, nj, 1 - rel, rj);
                for (auto& a : ri) for (auto& b : rj) for (int e = 0; e < 2; e++) {
                    if (a.seq.empty() && b.seq.empty()) continue;           // plain multiply is elementary
                    if ((int)(a.seq.size() + b.seq.size()) + 1 > o.maxcost) continue;
                    uint8_t bw[MAXT], p[MAXT]; int nb = e ? inv_word(b.w, b.n, bw) : (memcpy(bw, b.w, b.n), b.n);
                    int np = concat(a.w, a.n, bw, nb, p);
                    if (np <= 0 || np >= a.n + nb || np + b.n > o.maxlen) continue;   // needs cancellation
                    if (np + b.n >= a.n + b.n + o.maxgrow) continue;
                    t.n0 = rel == 0 ? np : b.n; t.n1 = rel == 0 ? b.n : np;
                    if (rel == 0) { memcpy(t.c, p, np); memcpy(t.c + np, b.w, b.n); } else { memcpy(t.c, b.w, b.n); memcpy(t.c + b.n, p, np); }
                    vector<int8_t> seq(a.seq.begin(), a.seq.end()); seq.insert(seq.end(), b.seq.begin(), b.seq.end());
                    seq.push_back(rel == 0 ? (e ? 3 : 2) : (e ? 5 : 4));
                    add(i, t, std::move(seq));
                }
            }
        }
        *expanded += ch.size();
        if (ch.empty()) break;
        sort(ch.begin(), ch.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; });
        vector<MNode> nxt; vector<Info> info; FlatSet cls(o.width * 2), lvl(o.width * 4);
        for (auto& c : ch) {
            if ((int)nxt.size() >= o.width) break;
            if (!lvl.insert(hst(c.s))) continue;
            if (o.class_dedup && !cls.insert(c.cls)) continue;
            nxt.push_back({c.s, c.g}); info.push_back({c.parent, std::move(c.seq)}); seen.insert(hst(c.s)); seen_cls.insert(c.cls);
        }
        if (o.verbose) {
            int mh = 1 << 30, mg = 1 << 30, mt = 1 << 30, xt = 0;
            for (auto& n : nxt) { mh = min(mh, cyc_len(n.s.r0(), n.s.n0) + cyc_len(n.s.r1(), n.s.n1)); mg = min(mg, n.g); mt = min(mt, n.s.tot()); xt = max(xt, n.s.tot()); }
            fprintf(stderr, "  L%d cand=%zu beam=%zu min_cyc=%d min_tot=%d max_tot=%d min_g=%d best=%d %.1fs\n", level, ch.size(), nxt.size(), mh, mt, xt, mg,
                    best_len == INT32_MAX ? -1 : best_len, chrono::duration<double>(chrono::steady_clock::now() - t0).count());
        }
        hist.push_back(std::move(info)); cur.swap(nxt);
        bool any = false; for (auto& n : cur) if (n.g + 1 < best_len) any = true;
        if (!any) break;
    }
    return best;
}



// Best cyclic length reachable with one rotate+multiply macro (lengths only, no words built).
static int best_child_len(const St& s, int maxcost) {
    int best = cyc_len(s.r0(), s.n0) + cyc_len(s.r1(), s.n1);
    uint8_t inv[MAXT];
    for (int rel = 0; rel < 2; rel++) {
        const uint8_t* wi = rel == 0 ? s.r0() : s.r1(); int ni = rel == 0 ? s.n0 : s.n1;
        const uint8_t* wj = rel == 0 ? s.r1() : s.r0(); int nj = rel == 0 ? s.n1 : s.n0;
        int cj = cyc_len(wj, nj), ci = cyc_len(wi, ni);
        for (int e = 0; e < 2; e++) {
            const uint8_t* b = wj; if (e) { inv_word(wj, nj, inv); b = inv; }
            // rotation a of r_i ends at position pa (cyclic), rotation of r_j^e starts at pb; count cancellation
            for (int pa = 0; pa < ni; pa++) for (int pb = 0; pb < nj; pb++) {
                int rc = min(min(pa, ni - pa) + min(pb, nj - pb), 1 << 20);
                if (rc + 1 > maxcost) continue;
                int k = 0;
                while (k < ni && k < nj && (wi[(pa - 1 - k + 2 * ni) % ni] ^ 1) == b[(pb + k) % nj]) k++;
                if (k == 0) continue;
                int len = ci + cj - 2 * k + cj;
                if (len < best) best = len;
            }
        }
    }
    return best;
}


// ---------------------------------------------------------------- GS class-distance oracle (lengths <= 22)
// Classes = presentations up to rotation/inversion of each relator, relator swap and signed letter permutations
// (same canonical form as proof/proof_direction/acenum.cpp). Depth = number of GS macro edges from (x, y) in the
// component of the trivial class among classes of total cyclic length <= cap (built by `acenum bfs --cap 22`).
typedef uint64_t u64;
static inline u64 gs_hmap(u64 p, int n, int h) {
    u64 m = n ? (~0ULL >> (64 - 2 * n)) : 0, lo = 0x5555555555555555ULL & m;
    if (h & 1) p ^= (lo << 1);
    if (h & 2) p ^= (~p >> 1) & lo;
    if (h & 4) p ^= (p >> 1) & lo;
    return p;
}
static inline u64 gs_invp(u64 p, int n) { u64 o = 0; for (int i = 0; i < n; i++) { o = o << 2 | ((p & 3) ^ 1); p >>= 2; } return o; }
static inline u64 gs_cyccanon(u64 p, int n) {
    if (n == 0) return 0;
    u64 m = ~0ULL >> (64 - 2 * n), best = ~0ULL, q = gs_invp(p, n);
    for (int i = 0; i < n; i++) { best = min(best, p); best = min(best, q); p = ((p << 2) | (p >> (2 * n - 2))) & m; q = ((q << 2) | (q >> (2 * n - 2))) & m; }
    return (u64)n << 58 | best;
}
struct GsKey { u64 a, b; bool operator<(const GsKey& o) const { return a != o.a ? a < o.a : b < o.b; } bool operator==(const GsKey& o) const { return a == o.a && b == o.b; } };
static GsKey gs_canon(const uint8_t* u, int nu, const uint8_t* v, int nv) {
    int i = 0, j = nu - 1; while (i < j && (u[i] ^ 1) == u[j]) i++, j--; const uint8_t* cu = u + i; int lu = nu ? j - i + 1 : 0;
    i = 0; j = nv - 1; while (i < j && (v[i] ^ 1) == v[j]) i++, j--; const uint8_t* cv = v + i; int lv = nv ? j - i + 1 : 0;
    u64 pu = 0, pv = 0; for (int k = 0; k < lu; k++) pu = pu << 2 | cu[k]; for (int k = 0; k < lv; k++) pv = pv << 2 | cv[k];
    GsKey best{~0ULL, ~0ULL};
    for (int h = 0; h < 8; h++) { u64 a = gs_cyccanon(gs_hmap(pu, lu, h), lu), b = gs_cyccanon(gs_hmap(pv, lv, h), lv);
        GsKey k{min(a, b), max(a, b)}; if (k < best) best = k; }
    return best;
}
struct GsTable {
    int cap = 0; vector<GsKey> keys; vector<uint8_t> depth; vector<double> mean_by_len;
    int get(const St& s) const {   // -1: length above cap; -2: not in the trivial component (needs to grow past cap)
        int c = cyc_len(s.r0(), s.n0) + cyc_len(s.r1(), s.n1);
        if (c > cap || c > 31) return -1;
        GsKey k = gs_canon(s.r0(), s.n0, s.r1(), s.n1);
        auto it = lower_bound(keys.begin(), keys.end(), k);
        return (it != keys.end() && *it == k) ? depth[it - keys.begin()] : -2;
    }
    bool load(const string& f) {
        FILE* fp = fopen(f.c_str(), "rb"); if (!fp) return false;
        u64 hdr[2]; if (fread(hdr, 8, 2, fp) != 2) return false;
        cap = hdr[0]; keys.resize(hdr[1]); depth.resize(hdr[1]); mean_by_len.assign(64, 0);
        bool ok = fread(keys.data(), sizeof(GsKey), keys.size(), fp) == keys.size() && fread(depth.data(), 1, depth.size(), fp) == depth.size()
                  && fread(mean_by_len.data(), 8, 64, fp) == 64;
        fclose(fp); return ok;
    }
};
static GsTable GS; static bool HAVE_GS = false;
static void gs_build(const string& comp, const string& out) {
    FILE* fp = fopen(comp.c_str(), "rb"); u64 hdr[3]; if (!fp || fread(hdr, 8, 3, fp) != 3) { fprintf(stderr, "bad comp\n"); exit(1); }
    size_t n = hdr[2]; vector<GsKey> k(n); vector<uint32_t> par(n);
    if (fread(k.data(), sizeof(GsKey), n, fp) != n || fread(par.data(), 4, n, fp) != n) { fprintf(stderr, "short comp\n"); exit(1); }
    fclose(fp);
    vector<uint8_t> d(n, 0);
    for (size_t i = 0; i < n; i++) d[i] = par[i] == UINT32_MAX ? 0 : min(255, d[par[i]] + 1);   // BFS order: parent first
    vector<double> sum(64, 0), cnt(64, 0);
    for (size_t i = 0; i < n; i++) { int L = (k[i].a >> 58) + (k[i].b >> 58); sum[L] += d[i]; cnt[L]++; }
    vector<double> mean(64, 0); for (int L = 0; L < 64; L++) mean[L] = cnt[L] ? sum[L] / cnt[L] : 0;
    vector<size_t> ord(n); for (size_t i = 0; i < n; i++) ord[i] = i;
    sort(ord.begin(), ord.end(), [&](size_t a, size_t b) { return k[a] < k[b]; });
    FILE* fo = fopen(out.c_str(), "wb"); u64 h2[2] = {hdr[0], n}; fwrite(h2, 8, 2, fo);
    for (size_t i : ord) fwrite(&k[i], sizeof(GsKey), 1, fo);
    for (size_t i : ord) fwrite(&d[i], 1, 1, fo);
    fwrite(mean.data(), 8, 64, fo); fclose(fo);
    for (int L = 0; L <= (int)hdr[0]; L++) if (cnt[L]) fprintf(stderr, "  len %2d: %9.0f classes, mean depth %.2f\n", L, cnt[L], mean[L]);
    fprintf(stderr, "gs table cap=%llu classes=%zu -> %s\n", (unsigned long long)hdr[0], n, out.c_str());
}


// ---------------------------------------------------------------- learned guide client (unix socket to serve_guide.py)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
static string GUIDE_SOCK;
static thread_local int guide_fd = -1;
static bool guide_score(const vector<St>& xs, vector<float>& out) {
    if (guide_fd < 0) {
        guide_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un a{}; a.sun_family = AF_UNIX; strncpy(a.sun_path, GUIDE_SOCK.c_str(), sizeof(a.sun_path) - 1);
        if (connect(guide_fd, (sockaddr*)&a, sizeof a) != 0) { close(guide_fd); guide_fd = -1; return false; }
    }
    uint32_t n = xs.size(); vector<uint8_t> buf(4 + (size_t)n * 66, 5);
    memcpy(buf.data(), &n, 4);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t* r = buf.data() + 4 + (size_t)i * 66; const St& s = xs[i];
        if (s.tot() + 1 > 66) continue;
        memcpy(r, s.r0(), s.n0); r[s.n0] = 4; memcpy(r + s.n0 + 1, s.r1(), s.n1);
    }
    size_t off = 0; while (off < buf.size()) { ssize_t w = write(guide_fd, buf.data() + off, buf.size() - off); if (w <= 0) { close(guide_fd); guide_fd = -1; return false; } off += w; }
    out.resize(n); size_t need = (size_t)n * 4; off = 0;
    while (off < need) { ssize_t r = read(guide_fd, (char*)out.data() + off, need - off); if (r <= 0) { close(guide_fd); guide_fd = -1; return false; } off += r; }
    return true;
}

// ---------------------------------------------------------------- cost-bucketed macro beam
// Candidates are grouped by exact path cost g (elementary moves so far). Buckets are processed in order
// of g; each keeps its best `width` states by length score, with at most `reps` raw forms per cyclic
// class (globally), so states only ever compete with states of equal cost.
static vector<int> bucket_beam(const St& start, const Ball& B, const vector<Source>& srcs, const Opt& o, long long* expanded) {
    struct Perm { int parent; vector<int8_t> seq; int lin = 0; };
    struct Pend { St s; int parent; uint64_t h; uint8_t nseq; int8_t seq[48]; };
    struct Key { uint64_t score; uint32_t idx; bool operator<(const Key& o) const { return score < o.score; } };
    vector<Perm> perm{{-1, {}}};
    int G = o.max_depth + o.maxcost + 2;
    vector<vector<Pend>> bk(G); vector<vector<Key>> kk(G); vector<uint64_t> cut(G, 0);
    FlatSet seen(1 << 16); unordered_map<uint64_t, uint8_t> cls_count;
    vector<int> best; int best_len = INT32_MAX;
    auto chain = [&](int id) { vector<vector<int8_t>> parts; while (id > 0) { parts.push_back(perm[id].seq); id = perm[id].parent; }
        vector<int> p; for (int k = parts.size() - 1; k >= 0; k--) p.insert(p.end(), parts[k].begin(), parts[k].end()); return p; };
    auto t0 = chrono::steady_clock::now();
    vector<Rot> ri, rj;
    auto push = [&](int pid, int g, const St& t, const Seq& seq) {
        int ng = g + seq.size();
        if (ng >= best_len || ng >= G) return;
        uint64_t h = hst(t);
        if (seen.contains(h)) return;
        int d = t.tot() <= B.cap ? B.get(t) : -1;
        if (d >= 0 && ng + d < best_len) {
            vector<int> fin;
            if (descend(B, srcs, t, fin)) {
                vector<int> p = chain(pid); for (auto m : seq) p.push_back(m); p.insert(p.end(), fin.begin(), fin.end());
                best = p; best_len = p.size();
            }
        }
        if (d >= 0) return;                               // inside the ball the exact finish is already recorded
        int c0 = cyc_len(t.r0(), t.n0), c1 = cyc_len(t.r1(), t.n1), c = c0 + c1;
        int64_t hs = (int64_t)c * 8 + (t.tot() - c) * o.junk - (int64_t)o.minw * (c - 2 * min(c0, c1));
        uint64_t hv = hs < 0 ? 0 : hs;
        if (o.noise > 0) hv += mix(h ^ (o.seed * 0x9E3779B97F4A7C15ULL)) % (uint64_t)(o.noise + 1);   // route diversity
        auto& b = bk[ng]; auto& k = kk[ng];
        uint64_t score = hv << 24 | (mix(h ^ o.seed) & 0xffffff);
        if ((int)k.size() >= o.bmult * o.width) {          // full: replace only if better than the current cut
            if (!(cut[ng] > 0 && score < cut[ng])) {
                nth_element(k.begin(), k.begin() + (o.bmult / 2) * o.width, k.end());
                vector<Pend> nb; nb.reserve(o.bmult * o.width); vector<Key> nk; nk.reserve(o.bmult * o.width);
                for (int q = 0; q < (o.bmult / 2) * o.width; q++) { nk.push_back({k[q].score, (uint32_t)nb.size()}); nb.push_back(b[k[q].idx]); }
                cut[ng] = k[(o.bmult / 2) * o.width].score; b.swap(nb); k.swap(nk);
            }
            if (score >= cut[ng]) return;
        }
        k.push_back({score, (uint32_t)b.size()});
        b.emplace_back(); Pend& q = b.back();
        q.s = t; q.parent = pid; q.h = h; q.nseq = seq.size(); memcpy(q.seq, seq.m, seq.size());
    };
    auto expand = [&](int pid, int g, const St& s) {
        St t;
        for (int m = 0; m < 14; m++) if (apply(s, m, t, o.maxlen)) { Seq q; q.push_back(m); push(pid, g, t, q); }
        for (auto& mac : o.macros) {                   // mined fixed move sequences
            St u = s, w; bool ok = true; Seq q;
            for (int m : mac) { if (!apply(u, m, w, o.maxlen)) { ok = false; break; } u = w; q.push_back(m); }
            if (ok) push(pid, g, u, q);
        }
        for (int rel = 0; rel < 2; rel++) {
            const uint8_t* wi = rel == 0 ? s.r0() : s.r1(); int ni = rel == 0 ? s.n0 : s.n1;
            const uint8_t* wj = rel == 0 ? s.r1() : s.r0(); int nj = rel == 0 ? s.n1 : s.n0;
            rotations(wi, ni, rel, ri); rotations(wj, nj, 1 - rel, rj);
            for (auto& a : ri) for (auto& b : rj) for (int e = 0; e < 2; e++) {
                if (a.seq.empty() && b.seq.empty()) continue;
                if ((int)(a.seq.size() + b.seq.size()) + 1 > o.maxcost) continue;
                uint8_t bw[MAXT], p[MAXT]; int nb = e ? inv_word(b.w, b.n, bw) : (memcpy(bw, b.w, b.n), b.n);
                int np = concat(a.w, a.n, bw, nb, p);
                if (np <= 0 || np >= a.n + nb || np + b.n > o.maxlen) continue;
                t.n0 = rel == 0 ? np : b.n; t.n1 = rel == 0 ? b.n : np;
                if (rel == 0) { memcpy(t.c, p, np); memcpy(t.c + np, b.w, b.n); } else { memcpy(t.c, b.w, b.n); memcpy(t.c + b.n, p, np); }
                Seq seq = a.seq; for (auto m : b.seq) seq.push_back(m);
                seq.push_back(rel == 0 ? (e ? 3 : 2) : (e ? 5 : 4));
                push(pid, g, t, seq);
                // power macros: keep multiplying by the same (rotated) r_j^{+-1} while it keeps cancelling
                uint8_t cur[MAXT]; int nc = np; memcpy(cur, p, np);
                int base_cost = a.seq.size() + b.seq.size() + 1;
                for (int k = 2; k <= o.power && base_cost + k - 1 <= o.maxcost; k++) {
                    uint8_t nxw[MAXT]; int nn = concat(cur, nc, bw, nb, nxw);
                    if (nn <= 0 || nn >= nc + nb || nn + b.n > o.maxlen) break;
                    memcpy(cur, nxw, nn); nc = nn;
                    t.n0 = rel == 0 ? nc : b.n; t.n1 = rel == 0 ? b.n : nc;
                    if (rel == 0) { memcpy(t.c, cur, nc); memcpy(t.c + nc, b.w, b.n); } else { memcpy(t.c, b.w, b.n); memcpy(t.c + b.n, cur, nc); }
                    Seq sq = a.seq; for (auto m : b.seq) sq.push_back(m);
                    for (int r = 0; r < k; r++) sq.push_back(rel == 0 ? (e ? 3 : 2) : (e ? 5 : 4));
                    push(pid, g, t, sq);
                }
            }
        }
    };
    { vector<int> fin; if (B.get(start) >= 0 && descend(B, srcs, start, fin)) return fin; }
    seen.insert(hst(start)); cls_count[mix(class_hash(start.r0(), start.n0) * 31 + class_hash(start.r1(), start.n1))] = 1;
    expand(0, 0, start);
    for (int g = 1; g < G && g < best_len; g++) {
        if (chrono::duration<double>(chrono::steady_clock::now() - t0).count() > o.time_limit) break;
        auto& b = bk[g]; auto& kv = kk[g];
        if (b.empty()) continue;
        sort(kv.begin(), kv.end());
        if (o.look > 0) {   // rescore the leading candidates by min(own length, best one-macro child length)
            size_t L = min(kv.size(), (size_t)o.look * o.width);
            for (size_t k = 0; k < L; k++) {
                const St& x = b[kv[k].idx].s;
                if (o.lin.size() == 6) {         // learned linear distance-to-go
                    int c0 = cyc_len(x.r0(), x.n0), c1 = cyc_len(x.r1(), x.n1), c = c0 + c1;
                    double v = o.lin[0] * c + o.lin[1] * (x.tot() - c) + o.lin[2] * abs(c0 - c1) + o.lin[3] * min(c0, c1)
                             + o.lin[4] * best_child_len(x, o.maxcost) + o.lin[5];
                    uint64_t sv = (uint64_t)max(0.0, v * 8 + 64);
                    kv[k].score = sv << 24 | (kv[k].score & 0xffffff);
                    continue;
                }
                uint64_t own = kv[k].score >> 24;
                uint64_t ch = (uint64_t)best_child_len(x, o.maxcost) * 8 + o.look_pen;
                if (ch < own) kv[k].score = ch << 24 | (kv[k].score & 0xffffff);
            }
            if (!GUIDE_SOCK.empty()) {         // learned guide, blended: own score + guidew * (prediction - mean prediction), top M only
                size_t M = min(L, (size_t)o.width * o.guidek);
                vector<St> xs; xs.reserve(M); for (size_t k = 0; k < M; k++) xs.push_back(b[kv[k].idx].s);
                vector<float> v;
                if (M && guide_score(xs, v)) {
                    double mean = 0; for (float f : v) mean += f; mean /= M;
                    for (size_t k = 0; k < M; k++) {
                        int64_t sv = (int64_t)(kv[k].score >> 24) + (int64_t)llround(o.guidew * (v[k] - mean));
                        kv[k].score = (uint64_t)max<int64_t>(0, sv) << 24 | (kv[k].score & 0xffffff);
                    }
                }
            }
            if (HAVE_GS) {                    // class-distance oracle for the 17-22 band: closer than typical -> better; outside the component -> penalty
                for (size_t k = 0; k < L; k++) {
                    const St& x = b[kv[k].idx].s;
                    int gd = GS.get(x);
                    if (gd == -1) continue;
                    int c = cyc_len(x.r0(), x.n0) + cyc_len(x.r1(), x.n1);
                    int64_t sv = (int64_t)(kv[k].score >> 24);
                    if (gd == -2) sv += o.gspen;
                    else sv += (int64_t)llround(o.gsw * (gd - GS.mean_by_len[c]));
                    if (sv < 0) sv = 0;
                    kv[k].score = (uint64_t)sv << 24 | (kv[k].score & 0xffffff);
                }
            }
            sort(kv.begin(), kv.begin() + L);
        }
        vector<pair<int, St>> sel; unordered_map<int, int> lincount;
        for (auto& key : kv) { Pend& c = b[key.idx];
            if ((int)sel.size() >= o.width) break;
            if (!seen.insert(c.h)) continue;
            uint8_t& k = cls_count[mix(class_hash(c.s.r0(), c.s.n0) * 31 + class_hash(c.s.r1(), c.s.n1))];
            if (k >= o.reps) continue;
            int lin = perm[c.parent].lin;                 // lineage diversity: new lineages every linper levels, capped slots each
            if (o.lincap > 0 && lincount[lin] >= o.lincap) continue;
            k++; if (o.lincap > 0) lincount[lin]++;
            perm.push_back({c.parent, vector<int8_t>(c.seq, c.seq + c.nseq)});
            perm.back().lin = (o.lincap > 0 && g % o.linper == 0) ? (int)perm.size() - 1 : lin;
            sel.push_back({(int)perm.size() - 1, c.s});
        }
        vector<Pend>().swap(b); vector<Key>().swap(kv);
        *expanded += sel.size();
        for (auto& [pid, st] : sel) expand(pid, g, st);
        if (o.verbose && g % 5 == 0)
            fprintf(stderr, "  g=%d sel=%zu best=%d classes=%zu %.1fs\n", g, sel.size(), best_len == INT32_MAX ? -1 : best_len, cls_count.size(),
                    chrono::duration<double>(chrono::steady_clock::now() - t0).count());
    }
    return best;
}

// ---------------------------------------------------------------- driver
struct Prob { string id; St s; };
static vector<Prob> read_problems(const string& f) {
    vector<Prob> out; ifstream in(f); string line;
    while (getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto bar = line.find('|'); istringstream a(line.substr(0, bar)), b(line.substr(bar + 1));
        Prob p; a >> p.id; vector<int> x, y; int v;
        while (a >> v) x.push_back(v);
        while (b >> v) y.push_back(v);
        p.s = make(x, y); out.push_back(p);
    }
    return out;
}
static map<string, vector<vector<int>>> read_all_paths(const vector<string>& files) {
    map<string, vector<vector<int>>> out;
    for (auto& f : files) { ifstream in(f); string line;
    while (getline(in, line)) {
        auto i = line.find("\"id\""), m = line.find("\"moves\"");
        if (i == string::npos || m == string::npos) continue;
        auto q1 = line.find('"', line.find(':', i) + 1), q2 = line.find('"', q1 + 1);
        auto l = line.find('[', m), r = line.find(']', l);
        vector<int> mv; string body = line.substr(l + 1, r - l - 1); replace(body.begin(), body.end(), ',', ' ');
        istringstream ss(body); int v; while (ss >> v) mv.push_back(v);
        if (!mv.empty()) out[line.substr(q1 + 1, q2 - q1 - 1)].push_back(mv);
    } }
    return out;
}
static map<string, vector<int>> read_paths(const string& f) {   // jsonl lines with "id" and "moves"
    map<string, vector<int>> out; ifstream in(f); string line;
    while (getline(in, line)) {
        auto i = line.find("\"id\""), m = line.find("\"moves\"");
        if (i == string::npos || m == string::npos) continue;
        auto q1 = line.find('"', line.find(':', i) + 1), q2 = line.find('"', q1 + 1);
        string id = line.substr(q1 + 1, q2 - q1 - 1);
        auto l = line.find('[', m), r = line.find(']', l);
        vector<int> mv; string body = line.substr(l + 1, r - l - 1); replace(body.begin(), body.end(), ',', ' ');
        istringstream ss(body); int v; while (ss >> v) mv.push_back(v);
        if (!out.count(id) || mv.size() < out[id].size()) out[id] = mv;
    }
    return out;
}
static string jsonl(const string& id, bool ok, const vector<int>& mv, double secs, const string& extra) {
    string s = "{\"id\": \"" + id + "\", \"solved\": " + (ok ? "true" : "false") + ", \"length\": " + to_string(ok ? (int)mv.size() : -1) + ", \"moves\": [";
    for (size_t i = 0; i < mv.size(); i++) s += (i ? ", " : "") + to_string(mv[i]);
    char buf[64]; snprintf(buf, sizeof buf, "], \"secs\": %.2f", secs);
    return s + buf + extra + "}\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: acs ball|solve|shorten [options]\n"); return 1; }
    string cmd = argv[1], problems, ball_f, out = "/dev/stdout", paths_f;
    int cap = 14, threads = thread::hardware_concurrency(); Opt o;
    for (int i = 2; i < argc; i++) {
        string a = argv[i]; auto nx = [&]() { return string(argv[++i]); };
        if (a == "--cap") cap = stoi(nx());
        else if (a == "--stable") o.stable = true;
        else if (a == "--out") out = nx();
        else if (a == "--problems") problems = nx();
        else if (a == "--ball") ball_f = nx();
        else if (a == "--paths") paths_f = nx();
        else if (a == "--width") o.width = stoi(nx());
        else if (a == "--maxlen") o.maxlen = stoi(nx());
        else if (a == "--extra") o.extra = stoi(nx());
        else if (a == "--radius") o.radius = stoi(nx());
        else if (a == "--slack") o.slack = stoi(nx());
        else if (a == "--heur") o.heur = stoi(nx());
        else if (a == "--max-depth") o.max_depth = stoi(nx());
        else if (a == "--mode") o.mode = nx();
        else if (a == "--lin") { string v = nx(); replace(v.begin(), v.end(), ',', ' '); istringstream ss(v); double d; while (ss >> d) o.lin.push_back(d); }
        else if (a == "--macros") { ifstream mf(nx()); string ln; while (getline(mf, ln)) { ln = ln.substr(0, ln.find('#')); istringstream ss(ln); vector<int> q; int v; while (ss >> v) q.push_back(v); if (q.size() >= 2) o.macros.push_back(q); } fprintf(stderr, "loaded %zu macros\n", o.macros.size()); }
        else if (a == "--gs") { if (!GS.load(nx())) { fprintf(stderr, "cannot load gs table\n"); return 1; } HAVE_GS = true; fprintf(stderr, "gs table: cap %d, %zu classes\n", GS.cap, GS.keys.size()); }
        else if (a == "--guide") GUIDE_SOCK = nx();
        else if (a == "--guidek") o.guidek = max(1, stoi(nx()));
        else if (a == "--guidew") o.guidew = stoi(nx());
        else if (a == "--gsw") o.gsw = stoi(nx());
        else if (a == "--gspen") o.gspen = stoi(nx());
        else if (a == "--wsizes") { WSIZES.clear(); string v = nx(); replace(v.begin(), v.end(), ',', ' '); istringstream ss(v); int x; while (ss >> x) WSIZES.push_back(x); }
        else if (a == "--bucket-mult") o.bmult = max(2, stoi(nx()));
        else if (a == "--noise") o.noise = stoi(nx());
        else if (a == "--minw") o.minw = stoi(nx());
        else if (a == "--power") o.power = stoi(nx());
        else if (a == "--look") o.look = stoi(nx());
        else if (a == "--look-pen") o.look_pen = stoi(nx());
        else if (a == "--reps") o.reps = stoi(nx());
        else if (a == "--lincap") o.lincap = stoi(nx());
        else if (a == "--linper") o.linper = stoi(nx());
        else if (a == "--gw") o.gw = stoi(nx());
        else if (a == "--junk") o.junk = stoi(nx());
        else if (a == "--verbose") o.verbose = stoi(nx());
        else if (a == "--time-limit") o.time_limit = stod(nx());
        else if (a == "--maxcost") o.maxcost = min(39, stoi(nx()));
        else if (a == "--maxgrow") o.maxgrow = stoi(nx());
        else if (a == "--class-dedup") o.class_dedup = stoi(nx());
        else if (a == "--seed") o.seed = stoull(nx());
        else if (a == "--threads") threads = stoi(nx());
        else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
    }
    if (cmd == "ball") { build_ball(cap, o.stable, out); return 0; }
    if (cmd == "gsbuild") { gs_build(paths_f, out); return 0; }

    vector<Prob> probs = read_problems(problems);
    Ball B; B.cap = -1;
    if (cmd == "solve" || cmd == "rebeam" || !ball_f.empty()) {
        if (!B.load(ball_f)) { fprintf(stderr, "cannot load ball %s\n", ball_f.c_str()); return 1; }
        if (B.stable != o.stable) { fprintf(stderr, "ball stable=%d but --stable=%d\n", B.stable, o.stable); return 1; }
        fprintf(stderr, "ball cap=%d states=%llu\n", B.cap, (unsigned long long)B.count);
    }
    map<string, vector<int>> given; if (cmd == "shorten" || cmd == "window" || cmd == "bidir") given = read_paths(paths_f);
    map<string, vector<vector<int>>> allp;
    if (cmd == "splice" || cmd == "rebeam") { vector<string> fs; string cur; for (char ch : paths_f + ",") { if (ch == ',') { if (!cur.empty()) fs.push_back(cur); cur.clear(); } else cur += ch; } allp = read_all_paths(fs); }
    vector<Source> srcs = sources(o.stable);
    {   // resume: skip ids already present in the output file
        ifstream prev(out); string line; set<string> done;
        while (getline(prev, line)) { auto i = line.find("\"id\": \""); if (i != string::npos) done.insert(line.substr(i + 7, line.find('"', i + 7) - i - 7)); }
        if (!done.empty()) { vector<Prob> keep; for (auto& p : probs) if (!done.count(p.id)) keep.push_back(p); fprintf(stderr, "resume: %zu already done, %zu left\n", probs.size() - keep.size(), keep.size()); probs.swap(keep); }
    }
    FILE* fo = fopen(out.c_str(), "a"); mutex mu; atomic<int> next{0}, solved{0};
    auto work = [&]() {
        for (int k; (k = next++) < (int)probs.size();) {
            auto& p = probs[k]; auto t0 = chrono::steady_clock::now();
            vector<int> mv; long long expanded = 0; int raw = -1;
            if (cmd == "solve") mv = o.mode == "bucket" ? bucket_beam(p.s, B, srcs, o, &expanded) : o.mode == "macro" ? macro_beam(p.s, B, srcs, o, &expanded) : beam(p.s, B, srcs, o, &expanded);
            else if (cmd == "rebeam") {
                if (!allp.count(p.id)) continue;
                auto& ps = allp[p.id];
                vector<int> best = *min_element(ps.begin(), ps.end(), [](auto& a, auto& b) { return a.size() < b.size(); });
                vector<St> st;
                if (!verify(p.s, best, o.stable, &st)) continue;
                vector<vector<int>> pool = ps;
                int n = st.size() - 1;
                for (double f : {0.2, 0.35, 0.5, 0.65, 0.8}) {      // waypoints along the AC part of the path
                    int k = max(1, min(n - 1, (int)(f * n)));
                    vector<int> tail = bucket_beam(st[k], B, srcs, o, &expanded);
                    if (tail.empty()) continue;
                    vector<int> cand(best.begin(), best.begin() + k); cand.insert(cand.end(), tail.begin(), tail.end());
                    if (verify(p.s, cand, o.stable)) pool.push_back(cand);
                }
                mv = *min_element(pool.begin(), pool.end(), [](auto& a, auto& b) { return a.size() < b.size(); });
                if (pool.size() > 1) { auto sp = splice(p.s, pool, o.stable, o.radius, o.slack, &B, srcs); if (!sp.empty() && sp.size() < mv.size()) mv = sp; }
            }
            else if (cmd == "bidir") {                  // whole-problem bidirectional similarity search: start <-> (x, y)
                int best = given.count(p.id) ? (int)given[p.id].size() : 400;
                St tgt = srcs[0].s;
                vector<int> seg = window_search(p.s, tgt, best, o.width, o.maxlen, o.seed);
                if (!seg.empty()) { vector<int> fin = seg; if (o.stable) fin.insert(fin.end(), srcs[0].suffix.begin(), srcs[0].suffix.end()); mv = fin; }
                else if (given.count(p.id)) mv = given[p.id];
            }
            else if (cmd == "window") {
                if (!given.count(p.id)) continue;
                int g = 0; mv = window_opt(p.s, given[p.id], o.stable, o.width, o.seed, &g); expanded = g;
            }
            else if (cmd == "splice") {
                if (!allp.count(p.id)) continue;
                auto& ps = allp[p.id]; mv = *min_element(ps.begin(), ps.end(), [](auto& a, auto& b) { return a.size() < b.size(); });
                if (ps.size() > 1) { auto sp = splice(p.s, ps, o.stable, o.radius, o.slack, B.cap >= 0 ? &B : nullptr, srcs); if (!sp.empty() && sp.size() < mv.size()) mv = sp; }
            }
            else if (given.count(p.id)) mv = given[p.id];
            else continue;
            bool ok = !mv.empty() && verify(p.s, mv, o.stable);
            if (ok) { raw = mv.size(); if (o.radius > 0) mv = shorten(p.s, mv, o.stable, o.radius, o.slack, B.cap >= 0 ? &B : nullptr, &srcs); ok = verify(p.s, mv, o.stable); }
            double secs = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
            string ex = ", \"raw\": " + to_string(raw) + ", \"expanded\": " + to_string(expanded);
            lock_guard<mutex> g(mu);
            fputs(jsonl(p.id, ok, mv, secs, ex).c_str(), fo); fflush(fo);
            solved += ok;
            fprintf(stderr, "%s %s len=%d raw=%d %.1fs [%d/%zu solved]\n", p.id.c_str(), ok ? "ok" : "FAIL", ok ? (int)mv.size() : -1, raw, secs, solved.load(), probs.size());
        }
    };
    vector<thread> ts; for (int i = 0; i < max(1, threads); i++) ts.emplace_back(work);
    for (auto& t : ts) t.join();
    fclose(fo);
    return 0;
}
