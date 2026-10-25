// Reuse an ordinary AC distance table for every valid rank-two Stable AC
// endpoint. Signed generator permutations preserve the length bound; each
// endpoint's required inversions and destabilizations are fully charged.
#define NIELSEN_NO_MAIN
#include "nielsen_transfer.cpp"

struct StableBallOracle : TailOracle {
    const Ball& ball;
    vector<pair<int,vector<int>>> endpoints;
    explicit StableBallOracle(const Ball& b):ball(b) {
        assert(!ball.stable);
        auto stable_goals=sources(true);St goal=make({1},{2});
        for(int g=0;g<16;g+=2){St basis=orbit::transform(goal,orbit::inverse[g]);
            for(const auto&s:stable_goals)if(s.s==basis)endpoints.push_back({g,s.suffix});}
        assert(endpoints.size()==8);
    }
    pair<int,int> best(const St&s) const {
        if(s.tot()>ball.cap)return {-1,-1};
        int length=INT32_MAX,index=-1;
        for(size_t i=0;i<endpoints.size();i++){int d=ball.get(orbit::transform(s,endpoints[i].first));
            if(d>=0&&d+(int)endpoints[i].second.size()<length){length=d+endpoints[i].second.size();index=i;}}
        return index<0?pair<int,int>{-1,-1}:pair<int,int>{length,index};
    }
    int length(const St&s) const override{return best(s).first;}
    vector<int> path(const St&s) const override {
        auto [d,i]=best(s);if(i<0)return {};
        int g=endpoints[i].first;vector<int> p;
        if(!descend(ball,sources(false),orbit::transform(s,g),p))return {};
        for(auto&m:p)m=orbit::mapped[orbit::inverse[g]][m];
        p.insert(p.end(),endpoints[i].second.begin(),endpoints[i].second.end());
        assert((int)p.size()==d&&verify(s,p,true));return p;
    }
};

struct SeedOracle : TailOracle {
    const StableBallOracle& ball;
    map<string,vector<int>> tails;
    SeedOracle(const StableBallOracle& b,const St&s,const vector<int>&p):ball(b){
        vector<St> states;assert(verify(s,p,true,&states));
        for(size_t i=0;i<states.size();i++){string k=orbit::encode(states[i]);vector<int> tail(p.begin()+i,p.end());
            auto it=tails.find(k);if(it==tails.end()||tail.size()<it->second.size())tails[k]=std::move(tail);}
    }
    int length(const St&s) const override{int d=ball.length(s);auto it=tails.find(orbit::encode(s));
        return it!=tails.end()&&(d<0||(int)it->second.size()<d)?it->second.size():d;}
    vector<int> path(const St&s) const override{int d=ball.length(s);auto it=tails.find(orbit::encode(s));
        return it!=tails.end()&&(d<0||(int)it->second.size()<d)?it->second:ball.path(s);}
};

static vector<int> polish_ball(const St&start,vector<int> best,const StableBallOracle& oracle,int radius,int cap,size_t budget){
    vector<St> path_states;assert(verify(start,best,true,&path_states));auto original=best;
    unordered_map<uint64_t,int> distances;size_t checked=0;
    for(size_t at=0;at<path_states.size()&&checked<budget;at++){
        struct Node{St s;int parent;uint8_t move,depth;};vector<Node> nodes{{path_states[at],-1,0,0}};
        FlatSet seen(4096);seen.insert(hst(path_states[at]));
        for(size_t q=0;q<nodes.size()&&checked<budget;q++){
            St s=nodes[q].s;int depth=nodes[q].depth;checked++;
            if(s.tot()<=oracle.ball.cap){uint64_t key=pack(s);auto [it,fresh]=distances.emplace(key,-1);if(fresh)it->second=oracle.length(s);
                if(it->second>=0&&at+depth+it->second<best.size()){
                    vector<int> steps;for(int k=q;nodes[k].parent>=0;k=nodes[k].parent)steps.push_back(nodes[k].move);reverse(steps.begin(),steps.end());
                    vector<int> candidate(original.begin(),original.begin()+at);candidate.insert(candidate.end(),steps.begin(),steps.end());auto tail=oracle.path(s);candidate.insert(candidate.end(),tail.begin(),tail.end());
                    if(verify(start,candidate,true)){fprintf(stderr,"ball finish %zu -> %zu at prefix %zu, detour %d\n",best.size(),candidate.size(),at,depth);best=std::move(candidate);}
                }}
            if(depth==radius||at+depth+2>=best.size())continue;
            for(int m=0;m<14;m++){St t;if(apply(s,m,t,cap)&&seen.insert(hst(t)))nodes.push_back({t,(int)q,(uint8_t)m,(uint8_t)(depth+1)});}
        }
    }
    fprintf(stderr,"ball-neighborhood states checked: %zu%s\n",checked,checked>=budget?" (budget reached)":"");return best;
}

