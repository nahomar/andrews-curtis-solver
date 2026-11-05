// Bounded corpus tail graph. Dropping states only removes paths; it never
// creates free edges across a dropped segment or a generator symmetry.
static orbit::Graph bounded_tail_graph(const vector<Prob>& problems,
        const vector<Prob>& targets, const string& paths, int cap, size_t limit, int tail_window=0) {
    orbit::Graph graph(true);
    map<string,St> starts;
    set<string> priority;
    for(const auto& p:problems)starts[p.id]=p.s;
    for(const auto& p:targets)priority.insert("ac-"+p.id.substr(p.id.find('-')+1));
    const uint32_t absent=UINT32_MAX;
    auto intern=[&](const St& state)->pair<uint32_t,int> {
        if(state.tot()>cap)return {absent,0};
        auto [key,orientation]=orbit::canonical(state,true);
        auto found=graph.ids.find(key);
        if(found!=graph.ids.end())return {found->second,orientation};
        if(graph.keys.size()>=limit)return {absent,0};
        return graph.intern(state);
    };
    intern(make({1},{2}));
    auto families=read_all_paths({paths});
    size_t accepted=0,unsupported=0,omitted=0,trimmed=0;
    string primary=targets.empty()?"":"ac-"+targets.front().id.substr(targets.front().id.find('-')+1);
    // Window mode puts the explicitly first target first, then other selected
    // targets, then the remainder. Default mode preserves the prior ordering.
    for(int phase=0;phase<(tail_window>0?3:2);phase++)for(const auto& [id, routes]:families) {
        string aid="ac-"+id.substr(id.find('-')+1);
        int group=tail_window>0?(aid==primary?0:(priority.count(aid)?1:2)):(priority.count(aid)?0:1);
        if(group!=phase)continue;
        auto start=starts.find(aid);if(start==starts.end())continue;
        for(const auto& route:routes) {
            vector<St> states;
            if(!verify(start->second,route,id.rfind("sac-",0)==0,&states)){unsupported++;continue;}
            accepted++;
            size_t first=tail_window>0&&states.size()>size_t(tail_window)+1?states.size()-1-tail_window:0;
            trimmed+=first;
            auto previous=intern(states[first]);
            for(size_t j=first+1;j<states.size();j++) {
                auto current=intern(states[j]);
                if(previous.first!=absent&&current.first!=absent)
                    graph.edge(previous,current,route[j-1]);
                else omitted++;
                previous=current;
            }
        }
    }
    // No intermediate-state growth: join(0) adds only actual one-move edges
    // between states already retained by the bounded loader.
    graph.join(0);graph.dedup();
    assert(graph.keys.size()<=limit);
    for(const auto& key:graph.keys)assert(orbit::decode(key).tot()<=cap);
    fprintf(stderr,"bounded tails: %zu accepted, %zu unsupported, %zu omitted edges, cap %d, limit %zu\n",
            accepted,unsupported,omitted,cap,limit);
    if(tail_window>0)fprintf(stderr,"tail window: %d edges per route, %zu prefix edges trimmed; primary %s first\n",tail_window,trimmed,primary.c_str());
    return graph;
}
