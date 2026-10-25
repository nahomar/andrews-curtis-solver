// sacs.cpp — search in the TRUE Stable AC move space (sac-r8-v1: 257 moves, ranks 2..8, target = empty presentation).
// Our previous Stable AC paths were AC paths + [16,15]; this tool looks for shortcuts that pass through rank-3/4 states.
//
//   sacs replay   --problems P.txt --paths IN.jsonl                      replay and print final shapes (self-check)
//   sacs window   --problems P.txt --paths IN.jsonl --out OUT.jsonl      exact bidirectional shortcut search along each path
//                 [--rf 3] [--rb 3] [--rank 3] [--cap 48] [--threads N] [--rounds 8] [--maxlen 400] [--min-save 1]
//   sacs selftest                                                        moves/inverse consistency on random states
//
// States: k freely reduced words over letters +-1..+-k (1 = x, 2 = y, 3.. = g3..). Move ids are the frozen official table.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using namespace std;

// ---------------------------------------------------------------- state
constexpr int MAXR = 8;
struct GS {
    vector<vector<int8_t>> r;   // relators; r.size() = rank
    int rank() const { return r.size(); }
    int tot() const { int t = 0; for (auto& w : r) t += w.size(); return t; }
    bool operator==(const GS& o) const { return r == o.r; }
};
static inline uint64_t mix64(uint64_t x) { x += 0x9E3779B97F4A7C15ULL; x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL; x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL; return x ^ (x >> 31); }
static uint64_t hgs(const GS& s) {
    uint64_t h = mix64(0x51ED270B + s.rank());
    for (auto& w : s.r) { h = mix64(h ^ (w.size() * 0x9E37ULL + 7)); for (int8_t a : w) h = mix64(h ^ (uint8_t)a ^ 0xA5); }
    return h;
}
static vector<int8_t> reduce_cat(const vector<int8_t>& a, const vector<int8_t>& b) {   // free reduction of a·b (a, b already reduced)
    vector<int8_t> out(a);
    for (int8_t c : b) { if (!out.empty() && out.back() == -c) out.pop_back(); else out.push_back(c); }
    return out;
}
static vector<int8_t> invert(const vector<int8_t>& a) { vector<int8_t> o(a.rbegin(), a.rend()); for (auto& c : o) c = -c; return o; }

// ---------------------------------------------------------------- frozen move table (mirrors verifier/stable_core.py)
enum Cat : uint8_t { INV, MUL, CONJ, STAB, DESTAB };
struct Move { int id; Cat cat; int i, j; bool inv_other; int c; int inverse; };
static vector<Move> MT;
static void build_table() {
    MT.clear();
    MT.push_back({0, INV, 0, -1, false, 0, 0}); MT.push_back({1, INV, 1, -1, false, 0, 1});
    MT.push_back({2, MUL, 0, 1, false, 0, 3}); MT.push_back({3, MUL, 0, 1, true, 0, 2});
    MT.push_back({4, MUL, 1, 0, false, 0, 5}); MT.push_back({5, MUL, 1, 0, true, 0, 4});
    int id = 6;
    for (int i = 0; i < 2; i++) for (int g = 1; g <= 2; g++) { MT.push_back({id, CONJ, i, -1, false, g, id + 1}); MT.push_back({id + 1, CONJ, i, -1, false, -g, id}); id += 2; }
    MT.push_back({14, STAB, -1, -1, false, 0, -1});
    for (int i = 0; i < MAXR; i++) MT.push_back({15 + i, DESTAB, i, -1, false, 0, -1});
    for (int i = 2; i < MAXR; i++) MT.push_back({23 + (i - 2), INV, i, -1, false, 0, 23 + (i - 2)});
    id = 29;
    for (int i = 0; i < MAXR; i++) for (int j = 0; j < MAXR; j++) {
        if (j == i || (i < 2 && j < 2)) continue;
        MT.push_back({id, MUL, i, j, false, 0, id + 1}); MT.push_back({id + 1, MUL, i, j, true, 0, id}); id += 2;
    }
    if (id != 137) { fprintf(stderr, "table bug %d\n", id); exit(1); }
    for (int i = 0; i < MAXR; i++) for (int g = 1; g <= MAXR; g++) {
        if (i < 2 && g <= 2) continue;
        MT.push_back({id, CONJ, i, -1, false, g, id + 1}); MT.push_back({id + 1, CONJ, i, -1, false, -g, id}); id += 2;
    }
    if (id != 257 || (int)MT.size() != 257) { fprintf(stderr, "table bug %d %zu\n", id, MT.size()); exit(1); }
    for (int k = 0; k < 257; k++) if (MT[k].id != k) { fprintf(stderr, "table id bug %d\n", k); exit(1); }
}