static vector<int> nielsen_ball(const St&start,vector<int> best,const StableBallOracle& oracle,int depth,int stride,int slack){
    auto maps=nielsen::generate(depth);vector<St> states;assert(verify(start,best,true,&states));auto original=best;auto ordinary=sources(false);size_t matches=0,polished=0;
    St goal=make({1},{2});
    for(size_t a=0;a<maps.size();a++){
        const auto&aut=maps[a];array<vector<int>,8> finishes;array<bool,8> valid{};
        for(size_t e=0;e<oracle.endpoints.size();e++){int g=oracle.endpoints[e].first;St end=orbit::transform(goal,orbit::inverse[g]),basis;
            if(nielsen::transform(end,aut.back,basis)&&oracle.length(basis)>=0){finishes[e]=oracle.path(basis);valid[e]=true;}}
        for(size_t at=0;at<states.size();at+=stride){St changed;if(!nielsen::transform(states[at],aut.forward,changed)||changed.tot()>oracle.ball.cap)continue;
            for(size_t e=0;e<oracle.endpoints.size();e++){if(!valid[e])continue;int g=oracle.endpoints[e].first;St query=orbit::transform(changed,g);int d=oracle.ball.get(query);
                if(d<0||at+d+finishes[e].size()>best.size()+slack)continue;matches++;
                vector<int> raw;if(!descend(oracle.ball,ordinary,query,raw))continue;for(auto&m:raw)m=orbit::mapped[orbit::inverse[g]][m];
                auto transported=nielsen::lift(raw,aut.back);if(at+transported.size()+finishes[e].size()>best.size()+slack)continue;
                vector<int> candidate(original.begin(),original.begin()+at);candidate.insert(candidate.end(),transported.begin(),transported.end());candidate.insert(candidate.end(),finishes[e].begin(),finishes[e].end());
                if(!verify(start,candidate,true))continue;
                if(slack>0){candidate=shorten(start,candidate,true,2,4);polished++;}
                if(candidate.size()<best.size()){fprintf(stderr,"Nielsen ball %zu -> %zu, map %zu, prefix %zu\n",best.size(),candidate.size(),a,at);best=std::move(candidate);}
            }}
    }
    fprintf(stderr,"Nielsen ball: %zu maps, %zu potentially useful matches, %zu polished\n",maps.size(),matches,polished);return best;
}

static void self_test(){
    // Build two independent small exact tables and compare every raw state.
    auto build=[](bool stable){Ball b;b.cap=6;b.stable=stable;b.init(4096);
        priority_queue<pair<int,uint64_t>,vector<pair<int,uint64_t>>,greater<>> q;
        for(const auto&s:sources(stable))q.push({s.suffix.size(),pack(s.s)});
        while(!q.empty()){auto [d,k]=q.top();q.pop();if(!b.insert(k,d))continue;St s=unpack(k),t;
            for(int m=0;m<14;m++)if(apply(s,m,t,b.cap)&&b.get(t)<0)q.push({d+1,pack(t)});}return b;};
    Ball ac=build(false),stable=build(true);StableBallOracle oracle(ac);size_t tested=0;
    for(auto k:stable.keys)if(k){St s=unpack(k);assert(oracle.length(s)==stable.get(s));assert(verify(s,oracle.path(s),true));tested++;}
    fprintf(stderr,"Stable endpoints vs independent stable table: %zu states PASS\n",tested);
}

