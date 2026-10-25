// Goal-directed exact-state bridges between verified incumbent waypoints.
// Word edit distance ranks a bounded beam; it is not a pruning lower bound.
#define main acs_embedded_main
#include "acs.cpp"
#undef main
#include <cassert>
#include <random>
#include <unordered_set>

namespace editbridge {
static string key(const St& s) {
    string k; k.push_back(s.n0); k.push_back(s.n1);
    k.append(reinterpret_cast<const char*>(s.c), s.tot()); return k;
}
struct Pattern {
    uint64_t eq[4] = {}; int n;
    Pattern(const uint8_t* p, int m): n(m) {
        assert(m <= 63);
        for(int i=0;i<m;++i) eq[p[i]] |= 1ULL<<i;
    }
    int distance(const uint8_t* text, int size) const {
        if(!n) return size;
        uint64_t pv=~0ULL, mv=0, high=1ULL<<(n-1); int score=n;
        for(int i=0;i<size;++i) {
            uint64_t e=eq[text[i]], xv=e|mv, xh=(((e&pv)+pv)^pv)|e;
            uint64_t ph=mv|~(xh|pv), mh=pv&xh;
            score += bool(ph&high); score -= bool(mh&high);
            ph=(ph<<1)|1; mh<<=1; pv=mh|~(xv|ph); mv=ph&xv;
        }
        return score;
    }
};
struct Goal {
    Pattern a,b; int total;
    Goal(const St& s): a(s.r0(),s.n0), b(s.r1(),s.n1), total(s.tot()) {}
    int score(const St& s) const {
        return 64*(a.distance(s.r0(),s.n0)+b.distance(s.r1(),s.n1))+abs(s.tot()-total);
    }
};
struct Node {St s; int parent,move;};
struct Side {
    vector<vector<Node>> levels;
    unordered_map<string,pair<int,int>> seen;
    Side(const St&s) {levels.push_back({{s,-1,-1}});seen.emplace(key(s),make_pair(0,0));}
    vector<int> path(int level,int i) const {
        vector<int> result;
        while(level>0) {const auto& n=levels[level][i];result.push_back(n.move);i=n.parent;--level;}
        reverse(result.begin(),result.end());return result;
    }
};
struct Result {bool found=false; vector<int> moves; size_t expanded=0; bool timed_out=false;};
static Result bridge(const St& a,const St& b,int maxd,int width,int cap,uint64_t seed,
                     chrono::steady_clock::time_point deadline) {
    Result result;
    if(a==b) {result.found=true;return result;}
    Side forward(a),backward(b);Goal ga(a),gb(b);
    for(int turn=1;turn<maxd;++turn) {
        Side& side=(turn%2)?forward:backward;
        Side& other=(turn%2)?backward:forward;
        Goal& goal=(turn%2)?gb:ga;
        int depth=side.levels.size();
        struct Candidate {St s;int parent,move;uint64_t score;};
        vector<Candidate> candidates;
        unordered_set<string> local;
        auto& current=side.levels.back();
        for(int i=0;i<(int)current.size();++i) {
            if((i&255)==0&&chrono::steady_clock::now()>=deadline) {result.timed_out=true;return result;}
            ++result.expanded;
            for(int move=0;move<14;++move) {
                if(current[i].move>=0&&INV_MOVE[current[i].move]==move)continue;
                St next;if(!apply(current[i].s,move,next,cap))continue;
                auto k=key(next);if(side.seen.count(k))continue;
                auto hit=other.seen.find(k);
                if(hit!=other.seen.end()&&depth+hit->second.first<maxd) {
                    auto first=side.path(depth-1,i);first.push_back(move);
                    auto second=other.path(hit->second.first,hit->second.second);
                    if(&side!=&forward)swap(first,second);
                    for(auto j=second.rbegin();j!=second.rend();++j)first.push_back(INV_MOVE[*j]);
                    St replay=a,t;for(int m:first){assert(apply(replay,m,t,cap));replay=t;}
                    assert(replay==b);result.found=true;result.moves=std::move(first);return result;
                }
                if(!local.insert(std::move(k)).second)continue;
                uint64_t score=(uint64_t(goal.score(next))<<32)|(mix(hst(next)^seed)&0xffffffff);
                candidates.push_back({next,i,move,score});
            }
        }
        if(candidates.empty())break;
        if((int)candidates.size()>width) {
            nth_element(candidates.begin(),candidates.begin()+width,candidates.end(),
                [](const Candidate&x,const Candidate&y){return x.score<y.score;});
            candidates.resize(width);
        }
        vector<Node> chosen;chosen.reserve(candidates.size());
        for(auto& c:candidates) {
            side.seen.emplace(key(c.s),make_pair(depth,(int)chosen.size()));
            chosen.push_back({c.s,c.parent,c.move});
        }
        side.levels.push_back(std::move(chosen));
    }
    return result;
}
static int slow_distance(const vector<uint8_t>&a,const vector<uint8_t>&b) {
    vector<int> d(b.size()+1);for(int j=0;j<(int)d.size();++j)d[j]=j;
    for(int i=1;i<=(int)a.size();++i){int prev=d[0];d[0]=i;for(int j=1;j<=(int)b.size();++j){int old=d[j];d[j]=min({d[j]+1,d[j-1]+1,prev+(a[i-1]!=b[j-1])});prev=old;}}
    return d.back();
}
static void self_test() {
    mt19937_64 rng(2129);
    for(int t=0;t<10000;++t){vector<uint8_t>a(rng()%64),b(rng()%64);for(auto&v:a)v=rng()%4;for(auto&v:b)v=rng()%4;assert(Pattern(a.data(),a.size()).distance(b.data(),b.size())==slow_distance(a,b));}
    St a=make({1,2},{2}),b,t;assert(apply(a,3,b,20));
    auto r=bridge(a,b,4,1000,20,1,chrono::steady_clock::now()+chrono::seconds(5));
    assert(r.found&&r.moves.size()==1&&r.moves[0]==3);
    auto same=bridge(a,a,2,100,20,1,chrono::steady_clock::now()+chrono::seconds(5));assert(same.found&&same.moves.empty());
    fprintf(stderr,"PASS: 10000 Myers/independent-DP comparisons, direct bridge, identical endpoints\n");
}
}

