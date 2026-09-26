// fcc: memory-light "frontier" BFS of a length-capped component (same graph G_L / quotient as lcc).
//
// In an undirected graph, BFS layer D_{k+1} = N(D_k) \ (D_k u D_{k-1}); so only three layers need to be
// kept in memory (Korf's frontier search).  Layers are sorted vectors of canonical 64-bit keys.
// This trades path reconstruction for memory and lets us exhaust components far larger than RAM-resident
// hash tables allow.  It reports the same RESULT line as lcc (size = sum of layer sizes), plus watch hits
// (by BFS distance, no paths).  Cross-validated against lcc on all caps where both run (see README).
//
//   fcc --cap L [--full] [--quot] [--watch FILE] [--chunk N] [--threads T] -- r0 | r1
#define LCC_NO_MAIN
#include "lcc.cpp"
#include <unordered_map>

static void expand(const vector<uint64_t>& src, size_t lo, size_t hi, int cap, vector<uint64_t>& out) {
    St t;
    for (size_t q = lo; q < hi; q++) {
        St s = unpack(src[q]);
        for (int m = 0; m < 14; m++) if (apply(s, m, t, cap)) out.push_back(key(t));
    }
}
static void sort_unique(vector<uint64_t>& v) { sort(v.begin(), v.end()); v.erase(unique(v.begin(), v.end()), v.end()); }
static void minus_sorted(vector<uint64_t>& v, const vector<uint64_t>& a, const vector<uint64_t>& b) {
    size_t w = 0;
    for (uint64_t x : v) if (!binary_search(a.begin(), a.end(), x) && !binary_search(b.begin(), b.end(), x)) v[w++] = x;
    v.resize(w);
}

int main(int argc, char** argv) {
    init_sym();
    int cap = 0, threads = 1; size_t chunk = 1 << 22; string watch; vector<int> a, b; bool second = false, words = false, full = false;
    for (int i = 1; i < argc; i++) {
        string t = argv[i];
        if (words) { if (t == "|") second = true; else (second ? b : a).push_back(atoi(argv[i])); continue; }
        if (t == "--cap") cap = atoi(argv[++i]);
        else if (t == "--quot") QUOT = true;
        else if (t == "--full") full = true;
        else if (t == "--threads") threads = max(1, min(2, atoi(argv[++i])));
        else if (t == "--chunk") chunk = atoll(argv[++i]);
        else if (t == "--watch") watch = argv[++i];
        else if (t == "--") words = true;
    }
    if (cap < 2 || cap > MAXL || a.empty() || b.empty() || (int)(a.size() + b.size()) > cap) { fprintf(stderr, "usage: see header\n"); return 2; }
    St S = parse(a, b);
    unordered_map<uint64_t, string> W;
    W[key(parse({1}, {2}))] = "TRIV";
    if (!watch.empty()) {
        FILE* f = fopen(watch.c_str(), "r"); char line[4096];
        while (f && fgets(line, sizeof line, f)) { string id; St s; if (parse_line(line, id, s) && s.tot() <= cap) W[key(s)] = id; }
        if (f) fclose(f);
    }
    auto t0 = chrono::steady_clock::now();
    printf("# fcc cap=%d mode=%s start=%s\n", cap, QUOT ? "quot16" : "exact", show(S).c_str());
    vector<uint64_t> prev, cur{key(S)}, nxt;
    uint64_t total = 1; int depth = 0; bool trivial = false; size_t peak = 1;
    vector<pair<string, int>> hits;
    auto check = [&](const vector<uint64_t>& layer, int d) {
        for (auto& [k, id] : W) if (binary_search(layer.begin(), layer.end(), k)) { hits.push_back({id, d}); if (id == "TRIV") trivial = true; }
    };
    check(cur, 0);
    while (!cur.empty() && (full || !trivial)) {
        nxt.clear();
        for (size_t lo = 0; lo < cur.size(); lo += chunk * threads) {
            vector<vector<uint64_t>> part(threads);
            vector<thread> th;
            for (int i = 0; i < threads; i++) {
                size_t l = min(cur.size(), lo + i * chunk), h = min(cur.size(), l + chunk);
                th.emplace_back([&, i, l, h] { expand(cur, l, h, cap, part[i]); sort_unique(part[i]); minus_sorted(part[i], cur, prev); });
            }
            for (auto& x : th) x.join();
            for (auto& p : part) { nxt.insert(nxt.end(), p.begin(), p.end()); vector<uint64_t>().swap(p); }
            if (nxt.size() > 4 * cur.size() + (1 << 24)) sort_unique(nxt);
        }
        sort_unique(nxt);
        depth++; total += nxt.size(); peak = max(peak, prev.size() + cur.size() + nxt.size());
        check(nxt, depth);
        prev.swap(cur); cur.swap(nxt);
        double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        printf("layer %d new=%zu total=%llu t=%.1fs\n", depth, cur.size(), (unsigned long long)total, el); fflush(stdout);
    }
    printf("RESULT cap=%d mode=%s size=%llu radius=%d %s\n", cap, QUOT ? "quot16" : "exact", (unsigned long long)total,
           cur.empty() ? depth - 1 : depth, trivial ? "TRIVIAL (standard presentation reached)" : "NOT-TRIVIAL (component exhausted, (x,y) absent)");
    printf("peak_three_layers=%zu\n", peak);
    for (auto& [id, d] : hits) printf("HIT %s dist=%d\n", id.c_str(), d);
    return trivial ? 1 : 0;
}
