// Direct discovery by bounded generator changes, with exact move transport.
// No incumbent or free generator-change move is assumed.
#define STABLE_BALL_NO_MAIN
#include "stable_ball_search.cpp"

struct DiscoveryMap{nielsen::Aut aut;array<vector<int>,8> finishes;array<bool,8> valid{};};
int main(int argc,char**argv){
    orbit::init();string problems,ballfile,out,paths;int depth=4,radius=0,cap=40,waypoint_stride=0;size_t budget=10000;double seconds=60;
    for(int i=1;i<argc;i++){string a=argv[i];if(i+1>=argc)return 1;string v=argv[++i];if(a=="--problems")problems=v;else if(a=="--ac-ball")ballfile=v;else if(a=="--out")out=v;else if(a=="--depth")depth=stoi(v);else if(a=="--radius")radius=stoi(v);else if(a=="--cap")cap=stoi(v);else if(a=="--budget")budget=stoull(v);else if(a=="--seconds")seconds=stod(v);else if(a=="--paths")paths=v;else if(a=="--waypoint-stride")waypoint_stride=stoi(v);else return 1;}
    if(!ifstream(problems)||!ifstream(ballfile)||out.empty()||depth<0||depth>5||radius<0||radius>8||cap<2||cap>MAXT||!budget||seconds<=0||waypoint_stride<0||(!paths.empty()&&!ifstream(paths)))return 1;
    Ball ball;if(!ball.load(ballfile)||ball.stable)return 1;StableBallOracle oracle(ball);vector<DiscoveryMap> maps;St basis=make({1},{2});auto goals=sources(false);
    for(auto&aut:nielsen::generate(depth)){DiscoveryMap entry;entry.aut=aut;
        for(size_t e=0;e<oracle.endpoints.size();e++){int g=oracle.endpoints[e].first;St end=orbit::transform(basis,orbit::inverse[g]),original;
            if(nielsen::transform(end,aut.back,original)&&oracle.length(original)>=0){entry.finishes[e]=oracle.path(original);entry.valid[e]=true;}}
        maps.push_back(std::move(entry));}
    fprintf(stderr,"prepared %zu charged generator maps\n",maps.size());ofstream f(out);if(!f)return 1;map<string,vector<int>> saved;if(!paths.empty())saved=read_paths(paths);
    for(const auto&job:read_problems(problems)){auto t0=chrono::steady_clock::now();vector<int> best;size_t matches=0,queried=0;bool truncated=false,timed_out=false;
        auto incumbent=saved.find(job.id);if(incumbent!=saved.end()){if(!verify(job.s,incumbent->second,true))return 2;best=incumbent->second;}
        struct Node{St s;int parent,move,depth,origin;};vector<Node> nodes{{job.s,-1,-1,0,0}};vector<vector<int>> seed_prefixes(1);unordered_map<string,size_t> seen;seen.emplace(orbit::encode(job.s),0);
        if(waypoint_stride&&!best.empty()){vector<St> states;assert(verify(job.s,best,true,&states));for(size_t at=waypoint_stride;at<states.size();at+=waypoint_stride){auto k=orbit::encode(states[at]);if(seen.count(k))continue;seen.emplace(std::move(k),nodes.size());nodes.push_back({states[at],-1,-1,0,(int)seed_prefixes.size()});seed_prefixes.emplace_back(best.begin(),best.begin()+at);}}
        auto cost=[&](const Node&n){return seed_prefixes[n.origin].size()+n.depth;};
        auto prefix=[&](size_t q){vector<int> p;while(nodes[q].parent>=0){p.push_back(nodes[q].move);q=nodes[q].parent;}reverse(p.begin(),p.end());auto r=seed_prefixes[nodes[q].origin];r.insert(r.end(),p.begin(),p.end());return r;};
        for(size_t q=0;q<nodes.size();q++){
            if(chrono::duration<double>(chrono::steady_clock::now()-t0).count()>=seconds){timed_out=true;break;}Node n=nodes[q];queried++;
            for(const auto&entry:maps){St changed;if(!nielsen::transform(n.s,entry.aut.forward,changed)||changed.tot()>ball.cap)continue;
            for(size_t e=0;e<oracle.endpoints.size();e++){if(!entry.valid[e])continue;int g=oracle.endpoints[e].first;St query=orbit::transform(changed,g);int d=ball.get(query);if(d<0)continue;matches++;
                if(!best.empty()&&cost(n)+(size_t)d+entry.finishes[e].size()>=best.size())continue;
                vector<int> raw;if(!descend(ball,goals,query,raw))continue;for(auto&m:raw)m=orbit::mapped[orbit::inverse[g]][m];auto candidate=prefix(q);auto lifted=nielsen::lift(raw,entry.aut.back);candidate.insert(candidate.end(),lifted.begin(),lifted.end());candidate.insert(candidate.end(),entry.finishes[e].begin(),entry.finishes[e].end());
                if((best.empty()||candidate.size()<best.size())&&verify(job.s,candidate,true))best=std::move(candidate);
            }}
            if(n.depth<radius)for(int m=0;m<14;m++){if(n.parent>=0&&INV_MOVE[n.move]==m)continue;St t;if(!apply(n.s,m,t,cap))continue;auto k=orbit::encode(t);auto previous=seen.find(k);if(previous!=seen.end()&&cost(nodes[previous->second])<=cost(n)+1)continue;if(nodes.size()>=budget){truncated=true;continue;}seen[std::move(k)]=nodes.size();nodes.push_back({t,(int)q,m,n.depth+1,n.origin});}
        }
        if(!best.empty())best=shorten(job.s,best,true,3,4);
        assert(best.empty()||verify(job.s,best,true));double secs=chrono::duration<double>(chrono::steady_clock::now()-t0).count();f<<jsonl(job.id,!best.empty(),best,secs,", \"method\": \"prefix-nielsen-discovery\"");f.flush();fprintf(stderr,"%s prefixes=%zu matches=%zu solved=%d length=%zu truncated=%d timed_out=%d seconds=%.2f\n",job.id.c_str(),queried,matches,!best.empty(),best.size(),truncated,timed_out,secs);
    }
}
