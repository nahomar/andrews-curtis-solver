// Reuse verified paths across all puzzles and all 16 signed generator/relator
// symmetries. The graph stores canonical states, but search retains orientation:
// renaming a generator or swapping relators is NEVER counted as a free AC move.
// Build: clang++ -O3 -std=c++17 -pthread orbit_splice.cpp -o orbit_splice
#define main acs_original_main
#include "acs.cpp"
#undef main
#include <array>
#include <cassert>
#include <limits>

namespace orbit {
struct Sym { array<uint8_t,4> letters; int swap; };
vector<Sym> syms;
int compose[16][16], inverse[16], mapped[16][14];
St transform(const St& s, int g) {
    St t; const auto& q=syms[g];
    t.n0=q.swap?s.n1:s.n0; t.n1=q.swap?s.n0:s.n1;
    const uint8_t* a=q.swap?s.r1():s.r0();
    const uint8_t* b=q.swap?s.r0():s.r1();
    for(int k=0;k<t.n0;k++) t.c[k]=q.letters[a[k]];
    for(int k=0;k<t.n1;k++) t.c[t.n0+k]=q.letters[b[k]];
    return t;
}
void init() {
    for(int gs=0;gs<2;gs++) for(int x=0;x<2;x++) for(int y=0;y<2;y++) for(int rs=0;rs<2;rs++) {
        int a=(gs?2:0)^x,b=(gs?0:2)^y;
        syms.push_back({{(uint8_t)a,(uint8_t)(a^1),(uint8_t)b,(uint8_t)(b^1)},rs});
    }
    for(int a=0;a<16;a++) for(int b=0;b<16;b++) {
        Sym q; q.swap=syms[a].swap^syms[b].swap;
        for(int k=0;k<4;k++) q.letters[k]=syms[a].letters[syms[b].letters[k]];
        for(int c=0;c<16;c++) if(q.swap==syms[c].swap && q.letters==syms[c].letters) compose[a][b]=c;
    }
    for(int a=0;a<16;a++) {
        for(int b=0;b<16;b++) if(compose[a][b]==0 && compose[b][a]==0) inverse[a]=b;
        for(int m=0;m<14;m++) {
            int z=m;
            if(m>=6) z=(m<10?6:10)+syms[a].letters[m-(m<10?6:10)];
            if(syms[a].swap) {
                static const int sw[6]={1,0,4,5,2,3};
                z=z<6?sw[z]:(z<10?z+4:z-4);
            }
            mapped[a][m]=z;
        }
    }
}
string encode(const St& s) {
    string k; k.reserve(s.tot()+2); k.push_back(s.n0); k.push_back(s.n1);
    k.append((const char*)s.c,s.tot()); return k;
}
St decode(const string& k) {
    St s; s.n0=(uint8_t)k[0];s.n1=(uint8_t)k[1];memcpy(s.c,k.data()+2,s.tot());return s;
}
pair<string,int> canonical(const St& s, bool symmetries) {
    string best=encode(s);int g=0;
    if(symmetries) for(int j=1;j<16;j++) {string k=encode(transform(s,j));if(k<best){best=std::move(k);g=j;}}
    return {std::move(best),g};
}
struct Edge {uint32_t to;uint8_t move,transport;};
struct Graph {
    bool symmetries;
    unordered_map<string,uint32_t> ids;
    vector<string> keys;
    vector<vector<Edge>> adj;
    explicit Graph(bool use):symmetries(use){}
    pair<uint32_t,int> intern(const St& s) {
        auto [k,g]=canonical(s,symmetries);
        auto it=ids.find(k);if(it!=ids.end())return {it->second,g};
        uint32_t id=keys.size();ids.emplace(k,id);keys.push_back(std::move(k));adj.emplace_back();return {id,g};
    }
    void edge(pair<uint32_t,int> a,pair<uint32_t,int> b,int m) {
        int mc=mapped[a.second][m],h=compose[b.second][inverse[a.second]];
        adj[a.first].push_back({b.first,(uint8_t)mc,(uint8_t)h});
        adj[b.first].push_back({a.first,(uint8_t)mapped[h][INV_MOVE[mc]],(uint8_t)inverse[h]});
    }
    void join(int midcap=0) {
        struct First {uint32_t from;uint8_t move,transport;};
        unordered_map<string,First> pending;
        size_t n=keys.size();long long hits=0,bridges=0;
        for(uint32_t i=0;i<n;i++) {
            St s=decode(keys[i]),t;
            for(int m=0;m<14;m++) if(apply(s,m,t,MAXT)) {
                auto [k,g]=canonical(t,symmetries);auto it=ids.find(k);
                if(it!=ids.end()){edge({i,0},{it->second,g},m);hits++;}
                else if(midcap>0 && t.tot()<=midcap) {
                    auto [p,fresh]=pending.emplace(k,First{i,(uint8_t)m,(uint8_t)g});
                    if(!fresh) {
                        uint32_t mid=keys.size();ids.emplace(k,mid);keys.push_back(k);adj.emplace_back();
                        const auto first=p->second;
                        edge({first.from,0},{mid,first.transport},first.move);
                        edge({i,0},{mid,g},m);pending.erase(p);bridges++;
                    }
                }
            }
        }
        fprintf(stderr,"one-move connections: %lld; two-move bridge states: %lld\n",hits,bridges);
    }
    void dedup() {
        long long n=0;
        for(auto& v:adj) {
            sort(v.begin(),v.end(),[](const Edge&a,const Edge&b){return tie(a.to,a.move,a.transport)<tie(b.to,b.move,b.transport);});
            v.erase(unique(v.begin(),v.end(),[](const Edge&a,const Edge&b){return a.to==b.to&&a.move==b.move&&a.transport==b.transport;}),v.end());n+=v.size();
        }
        fprintf(stderr,"graph: %zu canonical states, %lld directed edges\n",keys.size(),n);
    }
};
struct Search {
    static constexpr uint32_t INF=UINT32_MAX;
    vector<uint32_t> dist,next;
    vector<int8_t> move;
    unordered_map<uint32_t,vector<int>> suffix;
    Search(const Graph& g,bool stable) {
        size_t n=g.keys.size()*16;dist.assign(n,INF);next.assign(n,INF);move.assign(n,-1);
        priority_queue<pair<uint32_t,uint32_t>,vector<pair<uint32_t,uint32_t>>,greater<>> pq;
        for(const auto& src:sources(stable)) {
            for(int o=0;o<(g.symmetries?16:1);o++) {
                auto k=encode(transform(src.s,o));auto it=g.ids.find(k);
                if(it==g.ids.end())continue;
                uint32_t q=it->second*16+o;
                if(src.suffix.size()<dist[q]) {dist[q]=src.suffix.size();pq.push({dist[q],q});suffix[q]=src.suffix;}
            }
        }
        while(!pq.empty()) {
            auto [d,q]=pq.top();pq.pop();if(d!=dist[q])continue;
            uint32_t v=q/16;int o=q%16;
            for(const auto&e:g.adj[v]) {
                int oo=compose[e.transport][o];uint32_t qq=e.to*16+oo;
                if(d+1<dist[qq]) {
                    dist[qq]=d+1;next[qq]=q;move[qq]=INV_MOVE[mapped[inverse[o]][e.move]];pq.push({d+1,qq});
                }
            }
        }
    }
    vector<int> path(uint32_t q) const {
        if(dist[q]==INF)return {};
        vector<int> p;
        while(next[q]!=INF){p.push_back(move[q]);q=next[q];}
        const auto& tail=suffix.at(q);p.insert(p.end(),tail.begin(),tail.end());return p;
    }
};
void self_test() {
    St s=make({1,2,-1,2},{2,1,1});
    for(int step=0;step<100;step++) {
        for(int a=0;a<16;a++) {
            assert(transform(transform(s,a),inverse[a])==s);
            for(int b=0;b<16;b++)assert(transform(transform(s,b),a)==transform(s,compose[a][b]));
            for(int m=0;m<14;m++){St t,u;if(apply(s,m,t,MAXT)){assert(apply(transform(s,a),mapped[a][m],u,MAXT));assert(u==transform(t,a));}}
        }
        St t;if(apply(s,step%14,t,40))s=t;
    }
    // A solved path whose inverse is known, including relator/generator symmetries.
    Graph g(true);St t=make({1},{2});g.intern(t);St u;
    for(int m:{2,8,4,0,11}){assert(apply(t,m,u,MAXT));g.edge(g.intern(t),g.intern(u),m);t=u;}
    g.join();g.dedup();
    Search ac(g,false),sac(g,true);
    for(int o=0;o<16;o++) {
        St x=transform(t,o);auto [i,j]=g.intern(x);uint32_t q=i*16+j;
        if(ac.dist[q]!=Search::INF)assert(verify(x,ac.path(q),false));
        assert(sac.dist[q]!=Search::INF);assert(verify(x,sac.path(q),true));
    }
    // Compare the lifted graph against a separate, ordinary BFS on EVERY state
    // in a small length-bounded component. This checks that no symmetry becomes
    // a free move, including symmetries that fix the endpoint.
    vector<St> raw;unordered_map<string,uint32_t> raw_id;
    auto add_raw=[&](const St& x){string k=encode(x);auto it=raw_id.find(k);if(it!=raw_id.end())return it->second;
        uint32_t v=raw.size();raw_id[k]=v;raw.push_back(x);return v;};
    for(auto& src:sources(true))add_raw(src.s);
    vector<vector<uint32_t>> edges;
    for(uint32_t v=0;v<raw.size();v++) {
        St x=raw[v],y;vector<uint32_t> es;
        for(int m=0;m<14;m++)if(apply(x,m,y,6))es.push_back(add_raw(y));
        edges.push_back(std::move(es));
    }
    for(bool use_sym:{false,true}) {
        Graph small(use_sym);
        for(auto& x:raw)small.intern(x);
        for(auto& x:raw){St y;for(int m=0;m<14;m++)if(apply(x,m,y,6))small.edge(small.intern(x),small.intern(y),m);}
        small.dedup();
        for(bool stable:{false,true}) {
            vector<uint32_t> expected(raw.size(),Search::INF);
            for(auto& src:sources(stable)) {
                vector<uint32_t> d(raw.size(),Search::INF);queue<uint32_t> q;
                uint32_t root=raw_id.at(encode(src.s));d[root]=0;q.push(root);
                while(!q.empty()){uint32_t v=q.front();q.pop();for(auto w:edges[v])if(d[w]==Search::INF){d[w]=d[v]+1;q.push(w);}}
                for(size_t i=0;i<raw.size();i++)if(d[i]!=Search::INF)expected[i]=min(expected[i],d[i]+(uint32_t)src.suffix.size());
            }
            Search got(small,stable);
            for(size_t i=0;i<raw.size();i++) {
                auto [k,o]=canonical(raw[i],use_sym);uint32_t q=small.ids.at(k)*16+o;
                assert(got.dist[q]==expected[i]);
                if(expected[i]!=Search::INF)assert(verify(raw[i],got.path(q),stable));
            }
        }
    }
    fprintf(stderr,"symmetry identities, lifted paths, and exhaustive BFS comparison (%zu raw states, both tracks, with/without symmetry): PASS\n",raw.size());
}
Graph load_graph(const vector<Prob>& probs,const string& paths,bool symmetries,int midcap,int rounds,bool joins=true) {
    map<string,St> starts;Graph g(symmetries);g.intern(make({1},{2}));
    for(auto&p:probs){starts[p.id]=p.s;g.intern(p.s);}
    auto all=read_all_paths({paths});size_t accepted=0,rejected=0;
    for(auto&[id,ps]:all){string aid="ac-"+id.substr(id.find('-')+1);auto it=starts.find(aid);if(it==starts.end())continue;
        bool stable=id.rfind("sac-",0)==0;
        for(auto&p:ps){vector<St> states;if(!verify(it->second,p,stable,&states)){rejected++;continue;}
            auto a=g.intern(states[0]);
            for(size_t j=1;j<states.size();j++){auto b=g.intern(states[j]);g.edge(a,b,p[j-1]);a=b;}accepted++;
        }
    }
    fprintf(stderr,"input: %zu paths accepted, %zu unsupported/invalid; %zu canonical states\n",accepted,rejected,g.keys.size());
    if(joins)for(int r=0;r<rounds;r++){g.join(midcap);g.dedup();}
    else g.dedup();
    return g;
}
struct Oracle : TailOracle {
    const Graph& graph;const Search& search;
    Oracle(const Graph& g,const Search& s):graph(g),search(s){}
    uint32_t find(const St& s) const {
        auto [k,o]=canonical(s,graph.symmetries);auto it=graph.ids.find(k);
        return it==graph.ids.end()?Search::INF:it->second*16+o;
    }
    int length(const St& s) const override {
        auto q=find(s);return q==Search::INF||search.dist[q]==Search::INF?-1:(int)search.dist[q];
    }
    vector<int> path(const St& s) const override {auto q=find(s);return q==Search::INF?vector<int>{}:search.path(q);}
};
} // namespace orbit

