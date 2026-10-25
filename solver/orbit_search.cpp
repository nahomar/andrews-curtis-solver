// Search for new routes into a symmetry-aware database of verified tails.
// Build: clang++ -O3 -std=c++17 -pthread orbit_search.cpp -o orbit_search
#define ORBIT_NO_MAIN
#include "orbit_splice.cpp"

int main(int argc,char**argv) {
    orbit::init();
    string problems,paths,targets,ball,out;Opt o;int threads=2,rounds=2,midcap=64,waypoints=0;bool use_oracle=true;
    o.width=3000;o.time_limit=30;o.look=3;o.radius=3;
    for(int i=1;i<argc;i++) {
        string a=argv[i];
        if(a=="--self-test"){orbit::self_test();return 0;}
        if(a=="--stable"){o.stable=true;continue;}
        if(a=="--without-oracle"){use_oracle=false;continue;}
        if(i+1>=argc){fprintf(stderr,"missing argument\n");return 1;}
        string v=argv[++i];
        if(a=="--problems")problems=v;else if(a=="--paths")paths=v;else if(a=="--targets")targets=v;
        else if(a=="--ball")ball=v;else if(a=="--out")out=v;
        else if(a=="--width")o.width=stoi(v);else if(a=="--time-limit")o.time_limit=stod(v);
        else if(a=="--threads")threads=stoi(v);else if(a=="--seed")o.seed=stoull(v);
        else if(a=="--radius")o.radius=stoi(v);else if(a=="--reps")o.reps=stoi(v);
        else if(a=="--maxlen")o.maxlen=stoi(v);else if(a=="--look")o.look=stoi(v);
        else if(a=="--junk")o.junk=stoi(v);else if(a=="--look-pen")o.look_pen=stoi(v);
        else if(a=="--lincap")o.lincap=stoi(v);else if(a=="--linper")o.linper=stoi(v);
        else if(a=="--waypoints")waypoints=stoi(v);else if(a=="--join-rounds")rounds=stoi(v);
        else if(a=="--two-move-cap")midcap=stoi(v);else if(a=="--max-depth")o.max_depth=stoi(v);
        else {fprintf(stderr,"unknown option %s\n",a.c_str());return 1;}
    }
    if(problems.empty()||paths.empty()||targets.empty()||out.empty()||o.width<1||threads<1||o.time_limit<=0||o.maxlen>MAXT||midcap>MAXT||rounds<1||o.linper<1){fprintf(stderr,"usage: orbit_search --problems ALL.txt --paths POOL.jsonl --targets TARGETS.txt --out OUT.jsonl [--stable] [--ball FILE] [--width N] [--time-limit SECONDS] [--threads N] [--waypoints N]\n");return 1;}
    if(!ifstream(problems)||!ifstream(paths)||!ifstream(targets)||o.maxlen<2||midcap<0||waypoints<0||o.radius<0){fprintf(stderr,"invalid input or search bound\n");return 1;}
    Ball B;B.cap=-1;
    if(!ball.empty()&&(!B.load(ball)||B.stable!=o.stable)){fprintf(stderr,"invalid ball\n");return 1;}
    auto all=read_problems(problems),jobs=read_problems(targets);auto srcs=sources(o.stable);
    auto graph=orbit::load_graph(all,paths,true,midcap,rounds);
    orbit::Search lookup(graph,o.stable);orbit::Oracle oracle(graph,lookup);if(use_oracle)o.tail=&oracle;
    ofstream f(out);if(!f){fprintf(stderr,"cannot open output\n");return 1;}
    atomic<size_t> next{0};mutex mu;
    auto work=[&](){
        for(size_t k;(k=next++)<jobs.size();) {
            const auto&p=jobs[k];auto t0=chrono::steady_clock::now();long long expanded=0;
            auto best=oracle.path(p.s);int initial=best.empty()?-1:(int)best.size();
            auto found=bucket_beam(p.s,B,srcs,o,&expanded);
            if(!found.empty()&&(best.empty()||found.size()<best.size()))best=std::move(found);
            if(!best.empty()&&waypoints>0) {
                vector<St> states;if(verify(p.s,best,o.stable,&states)) {
                    auto original=best;size_t n=states.size()-1;
                    for(int j=1;j<=waypoints;j++) {
                        size_t at=n*j/(waypoints+1);if(at==0||at>=n)continue;
                        auto tail=bucket_beam(states[at],B,srcs,o,&expanded);
                        if(!tail.empty()&&at+tail.size()<best.size()) {
                            vector<int> candidate(original.begin(),original.begin()+at);candidate.insert(candidate.end(),tail.begin(),tail.end());
                            if(verify(p.s,candidate,o.stable))best=std::move(candidate);
                        }
                    }
                }
            }
            bool ok=!best.empty()&&verify(p.s,best,o.stable);
            if(ok&&o.radius>0)best=shorten(p.s,best,o.stable,o.radius,o.slack,B.cap>=0?&B:nullptr,&srcs);
            ok=!best.empty()&&verify(p.s,best,o.stable);
            double secs=chrono::duration<double>(chrono::steady_clock::now()-t0).count();
            lock_guard<mutex> lock(mu);
            f<<jsonl(p.id,ok,best,secs,", \"initial\": "+to_string(initial)+", \"expanded\": "+to_string(expanded));f.flush();
            fprintf(stderr,"%s %d -> %d %.1fs expanded=%lld\n",p.id.c_str(),initial,ok?(int)best.size():-1,secs,expanded);
        }
    };
    vector<thread> workers;for(int i=0;i<threads;i++)workers.emplace_back(work);for(auto&t:workers)t.join();
}
