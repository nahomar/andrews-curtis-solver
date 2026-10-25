// ball_bridge.cpp — exposes acs.cpp's exact rank-2 finish table (ball16s.bin: stable 8-endpoint finish) to sacs.cpp.
// acs.cpp's functions are file-static, so this translation unit links next to sacs.cpp without symbol clashes.
#define main acs_main
#include "acs.cpp"
#undef main

static Ball SB; static vector<Source> SSRC;

bool sball_load(const char* f) { if (!SB.load(f)) return false; SSRC = sources(SB.stable); return true; }
int sball_cap() { return SB.cap; }
bool sball_stable() { return SB.stable; }

// words as signed letters (+-1 = x, +-2 = y); exact distance to the (stable) endpoint set, or -1 when outside the table
int sball_dist(const int8_t* a, int na, const int8_t* b, int nb) {
    if (na + nb > SB.cap) return -1;
    vector<int> va(a, a + na), vb(b, b + nb); St s = make(va, vb);
    return SB.get(s);
}
// moves from the state to the endpoint (including the official destabilisation suffix); returns count or -1
int sball_finish(const int8_t* a, int na, const int8_t* b, int nb, int* out, int max_out) {
    if (na + nb > SB.cap) return -1;
    vector<int> va(a, a + na), vb(b, b + nb); St s = make(va, vb); vector<int> mv;
    if (!descend(SB, SSRC, s, mv) || (int)mv.size() > max_out) return -1;
    for (size_t i = 0; i < mv.size(); i++) out[i] = mv[i];
    return mv.size();
}
