// acenum: exhaustive classification of short balanced 2-generator presentations up to AC-equivalence.
//
// States are presentations <x,y | u, v> with u, v nonempty cyclically reduced words, taken modulo
//   (S1) cyclic permutation of either relator          (a conjugation: an AC move sequence)
//   (S2) inversion of either relator                   (an AC move)
//   (S3) swapping the two relators                     (derivable from AC moves)
//   (S4) the 8 "signed letter permutations" h of F(x,y) (x<->y, x->x^-1, y->y^-1), applied to both relators.
// (S1)-(S3) preserve the AC class. (S4) permutes AC classes and fixes the class of the trivial
// presentation <x,y|x,y>, so it preserves the property "AC-trivial".
//
// Edges ("GS moves"): replace u by the cyclic reduction of  rot_i(u) * rot_j(v^s)  (s = +-1), or
// symmetrically replace v. Each edge is realised by explicit official ac-r2-v1 moves (see `cert`).
//
//   acenum bfs   --cap C --start "u|v" [--nosym] --out F       component of a start state (states of total <= C)
//   acenum class --cap C --len L --comp F --out F           classify all det=+-1 classes of total <= L
//   acenum cert  --comp F --need F --out F                  explicit official-move edge certificates
//
// Letters: 0=x 1=X 2=y 3=Y (inverse = ^1). Text words: x X y Y.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace std;
typedef unsigned __int128 u128;
typedef uint64_t u64;

constexpr int MAXW = 29;   // max letters per word (packed key: 5-bit length + 58 bits)
static bool NOSYM = false;
struct Table; static Table* TRIV = nullptr; static bool HIT_TRIV = false; static string HIT_STATE; // if set, (S4) is not used (canonical form only up to S1-S3)

struct Wd { int n = 0; uint8_t c[64]; };

static Wd parse(const string& s) {
    Wd w;
    for (char ch : s) {
        int l = ch == 'x' ? 0 : ch == 'X' ? 1 : ch == 'y' ? 2 : ch == 'Y' ? 3 : -1;
        if (l < 0) continue;
        if (w.n && (w.c[w.n - 1] ^ 1) == l) w.n--; else w.c[w.n++] = l;
    }
    return w;
}
static string str(const Wd& w) { string s; for (int i = 0; i < w.n; i++) s += "xXyY"[w.c[i]]; return s; }
static Wd inv(const Wd& w) { Wd o; o.n = w.n; for (int i = 0; i < w.n; i++) o.c[i] = w.c[w.n - 1 - i] ^ 1; return o; }
static Wd cycred(const Wd& w) {
    int i = 0, j = w.n - 1;
    while (i < j && (w.c[i] ^ 1) == w.c[j]) i++, j--;
    Wd o; o.n = w.n ? j - i + 1 : 0; memcpy(o.c, w.c + i, o.n); return o;
}
static inline u64 packw(const Wd& w) { u64 p = 0; for (int i = 0; i < w.n; i++) p = p << 2 | w.c[i]; return p; }
static inline u64 wkey(int n, u64 p) { return (u64)n << 58 | p; }
static Wd unkey(u64 k) {
    Wd w; w.n = k >> 58;
    for (int i = 0; i < w.n; i++) w.c[i] = (k >> (2 * (w.n - 1 - i))) & 3;
    return w;
}
// Signed letter permutations acting on packed words. h bits: 1 = swap x<->y, 2 = invert x, 4 = invert y.
static inline u64 hmap(u64 p, int n, int h) {
    u64 m = n ? (~0ULL >> (64 - 2 * n)) : 0, lo = 0x5555555555555555ULL & m;
    if (h & 1) p ^= (lo << 1);                   // flip generator bit
    if (h & 2) p ^= (~p >> 1) & lo;              // letters with generator bit 0 (x): flip inverse bit
    if (h & 4) p ^= (p >> 1) & lo;               // letters with generator bit 1 (y)
    return p;
}
static inline u64 invp(u64 p, int n) {           // packed inverse word
    u64 o = 0;
    for (int i = 0; i < n; i++) { o = o << 2 | ((p & 3) ^ 1); p >>= 2; }
    return o;
}
// Least key among rotations of the (cyclically reduced) word and of its inverse.
static inline u64 cyccanon(u64 p, int n) {
    if (n == 0) return 0;
    u64 m = ~0ULL >> (64 - 2 * n), best = ~0ULL;
    u64 q = invp(p, n);
    for (int i = 0; i < n; i++) {
        best = min(best, p); best = min(best, q);
        p = ((p << 2) | (p >> (2 * n - 2))) & m;
        q = ((q << 2) | (q >> (2 * n - 2))) & m;
    }
    return wkey(n, best);
}
struct Key { u64 a, b; bool operator<(const Key& o) const { return a != o.a ? a < o.a : b < o.b; } bool operator==(const Key& o) const { return a == o.a && b == o.b; } };
static Key canon(const Wd& u, const Wd& v, int* hbest = nullptr) {
    u64 pu = packw(u), pv = packw(v);
    Key best{~0ULL, ~0ULL};
    for (int h = 0; h < (NOSYM ? 1 : 8); h++) {
        u64 a = cyccanon(hmap(pu, u.n, h), u.n), b = cyccanon(hmap(pv, v.n, h), v.n);
        Key k{min(a, b), max(a, b)};
        if (k < best) { best = k; if (hbest) *hbest = h; }
    }
    return best;
}
static int expo(const Wd& w, int g) { int e = 0; for (int i = 0; i < w.n; i++) if ((w.c[i] >> 1) == g) e += (w.c[i] & 1) ? -1 : 1; return e; }
static int detw(const Wd& u, const Wd& v) { return expo(u, 0) * expo(v, 1) - expo(u, 1) * expo(v, 0); }

