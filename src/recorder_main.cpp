#include "csv_feed.hpp"
#include "encoder.hpp"
#include "recorder.hpp"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

namespace {
struct Args { std::string input; std::string output; std::uint64_t receive_offset_ns{0}; };
void usage(const char* n) { std::cout << "Usage: " << n << " --input data/feed.csv --output captures/session.bin [--receive-offset-ns N]\n"; }
bool u64(const char* s, std::uint64_t& out) { char* e=nullptr; errno=0; const auto v=std::strtoull(s,&e,10); if(errno||e==s||*e)return false;out=v;return true; }
bool parse(int argc,char**argv,Args&a){for(int i=1;i<argc;++i){std::string k=argv[i];auto next=[&](const char*f)->const char*{if(i+1>=argc){std::cerr<<f<<" needs a value\n";std::exit(2);}return argv[++i];};if(k=="--help"){usage(argv[0]);std::exit(0);}else if(k=="--input")a.input=next("--input");else if(k=="--output")a.output=next("--output");else if(k=="--receive-offset-ns"){if(!u64(next("--receive-offset-ns"),a.receive_offset_ns))return false;}else return false;}return !a.input.empty()&&!a.output.empty();}
}
int main(int argc,char**argv){Args a;if(!parse(argc,argv,a)){usage(argv[0]);return 2;}md::CsvFeedReader csv;std::string err;if(!csv.open(a.input,err)){std::cerr<<err<<'\n';return 1;}md::FeedRecorder recorder;if(!recorder.open(a.output,err)){std::cerr<<err<<'\n';return 1;}md::MarketDataMessage msg{};std::uint64_t count=0;while(true){if(!csv.next(msg,err)){if(!err.empty()){std::cerr<<err<<'\n';return 1;}break;}md::WireBuffer raw{};std::size_t written=0;if(!md::Encoder::encode(msg,raw,written)){std::cerr<<"failed to encode sequence "<<msg.sequence_number<<'\n';return 1;}if(!recorder.record(msg.source_timestamp_ns+a.receive_offset_ns,std::span<const std::uint8_t>(raw.data(),written))){std::cerr<<"failed writing capture\n";return 1;}++count;}recorder.close();std::cout<<"recorded_messages="<<count<<"\n";return 0;}