// forward move; false when not applicable or over the caps
static bool apply(const GS& s, int m, GS& o, int maxrank, int cap) {
    const Move& mv = MT[m]; int k = s.rank();
    switch (mv.cat) {
    case STAB:
        if (k >= maxrank) return false;
        o = s; o.r.push_back({(int8_t)(k + 1)}); return true;
    case DESTAB: {
        if (mv.i >= k) return false;
        const auto& w = s.r[mv.i];
        if (w.size() != 1 || w[0] <= 0) return false;
        int8_t g = w[0];
        for (int j = 0; j < k; j++) { if (j == mv.i) continue; for (int8_t a : s.r[j]) if (a == g || a == -g) return false; }
        o.r.clear();
        for (int j = 0; j < k; j++) { if (j == mv.i) continue; vector<int8_t> v(s.r[j]); for (auto& a : v) a = a > g ? a - 1 : (a < -g ? a + 1 : a); o.r.push_back(std::move(v)); }
        return true; }
    case INV:
        if (mv.i >= k) return false;
        o = s; o.r[mv.i] = invert(s.r[mv.i]); return true;
    case MUL: {
        if (mv.i >= k || mv.j >= k) return false;
        vector<int8_t> nw = mv.inv_other ? reduce_cat(s.r[mv.i], invert(s.r[mv.j])) : reduce_cat(s.r[mv.i], s.r[mv.j]);
        if (s.tot() - (int)s.r[mv.i].size() + (int)nw.size() > cap) return false;
        o = s; o.r[mv.i] = std::move(nw); return true; }
    case CONJ: {
        if (mv.i >= k || abs(mv.c) > k) return false;
        vector<int8_t> nw; nw.reserve(s.r[mv.i].size() + 2); nw.push_back(mv.c);
        for (int8_t a : s.r[mv.i]) { if (!nw.empty() && nw.back() == -a) nw.pop_back(); else nw.push_back(a); }
        { int8_t a = -mv.c; if (!nw.empty() && nw.back() == -a) nw.pop_back(); else nw.push_back(a); }
        if (s.tot() - (int)s.r[mv.i].size() + (int)nw.size() > cap) return false;
        o = s; o.r[mv.i] = std::move(nw); return true; }
    }
    return false;
}

// predecessors: every (P, m) with apply(P, m) == t. Used by the backward half of the bidirectional search.
static void predecessors(const GS& t, int maxrank, int cap, vector<pair<GS, int>>& out) {
    out.clear(); int k = t.rank(); GS p;
    for (int m = 0; m < 257; m++) {
        const Move& mv = MT[m];
        if (mv.cat == INV || mv.cat == MUL || mv.cat == CONJ) {         // rank-preserving: predecessor = inverse move applied to t
            if (apply(t, mv.inverse, p, maxrank, cap)) out.push_back({p, m});
        } else if (mv.cat == STAB) {                                     // t = p + (k,)
            if (k >= 1 && t.r[k - 1].size() == 1 && t.r[k - 1][0] == k) { p = t; p.r.pop_back(); out.push_back({p, m}); }
        } else {                                                         // DESTAB r_i removed generator g: re-insert (g,) at i after shifting letters >= g up
            int i = mv.i; if (i > k || k + 1 > maxrank) continue;
            for (int g = 1; g <= k + 1; g++) {
                p.r.clear();
                for (int j = 0; j < k; j++) { vector<int8_t> v(t.r[j]); for (auto& a : v) a = a >= g ? a + 1 : (a <= -g ? a - 1 : a); p.r.push_back(std::move(v)); }
                p.r.insert(p.r.begin() + i, vector<int8_t>{(int8_t)g});
                if (p.tot() <= cap) out.push_back({p, m});
            }
        }
    }
}

