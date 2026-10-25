// Exhaustively explore bounded neighborhoods of selected incumbent paths,
// then connect them to the global graph of known solutions. No beam heuristic.
#define ORBIT_NO_MAIN
#include "orbit_splice.cpp"
#include <unordered_set>

static void expand_neighborhood(orbit::Graph& graph,const vector<St>& seeds,int radius,int cap,size_t budget) {
    unordered_set<uint32_t> seen;vector<uint32_t> layer;
    for(const auto&s:seeds){auto [id,o]=graph.intern(s);if(seen.insert(id).second)layer.push_back(id);}
    size_t original=graph.keys.size();bool stopped=false;
    for(int depth=1;depth<=radius&&!stopped;depth++) {
        vector<uint32_t> next;
        for(uint32_t from:layer){St s=orbit::decode(graph.keys[from]),t;
            for(int m=0;m<14;m++)if(apply(s,m,t,cap)) {
                auto [to,g]=graph.intern(t);graph.edge({from,0},{to,g},m);
                if(seen.insert(to).second)next.push_back(to);
                if(graph.keys.size()-original>=budget){stopped=true;break;}
            }
            if(stopped)break;
        }
        layer=std::move(next);
        fprintf(stderr,"target neighborhood depth %d: %zu reached, %zu new graph states%s\n",depth,seen.size(),graph.keys.size()-original,stopped?" (budget reached)":"");
    }
}

int main(int argc,char**argv) {
    orbit::init();string problems,paths,seed_paths,out;int radius=3,cap=64,rounds=1;size_t budget=2000000;
    for(int i=1;i<argc;i++) {
        string a=argv[i];if(a=="--self-test"){orbit::self_test();return 0;}
        if(i+1>=argc)return 1;string v=argv[++i];
        if(a=="--problems")problems=v;else if(a=="--paths")paths=v;else if(a=="--seed-paths")seed_paths=v;else if(a=="--out")out=v;
        else if(a=="--radius")radius=stoi(v);else if(a=="--cap")cap=stoi(v);else if(a=="--budget")budget=stoull(v);else if(a=="--join-rounds")rounds=stoi(v);else return 1;
    }
    if(problems.empty()||paths.empty()||seed_paths.empty()||out.empty()||radius<0||cap<2||cap>MAXT||!budget||rounds<0){fprintf(stderr,"usage: orbit_expand --problems ALL.txt --paths POOL.jsonl --seed-paths TARGET_PATHS.jsonl --out PREFIX [--radius N] [--cap N] [--budget N] [--join-rounds N]\n");return 1;}
    if(!ifstream(problems)||!ifstream(paths)||!ifstream(seed_paths)){fprintf(stderr,"unreadable input file\n");return 1;}
    auto probs=read_problems(problems);map<string,St> starts;for(auto&p:probs)starts[p.id]=p.s;
    auto graph=orbit::load_graph(probs,paths,true,0,1,false);
    vector<St> seeds;auto selected=read_all_paths({seed_paths});
    for(auto&[id,ps]:selected){auto it=starts.find("ac-"+id.substr(id.find('-')+1));if(it==starts.end())continue;
        for(auto&p:ps){vector<St> states;if(!verify(it->second,p,id.rfind("sac-",0)==0,&states)){fprintf(stderr,"invalid seed path\n");return 1;}seeds.insert(seeds.end(),states.begin(),states.end());}
    }
    if(seeds.empty()){fprintf(stderr,"no valid seeds\n");return 1;}
    expand_neighborhood(graph,seeds,radius,cap,budget);graph.dedup();
    // The expansion budget bounds new neighborhood states. Joins do not add
    // further nodes here, so memory cannot grow unexpectedly after expansion.
    for(int j=0;j<rounds;j++){graph.join(0);graph.dedup();}
    for(bool stable:{false,true}){
        orbit::Search search(graph,stable);ofstream f(out+(stable?"_sac.jsonl":"_ac.jsonl"));if(!f)return 1;size_t solved=0;
        for(auto&p:probs){auto [k,o]=orbit::canonical(p.s,true);uint32_t q=graph.ids.at(k)*16+o;if(search.dist[q]==orbit::Search::INF)continue;
            auto path=search.path(q);if(!verify(p.s,path,stable)){fprintf(stderr,"invalid output\n");return 2;}
            string id=(stable?"sac-":"ac-")+p.id.substr(p.id.find('-')+1);f<<jsonl(id,true,path,0,", \"method\": \"target-neighborhood\"");solved++;
        }
        fprintf(stderr,"%s: %zu replay-checked paths\n",stable?"Stable AC":"AC",solved);
    }
}
