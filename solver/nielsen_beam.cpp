// Use correctly charged Nielsen-transformed finishes throughout beam search,
// rather than limiting generator changes to a shallow initial neighborhood.
#define STABLE_BALL_NO_MAIN
#include "stable_ball_search.cpp"

struct NielsenTail : TailOracle {
    const StableBallOracle& oracle;
    struct Entry{nielsen::Aut aut;array<vector<int>,8> finish;array<bool,8> valid{};};
    vector<Entry> maps;map<string,vector<int>> known;
    mutable string cached_key;mutable vector<int> cached_path;mutable bool cached=false;
    NielsenTail(const StableBallOracle&o,int depth):oracle(o){St goal=make({1},{2});
        for(auto&aut:nielsen::generate(depth)){Entry e;e.aut=aut;for(size_t i=0;i<oracle.endpoints.size();i++){int g=oracle.endpoints[i].first;St basis=orbit::transform(goal,orbit::inverse[g]),back;
            if(nielsen::transform(basis,aut.back,back)&&oracle.length(back)>=0){e.finish[i]=oracle.path(back);e.valid[i]=true;}}maps.push_back(std::move(e));}
    }
    void set_incumbent(const St&s,const vector<int>&p){known.clear();cached=false;if(p.empty())return;vector<St> states;assert(verify(s,p,true,&states));for(size_t i=0;i<states.size();i++)known[orbit::encode(states[i])]=vector<int>(p.begin()+i,p.end());}
    void query(const St&s)const{
        auto k=orbit::encode(s);if(cached&&k==cached_key)return;cached=true;cached_key=k;cached_path.clear();auto old=known.find(k);if(old!=known.end())cached_path=old->second;
        auto goals=sources(false);
        for(const auto&e:maps){St changed;if(!nielsen::transform(s,e.aut.forward,changed)||changed.tot()>oracle.ball.cap)continue;
            for(size_t i=0;i<oracle.endpoints.size();i++){if(!e.valid[i])continue;int g=oracle.endpoints[i].first;St q=orbit::transform(changed,g);int d=oracle.ball.get(q);if(d<0||(!cached_path.empty()&&(size_t)d+e.finish[i].size()>=cached_path.size()))continue;
                vector<int> raw;if(!descend(oracle.ball,goals,q,raw))continue;for(auto&m:raw)m=orbit::mapped[orbit::inverse[g]][m];auto path=nielsen::lift(raw,e.aut.back);path.insert(path.end(),e.finish[i].begin(),e.finish[i].end());
                if((cached_path.empty()||path.size()<cached_path.size())&&verify(s,path,true))cached_path=std::move(path);
            }}
    }
    int length(const St&s)const override{query(s);return cached_path.empty()?-1:(int)cached_path.size();}
    vector<int> path(const St&s)const override{query(s);return cached_path;}
};

int main(int argc,char**argv){
    orbit::init();string targets,ballfile,out,paths;int depth=2;Opt opt;opt.stable=true;opt.width=6000;opt.time_limit=25;opt.max_depth=600;opt.maxlen=80;opt.look=3;opt.reps=8;opt.class_dedup=0;
    for(int i=1;i<argc;i++){string a=argv[i];if(i+1>=argc)return 1;string v=argv[++i];if(a=="--targets")targets=v;else if(a=="--ac-ball")ballfile=v;else if(a=="--out")out=v;else if(a=="--paths")paths=v;else if(a=="--depth")depth=stoi(v);else if(a=="--width")opt.width=stoi(v);else if(a=="--seconds")opt.time_limit=stod(v);else if(a=="--seed")opt.seed=stoull(v);else return 1;}
    if(!ifstream(targets)||!ifstream(ballfile)||out.empty()||depth<0||depth>3||opt.width<1||opt.time_limit<=0||(!paths.empty()&&!ifstream(paths)))return 1;
    Ball ball;if(!ball.load(ballfile)||ball.stable)return 1;StableBallOracle stable(ball);NielsenTail tail(stable,depth);opt.tail=&tail;Ball empty;empty.cap=-1;auto goals=sources(true);map<string,vector<int>> saved;if(!paths.empty())saved=read_paths(paths);ofstream f(out);if(!f)return 1;
    fprintf(stderr,"beam with %zu charged generator maps\n",tail.maps.size());
    for(const auto&job:read_problems(targets)){auto start=chrono::steady_clock::now();auto it=saved.find(job.id);vector<int> incumbent=it==saved.end()?vector<int>{}:it->second;tail.set_incumbent(job.s,incumbent);long long expanded=0;
        auto best=bucket_beam(job.s,empty,goals,opt,&expanded);if(best.empty()&&!incumbent.empty())best=incumbent;if(!best.empty()){best=shorten(job.s,best,true,3,4);assert(verify(job.s,best,true));}
        double seconds=chrono::duration<double>(chrono::steady_clock::now()-start).count();f<<jsonl(job.id,!best.empty(),best,seconds,", \"method\": \"nielsen-tail-beam\"");f.flush();fprintf(stderr,"%s solved=%d length=%zu seconds=%.2f expanded=%lld\n",job.id.c_str(),!best.empty(),best.size(),seconds,expanded);
    }
}