// ---------------------------------------------------------------- io
struct Prob { string id; GS s; };
static vector<Prob> read_problems(const string& f) {
    vector<Prob> out; ifstream in(f); string line;
    while (getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto bar = line.find('|'); istringstream a(line.substr(0, bar)), b(line.substr(bar + 1));
        Prob p; a >> p.id; int v; vector<int8_t> x, y;
        while (a >> v) x.push_back(v); while (b >> v) y.push_back(v);
        p.s.r = {x, y}; out.push_back(p);
    }
    return out;
}
static map<string, vector<int>> read_paths(const string& f) {   // minimal jsonl: {"id": "...", ..., "moves": [..]}
    map<string, vector<int>> out; ifstream in(f); string line;
    while (getline(in, line)) {
        auto i = line.find("\"id\""); if (i == string::npos) continue;
        auto q1 = line.find('"', line.find(':', i) + 1); auto q2 = line.find('"', q1 + 1); string id = line.substr(q1 + 1, q2 - q1 - 1);
        auto m = line.find("\"moves\""); if (m == string::npos) continue;
        auto lb = line.find('[', m), rb = line.find(']', lb); if (lb == string::npos || rb == string::npos) continue;
        vector<int> mv; string body = line.substr(lb + 1, rb - lb - 1); for (auto& ch : body) if (ch == ',') ch = ' ';
        istringstream ss(body); int v; while (ss >> v) mv.push_back(v);
        if (!out.count(id) || mv.size() < out[id].size()) out[id] = mv;
    }
    return out;
}
static bool replay(const GS& start, const vector<int>& moves, vector<GS>* states, int cap = 100000) {
    GS s = start, t; if (states) { states->clear(); states->push_back(s); }
    for (int m : moves) { if (m < 0 || m >= 257 || !apply(s, m, t, MAXR, cap)) return false; s = t; if (states) states->push_back(s); }
    return s.rank() == 0;
}
static string json_path(const string& id, const vector<int>& mv) {
    string o = "{\"id\": \"" + id + "\", \"solved\": true, \"length\": " + to_string(mv.size()) + ", \"moves\": [";
    for (size_t i = 0; i < mv.size(); i++) { if (i) o += ", "; o += to_string(mv[i]); }
    return o + "]}";
}

// ---------------------------------------------------------------- exact window shortcut (meet in the middle through rank <= R)
struct Opt { int rf = 3, rb = 3, rank = 3, cap = 48, threads = 4, rounds = 8, maxlen = 400, min_save = 1, verbose = 0; };