// ------------------------------------------------------------------ hash table: Key -> node index
struct Table {
    vector<Key> keys; vector<uint32_t> par; vector<uint32_t> slots; u64 mask = 0;
    void init(size_t cap) { size_t c = 1; while (c < cap * 2) c <<= 1; slots.assign(c, UINT32_MAX); mask = c - 1; }
    static u64 hh(const Key& k) { u64 x = k.a * 0x9E3779B97F4A7C15ULL ^ (k.b + 0x632BE59BD9B4E019ULL); x ^= x >> 31; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 29; return x; }
    int64_t find(const Key& k) const {
        u64 i = hh(k) & mask;
        while (slots[i] != UINT32_MAX) { if (keys[slots[i]] == k) return slots[i]; i = (i + 1) & mask; }
        return -1;
    }
    bool insert(const Key& k, uint32_t p) {
        if ((keys.size() + 1) * 10 > slots.size() * 7) {
            vector<uint32_t> o; o.swap(slots); slots.assign(o.size() * 2, UINT32_MAX); mask = slots.size() - 1;
            for (uint32_t id = 0; id < keys.size(); id++) { u64 i = hh(keys[id]) & mask; while (slots[i] != UINT32_MAX) i = (i + 1) & mask; slots[i] = id; }
        }
        u64 i = hh(k) & mask;
        while (slots[i] != UINT32_MAX) { if (keys[slots[i]] == k) return false; i = (i + 1) & mask; }
        slots[i] = keys.size(); keys.push_back(k); par.push_back(p); return true;
    }
};

// All GS neighbours of (u, v): calls f(w, v_rot_or_inverted_used, i, s, j, which) for each result.
// which = 0: u replaced by cycred(rot_i(u) * rot_j(v^s)); which = 1: v replaced by cycred(rot_i(v) * rot_j(u^s)).
template <class F>
static void neighbours(const Wd& u, const Wd& v, int cap, F f) {
    for (int which = 0; which < 2; which++) {
        const Wd& A = which ? v : u; const Wd& B0 = which ? u : v;
        if (A.n + B0.n - 0 > 0) {}
        for (int s = 0; s < 2; s++) {
            Wd B = s ? inv(B0) : B0;
            for (int i = 0; i < A.n; i++) {
                for (int j = 0; j < B.n; j++) {
                    // word = A[i..] A[..i) B[j..] B[..j)
                    uint8_t buf[128]; int n = 0;
                    for (int t = 0; t < A.n; t++) buf[n++] = A.c[(i + t) % A.n];
                    for (int t = 0; t < B.n; t++) {
                        uint8_t l = B.c[(j + t) % B.n];
                        if (n && (buf[n - 1] ^ 1) == l) n--; else buf[n++] = l;
                    }
                    int lo = 0, hi = n - 1;
                    while (lo < hi && (buf[lo] ^ 1) == buf[hi]) lo++, hi--;
                    int m = n ? hi - lo + 1 : 0;
                    if (m == 0 || m + B0.n > cap || m > MAXW) continue;
                    Wd w; w.n = m; memcpy(w.c, buf + lo, m);
                    f(w, which, i, s, j);
                }
            }
        }
    }
}

