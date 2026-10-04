#include "decoder.hpp"
#include "encoder.hpp"
#include "local_order_book.hpp"
#include "timestamp.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
struct Args { std::uint64_t messages=1000000; std::string output="results/feed_benchmark.csv"; };
void usage(const char*n){std::cout<<"Usage: "<<n<<" [--messages N] [--output PATH]\n";}
bool u64(const char*s,std::uint64_t&v){char*e=nullptr;auto x=std::strtoull(s,&e,10);if(e==s||*e)return false;v=x;return true;}
bool parse(int argc,char**argv,Args&a){for(int i=1;i<argc;++i){std::string k=argv[i];auto next=[&](){return argv[++i];};if(k=="--help"){usage(argv[0]);std::exit(0);}if(k=="--messages"){if(i+1>=argc||!u64(next(),a.messages))return false;}else if(k=="--output"){if(i+1>=argc)return false;a.output=next();}else return false;}return a.messages>0;}
std::uint64_t percentile(std::vector<std::uint64_t>& v, double p){if(v.empty())return 0;auto idx=static_cast<std::size_t>(p*static_cast<double>(v.size()-1));std::nth_element(v.begin(),v.begin()+idx,v.end());return v[idx];}
}
int main(int argc,char**argv){
    Args a;if(!parse(argc,argv,a)){usage(argv[0]);return 2;}
    std::vector<std::uint64_t> decode_ns;std::vector<std::uint64_t> e2e_ns;decode_ns.reserve(static_cast<std::size_t>(a.messages));e2e_ns.reserve(static_cast<std::size_t>(a.messages));
    md::LocalOrderBook book;
    md::WireBuffer wire{};
    const auto start_all=md::monotonic_now_ns();std::uint64_t dropped=0,gaps=0,decode_errors=0;
    std::uint64_t expected=1;
    for(std::uint64_t i=0;i<a.messages;++i){
        md::MarketDataMessage msg{};const auto cycle=i/5;const auto slot=static_cast<unsigned>(i%5);msg.protocol_version=md::kProtocolVersion;msg.sequence_number=i+1;msg.source_timestamp_ns=1'000'000'000ULL+i*1000;msg.instrument_id=static_cast<std::uint32_t>(cycle%4+1);msg.order_id=1'000'000+cycle*2+(slot==1||slot==4?2:1);msg.side=(slot==1||slot==4)?md::Side::SELL:md::Side::BUY;msg.type=(slot==0||slot==1)?md::MessageType::ADD_ORDER:(slot==2?md::MessageType::REPLACE_ORDER:(slot==3?md::MessageType::EXECUTE_ORDER:md::MessageType::CANCEL_ORDER));msg.price_ticks=(slot==1||slot==4)?10300:10100+static_cast<std::int64_t>(cycle%100);msg.quantity=(slot==2?120:(slot==3?20:(slot==4?50:(slot==1?50:100))));
        std::size_t written=0;if(!md::Encoder::encode(msg,wire,written)){++dropped;continue;}
        const auto recv=md::monotonic_now_ns();const auto d0=md::monotonic_now_ns();md::MarketDataMessage decoded{};md::DecodeError err=md::DecodeError::NONE;
        if(!md::Decoder::decode(wire,decoded,err)){++decode_errors;++dropped;continue;}const auto d1=md::monotonic_now_ns();decode_ns.push_back(d1-d0);if(decoded.sequence_number!=expected){++gaps;expected=decoded.sequence_number+1;}else ++expected;
        const bool ok=book.apply(decoded);const auto done=md::monotonic_now_ns();if(!ok){++dropped;continue;}e2e_ns.push_back(done-recv);
    }
    const auto total=md::monotonic_now_ns()-start_all;const double mps=static_cast<double>(a.messages)*1e9/static_cast<double>(total);
    std::ofstream out(a.output,std::ios::app);if(!out){std::cerr<<"cannot open "<<a.output<<'\n';return 1;}
    out<<"in-memory,"<<a.messages<<','<<total<<','<<std::fixed<<std::setprecision(3)<<mps<<','
       <<percentile(e2e_ns,0.50)<<','<<percentile(e2e_ns,0.95)<<','<<percentile(e2e_ns,0.99)<<','<<percentile(e2e_ns,0.999)<<'\n';
    std::cout<<"transport=in-memory\nmessages="<<a.messages<<"\ntotal_time_ns="<<total<<"\nmessages_per_sec="<<mps<<"\ndecode_p50_ns="<<percentile(decode_ns,0.50)<<"\ndecode_p99_ns="<<percentile(decode_ns,0.99)<<"\ne2e_p50_ns="<<percentile(e2e_ns,0.50)<<"\ne2e_p95_ns="<<percentile(e2e_ns,0.95)<<"\ne2e_p99_ns="<<percentile(e2e_ns,0.99)<<"\ne2e_p99.9_ns="<<percentile(e2e_ns,0.999)<<"\ndropped_messages="<<dropped<<"\nsequence_gaps="<<gaps<<"\ndecode_errors="<<decode_errors<<"\n";
    return 0;
}
