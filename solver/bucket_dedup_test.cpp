// Compare the elementary-only bucket search with an independent exact BFS.
#define main acs_original_main
#include "acs.cpp"
#undef main
#include <cassert>
#include <set>

static string exact_key(const St& s) {
    string key;
    key.push_back(s.n0); key.push_back(s.n1);
    key.append(reinterpret_cast<const char*>(s.c), s.tot());
    return key;
}
struct Probe : TailOracle {
    mutable set<string> visited;
    int length(const St& s) const override { visited.insert(exact_key(s)); return -1; }
    vector<int> path(const St&) const override { return {}; }
};
int main() {
    St start=make({1,2,1},{2,1});
    Opt o; o.max_depth=3; o.maxcost=1; o.maxlen=10; o.width=100000;
    o.look=0; o.time_limit=10; o.power=1; o.class_dedup=0; o.reps=1;
    Ball empty; empty.cap=-1;
    auto run=[&](int enabled,int reps) {
        Probe probe; o.class_dedup=enabled; o.reps=reps; o.tail=&probe;
        long long expanded=0; bucket_beam(start,empty,sources(false),o,&expanded);
        return probe.visited;
    };
    auto disabled=run(0,1),disabled_many=run(0,100),enabled=run(1,1);
    set<string> expected{exact_key(start)};
    vector<pair<St,int>> queue{{start,0}};
    // The bucket allocation includes maxcost+1 extra levels: G=6 here.
    for(size_t i=0;i<queue.size();++i) {
        auto [s,depth]=queue[i]; if(depth==5)continue;
        for(int move=0;move<14;++move) {
            St t; if(apply(s,move,t,10)&&expected.insert(exact_key(t)).second)
                queue.push_back({t,depth+1});
        }
    }
    assert(disabled==expected);
    assert(disabled==disabled_many);
    assert(enabled.size()<disabled.size());
    fprintf(stderr,"bucket class-dedup: %zu exact BFS states, %zu class-limited; disabled independent of reps PASS\n",disabled.size(),enabled.size());
}