static void save_comp(const Table& T, int cap, const string& out) {
    FILE* fp = fopen(out.c_str(), "wb");
    u64 hdr[3] = {(u64)cap, (u64)NOSYM, T.keys.size()};
    fwrite(hdr, 8, 3, fp); fwrite(T.keys.data(), sizeof(Key), T.keys.size(), fp); fwrite(T.par.data(), 4, T.par.size(), fp); fclose(fp);
}
static int load_comp(Table& T, const string& f) {
    FILE* fp = fopen(f.c_str(), "rb"); if (!fp) { fprintf(stderr, "cannot open %s\n", f.c_str()); exit(1); }
    u64 hdr[3]; if (fread(hdr, 8, 3, fp) != 3) exit(1);
    NOSYM = hdr[1];
    vector<Key> k(hdr[2]); vector<uint32_t> p(hdr[2]);
    if (fread(k.data(), sizeof(Key), k.size(), fp) != k.size() || fread(p.data(), 4, p.size(), fp) != p.size()) exit(1);
    fclose(fp);
    T.init(k.size() + 16); T.keys.reserve(k.size()); T.par.reserve(k.size());
    for (size_t i = 0; i < k.size(); i++) T.insert(k[i], p[i]);
    return hdr[0];
}

static void parse_pair(const string& s, Wd& u, Wd& v) {
    size_t b = s.find('|'); u = cycred(parse(s.substr(0, b))); v = cycred(parse(s.substr(b + 1)));
}

static bool bfs_table(Table& T, int cap, const string& start, u64 maxnodes, bool verbose);
static void bfs(int cap, const string& start, const string& out, u64 maxnodes) {
    Table T; bfs_table(T, cap, start, maxnodes, true);
    save_comp(T, cap, out);
}
static bool bfs_table(Table& T, int cap, const string& start, u64 maxnodes, bool verbose) {
    auto t0 = chrono::steady_clock::now();
    Wd u, v; parse_pair(start, u, v);
    T.init(1 << 20);
    T.insert(canon(u, v), UINT32_MAX);
    size_t head = 0; int depth = 0; size_t lvl_end = 1;
    while (head < T.keys.size()) {
        if (head == lvl_end) {
            depth++; lvl_end = T.keys.size();
            double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
            if (verbose) fprintf(stderr, "  depth %d: %zu nodes (%.0fs)\n", depth, T.keys.size(), el);
        }
        Key k = T.keys[head]; uint32_t id = head++;
        Wd a = unkey(k.a), b = unkey(k.b);
        neighbours(a, b, cap, [&](const Wd& w, int which, int, int, int) {
            if (HIT_TRIV) return;
            Key nk = which ? canon(a, w) : canon(w, b);
            if (T.insert(nk, id) && TRIV && TRIV->find(nk) >= 0) { HIT_TRIV = true; HIT_STATE = str(unkey(nk.a)) + " " + str(unkey(nk.b)); }
        });
        if (HIT_TRIV) { fprintf(stderr, "reached the trivial component at %s\n", HIT_STATE.c_str()); return false; }
        if (T.keys.size() > maxnodes) { fprintf(stderr, "node limit reached\n"); break; }
    }
    double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
    fprintf(stderr, "bfs cap=%d nosym=%d start=%s: %zu nodes, complete=%d, %.0fs\n", cap, NOSYM, start.c_str(), T.keys.size(), head == T.keys.size(), el);
    return head == T.keys.size();
}
// Partition a list of presentations ("t u v ..." lines) into capped components: repeated BFS from the first unlabelled one.
static void label(int cap, const string& inf, const string& out, u64 maxnodes) {
    vector<pair<Wd, Wd>> P; vector<string> lines;
    FILE* in = fopen(inf.c_str(), "r"); char t[16], a[128], b[128];
    while (fscanf(in, "%15s %127s %127s%*[^\n]", t, a, b) == 3) { Wd u = parse(a), v = parse(b); P.push_back({u, v}); lines.push_back(string(t) + " " + a + " " + b); }
    fclose(in);
    vector<int> lab(P.size(), -1); int L = 0;
    FILE* fp = fopen(out.c_str(), "w");
    for (size_t i = 0; i < P.size(); i++) {
        if (lab[i] >= 0) continue;
        Table T; HIT_TRIV = false; bool complete = bfs_table(T, cap, str(P[i].first) + "|" + str(P[i].second), maxnodes, false);
        if (HIT_TRIV) { fprintf(fp, "# seed %s: AC-TRIVIAL (reaches trivial component at %s)\n", lines[i].c_str(), HIT_STATE.c_str()); fflush(fp); }
        int cnt = 0;
        for (size_t j = i; j < P.size(); j++) if (lab[j] < 0 && T.find(canon(P[j].first, P[j].second)) >= 0) { lab[j] = L; cnt++; }
        fprintf(fp, "# component %d: seed %s, %zu states (complete=%d), %d listed members\n", L, lines[i].c_str(), T.keys.size(), complete, cnt);
        fflush(fp); L++;
    }
    for (size_t i = 0; i < P.size(); i++) fprintf(fp, "%d %s\n", lab[i], lines[i].c_str());
    fclose(fp);
}