#ifndef ORBIT_NO_MAIN
int main(int argc,char**argv) {
    using namespace orbit;init();
    string problems,paths,out;bool symmetries=true,joins=true;int midcap=0,rounds=1;
    for(int i=1;i<argc;i++) {
        string a=argv[i];
        if(a=="--self-test"){self_test();return 0;}
        if(a=="--no-symmetry"){symmetries=false;continue;}
        if(a=="--no-joins"){joins=false;continue;}
        if(i+1>=argc){fprintf(stderr,"missing value\n");return 1;}
        string v=argv[++i];if(a=="--problems")problems=v;else if(a=="--paths")paths=v;else if(a=="--out")out=v;else if(a=="--two-move-cap")midcap=stoi(v);else if(a=="--join-rounds")rounds=stoi(v);else return 1;
    }
    if(problems.empty()||paths.empty()||out.empty()){fprintf(stderr,"usage: orbit_splice --problems FILE --paths FILE.jsonl --out PREFIX [--no-symmetry] [--no-joins] [--two-move-cap N] [--join-rounds N]\n");return 1;}
    if(midcap<0||midcap>MAXT||rounds<1||!ifstream(problems)||!ifstream(paths)){fprintf(stderr,"invalid cap/round count or unreadable input\n");return 1;}
    auto probs=read_problems(problems);
    Graph g=load_graph(probs,paths,symmetries,midcap,rounds,joins);
    for(bool stable:{false,true}){
        Search search(g,stable);ofstream f(out+(stable?"_sac.jsonl":"_ac.jsonl"));if(!f){fprintf(stderr,"cannot open output\n");return 1;}size_t solved=0;
        for(auto&p:probs){auto [k,o]=canonical(p.s,symmetries);auto it=g.ids.find(k);uint32_t q=it->second*16+o;if(search.dist[q]==Search::INF)continue;
            auto path=search.path(q);if(!verify(p.s,path,stable)){fprintf(stderr,"INVALID lift for %s\n",p.id.c_str());return 2;}
            string id=(stable?"sac-":"ac-")+p.id.substr(p.id.find('-')+1);
            f<<jsonl(id,true,path,0,", \"method\": \"orbit-splice\"");solved++;
        }
        fprintf(stderr,"%s: %zu solved paths, all internally replayed\n",stable?"Stable AC":"AC",solved);
    }
}
#endif
