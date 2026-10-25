// Transport known tails through short Nielsen automorphisms, paying every
// conjugation letter and the complete basis-reduction cost at the endpoint.
#define ORBIT_NO_MAIN
#include "orbit_splice.cpp"

namespace nielsen {
using Word=vector<uint8_t>;
using Map=array<Word,4>;
static Word inv(const Word&w){Word r;for(auto i=w.rbegin();i!=w.rend();++i)r.push_back(*i^1);return r;}
static Word subst(const Word&w,const Map&m){Word r;for(auto c:w)for(auto x:m[c]){if(!r.empty()&&(r.back()^1)==x)r.pop_back();else r.push_back(x);}return r;}
static Map make_map(Word x,Word y){return {x,inv(x),y,inv(y)};}
static Map identity(){return make_map({0},{2});}
static Map compose(const Map&a,const Map&b){return make_map(subst(b[0],a),subst(b[2],a));}
static bool transform(const St&s,const Map&m,St&t){Word a(s.r0(),s.r0()+s.n0),b(s.r1(),s.r1()+s.n1);a=subst(a,m);b=subst(b,m);
    if(a.empty()||b.empty()||a.size()+b.size()>MAXT)return false;t.n0=a.size();t.n1=b.size();copy(a.begin(),a.end(),t.c);copy(b.begin(),b.end(),t.c+a.size());return true;}
struct Aut{Map forward,back;};
static vector<Aut> generate(int depth){
    vector<Aut> generators;
    for(int rel=0;rel<2;rel++)for(int left=0;left<2;left++)for(int sign=0;sign<2;sign++){
        int a=2*rel,b=(2*(1-rel))^sign;Word w=left?Word{(uint8_t)b,(uint8_t)a}:Word{(uint8_t)a,(uint8_t)b};
        Word z=left?Word{(uint8_t)(b^1),(uint8_t)a}:Word{(uint8_t)a,(uint8_t)(b^1)};
        generators.push_back({rel?make_map({0},w):make_map(w,{2}),rel?make_map({0},z):make_map(z,{2})});
    }
    vector<Aut> all{{identity(),identity()}};set<Map> seen{identity()};size_t begin=0;
    for(int d=0;d<depth;d++){size_t end=all.size();for(size_t i=begin;i<end;i++)for(const auto&g:generators){Aut n{compose(g.forward,all[i].forward),compose(all[i].back,g.back)};if(seen.insert(n.forward).second)all.push_back(std::move(n));}begin=end;}
    return all;
}
static vector<int> lift(const vector<int>&moves,const Map&m){vector<int> r;for(int mv:moves){if(mv>=14)break;if(mv<6)r.push_back(mv);else{int base=mv<10?6:10;const auto&w=m[mv-base];for(auto c=w.rbegin();c!=w.rend();++c)r.push_back(base+*c);}}return r;}
static void self_test(){
    auto maps=generate(3);St s=make({1,2,-1,2},{2,1,1});
    for(auto&a:maps){assert(compose(a.forward,a.back)==identity());assert(compose(a.back,a.forward)==identity());
        St x;assert(transform(s,a.forward,x));for(int m=0;m<14;m++){St t,u,v;if(!apply(s,m,t,MAXT)||!transform(t,a.forward,u))continue;v=x;for(int n:lift({m},a.forward)){St next;assert(apply(v,n,next,MAXT));v=next;}assert(u==v);}}
    fprintf(stderr,"Nielsen inverse and move-transport identities: %zu automorphisms PASS\n",maps.size());
}
}