// Necklace representatives: cyclically reduced words of length n that are least among their rotations and inverse rotations.
static void necklaces(int n, vector<u64>& out) {
    Wd w; w.n = n;
    // iterative DFS over freely reduced words
    vector<int> st(n, -1); int d = 0;
    while (d >= 0) {
        st[d]++;
        if (st[d] > 3) { st[d] = -1; d--; continue; }
        if (d && (w.c[d - 1] ^ 1) == st[d]) continue;
        w.c[d] = st[d];
        if (d == n - 1) {
            if (n > 1 && (w.c[0] ^ 1) == w.c[n - 1]) continue;
            u64 p = packw(w);
            if (cyccanon(p, n) == wkey(n, p)) out.push_back(wkey(n, p));
            continue;
        }
        d++;
    }
}

static void classify(int L, const string& compf, const string& out) {
    Table T; int cap = load_comp(T, compf);
    fprintf(stderr, "component %s: cap=%d nosym=%d nodes=%zu\n", compf.c_str(), cap, NOSYM, T.keys.size());
    vector<vector<u64>> nk(L + 1);
    for (int n = 1; n < L; n++) { necklaces(n, nk[n]); fprintf(stderr, "  necklaces(%d) = %zu\n", n, nk[n].size()); }
    FILE* fp = fopen(out.c_str(), "w");
    vector<long long> classes(L + 1), triv(L + 1);
    for (int t = 2; t <= L; t++) {
        for (int a = 1; 2 * a <= t; a++) {
            int b = t - a;
            for (size_t i = 0; i < nk[a].size(); i++) {
                Wd u = unkey(nk[a][i]); int ux = expo(u, 0), uy = expo(u, 1);
                for (size_t j = (a == b ? i : 0); j < nk[b].size(); j++) {
                    Wd v = unkey(nk[b][j]);
                    int det = ux * expo(v, 1) - uy * expo(v, 0);
                    if (det != 1 && det != -1) continue;
                    Key k = canon(u, v);
                    Key self{min(nk[a][i], nk[b][j]), max(nk[a][i], nk[b][j])};
                    if (!(k == self)) continue;          // not the class representative
                    classes[t]++;
                    bool tr = T.find(k) >= 0;
                    if (tr) triv[t]++;
                    fprintf(fp, "%d %s %s %d\n", t, str(u).c_str(), str(v).c_str(), tr ? 1 : 0);
                }
            }
        }
        fprintf(stderr, "length %d: %lld det+-1 classes, %lld in component, %lld residual\n", t, classes[t], triv[t], classes[t] - triv[t]);
    }
    fclose(fp);
}

