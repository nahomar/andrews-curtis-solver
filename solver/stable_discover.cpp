// Discover Stable AC solutions without requiring an incumbent route.
// Uses every correctly charged signed-basis endpoint of an ordinary AC ball.
#define STABLE_BALL_NO_MAIN
#include "stable_ball_search.cpp"

int main(int argc,char**argv){
    orbit::init();string targets,ballfile,out;Opt opt;opt.stable=true;opt.width=8000;opt.time_limit=30;opt.max_depth=600;opt.look=3;opt.radius=3;
    for(int i=1;i<argc;i++){string a=argv[i];if(a=="--self-test"){self_test();return 0;}if(i+1>=argc)return 1;string v=argv[++i];
        if(a=="--targets")targets=v;else if(a=="--ac-ball")ballfile=v;else if(a=="--out")out=v;
        else if(a=="--width")opt.width=stoi(v);else if(a=="--time-limit")opt.time_limit=stod(v);else if(a=="--max-depth")opt.max_depth=stoi(v);else if(a=="--maxlen")opt.maxlen=stoi(v);
        else if(a=="--seed")opt.seed=stoull(v);else if(a=="--look")opt.look=stoi(v);else if(a=="--reps")opt.reps=stoi(v);else if(a=="--class-dedup")opt.class_dedup=stoi(v);else return 1;
    }
    if(!ifstream(targets)||!ifstream(ballfile)||out.empty()||opt.width<1||opt.time_limit<=0||opt.max_depth<1||opt.maxlen<2||opt.maxlen>MAXT||opt.reps<1)return 1;
    Ball ac;if(!ac.load(ballfile)||ac.stable)return 1;StableBallOracle oracle(ac);Ball empty;empty.cap=-1;auto goals=sources(true);opt.tail=&oracle;ofstream f(out);if(!f)return 1;
    for(const auto&job:read_problems(targets)){
        auto t0=chrono::steady_clock::now();long long expanded=0;auto best=bucket_beam(job.s,empty,goals,opt,&expanded);
        if(!best.empty()){assert(verify(job.s,best,true));best=polish_ball(job.s,best,oracle,4,32,2000000);best=shorten(job.s,best,true,3,4);assert(verify(job.s,best,true));}
        double secs=chrono::duration<double>(chrono::steady_clock::now()-t0).count();f<<jsonl(job.id,!best.empty(),best,secs,", \"method\": \"stable-discovery-ball-endpoints\"");f.flush();fprintf(stderr,"%s solved=%d length=%zu %.1fs expanded=%lld\n",job.id.c_str(),!best.empty(),best.size(),secs,expanded);
    }
}
