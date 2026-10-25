// Find the cheapest transported tail, charging each conjugating letter and
// the full final basis reduction. Generator changes themselves are not moves.
#define NIELSEN_NO_MAIN
#include "nielsen_transfer.cpp"
#include "orbit_graph_cache.inc"

struct WeightedSearch {
    static constexpr uint32_t INF=UINT32_MAX;
    vector<uint32_t> dist,next;
    vector<int8_t> move;
    map<uint32_t,vector<int>> finish;
    const nielsen::Map& back;
    WeightedSearch(const orbit::Graph& g,const nielsen::Map& inverse_map,const Ball& ball,bool stable,uint32_t max_distance=INF):back(inverse_map) {
        size_t n=g.keys.size()*16;dist.assign(n,INF);next.assign(n,INF);move.assign(n,-1);
        priority_queue<pair<uint32_t,uint32_t>,vector<pair<uint32_t,uint32_t>>,greater<>> pq;
        auto goals=sources(stable);
        // Every signed basis is eligible, but its actual reduction is paid.
        for(const auto& src:sources(true)) {
            St basis;if(!nielsen::transform(src.s,back,basis))continue;
            vector<int> tail;if(!descend(ball,goals,basis,tail))continue;
            assert(verify(basis,tail,stable));if(tail.size()>max_distance)continue;
            for(int o=0;o<16;o++) {
                auto it=g.ids.find(orbit::encode(orbit::transform(src.s,o)));if(it==g.ids.end())continue;
                uint32_t q=it->second*16+o;
                if(tail.size()<dist[q]){dist[q]=tail.size();finish[q]=tail;pq.push({dist[q],q});}
            }
        }
        while(!pq.empty()) {
            auto [d,q]=pq.top();pq.pop();if(d!=dist[q])continue;
            uint32_t v=q/16;int o=q%16;
            for(const auto&e:g.adj[v]) {
                int oo=orbit::compose[e.transport][o];uint32_t qq=e.to*16+oo;
                int m=INV_MOVE[orbit::mapped[orbit::inverse[o]][e.move]];
                uint32_t cost=m<6?1:back[(m-6)%4].size();
                if(d<=max_distance&&cost<=max_distance-d&&d+cost<dist[qq]){dist[qq]=d+cost;next[qq]=q;move[qq]=m;pq.push({dist[qq],qq});}
            }
        }
    }
    vector<int> path(uint32_t q) const {
        if(dist[q]==INF)return {};
        vector<int> p;uint32_t length=dist[q];
        while(next[q]!=INF){auto lifted=nielsen::lift({move[q]},back);p.insert(p.end(),lifted.begin(),lifted.end());q=next[q];}
        const auto& tail=finish.at(q);p.insert(p.end(),tail.begin(),tail.end());assert(p.size()==length);return p;
    }
};