// ------------------------------------------------------------------ certificates
// Official ac-r2-v1 move ids: 0 inv r0, 1 inv r1, 2 r0*=r1, 3 r0*=r1^-1, 4 r1*=r0, 5 r1*=r0^-1,
// 6..9 r0 <- g r0 g^-1 with g = x, X, y, Y; 10..13 same on r1.
static int conjmove(int rel, int letter) { return (rel ? 10 : 6) + letter; }
// Rotate relator `rel` (cyclically reduced, currently w) left by k letters: w = a w' -> w' a is conjugation by a^-1.
static void rot_moves(int rel, Wd& w, int k, vector<int>& mv) {
    for (int t = 0; t < k; t++) {
        uint8_t a = w.c[0];
        mv.push_back(conjmove(rel, a ^ 1));
        memmove(w.c, w.c + 1, w.n - 1); w.c[w.n - 1] = a;
    }
}
// Moves turning relator `rel` (currently w, cyclically reduced) into exactly target t, which must be a rotation of w or w^-1.
static bool to_target(int rel, Wd& w, const Wd& t, vector<int>& mv) {
    if (w.n != t.n) return false;
    for (int s = 0; s < 2; s++) {
        Wd x = s ? inv(w) : w;
        for (int k = 0; k < x.n; k++) {
            bool ok = true;
            for (int q = 0; q < x.n && ok; q++) ok = x.c[(k + q) % x.n] == t.c[q];
            if (!ok) continue;
            if (s) { mv.push_back(rel); w = inv(w); }
            rot_moves(rel, w, k, mv);
            return true;
        }
    }
    return false;
}
static Wd happly(const Wd& w, int h) { Wd o = w; u64 p = hmap(packw(w), w.n, h); for (int i = 0; i < w.n; i++) o.c[i] = (p >> (2 * (w.n - 1 - i))) & 3; return o; }
static int hinv(int h) {   // inverse of the letter map h (as a group element); brute force
    for (int g = 0; g < 8; g++) { bool ok = true; for (int l = 0; l < 4 && ok; l++) { Wd w; w.n = 1; w.c[0] = l; ok = happly(happly(w, h), g).c[0] == l; } if (ok) return g; }
    return -1;
}