struct BNode { uint64_t h; int parent; int move; int j; int d; };   // backward tree node: apply(state, move) == state(parent)
// One improvement pass over a path: returns true and rewrites `moves` if a strict shortcut was found (best saving first).
static bool shortcut_pass(const GS& start, vector<int>& moves, const Opt& o, int* saved_out) {
    vector<GS> st; if (!replay(start, moves, &st)) return false;
    int L = st.size() - 1;
    // backward balls around every path state s_j: best (j - d2) per state, with parent pointers for reconstruction
    unordered_map<uint64_t, int> best;  best.reserve(1 << 20);          // hash -> node index with max (j - d)
    vector<BNode> nodes; nodes.reserve(1 << 20);
    vector<GS> nst; nst.reserve(1 << 20);
    vector<pair<GS, int>> preds;
    for (int j = L; j >= 1; j--) {
        vector<int> layer, next;
        { BNode n{hgs(st[j]), -1, -1, j, 0}; auto it = best.find(n.h);
          if (it == best.end() || nodes[it->second].j - nodes[it->second].d < j) { nodes.push_back(n); nst.push_back(st[j]); best[n.h] = nodes.size() - 1; layer.push_back(nodes.size() - 1); } }
        for (int d = 1; d <= o.rb && !layer.empty(); d++) {
            next.clear();
            for (int ni : layer) {
                predecessors(nst[ni], o.rank, o.cap, preds);
                for (auto& [p, m] : preds) {
                    uint64_t h = hgs(p); auto it = best.find(h);
                    if (it != best.end() && nodes[it->second].j - nodes[it->second].d >= j - d) continue;
                    nodes.push_back({h, ni, m, j, d}); nst.push_back(p); best[h] = nodes.size() - 1; next.push_back(nodes.size() - 1);
                }
            }
            layer.swap(next);
        }
    }
    size_t fwd_states = 0;
    // forward balls from every s_i: a state u with best[u] = (j - d2) and (j - d2) - (i + d1) > 0 is a shortcut
    int bsave = 0, bi = -1, bnode = -1; vector<int> bfwd;
    struct FN { int parent; int move; };
    for (int i = 0; i < L; i++) {
        vector<FN> fn{{-1, -1}}; vector<GS> fs{st[i]}; unordered_set<uint64_t> seen{hgs(st[i])};
        vector<int> layer{0}, next; GS t;
        auto check = [&](int idx, int d) {
            auto it = best.find(hgs(fs[idx])); if (it == best.end()) return;
            const BNode& b = nodes[it->second]; int save = (b.j - b.d) - (i + d);
            if (save > bsave) {
                vector<int> fwd; for (int x = idx; fn[x].parent >= 0; x = fn[x].parent) fwd.push_back(fn[x].move); reverse(fwd.begin(), fwd.end());
                bsave = save; bi = i; bnode = it->second; bfwd = fwd;
            }
        };
        check(0, 0);
        for (int d = 1; d <= o.rf && !layer.empty(); d++) {
            next.clear();
            for (int idx : layer) {
                for (int m = 0; m < 257; m++) if (apply(fs[idx], m, t, o.rank, o.cap)) {
                    if (!seen.insert(hgs(t)).second) continue;
                    fs.push_back(t); fn.push_back({idx, m}); next.push_back(fs.size() - 1); check(fs.size() - 1, d);
                }
            }
            layer.swap(next);
        }
        fwd_states += fs.size();
    }
    if (o.verbose > 1) fprintf(stderr, "  L=%d backward nodes=%zu forward states=%zu best save=%d\n", L, nodes.size(), fwd_states, bsave);
    if (bsave < o.min_save) return false;
    // rebuild: moves[0..bi) + fwd + backward chain (node -> parent ... -> s_j) + moves[j..)
    vector<int> nm(moves.begin(), moves.begin() + bi); nm.insert(nm.end(), bfwd.begin(), bfwd.end());
    int j = nodes[bnode].j; for (int x = bnode; nodes[x].parent >= 0; x = nodes[x].parent) nm.push_back(nodes[x].move);
    nm.insert(nm.end(), moves.begin() + j, moves.end());
    if (!replay(start, nm, nullptr) || nm.size() >= moves.size()) { fprintf(stderr, "internal: rebuilt path invalid (save %d)\n", bsave); return false; }
    if (saved_out) *saved_out = moves.size() - nm.size();
    moves = nm; return true;
}