static void weighted_self_test(const Ball& ball,bool stable) {
    // An independent raw-state weighted Dijkstra checks orientation and costs.
    vector<St> raw;map<string,uint32_t> ids;vector<vector<pair<uint32_t,int>>> edges;
    auto add=[&](const St&s){auto [it,fresh]=ids.emplace(orbit::encode(s),raw.size());if(fresh)raw.push_back(s);return it->second;};
    for(const auto&s:sources(true))add(s.s);
    for(size_t i=0;i<raw.size();i++){St s=raw[i],t;vector<pair<uint32_t,int>> row;
        for(int m=0;m<14;m++)if(apply(s,m,t,6))row.push_back({add(t),m});edges.push_back(std::move(row));}
    orbit::Graph graph(true);for(const auto&s:raw)graph.intern(s);
    for(size_t i=0;i<raw.size();i++)for(auto [j,m]:edges[i])graph.edge(graph.intern(raw[i]),graph.intern(raw[j]),m);graph.dedup();
    graphcache::self_test(graph);
    for(const auto&a:nielsen::generate(1)) {
        WeightedSearch got(graph,a.back,ball,stable);vector<uint32_t> expected(raw.size(),WeightedSearch::INF);
        priority_queue<pair<uint32_t,uint32_t>,vector<pair<uint32_t,uint32_t>>,greater<>> pq;
        for(const auto&s:sources(true)){St basis;assert(nielsen::transform(s.s,a.back,basis));int d=ball.get(basis);if(d<0)continue;
            uint32_t q=ids.at(orbit::encode(s.s));if((uint32_t)d<expected[q]){expected[q]=d;pq.push({(uint32_t)d,q});}}
        while(!pq.empty()){auto [d,q]=pq.top();pq.pop();if(expected[q]!=d)continue;
            for(auto [v,m]:edges[q]){uint32_t w=nielsen::lift({INV_MOVE[m]},a.back).size();if(d+w<expected[v]){expected[v]=d+w;pq.push({d+w,v});}}}
        for(size_t i=0;i<raw.size();i++){auto [key,o]=orbit::canonical(raw[i],true);uint32_t q=graph.ids.at(key)*16+o;assert(got.dist[q]==expected[i]);
            if(expected[i]!=WeightedSearch::INF){St original;assert(nielsen::transform(raw[i],a.back,original));assert(verify(original,got.path(q),stable));}}
        for(uint32_t limit:{4U,8U,12U}) {
            WeightedSearch limited(graph,a.back,ball,stable,limit);
            for(size_t i=0;i<raw.size();i++){auto [key,o]=orbit::canonical(raw[i],true);uint32_t q=graph.ids.at(key)*16+o;
                assert(limited.dist[q]==(expected[i]<=limit?expected[i]:WeightedSearch::INF));}
        }
    }
    fprintf(stderr,"weighted raw-state comparison: %zu states, 9 maps, %s PASS\n",raw.size(),stable?"SAC":"AC");
}

