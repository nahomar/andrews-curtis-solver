// grp: decide (when possible) whether <x,y | u, v> presents the trivial group.
//
//   grp < residuals.txt > groups.txt       input lines "t u v ..." (words over x X y Y)
//
// Method, for a presentation with exponent-sum determinant +-1 (trivial abelianisation):
//  1. Todd-Coxeter (HLT + coincidences) coset enumeration over H = <g> for g in {x, y, xy, xY}.
//     index 1  =>  G = <g> is cyclic; cyclic + perfect => G = 1           (TRIVIAL, via coset enumeration)
//     index k>1 => the action on cosets is a transitive permutation representation of degree k;
//                  we print the two permutations as an independently checkable certificate (NONTRIVIAL).
//  2. If every enumeration exceeds the coset limit: exhaustive search for a homomorphism onto a
//     nontrivial subgroup of S_n (n = 5..8), images of x up to conjugacy (NONTRIVIAL, with certificate).
//  3. Otherwise UNKNOWN.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <numeric>
#include <string>
#include <vector>
using namespace std;

static vector<int> parse(const string& s) {
    vector<int> w;
    for (char c : s) { int l = c == 'x' ? 0 : c == 'X' ? 1 : c == 'y' ? 2 : c == 'Y' ? 3 : -1; if (l >= 0) w.push_back(l); }
    return w;
}

struct TC {
    int limit; vector<int> tab;   // tab[4*c + g], 0 = undefined; cosets 1..n
    vector<int> p; vector<int> q; int n = 0; bool overflow = false;
    TC(int lim) : limit(lim) { tab.assign(4 * (lim + 2), 0); p.resize(lim + 2); }
    int& T(int c, int g) { return tab[4 * c + g]; }
    int rep(int k) { int l = k; while (p[l] != l) l = p[l]; while (p[k] != k) { int m = p[k]; p[k] = l; k = m; } return l; }
    void merge(int k, int l) { k = rep(k); l = rep(l); if (k == l) return; int m = min(k, l), nn = max(k, l); p[nn] = m; q.push_back(nn); }
    void coincidence(int a, int b) {
        q.clear(); merge(a, b);
        for (size_t i = 0; i < q.size(); i++) {
            int g0 = q[i];
            for (int x = 0; x < 4; x++) {
                int d = T(g0, x); if (!d) continue;
                T(d, x ^ 1) = 0;
                int mu = rep(g0), nu = rep(d);
                if (T(mu, x)) merge(nu, T(mu, x));
                else if (T(nu, x ^ 1)) merge(mu, T(nu, x ^ 1));
                else { T(mu, x) = nu; T(nu, x ^ 1) = mu; }
            }
        }
    }
    bool define(int a, int x) {
        if (n >= limit) { overflow = true; return false; }
        n++; p[n] = n; memset(&tab[4 * n], 0, 16);
        T(a, x) = n; T(n, x ^ 1) = a; return true;
    }
    bool live(int c) { return p[c] == c; }
    void scan_fill(int a, const vector<int>& w) {
        int f = a, b = a, i = 0, j = (int)w.size() - 1;
        while (true) {
            while (i <= j && T(f, w[i])) f = T(f, w[i++]);
            if (i > j) { if (f != b) coincidence(f, b); return; }
            while (j >= i && T(b, w[j] ^ 1)) b = T(b, w[j--] ^ 1);
            if (j < i) { coincidence(f, b); return; }
            if (i == j) { T(f, w[i]) = b; T(b, w[i] ^ 1) = f; return; }
            if (!define(f, w[i])) return;
        }
    }
    // returns index, or -1 on overflow
    int run(const vector<vector<int>>& rels, const vector<vector<int>>& sub) {
        n = 1; p[1] = 1; overflow = false; memset(&tab[4], 0, 16);
        for (auto& w : sub) { scan_fill(1, w); if (overflow) return -1; }
        for (int a = 1; a <= n; a++) {
            for (auto& r : rels) { if (!live(a)) break; scan_fill(a, r); if (overflow) return -1; }
            if (live(a)) for (int x = 0; x < 4; x++) if (!T(a, x)) { if (!define(a, x)) return -1; }
        }
        int k = 0; for (int c = 1; c <= n; c++) if (live(c)) k++;
        return k;
    }
    // permutation images of x and y on live cosets (0-based)
    void perms(vector<int>& px, vector<int>& py) {
        vector<int> id(n + 1, -1); int k = 0;
        for (int c = 1; c <= n; c++) if (live(c)) id[c] = k++;
        px.assign(k, 0); py.assign(k, 0);
        for (int c = 1; c <= n; c++) if (live(c)) { px[id[c]] = id[rep(T(c, 0))]; py[id[c]] = id[rep(T(c, 2))]; }
    }
};

