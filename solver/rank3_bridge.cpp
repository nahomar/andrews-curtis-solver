// Bounded, exact-state neighborhoods of stabilized verified route states.
// Every stabilization, rank-three move, and destabilization is charged.
// Voronoi-region collisions provide valid candidate shortcuts; this bounded
// search is heuristic and does not establish optimality when no shortcut wins.
#define main acs_embedded_main
#include "acs.cpp"
#undef main
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
struct Move {int id,kind,i,j,inv,undo;};
static const Move moves[]={
#include "rank3_moves.inc"
};
struct State {uint8_t n[3]{}, c[64]{}; int total()const{return n[0]+n[1]+n[2];}};
static string key(const State&s){string k((char*)s.n,3);k.append((char*)s.c,s.total());return k;}
static bool step(const State&s,const Move&m,State&t,int cap){
    int offsets[3]={0,s.n[0],s.n[0]+s.n[1]}, n=0;uint8_t w[130];
    auto push=[&](int c){if(n&&w[n-1]==(c^1))--n;else w[n++]=c;};
    int at=offsets[m.i],len=s.n[m.i];
    if(m.kind==0)for(int a=len-1;a>=0;--a)push(s.c[at+a]^1);
    else{
        if(m.kind==2)push(m.j);
        for(int a=0;a<len;++a)push(s.c[at+a]);
        if(m.kind==1){int b=offsets[m.j],bl=s.n[m.j];
            if(m.inv)for(int a=bl-1;a>=0;--a)push(s.c[b+a]^1);
            else for(int a=0;a<bl;++a)push(s.c[b+a]);}
        if(m.kind==2)push(m.j^1);
    }
    int total=s.total()-len+n;if(total>cap||total>64)return false;
    t=s;t.n[m.i]=n;memcpy(t.c+at,w,n);memcpy(t.c+at+n,s.c+at+len,s.total()-at-len);return true;
}
struct Seed {State s;vector<int> prefix,tail;};
struct Node {State s;uint32_t parent,origin;uint16_t move;uint8_t depth;};
static vector<int> trace(const vector<Node>&nodes,uint32_t i){vector<int> p;while(nodes[i].parent!=UINT32_MAX){p.push_back(nodes[i].move);i=nodes[i].parent;}reverse(p.begin(),p.end());return p;}
static int undo(int id){for(auto&m:moves)if(m.id==id)return m.undo;abort();}
static bool remove_generator(const State&s,int i,St&t){
    int offsets[3]={0,s.n[0],s.n[0]+s.n[1]};
    if(s.n[i]!=1||(s.c[offsets[i]]&1))return false;
    int g=s.c[offsets[i]]/2;
    for(int j=0;j<3;j++)if(j!=i)for(int a=0;a<s.n[j];a++)if(s.c[offsets[j]+a]/2==g)return false;
    int n=0,r=0;for(int j=0;j<3;j++)if(j!=i){if(r++==0)t.n0=s.n[j];else t.n1=s.n[j];for(int a=0;a<s.n[j];a++){int c=s.c[offsets[j]+a];t.c[n++]=c/2>g?c-2:c;}}
    return true;
}
static void emit(const string&id,const vector<int>&p,ofstream&f,const string&method="rank3-bridge"){f<<"{\"id\":\""<<id<<"\",\"solved\":true,\"method\":\""<<method<<"\",\"moves\":[";for(size_t i=0;i<p.size();i++){if(i)f<<',';f<<p[i];}f<<"]}\n";f.flush();}
#ifndef RANK3_BRIDGE_NO_MAIN
int main(int argc,char**argv){
    if(argc==2&&string(argv[1])=="--destab"){
        State s;int a,b,c;while(cin>>a>>b>>c){s.n[0]=a;s.n[1]=b;s.n[2]=c;int x;for(int j=0;j<s.total();j++){cin>>x;s.c[j]=2*(abs(x)-1)+(x<0);}for(int i=0;i<3;i++){St t;if(!remove_generator(s,i,t))continue;cout<<15+i<<' '<<(int)t.n0<<' '<<(int)t.n1<<' ';for(int j=0;j<t.tot();j++)cout<<((t.c[j]&1)?-1:1)*(t.c[j]/2+1)<<' ';cout<<'\n';}cout<<"end\n";}return 0;
    }
    if(argc==2&&string(argv[1])=="--moves"){
        State s;int a,b,c;while(cin>>a>>b>>c){s.n[0]=a;s.n[1]=b;s.n[2]=c;int x;for(int j=0;j<s.total();j++){cin>>x;s.c[j]=2*(abs(x)-1)+(x<0);}for(const auto&m:moves){State t;if(!step(s,m,t,64))continue;cout<<m.id<<' ';for(int j=0;j<3;j++)cout<<(int)t.n[j]<<' ';for(int j=0;j<t.total();j++)cout<<((t.c[j]&1)?-1:1)*(t.c[j]/2+1)<<' ';cout<<'\n';}}return 0;
    }
    if(argc!=8&&argc!=10){cerr<<"rank3_bridge SEEDS ID RADIUS CAP BUDGET BEST_LENGTH OUT [--ball STABLE_BALL]\n";return 1;}
    ifstream in(argv[1]);string id=argv[2];int radius=stoi(argv[3]),cap=stoi(argv[4]),best=stoi(argv[6]);size_t budget=stoull(argv[5]);ofstream out(argv[7]);
    if(!in||!out||radius<1||radius>8||cap<3||cap>64||!budget)return 1;
    Ball ball;ball.cap=-1;if(argc==10&&(string(argv[8])!="--ball"||!ball.load(argv[9])||!ball.stable))return 1;auto goals=sources(true);
    vector<Seed> seeds;unordered_map<string,uint32_t> seedids;int np,nt,n0,n1;
    while(in>>np>>nt>>n0>>n1){if(np<0||nt<0||n0<0||n1<0||n0+n1+1>64)return 2;Seed s;s.s.n[0]=n0;s.s.n[1]=n1;s.s.n[2]=1;s.prefix.resize(np);s.tail.resize(nt);for(auto&v:s.prefix)in>>v;for(auto&v:s.tail)in>>v;int x;for(int j=0;j<n0+n1;j++){in>>x;s.s.c[j]=2*(abs(x)-1)+(x<0);}s.s.c[n0+n1]=4;if(s.s.total()>cap)continue;auto [it,fresh]=seedids.emplace(key(s.s),seeds.size());if(fresh)seeds.push_back(s);else{auto&old=seeds[it->second];if(s.prefix.size()<old.prefix.size())old.prefix=s.prefix;if(s.tail.size()<old.tail.size())old.tail=s.tail;}}
    vector<Node> nodes;nodes.reserve(min(budget,size_t(5000000)));unordered_map<string,uint32_t> seen;seen.reserve(min(budget,size_t(5000000)));
    for(size_t i=0;i<seeds.size();i++){seen.emplace(key(seeds[i].s),nodes.size());nodes.push_back({seeds[i].s,UINT32_MAX,(uint32_t)i,0,0});}
    size_t collisions=0,expanded=0;int lastdepth=-1;auto start=chrono::steady_clock::now();bool truncated=false;
    auto bridge=[&](uint32_t a,uint32_t b,int m){auto&sa=seeds[nodes[a].origin];auto&sb=seeds[nodes[b].origin];
        size_t length=sa.prefix.size()+nodes[a].depth+nodes[b].depth+sb.tail.size()+3;
        if(length>=(size_t)best)return;
        auto pa=trace(nodes,a),pb=trace(nodes,b);vector<int> p=sa.prefix;p.push_back(14);p.insert(p.end(),pa.begin(),pa.end());p.push_back(m);for(auto it=pb.rbegin();it!=pb.rend();++it)p.push_back(undo(*it));p.push_back(17);p.insert(p.end(),sb.tail.begin(),sb.tail.end());assert(p.size()==length);
        // Replay the entire rank-three bridge before emitting a candidate.
        State state=sa.s;vector<int> mid=pa;mid.push_back(m);for(auto it=pb.rbegin();it!=pb.rend();++it)mid.push_back(undo(*it));
        for(int move:mid){bool ok=false;for(auto&spec:moves)if(spec.id==move){State next;ok=step(state,spec,next,cap);state=next;break;}assert(ok);}assert(key(state)==key(sb.s));
        cerr<<"candidate "<<best<<" -> "<<length<<" at regions "<<nodes[a].origin<<','<<nodes[b].origin<<'\n';best=length;emit(id,p,out);
    };
    for(size_t q=0;q<nodes.size();q++){
        Node n=nodes[q];
        if(ball.cap>=0)for(int i=0;i<3;i++){St t;if(!remove_generator(n.s,i,t)||t.tot()>ball.cap)continue;int d=ball.get(t);if(d<0||seeds[n.origin].prefix.size()+n.depth+2+d>=(size_t)best)continue;
            vector<int> tail;if(!descend(ball,goals,t,tail))continue;auto p=seeds[n.origin].prefix;p.push_back(14);auto part=trace(nodes,q);p.insert(p.end(),part.begin(),part.end());p.push_back(15+i);p.insert(p.end(),tail.begin(),tail.end());cerr<<"ball after removing generator "<<i<<": "<<best<<" -> "<<p.size()<<'\n';best=p.size();emit(id,p,out);
        }
        if(n.depth>=radius)continue;expanded++;
        if(n.depth!=lastdepth){lastdepth=n.depth;cerr<<"depth "<<lastdepth<<" states "<<nodes.size()<<" seeds "<<seeds.size()<<'\n';}
        for(const auto&m:moves){if(n.parent!=UINT32_MAX&&undo(n.move)==m.id)continue;State t;if(!step(n.s,m,t,cap))continue;auto k=key(t);auto it=seen.find(k);
            if(it!=seen.end()){auto j=it->second;if(n.origin!=nodes[j].origin){collisions++;bridge(q,j,m.id);bridge(j,q,m.undo);}continue;}
            if(nodes.size()>=budget){truncated=true;continue;}
            seen.emplace(std::move(k),nodes.size());nodes.push_back({t,(uint32_t)q,n.origin,(uint16_t)m.id,(uint8_t)(n.depth+1)});
        }
    }
    cerr<<"finished states="<<nodes.size()<<" expanded="<<expanded<<" collisions="<<collisions<<" best="<<best<<" truncated="<<truncated<<" seconds="<<chrono::duration<double>(chrono::steady_clock::now()-start).count()<<'\n';
}
#endif