#ifndef NIELSEN_NO_MAIN
int main(int argc,char**argv){
    orbit::init();string problems,paths,targets,incumbents,seeds,out,acball,sacball;int depth=2,stride=10;
    for(int i=1;i<argc;i++){string a=argv[i];if(a=="--self-test"){nielsen::self_test();return 0;}if(i+1>=argc)return 1;string v=argv[++i];
        if(a=="--problems")problems=v;else if(a=="--paths")paths=v;else if(a=="--targets")targets=v;else if(a=="--incumbents")incumbents=v;
        else if(a=="--seed-paths")seeds=v;else if(a=="--out")out=v;else if(a=="--ac-ball")acball=v;else if(a=="--sac-ball")sacball=v;else if(a=="--depth")depth=stoi(v);else if(a=="--stride")stride=stoi(v);else return 1;}
    if(problems.empty()||paths.empty()||targets.empty()||incumbents.empty()||out.empty()||acball.empty()||sacball.empty()||depth<0||depth>4||stride<1){fprintf(stderr,"usage: nielsen_transfer --problems ALL.txt --paths POOL.jsonl --targets TARGETS.txt --incumbents PATHS.jsonl --seed-paths PATHS.jsonl --ac-ball FILE --sac-ball FILE --out PREFIX [--depth 2] [--stride 10]\n");return 1;}
    if(!ifstream(problems)||!ifstream(paths)||!ifstream(targets)||!ifstream(incumbents)||(!seeds.empty()&&!ifstream(seeds))){fprintf(stderr,"unreadable input file\n");return 1;}
    auto all=read_problems(problems),jobs=read_problems(targets);auto db=read_paths(incumbents),chosen=read_paths(seeds);auto maps=nielsen::generate(depth);
    auto graph=orbit::load_graph(all,paths,true,64,2);fprintf(stderr,"testing %zu automorphisms on %zu starts\n",maps.size(),jobs.size());
    for(bool stable:{false,true}){
        Ball ball;if(!ball.load(stable?sacball:acball)||ball.stable!=stable)return 1;auto srcs=sources(stable);
        orbit::Search search(graph,stable);ofstream f(out+(stable?"_sac.jsonl":"_ac.jsonl"));if(!f)return 1;size_t improvements=0,hits=0;
        for(auto&p:jobs){string id=(stable?"sac-":"ac-")+p.id.substr(p.id.find('-')+1);auto orig=db.find(id);if(orig==db.end())continue;auto best=orig->second;
            vector<pair<St,vector<int>>> waypoints{{p.s,{}}};auto seed=chosen.find(id);
            if(seed!=chosen.end()){vector<St> states;if(verify(p.s,seed->second,stable,&states))for(size_t k=stride;k<states.size();k+=stride)waypoints.push_back({states[k],vector<int>(seed->second.begin(),seed->second.begin()+k)});}
            for(auto&[s,prefix]:waypoints)for(const auto&a:maps){
                St transformed;if(!nielsen::transform(s,a.forward,transformed))continue;auto [key,o]=orbit::canonical(transformed,true);auto it=graph.ids.find(key);if(it==graph.ids.end())continue;
                uint32_t q=it->second*16+o;if(search.dist[q]==orbit::Search::INF)continue;
                // Conjugation images are nonempty: transporting cannot reduce
                // the number of moves in this particular stored tail.
                if(prefix.size()+search.dist[q]-(stable?2:0)>=best.size())continue;
                hits++;auto tail=search.path(q);St end=transformed,next;bool ok=true;
                for(int m:tail){if(m>=14)break;if(!apply(end,m,next,MAXT)){ok=false;break;}end=next;}
                St basis;if(!ok||!nielsen::transform(end,a.back,basis))continue;int finish_len=ball.get(basis);if(finish_len<0)continue;
                auto transported=nielsen::lift(tail,a.back);if(prefix.size()+transported.size()+finish_len>=best.size())continue;
                vector<int> finish;if(!descend(ball,srcs,basis,finish))continue;auto candidate=prefix;candidate.insert(candidate.end(),transported.begin(),transported.end());candidate.insert(candidate.end(),finish.begin(),finish.end());
                if(verify(p.s,candidate,stable))best=std::move(candidate);
            }
            if(best.size()<orig->second.size()){f<<jsonl(id,true,best,0,", \"method\": \"nielsen-transfer\"");f.flush();improvements++;fprintf(stderr,"%s %zu -> %zu\n",id.c_str(),orig->second.size(),best.size());}
        }
        fprintf(stderr,"%s: %zu improved, %zu potentially useful tail matches\n",stable?"Stable AC":"AC",improvements,hits);
    }
}
#endif
