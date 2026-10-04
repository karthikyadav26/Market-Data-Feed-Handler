#include "csv_output.hpp"
#include "decoder.hpp"
#include "local_order_book.hpp"
#include "recorder.hpp"
#include "sequence_validator.hpp"
#include "timestamp.hpp"

#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {
struct Args {
    std::string input;
    std::string output;
    std::string latency_output;
    std::string mode{"max"};
    double speed{1.0};
    std::uint64_t initial_seq{1};
};
void usage(const char* n){
    std::cout<<"Usage: "<<n<<" --input PATH [--output PATH] [--mode max|realtime] [--speed N] [--latency-output PATH]\n";
}
bool parse_u64(const char* s,std::uint64_t& v){char*e=nullptr;auto x=std::strtoull(s,&e,10);if(e==s||*e)return false;v=static_cast<std::uint64_t>(x);return true;}
bool args_parse(int argc,char**argv,Args&a){
    for(int i=1;i<argc;++i){std::string k=argv[i];auto next=[&](const char* f)->const char*{if(i+1>=argc){std::cerr<<f<<" needs a value\n";std::exit(2);}return argv[++i];};
        if(k=="--help"){usage(argv[0]);std::exit(0);} else if(k=="--input")a.input=next("--input"); else if(k=="--output")a.output=next("--output"); else if(k=="--latency-output")a.latency_output=next("--latency-output"); else if(k=="--mode")a.mode=next("--mode"); else if(k=="--speed"){a.speed=std::strtod(next("--speed"),nullptr);} else if(k=="--initial-seq"){if(!parse_u64(next("--initial-seq"),a.initial_seq))return false;} else return false;}
    return !a.input.empty() && (a.mode=="max"||a.mode=="realtime") && a.speed>0.0;
}
void print_metrics(std::uint64_t decoded,std::uint64_t dropped,const md::SequenceValidator&seq,std::uint64_t book_fail){
    std::cout<<"messages_decoded="<<decoded<<"\nmessages_dropped="<<dropped<<"\ngaps_detected="<<seq.counters().gaps_detected<<"\nduplicates_detected="<<seq.counters().duplicates_detected<<"\nout_of_order_messages="<<seq.counters().out_of_order_messages<<"\nbook_updates_failed="<<book_fail<<"\n";
}
}
int main(int argc,char**argv){
    Args a;if(!args_parse(argc,argv,a)){usage(argv[0]);return 2;}
    md::FeedCaptureReader reader;std::string err;if(!reader.open(a.input,err)){std::cerr<<err<<'\n';return 1;}
    md::LocalOrderBook book;md::SequenceValidator seq(a.initial_seq);md::SnapshotWriter writer;
    md::SnapshotWriter* w=nullptr;if(!a.output.empty()){if(!writer.open(a.output,1,err)){std::cerr<<err<<'\n';return 1;}w=&writer;}
    if(!a.latency_output.empty()){if(!writer.open_latency(a.latency_output,err)){std::cerr<<err<<'\n';return 1;}}
    std::uint64_t decoded=0,dropped=0,prior_receive=0;std::span<const std::uint8_t> raw;
    std::uint64_t recv_ts=0;
    while(reader.next(recv_ts,raw)){
        if(a.mode=="realtime" && prior_receive!=0 && recv_ts>prior_receive){
            const auto gap=recv_ts-prior_receive;
            const auto sleep_ns=static_cast<std::uint64_t>(static_cast<double>(gap)/a.speed);
            std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
        }
        prior_receive=recv_ts;
        md::MarketDataMessage msg{};md::DecodeError de=md::DecodeError::NONE;
        if(!md::Decoder::decode(raw,msg,de)){++dropped;continue;}
        ++decoded;auto se=seq.observe(msg.sequence_number);
        if(se==md::SequenceEvent::DUPLICATE||se==md::SequenceEvent::OUT_OF_ORDER){++dropped;continue;}
        const bool ok=book.apply(msg);if(!ok){++dropped;continue;}
        const auto processing=md::monotonic_now_ns();
        if(w)w->write(book,msg.source_timestamp_ns,msg.instrument_id);
        if(!a.latency_output.empty())writer.write_latency(msg.source_timestamp_ns,recv_ts,recv_ts,processing,processing);
    }
    writer.close();
    print_metrics(decoded,dropped,seq,0);
    return 0;
}
