// Explore equal-cost and slightly longer local route deformations, then polish.
// Unlike strict shortening, plateau moves can reach intermediate states absent
// from the saved solution graph. Every emitted complete path is replayed.
#define main acs_embedded_main
#include "acs.cpp"
#undef main
#include <cassert>
#include <random>

static string exact_key(const St&s){string k;k.push_back(s.n0);k.push_back(s.n1);k.append((const char*)s.c,s.tot());return k;}
struct LocalNode{St s;int parent,move,depth;};
struct Neighborhood{
    vector<LocalNode> nodes;unordered_map<string,int> ids;
    Neighborhood(const St&start,int radius,int cap,size_t budget,mt19937_64&rng){
        nodes.push_back({start,-1,-1,0});ids.emplace(exact_key(start),0);vector<int> order(14);for(int i=0;i<14;i++)order[i]=i;
        for(size_t q=0;q<nodes.size()&&nodes.size()<budget;q++){
            auto n=nodes[q];if(n.depth>=radius)continue;shuffle(order.begin(),order.end(),rng);
            for(int m:order){if(n.parent>=0&&INV_MOVE[n.move]==m)continue;St t;if(!apply(n.s,m,t,cap))continue;
                auto [it,fresh]=ids.emplace(exact_key(t),nodes.size());if(fresh)nodes.push_back({t,(int)q,m,n.depth+1});if(nodes.size()>=budget)break;}
        }
    }
    vector<int> path(int i)const{vector<int> p;while(nodes[i].parent>=0){p.push_back(nodes[i].move);i=nodes[i].parent;}reverse(p.begin(),p.end());return p;}
};

int main(int argc,char**argv){
    string problems,paths,out;int radius=4,slack=4,steps=1000,cap=64;size_t budget=50000;double seconds=60;uint64_t seed=1;
    for(int i=1;i<argc;i++){string a=argv[i];if(i+1>=argc)return 1;string v=argv[++i];
        if(a=="--problems")problems=v;else if(a=="--paths")paths=v;else if(a=="--out")out=v;else if(a=="--radius")radius=stoi(v);else if(a=="--slack")slack=stoi(v);else if(a=="--steps")steps=stoi(v);else if(a=="--cap")cap=stoi(v);else if(a=="--budget")budget=stoull(v);else if(a=="--seconds")seconds=stod(v);else if(a=="--seed")seed=stoull(v);else return 1;
    }
    if(!ifstream(problems)||!ifstream(paths)||out.empty()||radius<2||radius>6||slack<0||slack>10||steps<1||cap<2||cap>MAXT||budget<1||seconds<=0)return 1;
    auto jobs=read_problems(problems);auto saved=read_paths(paths);ofstream f(out);if(!f)return 1;mt19937_64 rng(seed);
    for(auto&job:jobs){auto it=saved.find(job.id);if(it==saved.end())continue;bool stable=job.id.rfind("sac-",0)==0;auto best=it->second,current=best;if(!verify(job.s,best,stable))return 2;
        auto t0=chrono::steady_clock::now();int accepted=0,tried=0;set<vector<int>> emitted;
        auto save=[&](const vector<int>&p){assert(verify(job.s,p,stable));if(emitted.insert(p).second){f<<jsonl(job.id,true,p,0,", \"method\": \"path-deformation\"");f.flush();}};save(best);
        for(int step=0;step<steps;step++){
            if(chrono::duration<double>(chrono::steady_clock::now()-t0).count()>=seconds)break;tried++;
            vector<St> states;assert(verify(job.s,current,stable,&states));int n=states.size()-1;if(n<3)break;
            int width=min(n,2*radius-2+(int)(rng()%3));int at=rng()%(n-width+1),end=at+width;
            Neighborhood left(states[at],radius,cap,budget,rng),right(states[end],radius,cap,budget,rng);
            vector<pair<int,int>> meets;for(size_t i=0;i<left.nodes.size();i++){auto jt=right.ids.find(exact_key(left.nodes[i].s));if(jt!=right.ids.end()&&left.nodes[i].depth+right.nodes[jt->second].depth<=width+2)meets.push_back({i,jt->second});}
            if(meets.empty())continue;shuffle(meets.begin(),meets.end(),rng);vector<int> candidate;
            for(size_t q=0;q<min(size_t(12),meets.size());q++){auto [a,b]=meets[q];auto pa=left.path(a),pb=right.path(b);candidate.assign(current.begin(),current.begin()+at);candidate.insert(candidate.end(),pa.begin(),pa.end());for(auto m=pb.rbegin();m!=pb.rend();m++)candidate.push_back(INV_MOVE[*m]);candidate.insert(candidate.end(),current.begin()+end,current.end());if(candidate!=current&&candidate.size()<=best.size()+slack)break;candidate.clear();}
            if(candidate.empty())continue;assert(verify(job.s,candidate,stable));
            int delta=(int)candidate.size()-(int)current.size();if(delta>0&&generate_canonical<double,53>(rng)>exp(-delta/1.4))continue;
            current=std::move(candidate);accepted++;
            if(accepted%5==0||current.size()<best.size()){current=shorten(job.s,current,stable,3,6);assert(verify(job.s,current,stable));save(current);}
            if(current.size()<best.size()){fprintf(stderr,"%s deformation %d: %zu -> %zu\n",job.id.c_str(),accepted,best.size(),current.size());best=current;save(best);}
            if(accepted%100==0){fprintf(stderr,"%s %d accepted, best %zu, current %zu\n",job.id.c_str(),accepted,best.size(),current.size());current=best;}
        }
        save(best);fprintf(stderr,"%s finished: tried=%d accepted=%d distinct=%zu %zu -> %zu seconds=%.2f\n",job.id.c_str(),tried,accepted,emitted.size(),it->second.size(),best.size(),chrono::duration<double>(chrono::steady_clock::now()-t0).count());
    }
}
