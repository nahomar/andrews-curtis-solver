// Sample different shortest and near-shortest routes through the verified
// corpus, then polish them outside that graph. Equal-cost routes can expose
// different shortcuts; always replay the complete resulting move sequence.
#define ORBIT_NO_MAIN
#include "orbit_splice.cpp"
#include <random>

static vector<int> sample_path(const orbit::Graph&g,const orbit::Search&search,uint32_t q,int slack,mt19937_64&rng){
    if(search.dist[q]==orbit::Search::INF)return {};
    int remaining=search.dist[q]+slack;vector<int> moves;set<uint32_t> visited{q};int previous=-1;
    while(remaining>=0){
        auto end=search.suffix.find(q);
        if(end!=search.suffix.end()&&(int)end->second.size()<=remaining){moves.insert(moves.end(),end->second.begin(),end->second.end());return moves;}
        struct Option{uint32_t next;int move;double weight;};vector<Option> options;double total=0;
        int o=q%16;
        for(const auto&e:g.adj[q/16]){
            int m=orbit::mapped[orbit::inverse[o]][e.move];uint32_t next=e.to*16+orbit::compose[e.transport][o];
            if(search.dist[next]==orbit::Search::INF||1+search.dist[next]>(uint32_t)remaining||visited.count(next)||(previous>=0&&m==INV_MOVE[previous]))continue;
            int extra=1+(int)search.dist[next]-(int)search.dist[q];assert(extra>=0);
            double weight=extra==0?1.0:0.12/(extra*extra);options.push_back({next,m,weight});total+=weight;
        }
        if(options.empty())return {};
        double choice=generate_canonical<double,53>(rng)*total;const Option* pick=&options.back();
        for(const auto&op:options){choice-=op.weight;if(choice<=0){pick=&op;break;}}
        moves.push_back(pick->move);previous=pick->move;q=pick->next;visited.insert(q);remaining--;
    }
    return {};
}

static void variant_self_test(){
    orbit::Graph g(true);St s=make({1},{2}),t;g.intern(s);
    for(int m:{2,8,4,0,11,3,12}){assert(apply(s,m,t,MAXT));g.edge(g.intern(s),g.intern(t),m);s=t;}
    g.join();g.dedup();mt19937_64 rng(37);size_t checked=0;
    for(bool stable:{false,true}){orbit::Search search(g,stable);
        for(int o=0;o<16;o++){St raw=orbit::transform(s,o);auto [key,orientation]=orbit::canonical(raw,true);uint32_t q=g.ids.at(key)*16+orientation;
            if(search.dist[q]==orbit::Search::INF)continue;
            for(int k=0;k<100;k++){auto p=sample_path(g,search,q,4,rng);if(p.empty())continue;assert(p.size()<=search.dist[q]+4);assert(verify(raw,p,stable));checked++;}}}
    assert(checked>0);fprintf(stderr,"sampled orientation-aware paths: %zu valid, both tracks PASS\n",checked);
}

int main(int argc,char**argv){
    orbit::init();string problems,paths,targets,out,ballfile;int samples=300,slack=4,radius=3;uint64_t seed=1;bool stable=false;
    for(int i=1;i<argc;i++){string a=argv[i];if(a=="--self-test"){variant_self_test();return 0;}if(a=="--stable"){stable=true;continue;}if(i+1>=argc)return 1;string v=argv[++i];
        if(a=="--problems")problems=v;else if(a=="--paths")paths=v;else if(a=="--targets")targets=v;else if(a=="--out")out=v;else if(a=="--ball")ballfile=v;
        else if(a=="--samples")samples=stoi(v);else if(a=="--slack")slack=stoi(v);else if(a=="--radius")radius=stoi(v);else if(a=="--seed")seed=stoull(v);else return 1;}
    if(!ifstream(problems)||!ifstream(paths)||!ifstream(targets)||out.empty()||samples<1||slack<0||slack>20||radius<0||radius>6)return 1;
    auto graph=orbit::load_graph(read_problems(problems),paths,true,64,2);orbit::Search search(graph,stable);Ball ball;ball.cap=-1;
    if(!ballfile.empty()&&(!ball.load(ballfile)||ball.stable!=stable))return 1;
    auto goals=sources(stable);ofstream f(out);if(!f)return 1;mt19937_64 rng(seed);
    for(const auto&job:read_problems(targets)){
        auto [key,o]=orbit::canonical(job.s,true);auto it=graph.ids.find(key);if(it==graph.ids.end())continue;uint32_t q=it->second*16+o;if(search.dist[q]==orbit::Search::INF)continue;
        set<vector<int>> seen,polished;auto best=search.path(q);assert(verify(job.s,best,stable));size_t count=0;auto t0=chrono::steady_clock::now();
        for(int attempt=0;attempt<samples*20&&(int)count<samples;attempt++){
            auto p=attempt==0?best:sample_path(graph,search,q,slack,rng);if(p.empty()||!seen.insert(p).second)continue;if(!verify(job.s,p,stable)){fprintf(stderr,"invalid sampled path\n");return 2;}count++;
            if(radius)p=shorten(job.s,p,stable,radius,4,ball.cap>=0?&ball:nullptr,&goals);
            if(!verify(job.s,p,stable)){fprintf(stderr,"invalid polished path\n");return 2;}
            if(p.size()<best.size()){fprintf(stderr,"%s variant %zu: %zu -> %zu\n",job.id.c_str(),count,best.size(),p.size());best=p;}
            if(polished.insert(p).second){f<<jsonl(job.id,true,p,0,", \"method\": \"orbit-variant\"");f.flush();}
            if(count%100==0)fprintf(stderr,"%s sampled %zu, shortest %zu\n",job.id.c_str(),count,best.size());
        }
        double secs=chrono::duration<double>(chrono::steady_clock::now()-t0).count();fprintf(stderr,"%s: %zu distinct routes, %u -> %zu, %.1fs\n",job.id.c_str(),count,search.dist[q],best.size(),secs);
    }
}