// Certificate for a tree edge parent P -> node N (N was discovered as a GS-neighbour of P):
// explicit official moves taking the exact state P = (pu, pv) to a state Q, and a letter map h with
// h(Q) = N exactly (after swapping the two relators if swp = 1). Line format:
//   EDGE nu nv pu pv h swp m1 m2 ...
// Emits "EDGE nu nv pu pv h swp moves..." proving the tree edge parent pk -> child ck.
static bool emit_edge(FILE* fp, const Key& pk, const Key& ck) {
    Wd nu = unkey(ck.a), nv = unkey(ck.b), pu = unkey(pk.a), pv = unkey(pk.b);
    bool done = false;
    neighbours(pu, pv, 64, [&](const Wd& w, int which, int i, int s, int j) {
        if (done) return;
        Wd qu = which ? pu : w, qv = which ? w : pv;
        int hb = 0; Key k = canon(qu, qv, &hb);
        if (!(k == ck)) return;
        vector<int> mv; Wd r[2] = {pu, pv};
        int A = which, B = 1 - which;
        rot_moves(A, r[A], i, mv);
        if (s) { mv.push_back(B); r[B] = inv(r[B]); }
        rot_moves(B, r[B], j, mv);
        mv.push_back(A == 0 ? 2 : 4);                // r_A <- r_A * r_B (freely reduced)
        Wd prod; prod.n = 0;
        for (int q = 0; q < r[A].n; q++) { uint8_t l = r[A].c[q]; if (prod.n && (prod.c[prod.n - 1] ^ 1) == l) prod.n--; else prod.c[prod.n++] = l; }
        for (int q = 0; q < r[B].n; q++) { uint8_t l = r[B].c[q]; if (prod.n && (prod.c[prod.n - 1] ^ 1) == l) prod.n--; else prod.c[prod.n++] = l; }
        while (prod.n > 1 && (prod.c[0] ^ 1) == prod.c[prod.n - 1]) {   // cyclic reduction by conjugation
            mv.push_back(conjmove(A, prod.c[0] ^ 1));
            memmove(prod.c, prod.c + 1, prod.n - 2); prod.n -= 2;
        }
        r[A] = prod;
        int hi = hinv(hb);
        Wd q0 = happly(nu, hi), q1 = happly(nv, hi);   // Q must equal h^-1(N)
        int swp = -1;
        vector<int> m2 = mv; Wd s0 = r[0], s1 = r[1];
        if (to_target(0, s0, q0, m2) && to_target(1, s1, q1, m2)) { mv = m2; swp = 0; }
        else {
            m2 = mv; s0 = r[0]; s1 = r[1];
            if (to_target(0, s0, q1, m2) && to_target(1, s1, q0, m2)) { mv = m2; swp = 1; }
        }
        if (swp < 0) return;
        fprintf(fp, "EDGE %s %s %s %s %d %d", str(nu).c_str(), str(nv).c_str(), str(pu).c_str(), str(pv).c_str(), hb, swp);
        for (int m : mv) fprintf(fp, " %d", m);
        fprintf(fp, "\n");
        done = true;
    });
    if (!done) { fprintf(stderr, "FAILED to certify edge for %s %s\n", str(nu).c_str(), str(nv).c_str()); exit(2); }
    return true;
}
// Mark the tree path from node id to the root of T.
static void mark(const Table& T, int64_t id, vector<char>& need) {
    while (id >= 0 && !need[id]) { need[id] = 1; uint32_t p = T.par[id]; id = p == UINT32_MAX ? -1 : (int64_t)p; }
}
static long long emit_marked(FILE* fp, const Table& T, const vector<char>& need, const char* roottag, const string& rootname = "") {
    long long cnt = 0;
    for (size_t id = 0; id < T.keys.size(); id++) {
        if (!need[id]) continue;
        if (T.par[id] == UINT32_MAX) { fprintf(fp, "%s %s %s%s%s\n", roottag, str(unkey(T.keys[id].a)).c_str(), str(unkey(T.keys[id].b)).c_str(), rootname.empty() ? "" : " ", rootname.c_str()); cnt++; continue; }
        emit_edge(fp, T.keys[T.par[id]], T.keys[id]); cnt++;
    }
    return cnt;
}
// cert: tree edges of a stored component (rooted at (x,y)) for all listed presentations it contains.
static void cert(const string& compf, const string& needf, const string& out) {
    Table T; load_comp(T, compf);
    vector<char> need(T.keys.size(), 0);
    FILE* in = fopen(needf.c_str(), "r"); char a[128], b[128];
    long long missing = 0;
    while (fscanf(in, "%127s %127s%*[^\n]", a, b) == 2) {
        int64_t id = T.find(canon(parse(a), parse(b)));
        if (id < 0) { missing++; continue; }
        mark(T, id, need);
    }
    fclose(in);
    FILE* fp = fopen(out.c_str(), "w");
    long long cnt = emit_marked(fp, T, need, "ROOT");
    fclose(fp);
    fprintf(stderr, "certified %lld nodes, %lld requested classes not in component\n", cnt, missing);
}
// compcert: BFS from --start at --cap (stopping early if the trivial component --comp is reached);
// emits tree edges from the start to every listed presentation found (and to the meeting state).
// The start is written as a "SEED" line. Listed members not reached are reported.
static void compcert(int cap, const string& start, const string& needf, const string& out, u64 maxnodes, const string& hitneed) {
    Table T; HIT_TRIV = false;
    bool complete = bfs_table(T, cap, start, maxnodes, false);
    vector<char> need(T.keys.size(), 0);
    long long found = 0, missing = 0;
    if (!needf.empty()) {
        FILE* in = fopen(needf.c_str(), "r"); char t[16], a[128], b[128];
        while (fscanf(in, "%15s %127s %127s%*[^\n]", t, a, b) == 3) {
            int64_t id = T.find(canon(parse(a), parse(b)));
            if (id < 0) { missing++; continue; }
            found++; mark(T, id, need);
        }
        fclose(in);
    }
    if (HIT_TRIV) {
        Wd hu = parse(HIT_STATE.substr(0, HIT_STATE.find(' '))), hv = parse(HIT_STATE.substr(HIT_STATE.find(' ') + 1));
        mark(T, T.find(canon(hu, hv)), need);
        FILE* h = fopen(hitneed.c_str(), "a"); fprintf(h, "%s\n", HIT_STATE.c_str()); fclose(h);
    }
    FILE* fp = fopen(out.c_str(), "a");
    long long cnt = emit_marked(fp, T, need, "SEED");
    fclose(fp);
    fprintf(stderr, "compcert start=%s cap=%d: %zu states complete=%d hit_trivial=%d; %lld listed found, %lld not found; %lld lines\n",
            start.c_str(), cap, T.keys.size(), complete, HIT_TRIV, found, missing, cnt);
}