// ---- homomorphism search into S_n
static bool eval_id(const vector<int>& w, const vector<int>& a, const vector<int>& ai, const vector<int>& b, const vector<int>& bi, int n) {
    // compute action of word w on each point; word acts left-to-right (point^(w1 w2 ...))
    for (int pt = 0; pt < n; pt++) {
        int c = pt;
        for (int l : w) c = l == 0 ? a[c] : l == 1 ? ai[c] : l == 2 ? b[c] : bi[c];
        if (c != pt) return false;
    }
    return true;
}
static vector<vector<int>> class_reps(int n) {   // one permutation per cycle type
    vector<vector<int>> out;
    vector<int> parts;
    function<void(int, int)> rec = [&](int left, int mx) {
        if (!left) {
            vector<int> pm(n); int s = 0;
            for (int L : parts) { for (int i = 0; i < L; i++) pm[s + i] = s + (i + 1) % L; s += L; }
            out.push_back(pm); return;
        }
        for (int L = min(left, mx); L >= 1; L--) { parts.push_back(L); rec(left - L, L); parts.pop_back(); }
    };
    rec(n, n);
    return out;
}
static bool hom_search(const vector<int>& u, const vector<int>& v, int n, vector<int>& A, vector<int>& B) {
    auto reps = class_reps(n);
    vector<int> b(n); vector<int> bi(n), ai(n);
    for (auto& a : reps) {
        for (int i = 0; i < n; i++) ai[a[i]] = i;
        iota(b.begin(), b.end(), 0);
        do {
            bool nontriv = false;
            for (int i = 0; i < n; i++) if (a[i] != i || b[i] != i) nontriv = true;
            if (!nontriv) continue;
            for (int i = 0; i < n; i++) bi[b[i]] = i;
            if (eval_id(u, a, ai, b, bi, n) && eval_id(v, a, ai, b, bi, n)) { A = a; B = b; return true; }
        } while (next_permutation(b.begin(), b.end()));
    }
    return false;
}
static string pstr(const vector<int>& p) { string s = "["; for (size_t i = 0; i < p.size(); i++) s += (i ? "," : "") + to_string(p[i]); return s + "]"; }

int main(int argc, char** argv) {
    int limit = argc > 1 ? atoi(argv[1]) : 2000000;
    int maxdeg = argc > 2 ? atoi(argv[2]) : 8;
    char line[1024];
    while (fgets(line, sizeof line, stdin)) {
        char t[16], su[128], sv[128];
        if (sscanf(line, "%15s %127s %127s", t, su, sv) != 3) continue;
        vector<int> u = parse(su), v = parse(sv);
        vector<vector<int>> rels = {u, v};
        string res = "UNKNOWN", cert;
        const char* subs[] = {"x", "y", "xy", "xY"};
        TC tc(limit);
        for (const char* s : subs) {
            int k = tc.run(rels, {parse(s)});
            if (k == 1) { res = "TRIVIAL"; cert = string("tc_index1_over_<") + s + ">"; break; }
            if (k > 1) {
                vector<int> px, py; tc.perms(px, py);
                res = "NONTRIVIAL";
                cert = "perm_deg" + to_string(k) + " " + (k <= 400 ? pstr(px) + " " + pstr(py) : string("(large; from tc over <") + s + ">)");
                break;
            }
        }
        if (res == "UNKNOWN") {
            for (int n = 5; n <= maxdeg; n++) {
                vector<int> A, B;
                if (hom_search(u, v, n, A, B)) { res = "NONTRIVIAL"; cert = "perm_deg" + to_string(n) + " " + pstr(A) + " " + pstr(B); break; }
            }
        }
        printf("%s %s %s %s %s\n", t, su, sv, res.c_str(), cert.c_str());
        fflush(stdout);
    }
}