int main(int argc,char**argv) {
    orbit::init();string problems,paths,targets,incumbents,seeds,out,acball,sacball,cache;int depth=1,stride=5,map_start=0,max_maps=10000,candidate_slack=0,polish_radius=0;double seconds=0;bool test=false,stable_only=false;
    for(int i=1;i<argc;i++){string a=argv[i];if(a=="--self-test"){test=true;continue;}if(a=="--stable-only"){stable_only=true;continue;}if(i+1>=argc)return 1;string v=argv[++i];
        if(a=="--problems")problems=v;else if(a=="--paths")paths=v;else if(a=="--targets")targets=v;else if(a=="--incumbents")incumbents=v;
        else if(a=="--seed-paths")seeds=v;else if(a=="--out")out=v;else if(a=="--ac-ball")acball=v;else if(a=="--sac-ball")sacball=v;else if(a=="--depth")depth=stoi(v);else if(a=="--stride")stride=stoi(v);
        else if(a=="--map-start")map_start=stoi(v);else if(a=="--max-maps")max_maps=stoi(v);else if(a=="--seconds")seconds=stod(v);
        else if(a=="--candidate-slack")candidate_slack=stoi(v);else if(a=="--polish-radius")polish_radius=stoi(v);else if(a=="--graph-cache")cache=v;else return 1;}
    if(acball.empty()||sacball.empty()||depth<0||depth>3||stride<1||map_start<0||max_maps<1||seconds<0||candidate_slack<0||polish_radius<0||polish_radius>4)return 1;
    if(test){nielsen::self_test();for(bool stable:{false,true}){Ball ball;if(!ball.load(stable?sacball:acball)||ball.stable!=stable)return 1;weighted_self_test(ball,stable);}return 0;}
    if(problems.empty()||paths.empty()||targets.empty()||incumbents.empty()||out.empty())return 1;
    if(!ifstream(problems)||!ifstream(paths)||!ifstream(targets)||!ifstream(incumbents)||(!seeds.empty()&&!ifstream(seeds))){fprintf(stderr,"unreadable input file\n");return 1;}
    auto all=read_problems(problems),jobs=read_problems(targets);auto db=read_paths(incumbents),chosen=read_paths(seeds);auto maps=nielsen::generate(depth);
    orbit::Graph graph(true);
    if(cache.empty())graph=orbit::load_graph(all,paths,true,64,2);
    else {
        graphcache::Identity identity{graphcache::fingerprint(problems),graphcache::fingerprint(paths),64,2};
        if(graphcache::load(cache,identity,graph))fprintf(stderr,"loaded checked graph cache: %zu states\n",graph.keys.size());
        else {
            graph=orbit::load_graph(all,paths,true,64,2);
            fprintf(stderr,"graph cache %s\n",graphcache::save(cache,graph,identity)?"saved atomically":"write failed; search continues without cache");
        }
    }
    auto started=chrono::steady_clock::now();
    for(bool stable:{false,true}) {
        if(stable_only&&!stable)continue;
        Ball ball;if(!ball.load(stable?sacball:acball)||ball.stable!=stable)return 1;
        struct Job{string id;St start;vector<int> best;vector<pair<St,vector<int>>> waypoints;};vector<Job> work;
        for(auto&p:jobs){string id=(stable?"sac-":"ac-")+p.id.substr(p.id.find('-')+1);auto orig=db.find(id);if(orig==db.end())continue;
            Job j{id,p.s,orig->second,{{p.s,{}}}};auto seed=chosen.find(id);
            if(seed!=chosen.end()){vector<St> states;if(verify(p.s,seed->second,stable,&states))for(size_t k=stride;k<states.size();k+=stride)j.waypoints.push_back({states[k],vector<int>(seed->second.begin(),seed->second.begin()+k)});}
            work.push_back(std::move(j));}
        ofstream f(out+(stable?"_sac.jsonl":"_ac.jsonl"));if(!f)return 1;
        for(size_t index=map_start;index<maps.size()&&index<size_t(map_start)+max_maps;index++) {
            if(seconds>0&&chrono::duration<double>(chrono::steady_clock::now()-started).count()>=seconds){fprintf(stderr,"time budget reached; next map index %zu (%s)\n",index,stable?"SAC":"AC");return 0;}
            auto map_started=chrono::steady_clock::now();
            // The existing candidate filter requires prefix+tail < best+slack.
            // Nonnegative prefix costs therefore make larger tails irrelevant,
            // even when cancellation-aware post-polishing is enabled.
            uint32_t horizon=0;for(const auto&j:work)horizon=max(horizon,uint32_t(j.best.size()+candidate_slack-1));
            const auto&a=maps[index];WeightedSearch search(graph,a.back,ball,stable,horizon);size_t changed=0;
            for(auto&j:work)for(auto&[s,prefix]:j.waypoints){St transformed;if(!nielsen::transform(s,a.forward,transformed))continue;
                auto [key,o]=orbit::canonical(transformed,true);auto it=graph.ids.find(key);if(it==graph.ids.end())continue;uint32_t q=it->second*16+o;
                // Transported length is only an upper bound. A slightly longer
                // route can improve after cancellation across the splice.
                if(search.dist[q]==WeightedSearch::INF||prefix.size()+search.dist[q]>=j.best.size()+candidate_slack)continue;
                auto candidate=prefix;auto tail=search.path(q);candidate.insert(candidate.end(),tail.begin(),tail.end());
                if(!verify(j.start,candidate,stable))continue;
                if(polish_radius>0)candidate=shorten(j.start,std::move(candidate),stable,polish_radius,4);
                if(candidate.size()<j.best.size()&&verify(j.start,candidate,stable)){fprintf(stderr,"%s %zu -> %zu (map %zu)\n",j.id.c_str(),j.best.size(),candidate.size(),index);
                    j.best=std::move(candidate);f<<jsonl(j.id,true,j.best,0,", \"method\": \"nielsen-weighted\"");f.flush();changed++;}}
            fprintf(stderr,"%s weighted map %zu/%zu: %zu improvements, %.2fs\n",stable?"SAC":"AC",index+1,maps.size(),changed,chrono::duration<double>(chrono::steady_clock::now()-map_started).count());
        }
    }
}
