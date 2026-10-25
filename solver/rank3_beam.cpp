// Search beyond shallow stabilized neighborhoods, charging the complete prefix,
// stabilization, rank-three route, destabilization, and verified finish.
#define RANK3_BRIDGE_NO_MAIN
#include "rank3_bridge.cpp"
#include <unordered_set>

int main(int argc,char**argv) {
    if(argc!=10){cerr<<"rank3_beam SEEDS ID STABLE_BALL WIDTH CAP SECONDS BEST_LENGTH SEED OUT\n";return 1;}
    ifstream in(argv[1]);string id=argv[2];Ball ball;
    int width=stoi(argv[4]),cap=stoi(argv[5]),best=stoi(argv[7]);double seconds=stod(argv[6]);uint64_t random_seed=stoull(argv[8]);ofstream out(argv[9]);
    if(!in||!out||!ball.load(argv[3])||!ball.stable||width<1||cap<3||cap>64||seconds<=0||best<3||best>250)return 1;
    auto goals=sources(true);vector<Seed> seeds;unordered_map<string,vector<int>> known;
    int np,nt,n0,n1;
    while(in>>np>>nt>>n0>>n1) {
        if(np<0||nt<0||n0<0||n1<0||n0+n1+1>64)return 2;
        Seed s;s.s.n[0]=n0;s.s.n[1]=n1;s.s.n[2]=1;s.prefix.resize(np);s.tail.resize(nt);
        for(auto&v:s.prefix)in>>v;for(auto&v:s.tail)in>>v;
        int x;for(int j=0;j<n0+n1;++j){in>>x;s.s.c[j]=2*(abs(x)-1)+(x<0);}s.s.c[n0+n1]=4;
        if(!in)return 2;
        St raw;assert(remove_generator(s.s,2,raw));if(!verify(raw,s.tail,true))return 2;
        string k; k.push_back(raw.n0);k.push_back(raw.n1);k.append((char*)raw.c,raw.tot());
        auto it=known.find(k);if(it==known.end()||s.tail.size()<it->second.size())known[k]=s.tail;
        if(s.s.total()<=cap&&np+2<best)seeds.push_back(std::move(s));
    }
    struct Pending {State state;uint32_t parent,origin;uint16_t move;uint8_t depth;};
    vector<vector<Pending>> buckets(best);vector<Node> nodes;unordered_set<string> seen;
    for(size_t i=0;i<seeds.size();++i)buckets[seeds[i].prefix.size()+1].push_back({seeds[i].s,UINT32_MAX,(uint32_t)i,0,0});
    auto started=chrono::steady_clock::now();size_t expanded=0;bool timed_out=false;
    auto score=[&](const State&s){int cyc=0,at=0;for(int i=0;i<3;++i){cyc+=cyc_len(s.c+at,s.n[i]);at+=s.n[i];}return cyc*8+(s.total()-cyc)*4;};
    for(int g=1;g<best-1&&g<(int)buckets.size();++g) {
        if(chrono::duration<double>(chrono::steady_clock::now()-started).count()>seconds){timed_out=true;break;}
        auto& candidates=buckets[g];vector<pair<uint64_t,size_t>> ordered;unordered_set<string> local;
        for(size_t i=0;i<candidates.size();++i){auto k=key(candidates[i].state);if(seen.count(k)||!local.insert(k).second)continue;
            uint64_t h=1469598103934665603ULL;for(unsigned char c:k)h=(h^c)*1099511628211ULL;
            ordered.push_back({(uint64_t(score(candidates[i].state))<<32)|(mix(h^random_seed)&0xffffffff),i});}
        sort(ordered.begin(),ordered.end());if(ordered.size()>(size_t)width)ordered.resize(width);
        for(auto [rank,index]:ordered) {
            const auto& p=candidates[index];if(!seen.insert(key(p.state)).second)continue;
            uint32_t q=nodes.size();nodes.push_back({p.state,p.parent,p.origin,p.move,p.depth});expanded++;
            for(int i=0;i<3;++i) {
                St raw;if(!remove_generator(p.state,i,raw))continue;
                string k;k.push_back(raw.n0);k.push_back(raw.n1);k.append((char*)raw.c,raw.tot());
                vector<int> tail;auto old=known.find(k);if(old!=known.end())tail=old->second;
                int d=raw.tot()<=ball.cap?ball.get(raw):-1;
                if(d>=0&&(tail.empty()||d<(int)tail.size())){vector<int> t;if(descend(ball,goals,raw,t))tail=std::move(t);}
                if(tail.empty()||g+1+(int)tail.size()>=best)continue;
                auto path=seeds[p.origin].prefix;path.push_back(14);auto middle=trace(nodes,q);path.insert(path.end(),middle.begin(),middle.end());path.push_back(15+i);path.insert(path.end(),tail.begin(),tail.end());
                assert(path.size()==g+1+tail.size());assert(verify(raw,tail,true));
                State replay=seeds[p.origin].s;
                for(int move:middle){bool ok=false;for(const auto&m:moves)if(m.id==move){State next;ok=step(replay,m,next,cap);replay=next;break;}assert(ok);}
                assert(key(replay)==key(p.state));best=path.size();emit(id,path,out,"rank3-beam");cerr<<"candidate length="<<best<<" at cost="<<g<<'\n';
            }
            if(g+2>=best||g+1>=(int)buckets.size())continue;
            for(const auto&m:moves){if(p.parent!=UINT32_MAX&&undo(p.move)==m.id)continue;State next;if(step(p.state,m,next,cap)&&!seen.count(key(next)))buckets[g+1].push_back({next,q,p.origin,(uint16_t)m.id,(uint8_t)(p.depth+1)});}
        }
        vector<Pending>().swap(candidates);
        if(g%5==0)cerr<<"cost="<<g<<" selected="<<ordered.size()<<" expanded="<<expanded<<" best="<<best<<'\n';
    }
    cerr<<"finished best="<<best<<" expanded="<<expanded<<" timed_out="<<timed_out<<" seconds="<<chrono::duration<double>(chrono::steady_clock::now()-started).count()<<'\n';
}