int main(int argc,char**argv) {
    string problems,paths,out;int width=8000,cap=64,stride=2,first_at=0;double seconds=120;uint64_t seed=1;
    vector<int>windows={39,35,30,25,20,15,10,8};
    for(int i=1;i<argc;++i){string a=argv[i];if(a=="--self-test"){editbridge::self_test();return 0;}
        if(i+1>=argc)return 1;string v=argv[++i];
        if(a=="--problems")problems=v;else if(a=="--paths")paths=v;else if(a=="--out")out=v;
        else if(a=="--width")width=stoi(v);else if(a=="--cap")cap=stoi(v);else if(a=="--stride")stride=stoi(v);else if(a=="--first-at")first_at=stoi(v);
        else if(a=="--seconds")seconds=stod(v);else if(a=="--seed")seed=stoull(v);
        else if(a=="--windows"){windows.clear();replace(v.begin(),v.end(),',',' ');istringstream s(v);int n;while(s>>n)windows.push_back(n);}
        else return 1;
    }
    if(problems.empty()||paths.empty()||out.empty()||width<1||cap<2||cap>64||stride<1||first_at<0||seconds<=0||windows.empty())return 1;
    for(int w:windows)if(w<2||w>100)return 1;
    auto jobs=read_problems(problems);auto saved=read_paths(paths);ofstream f(out);if(!f)return 1;
    for(auto&job:jobs){auto it=saved.find(job.id);if(it==saved.end())continue;
        bool stable=job.id.rfind("sac-",0)==0;auto best=it->second;if(!verify(job.s,best,stable))return 2;
        auto start=chrono::steady_clock::now(),deadline=start+chrono::milliseconds((int)(1000*seconds));
        size_t expanded=0;int attempts=0,resume_at=first_at;bool timeout=false,improved=true;
        while(improved&&!timeout){improved=false;vector<St>states;assert(verify(job.s,best,stable,&states));
            int n=states.size()-1;
            for(int w:windows){if(w>n)continue;
                int begin_at=resume_at;resume_at=0;
                for(int at=begin_at;at+w<=n;at+=stride){
                    if(chrono::steady_clock::now()>=deadline){fprintf(stderr,"%s paused window=%d at=%d\n",job.id.c_str(),w,at);timeout=true;break;}
                    if(states[at].tot()>cap||states[at+w].tot()>cap)continue;
                    auto r=editbridge::bridge(states[at],states[at+w],w,width,cap,seed+attempts,deadline);++attempts;expanded+=r.expanded;
                    if(r.found){vector<int>candidate(best.begin(),best.begin()+at);candidate.insert(candidate.end(),r.moves.begin(),r.moves.end());candidate.insert(candidate.end(),best.begin()+at+w,best.end());assert(verify(job.s,candidate,stable));assert(candidate.size()<best.size());
                        fprintf(stderr,"%s window %d@%d: %zu -> %zu\n",job.id.c_str(),w,at,best.size(),candidate.size());best=std::move(candidate);f<<jsonl(job.id,true,best,0,", \"method\": \"edit-bridge\"");f.flush();improved=true;break;}
                    if(r.timed_out){fprintf(stderr,"%s paused window=%d at=%d\n",job.id.c_str(),w,at);timeout=true;break;}
                }
                fprintf(stderr,"%s window=%d attempts=%d expanded=%zu best=%zu\n",job.id.c_str(),w,attempts,expanded,best.size());
                if(improved||timeout)break;
            }
        }
        f<<jsonl(job.id,true,best,0,", \"method\": \"edit-bridge\"");f.flush();
        fprintf(stderr,"%s finished %zu -> %zu attempts=%d expanded=%zu timeout=%d seconds=%.3f\n",job.id.c_str(),it->second.size(),best.size(),attempts,expanded,timeout,chrono::duration<double>(chrono::steady_clock::now()-start).count());
    }
}