// resolve: partition residual presentations (file --need, lines "t u v ...") into capped components.
// Seeds are taken first from --seeds (lines "t u v name"), then from the first unresolved residual.
// Each BFS stops early if it reaches the trivial component (--comp). Emits SEED/EDGE certificates to --out
// and the meeting states to --out.hits (to be certified from the trivial component with `cert`).
static void resolve(int cap, const string& seedf, const string& needf, const string& out, u64 maxnodes) {
    struct P { string t, a, b, name; Key k; int lab = -1; };
    auto rd = [](const string& f, bool named) {
        vector<P> v; FILE* in = fopen(f.c_str(), "r"); char line[512];
        while (fgets(line, sizeof line, in)) {
            char t[16], a[128], b[128], nm[128] = "";
            int k = sscanf(line, "%15s %127s %127s %127s", t, a, b, nm);
            if (k < 3) continue;
            P p; p.t = t; p.a = a; p.b = b; p.name = named ? nm : ""; p.k = canon(parse(a), parse(b)); v.push_back(p);
        }
        fclose(in); return v;
    };
    vector<P> seeds = rd(seedf, true), res = rd(needf, false);
    FILE* fp = fopen(out.c_str(), "w"); fclose(fp);
    FILE* sum = fopen((out + ".summary").c_str(), "w");
    FILE* hits = fopen((out + ".hits").c_str(), "w");
    size_t si = 0; int L = 0;
    while (true) {
        P seed;
        if (si < seeds.size()) seed = seeds[si++];
        else {
            size_t i = 0; while (i < res.size() && res[i].lab != -1) i++;
            if (i == res.size()) break;
            seed = res[i]; seed.name = "R" + to_string(L);
        }
        Table T; HIT_TRIV = false;
        bool complete = bfs_table(T, cap, seed.a + "|" + seed.b, maxnodes, false);
        vector<char> need(T.keys.size(), 0);
        int found = 0;
        for (auto& r : res) if (r.lab == -1) { int64_t id = T.find(r.k); if (id >= 0) { r.lab = HIT_TRIV ? -2 : L; found++; mark(T, id, need); } }
        int64_t sid = T.find(seed.k);
        for (auto& r : res) if (r.lab == -1 && r.k == seed.k) r.lab = HIT_TRIV ? -2 : L;
        mark(T, sid, need);
        if (HIT_TRIV) {
            Wd hu = parse(HIT_STATE.substr(0, HIT_STATE.find(' '))), hv = parse(HIT_STATE.substr(HIT_STATE.find(' ') + 1));
            mark(T, T.find(canon(hu, hv)), need);
            fprintf(hits, "%s\n", HIT_STATE.c_str()); fflush(hits);
        }
        FILE* f2 = fopen(out.c_str(), "a"); emit_marked(f2, T, need, "SEED", seed.name); fclose(f2);
        fprintf(sum, "component %d name=%s seed=%s %s %s states=%zu complete=%d trivial=%d residual_members=%d\n",
                L, seed.name.c_str(), seed.t.c_str(), seed.a.c_str(), seed.b.c_str(), T.keys.size(), complete, HIT_TRIV, found);
        fflush(sum);
        fprintf(stderr, "component %d %s: %zu states, trivial=%d, members=%d\n", L, seed.name.c_str(), T.keys.size(), HIT_TRIV, found);
        L++;
    }
    for (auto& r : res) fprintf(sum, "member %s %s %s %d\n", r.t.c_str(), r.a.c_str(), r.b.c_str(), r.lab);
    fclose(sum); fclose(hits);
}


