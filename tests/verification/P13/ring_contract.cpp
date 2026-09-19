#include "../../../bench/capture.hpp"
#include <thread>
using namespace vita;using namespace vita::bench;
int main(){Ring<TraceEvent,8> ring;for(unsigned n=0;n<8;++n){TraceEvent event;event.key.operation=n;event.monotonic_ns=n^0xabcdef;if(!ring.push(event))return 1;}TraceEvent poison;poison.key.operation=999;if(ring.push(poison)||ring.overflow.load()!=1)return 2;for(unsigned n=0;n<8;++n){TraceEvent out;if(!ring.pop(out)||out.key.operation!=n||out.monotonic_ns!=(n^0xabcdef))return 3;}TraceEvent empty;if(ring.pop(empty))return 4;
 std::atomic<bool> failed=false;std::thread producer([&]{for(unsigned n=0;n<10000;++n){TraceEvent event;event.key.operation=n;event.monotonic_ns=n^0x123456;while(!ring.push(event))std::this_thread::yield();}});std::thread consumer([&]{for(unsigned n=0;n<10000;++n){TraceEvent out;while(!ring.pop(out))std::this_thread::yield();if(out.key.operation!=n||out.monotonic_ns!=(n^0x123456))failed=true;}});producer.join();consumer.join();if(failed||ring.pop(empty))return 5;
 // A rejected write must be visible, not counted as a captured row.
 Capture capture;capture.trace_file=std::fopen(__FILE__,"r");capture.peer_file=std::tmpfile();
 if(!capture.trace_file||!capture.peer_file)return 6;
 capture.trace.push(poison);if(!capture.drain()||!capture.io_error.load()||capture.written_trace.load()!=0)return 7;
 std::fclose(capture.trace_file);std::fclose(capture.peer_file);return 0;}