#ifndef STABLE_BALL_NO_MAIN
int main(int argc,char**argv){
    orbit::init();string targets,paths,ballfile,out;int radius=4,cap=32,waypoints=0,autdepth=0,stride=2,polish_slack=0;size_t budget=3000000;Opt opt;opt.stable=true;opt.width=4000;opt.time_limit=30;opt.look=3;opt.radius=3;
    for(int i=1;i<argc;i++){string a=argv[i];if(a=="--self-test"){self_test();return 0;}if(i+1>=argc)return 1;string v=argv[++i];
        if(a=="--targets")targets=v;else if(a=="--paths")paths=v;else if(a=="--ac-ball")ballfile=v;else if(a=="--out")out=v;
        else if(a=="--radius")radius=stoi(v);else if(a=="--cap")cap=stoi(v);else if(a=="--budget")budget=stoull(v);else if(a=="--width")opt.width=stoi(v);else if(a=="--time-limit")opt.time_limit=stod(v);else if(a=="--waypoints")waypoints=stoi(v);
        else if(a=="--nielsen-depth")autdepth=stoi(v);else if(a=="--stride")stride=stoi(v);else if(a=="--polish-slack")polish_slack=stoi(v);
        else if(a=="--macros"){ifstream mf(v);if(!mf){fprintf(stderr,"unreadable macro file\n");return 1;}string line;while(getline(mf,line)){line=line.substr(0,line.find('#'));istringstream row(line);vector<int> moves;int m;while(row>>m){if(m<0||m>=14)return 1;moves.push_back(m);}if(moves.size()>48)return 1;if(moves.size()>=2)opt.macros.push_back(moves);}fprintf(stderr,"loaded %zu fully charged macros\n",opt.macros.size());}
        else if(a=="--gs"){if(!GS.load(v)){fprintf(stderr,"cannot load GS table\n");return 1;}HAVE_GS=true;fprintf(stderr,"GS heuristic: cap%d, %zu classes\n",GS.cap,GS.keys.size());}
        else if(a=="--gsw")opt.gsw=stoi(v);else if(a=="--gspen")opt.gspen=stoi(v);
        else if(a=="--profile-weight")opt.profile_weight=stoi(v);else if(a=="--profile-ahead")opt.profile_ahead=stoi(v);
        else if(a=="--seed")opt.seed=stoull(v);else if(a=="--look")opt.look=stoi(v);else if(a=="--junk")opt.junk=stoi(v);else if(a=="--look-pen")opt.look_pen=stoi(v);else if(a=="--maxlen")opt.maxlen=stoi(v);else if(a=="--lincap")opt.lincap=stoi(v);else if(a=="--linper")opt.linper=stoi(v);else if(a=="--reps")opt.reps=stoi(v);else if(a=="--class-dedup")opt.class_dedup=stoi(v);else return 1;}
    if(!ifstream(targets)||!ifstream(paths)||!ifstream(ballfile)||out.empty()||radius<0||radius>8||cap<2||cap>MAXT||!budget||opt.width<0||opt.time_limit<=0||waypoints<0||opt.maxlen<2||opt.maxlen>MAXT||opt.linper<1)return 1;
    if(autdepth<0||autdepth>4||stride<1||polish_slack<0||polish_slack>10||opt.reps<1)return 1;
    if(opt.profile_weight<0||opt.profile_ahead<0||opt.profile_ahead>20||(opt.profile_weight>0&&HAVE_GS))return 1;
    if(opt.profile_weight>0)opt.look=0; // The ordinary lookahead would erase the growth preference.
    Ball ac;if(!ac.load(ballfile)||ac.stable)return 1;StableBallOracle oracle(ac);auto jobs=read_problems(targets);auto saved=read_paths(paths);ofstream f(out);if(!f)return 1;
    Ball empty;empty.cap=-1;auto goals=sources(true);
    auto set_profile=[&](const St&s,const vector<int>&p){opt.cyclic_profile.clear();if(!opt.profile_weight)return;
        vector<St> states;assert(verify(s,p,true,&states));for(const auto&t:states)opt.cyclic_profile.push_back(cyc_len(t.r0(),t.n0)+cyc_len(t.r1(),t.n1));};
    for(const auto&job:jobs){auto it=saved.find(job.id);if(it==saved.end()||!verify(job.s,it->second,true))continue;auto t0=chrono::steady_clock::now();
        if(HAVE_GS)fprintf(stderr,"%s initial GS distance=%d, cyclic length=%d\n",job.id.c_str(),GS.get(job.s),cyc_len(job.s.r0(),job.s.n0)+cyc_len(job.s.r1(),job.s.n1));
        auto best=polish_ball(job.s,it->second,oracle,radius,cap,budget);long long expanded=0;
        if(autdepth>0)best=nielsen_ball(job.s,best,oracle,autdepth,stride,polish_slack);
        if(opt.width>0){SeedOracle seed(oracle,job.s,best);opt.tail=&seed;opt.max_depth=best.size();set_profile(job.s,best);auto found=bucket_beam(job.s,empty,goals,opt,&expanded);if(!found.empty()&&found.size()<best.size())best=std::move(found);
            vector<St> states;assert(verify(job.s,best,true,&states));auto base=best;
            for(int j=1;j<=waypoints;j++){size_t at=(states.size()-1)*j/(waypoints+1);if(!at)continue;
                vector<int> current(base.begin()+at,base.end());SeedOracle waypoint(oracle,states[at],current);opt.tail=&waypoint;set_profile(states[at],current);
                auto tail=bucket_beam(states[at],empty,goals,opt,&expanded);if(!tail.empty()&&at+tail.size()<best.size()){vector<int> candidate(base.begin(),base.begin()+at);candidate.insert(candidate.end(),tail.begin(),tail.end());if(verify(job.s,candidate,true))best=std::move(candidate);}}
        }
        best=shorten(job.s,best,true,3,4);assert(verify(job.s,best,true));double secs=chrono::duration<double>(chrono::steady_clock::now()-t0).count();
        f<<jsonl(job.id,true,best,secs,", \"method\": \"stable-ball-endpoints\"");f.flush();fprintf(stderr,"%s %zu -> %zu %.1fs expanded=%lld\n",job.id.c_str(),it->second.size(),best.size(),secs,expanded);
    }
}
#endif