static void selftest() {
    mt19937_64 rng(7); int ok = 0, tested = 0;
    for (int trial = 0; trial < 3000; trial++) {
        GS s; s.r = {{1, 2, -1, -2}, {2, 1, 1, -2}}; GS t;   // freely reduced start
        for (int step = 0; step < 12; step++) { int m = rng() % 257; if (apply(s, m, t, 4, 60)) s = t; }
        for (int m = 0; m < 257; m++) {
            if (!apply(s, m, t, 4, 60)) continue;
            vector<pair<GS, int>> pr; predecessors(t, 4, 60, pr); tested++;
            bool found = false; for (auto& [p, mm] : pr) if (mm == m && p == s) found = true;
            if (found) ok++; else fprintf(stderr, "predecessor miss: move %d cat %d\n", m, MT[m].cat);
        }
    }
    printf("selftest: %d/%d (state, move) pairs recovered by predecessors()\n", ok, tested);
}


// ---------------------------------------------------------------- from-scratch beam in the stable space, exact rank-2 finish table
bool sball_load(const char*); int sball_cap(); bool sball_stable();
int sball_dist(const int8_t*, int, const int8_t*, int); int sball_finish(const int8_t*, int, const int8_t*, int, int*, int);
struct SOpt { int width = 3000, rank = 3, cap = 48, maxdepth = 150, rankpen = 2, threads = 4; double time_limit = 120; uint64_t seed = 1; int verbose = 0; };
static vector<int> stable_beam(const GS& start, const SOpt& o, long long* expanded) {
    struct Node { int parent; int move; };
    vector<vector<Node>> hist(1, vector<Node>{{-1, -1}}); vector<GS> level{start}; unordered_set<uint64_t> seen{hgs(start)};
    vector<int> best; int best_len = 1 << 30; auto t0 = chrono::steady_clock::now(); int bcap = sball_cap();
    auto chain = [&](int g, int idx) { vector<int> p; for (int l = g; l > 0; l--) { p.push_back(hist[l][idx].move); idx = hist[l][idx].parent; } reverse(p.begin(), p.end()); return p; };
    for (int g = 0; g < o.maxdepth && !level.empty() && g + 1 < best_len; g++) {
        if (chrono::duration<double>(chrono::steady_clock::now() - t0).count() > o.time_limit) break;
        struct Cand { GS s; int parent; int move; uint64_t score; };
        vector<Cand> cands; GS t; int fin[64];
        for (size_t idx = 0; idx < level.size(); idx++) {
            const GS& s = level[idx]; (*expanded)++;
            for (int m = 0; m < 257; m++) {
                if (!apply(s, m, t, o.rank, o.cap)) continue;
                uint64_t h = hgs(t); if (!seen.insert(h).second) continue;
                if (t.rank() == 0) { if (g + 1 < best_len) { best = chain(g, idx); best.push_back(m); best_len = best.size(); } continue; }
                if (t.rank() == 2 && t.tot() <= bcap) {           // exact finish through the rank-2 stable table
                    int d = sball_dist(t.r[0].data(), t.r[0].size(), t.r[1].data(), t.r[1].size());
                    if (d >= 0 && g + 1 + d < best_len) {
                        int n = sball_finish(t.r[0].data(), t.r[0].size(), t.r[1].data(), t.r[1].size(), fin, 64);
                        if (n >= 0) { best = chain(g, idx); best.push_back(m); best.insert(best.end(), fin, fin + n); best_len = best.size(); }
                    }
                }
                uint64_t sc = (uint64_t)(t.tot() + o.rankpen * max(0, t.rank() - 2)) << 24 | (mix64(h ^ o.seed) & 0xffffff);
                cands.push_back({std::move(t), (int)idx, m, sc});
            }
        }
        if ((int)cands.size() > o.width) { nth_element(cands.begin(), cands.begin() + o.width, cands.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; }); cands.resize(o.width); }
        level.clear(); hist.emplace_back();
        for (auto& c : cands) { level.push_back(std::move(c.s)); hist.back().push_back({c.parent, c.move}); }
        if (o.verbose && g % 10 == 0) fprintf(stderr, "  g=%d level=%zu best=%d\n", g, level.size(), best_len == (1 << 30) ? -1 : best_len);
    }
    return best;
}