// ---------------------------------------------------------------- weighted class distances (for acsolver --gs)
// Dijkstra from the trivial class over the component table with realistic AC move costs per GS edge:
// rotate A by i (min(i, |A|-i) conjugations) + rotate B by j (same) + 1 multiply + cyclic cleanup ((n - m) / 2 conjugations).
static void wdist(const string& comp, const string& out) {
    Table T; int cap = load_comp(T, comp);
    size_t N = T.keys.size(); vector<uint16_t> D(N, 65535);
    vector<vector<uint32_t>> bucket(4096);
    int64_t t0 = T.find(canon(parse("x"), parse("y")));
    if (t0 < 0) { fprintf(stderr, "trivial class missing\n"); exit(1); }
    D[t0] = 0; bucket[0].push_back(t0);
    size_t done = 0;
    for (int d = 0; d < 4096; d++) {
        for (size_t q = 0; q < bucket[d].size(); q++) {
            uint32_t id = bucket[d][q]; if (D[id] != d) continue; done++;
            Wd a = unkey(T.keys[id].a), b = unkey(T.keys[id].b);
            for (int which = 0; which < 2; which++) {
                const Wd& A = which ? b : a; const Wd& B0 = which ? a : b;
                for (int s = 0; s < 2; s++) {
                    Wd B = s ? inv(B0) : B0;
                    for (int i = 0; i < A.n; i++) for (int j = 0; j < B.n; j++) {
                        uint8_t buf[128]; int n = 0;
                        for (int t = 0; t < A.n; t++) buf[n++] = A.c[(i + t) % A.n];
                        for (int t = 0; t < B.n; t++) { uint8_t l = B.c[(j + t) % B.n]; if (n && (buf[n - 1] ^ 1) == l) n--; else buf[n++] = l; }
                        int lo = 0, hi = n - 1; while (lo < hi && (buf[lo] ^ 1) == buf[hi]) lo++, hi--;
                        int m = n ? hi - lo + 1 : 0;
                        if (m == 0 || m + B0.n > cap || m > MAXW) continue;
                        Wd w; w.n = m; memcpy(w.c, buf + lo, m);
                        Key nk = which ? canon(a, w) : canon(w, b);
                        int64_t nid = T.find(nk); if (nid < 0) continue;
                        int c = min(i, A.n - i) + min(j, B.n - j) + 1 + (n - m) / 2;
                        int nd = d + c; if (nd >= 4096) continue;
                        if (nd < D[nid]) { D[nid] = nd; bucket[nd].push_back((uint32_t)nid); }
                    }
                }
            }
        }
        vector<uint32_t>().swap(bucket[d]);
        if (d % 10 == 0) fprintf(stderr, "  dist %d: %zu settled\n", d, done);
    }
    // write in acsolver GsTable format: sorted keys, uint8 dist, mean per total length
    vector<size_t> ord(N); for (size_t i = 0; i < N; i++) ord[i] = i;
    sort(ord.begin(), ord.end(), [&](size_t x, size_t y) { return T.keys[x] < T.keys[y]; });
    vector<double> sum(64, 0), cnt(64, 0);
    for (size_t i = 0; i < N; i++) if (D[i] < 65535) { int L = (T.keys[i].a >> 58) + (T.keys[i].b >> 58); sum[L] += D[i]; cnt[L]++; }
    vector<double> mean(64, 0); for (int L = 0; L < 64; L++) mean[L] = cnt[L] ? sum[L] / cnt[L] : 0;
    FILE* fo = fopen(out.c_str(), "wb"); u64 h2[2] = {(u64)cap, N}; fwrite(h2, 8, 2, fo);
    for (size_t i : ord) fwrite(&T.keys[i], sizeof(Key), 1, fo);
    for (size_t i : ord) { uint8_t v = D[i] > 255 ? 255 : D[i]; fwrite(&v, 1, 1, fo); }
    fwrite(mean.data(), 8, 64, fo); fclose(fo);
    for (int L = 10; L <= cap; L++) if (cnt[L]) fprintf(stderr, "  len %2d: mean weighted dist %.1f\n", L, mean[L]);
    fprintf(stderr, "weighted table -> %s (%zu classes, %zu settled)\n", out.c_str(), N, done);
}
int main(int argc, char** argv) {
    if (argc >= 4 && string(argv[1]) == "wdist") { wdist(argv[2], argv[3]); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: acenum bfs|class|cert ...\n"); return 1; }
    string cmd = argv[1], start = "x|y", out = "/dev/stdout", comp, need;
    int cap = 16, L = 14; u64 maxnodes = 60000000;
    for (int i = 2; i < argc; i++) {
        string a = argv[i];
        auto nx = [&]() { return string(argv[++i]); };
        if (a == "--cap") cap = stoi(nx());
        else if (a == "--len") L = stoi(nx());
        else if (a == "--start") start = nx();
        else if (a == "--out") out = nx();
        else if (a == "--comp") comp = nx();
        else if (a == "--need") need = nx();
        else if (a == "--nosym") NOSYM = true;
        else if (a == "--maxnodes") maxnodes = stoull(nx());
        else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
    }
    if (cap > MAXW + 1) { fprintf(stderr, "cap too large\n"); return 1; }
    if (cmd == "bfs") bfs(cap, start, out, maxnodes);
    else if (cmd == "class") classify(L, comp, out);
    else if (cmd == "cert") cert(comp, need, out);
    else if (cmd == "compcert") {
        static Table TT;
        if (!comp.empty()) { bool ns = NOSYM; load_comp(TT, comp); NOSYM = ns; TRIV = &TT; }
        compcert(cap, start, need, out, maxnodes, out + ".hits");
    }
    else if (cmd == "resolve") {
        static Table TT;
        if (!comp.empty()) { bool ns = NOSYM; load_comp(TT, comp); NOSYM = ns; TRIV = &TT; }
        resolve(cap, start, need, out, maxnodes);
    }
    else if (cmd == "label") {
        static Table TT;
        if (!comp.empty()) { bool ns = NOSYM; load_comp(TT, comp); NOSYM = ns; TRIV = &TT; }
        label(cap, need, out, maxnodes);
    }
    else { fprintf(stderr, "unknown command\n"); return 1; }
    return 0;
}
