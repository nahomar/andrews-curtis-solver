// Local graph cache for an unchanged corpus and graph-construction settings.
// Increment the version if graph construction/transport semantics change.
#include <stdexcept>
#include <unistd.h>
namespace graphcache {
constexpr uint64_t OFFSET=14695981039346656037ULL;
static void hash_bytes(uint64_t& h,const char* data,size_t n) {
    for(size_t i=0;i<n;++i){h^=static_cast<unsigned char>(data[i]);h*=1099511628211ULL;}
}
static uint64_t fingerprint(const string& path) {
    ifstream f(path,ios::binary);if(!f)throw runtime_error("cannot fingerprint graph input");
    uint64_t h=OFFSET;char buffer[65536];
    while(f){f.read(buffer,sizeof buffer);hash_bytes(h,buffer,size_t(f.gcount()));}
    if(!f.eof())throw runtime_error("graph input read failed");return h;
}
struct Identity {uint64_t problems,paths;uint32_t cap=64,rounds=2;};
struct Writer {
    ofstream stream;uint64_t checksum=OFFSET;
    explicit Writer(const string& path):stream(path,ios::binary){}
    void bytes(const char* p,size_t n){stream.write(p,n);hash_bytes(checksum,p,n);}
    template<class T> void value(T v){bytes(reinterpret_cast<const char*>(&v),sizeof v);}
};
struct Reader {
    ifstream stream;uint64_t checksum=OFFSET;
    explicit Reader(const string& path):stream(path,ios::binary){}
    bool bytes(char* p,size_t n){if(!stream.read(p,n))return false;hash_bytes(checksum,p,n);return true;}
    template<class T> bool value(T& v){return bytes(reinterpret_cast<char*>(&v),sizeof v);}
};
static bool save(const string& path,const orbit::Graph& graph,const Identity& identity) {
    string temporary=path+".tmp."+to_string(getpid());Writer w(temporary);
    if(!w.stream)return false;
    w.bytes("ACORBG01",8);w.value(uint32_t(0x01020304));
    w.value(identity.problems);w.value(identity.paths);w.value(identity.cap);w.value(identity.rounds);
    w.value(uint8_t(graph.symmetries));w.value(uint64_t(graph.keys.size()));
    for(size_t i=0;i<graph.keys.size();++i){
        const auto& key=graph.keys[i];w.value(uint16_t(key.size()));w.bytes(key.data(),key.size());
        w.value(uint32_t(graph.adj[i].size()));
        for(const auto& edge:graph.adj[i]){w.value(edge.to);w.value(edge.move);w.value(edge.transport);}
    }
    w.stream.write(reinterpret_cast<const char*>(&w.checksum),sizeof w.checksum);w.stream.close();
    if(!w.stream||rename(temporary.c_str(),path.c_str())){remove(temporary.c_str());return false;}
    return true;
}
static bool load(const string& path,const Identity& expected,orbit::Graph& result) {
    Reader r(path);if(!r.stream)return false;
    char magic[8];uint32_t endian=0;Identity actual{};uint8_t sym=0;uint64_t size=0;
    if(!r.bytes(magic,8)||memcmp(magic,"ACORBG01",8)||!r.value(endian)||endian!=0x01020304||
       !r.value(actual.problems)||!r.value(actual.paths)||!r.value(actual.cap)||!r.value(actual.rounds)||
       !r.value(sym)||!r.value(size))return false;
    if(actual.problems!=expected.problems||actual.paths!=expected.paths||actual.cap!=expected.cap||
       actual.rounds!=expected.rounds||sym!=1||!size||size>10000000)return false;
    orbit::Graph graph(true);graph.keys.reserve(size);graph.adj.reserve(size);graph.ids.reserve(size);
    for(uint32_t i=0;i<size;++i){
        uint16_t length=0;uint32_t degree=0;
        if(!r.value(length)||length<4||length>MAXT+2)return false;
        string key(length,'\0');if(!r.bytes(key.data(),length))return false;
        unsigned a=static_cast<unsigned char>(key[0]),b=static_cast<unsigned char>(key[1]);
        if(!a||!b||a+b!=length-2)return false;
        for(size_t j=2;j<key.size();++j)if(static_cast<unsigned char>(key[j])>=4)return false;
        if(!graph.ids.emplace(key,i).second||!r.value(degree)||degree>10000)return false;
        graph.keys.push_back(std::move(key));graph.adj.emplace_back();graph.adj.back().reserve(degree);
        for(uint32_t j=0;j<degree;++j){orbit::Edge edge{};
            if(!r.value(edge.to)||!r.value(edge.move)||!r.value(edge.transport)||edge.to>=size||edge.move>=14||edge.transport>=16)return false;
            graph.adj.back().push_back(edge);
        }
    }
    uint64_t checksum=0;
    if(!r.stream.read(reinterpret_cast<char*>(&checksum),sizeof checksum)||checksum!=r.checksum||r.stream.peek()!=EOF)return false;
    result=std::move(graph);return true;
}
static void self_test(const orbit::Graph& graph) {
    string path="/tmp/ac-graph-cache-test-"+to_string(getpid())+"-"+
        to_string(chrono::steady_clock::now().time_since_epoch().count());
    Identity identity{101,202,64,2};orbit::Graph restored(true);
    assert(save(path,graph,identity));assert(load(path,identity,restored));
    assert(graph.keys==restored.keys&&graph.ids==restored.ids&&graph.adj.size()==restored.adj.size());
    for(size_t i=0;i<graph.adj.size();++i){assert(graph.adj[i].size()==restored.adj[i].size());
        for(size_t j=0;j<graph.adj[i].size();++j){const auto&a=graph.adj[i][j];const auto&b=restored.adj[i][j];
            assert(a.to==b.to&&a.move==b.move&&a.transport==b.transport);}}
    Identity changed=identity;++changed.paths;assert(!load(path,changed,restored));
    changed=identity;++changed.cap;assert(!load(path,changed,restored));
    // A payload bit flip must fail even when the modified byte is in range.
    {fstream f(path,ios::in|ios::out|ios::binary);f.seekg(-9,ios::end);char value;f.read(&value,1);value^=1;f.seekp(-9,ios::end);f.write(&value,1);}
    assert(!load(path,identity,restored));assert(restored.keys==graph.keys);
    assert(save(path,graph,identity));
    {ofstream f(path,ios::binary|ios::app);f.put('x');}
    assert(!load(path,identity,restored));remove(path.c_str());
    fprintf(stderr,"graph-cache roundtrip, changed-input/settings, corruption and trailing-data checks PASS\n");
}
}