int main(int argc, char** argv) {
    build_table();
    if (argc < 2) { fprintf(stderr, "usage: sacs replay|window|selftest ...\n"); return 1; }
    string cmd = argv[1], problems, paths, out, ball; Opt o; SOpt so;
    for (int i = 2; i + 1 < argc; i += 2) {
        string a = argv[i], v = argv[i + 1];
        if (a == "--problems") problems = v; else if (a == "--paths") paths = v; else if (a == "--out") out = v;
        else if (a == "--rf") o.rf = stoi(v); else if (a == "--rb") o.rb = stoi(v); else if (a == "--rank") o.rank = stoi(v);
        else if (a == "--cap") o.cap = stoi(v); else if (a == "--threads") o.threads = stoi(v); else if (a == "--rounds") o.rounds = stoi(v);
        else if (a == "--maxlen") o.maxlen = stoi(v); else if (a == "--ball") ball = v; else if (a == "--width") so.width = stoi(v);
        else if (a == "--maxdepth") so.maxdepth = stoi(v); else if (a == "--rankpen") so.rankpen = stoi(v); else if (a == "--time-limit") so.time_limit = stod(v); else if (a == "--seed") so.seed = stoull(v); else if (a == "--min-save") o.min_save = stoi(v); else if (a == "--verbose") o.verbose = stoi(v);
        else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
    }
    if (cmd == "selftest") { selftest(); return 0; }
    auto probs = read_problems(problems); auto given = paths.empty() ? map<string, vector<int>>{} : read_paths(paths);
    if (cmd == "solve") {
        if (ball.empty() || !sball_load(ball.c_str()) || !sball_stable()) { fprintf(stderr, "solve needs --ball ball16s.bin (a STABLE table)\n"); return 1; }
        so.rank = o.rank; so.cap = o.cap; so.threads = o.threads; so.verbose = o.verbose;
        FILE* fo = fopen(out.c_str(), "a"); mutex mu; atomic<size_t> next{0}; atomic<int> solved{0}; long long expanded = 0;
        auto worker = [&]() {
            for (size_t k; (k = next++) < probs.size();) {
                auto& p = probs[k]; long long ex = 0; auto mv = stable_beam(p.s, so, &ex);
                bool okr = !mv.empty() && replay(p.s, mv, nullptr);
                lock_guard<mutex> g(mu); expanded += ex;
                if (okr) { solved++; fprintf(fo, "%s\n", json_path(p.id, mv).c_str()); fflush(fo); }
                fprintf(stderr, "%s: %s%zu (given %zu) [%zu/%zu]\n", p.id.c_str(), okr ? "" : "UNSOLVED ", mv.size(), given.count(p.id) ? given[p.id].size() : 0, k + 1, probs.size());
            }
        };
        vector<thread> th; for (int i = 0; i < so.threads; i++) th.emplace_back(worker); for (auto& t : th) t.join();
        fclose(fo); fprintf(stderr, "solve: %d/%zu solved, %lld expansions -> %s\n", solved.load(), probs.size(), expanded, out.c_str()); return 0;
    }
    if (cmd == "replay") {
        int okc = 0, n = 0;
        for (auto& p : probs) { auto it = given.find(p.id); if (it == given.end()) continue; n++;
            if (o.verbose) {   // --verbose 1: dump the final state (any rank) for cross-checking against the official verifier core
                GS s = p.s, t; bool legal = true; for (int m : it->second) { if (!apply(s, m, t, MAXR, 100000)) { legal = false; break; } s = t; }
                printf("%s %s", p.id.c_str(), legal ? "" : "ILLEGAL"); for (auto& w : s.r) { printf(" ["); for (size_t q = 0; q < w.size(); q++) printf("%s%d", q ? "," : "", w[q]); printf("]"); } printf("\n"); continue; }
            bool okr = replay(p.s, it->second, nullptr); okc += okr; if (!okr) printf("%s: FAILS to reach empty\n", p.id.c_str()); }
        if (o.verbose) return 0;
        printf("replay: %d/%d paths reach the empty presentation\n", okc, n); return 0;
    }
    if (cmd == "tail") {   // from the state K moves before the end of each path: can a rank-3 route finish in < K moves?
        if (ball.empty() || !sball_load(ball.c_str()) || !sball_stable()) { fprintf(stderr, "tail needs --ball ball16s.bin\n"); return 1; }
        so.rank = o.rank; so.cap = o.cap; so.threads = o.threads; so.verbose = o.verbose; int K = o.rf;   // --rf doubles as K
        FILE* fo = fopen(out.c_str(), "a"); mutex mu; atomic<size_t> next{0}; atomic<int> improved{0}, tried{0};
        vector<Prob> todo; for (auto& p : probs) if (given.count(p.id) && (int)given[p.id].size() > K + 2) todo.push_back(p);
        auto worker = [&]() {
            for (size_t k; (k = next++) < todo.size();) {
                auto& p = todo[k]; const vector<int>& mv = given[p.id]; vector<GS> st; if (!replay(p.s, mv, &st)) continue;
                int cut = mv.size() - K; SOpt lo = so; lo.maxdepth = K - 1; long long ex = 0;
                if (o.verbose) lo.maxdepth = K;   // --verbose: allow equal length, to confirm the beam finds the known tail at all
                auto t = stable_beam(st[cut], lo, &ex); tried++;
                if (o.verbose) { lock_guard<mutex> g(mu); fprintf(stderr, "%s: K=%d found %s%zu\n", p.id.c_str(), K, t.empty() ? "none " : "", t.size()); }
                if (!t.empty() && (int)t.size() < K) {
                    vector<int> nm(mv.begin(), mv.begin() + cut); nm.insert(nm.end(), t.begin(), t.end());
                    if (replay(p.s, nm, nullptr)) { lock_guard<mutex> g(mu); improved++; fprintf(fo, "%s\n", json_path(p.id, nm).c_str()); fflush(fo);
                        fprintf(stderr, "%s: tail %d -> %zu (total %zu -> %zu)\n", p.id.c_str(), K, t.size(), mv.size(), nm.size()); }
                }
            }
        };
        vector<thread> th; for (int i = 0; i < so.threads; i++) th.emplace_back(worker); for (auto& t : th) t.join();
        fclose(fo); fprintf(stderr, "tail K=%d: %d/%d tails shortened -> %s\n", K, improved.load(), tried.load(), out.c_str()); return 0;
    }
    if (cmd != "window" || out.empty()) { fprintf(stderr, "usage\n"); return 1; }
    FILE* fo = fopen(out.c_str(), "a"); mutex mu; atomic<size_t> next{0}; atomic<int> improved{0}, done{0};
    vector<Prob> todo; for (auto& p : probs) if (given.count(p.id) && (int)given[p.id].size() <= o.maxlen) todo.push_back(p);
    auto t0 = chrono::steady_clock::now();
    auto worker = [&]() {
        for (size_t k; (k = next++) < todo.size();) {
            auto& p = todo[k]; vector<int> mv = given[p.id]; int total = 0, s;
            for (int r = 0; r < o.rounds && shortcut_pass(p.s, mv, o, &s); r++) total += s;
            { lock_guard<mutex> g(mu);
              if (total > 0) { improved++; fprintf(fo, "%s\n", json_path(p.id, mv).c_str()); fflush(fo); }
              done++;
              if (o.verbose || total > 0) fprintf(stderr, "%s: %zu -> %zu (-%d) [%d/%zu, %.0fs]\n", p.id.c_str(), given[p.id].size(), mv.size(), total, done.load(), todo.size(), chrono::duration<double>(chrono::steady_clock::now() - t0).count()); }
        }
    };
    vector<thread> th; for (int i = 0; i < o.threads; i++) th.emplace_back(worker); for (auto& t : th) t.join();
    fclose(fo);
    fprintf(stderr, "window: %d/%zu paths shortened -> %s\n", improved.load(), todo.size(), out.c_str());
    return 0;
}
