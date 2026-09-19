#include "../../../examples/frequency_scan/endpoint_options.hpp"
#include <cassert>
using namespace vita::examples::frequency_scan;
int main(){
    auto c=parse_endpoint_options(EndpointRole::controller,{});
    auto d=parse_endpoint_options(EndpointRole::controllee,{});
    assert(c&&d&&c->local.data==41000&&c->peer.cancellation==42002);
    assert(d->local.data==42000&&d->peer.control==41001);
    assert(c->isolated_lab&&c->simulated_pps&&c->address=="127.0.0.1");
    const std::array<std::string_view,6> custom{"--local-base-port","65533","--peer-base-port","1","--step-hz","25000"};
    auto changed=parse_endpoint_options(EndpointRole::controller,custom);
    assert(changed&&changed->local.cancellation==65535&&changed->peer.control==2);
    for(auto value:{"0","65534","65535","65536","-1","+1","1x"," 1","18446744073709551616"}){
        const std::array<std::string_view,2> args{"--local-base-port",value};assert(!parse_endpoint_options(EndpointRole::controller,args));
    }
    const std::array<std::string_view,2> overlap{"--local-base-port","42001"},address{"--peer-address","192.0.2.1"},unknown{"--unknown","1"};
    assert(!parse_endpoint_options(EndpointRole::controller,overlap));
    assert(!parse_endpoint_options(EndpointRole::controller,address));
    assert(!parse_endpoint_options(EndpointRole::controller,unknown));
    const std::array<std::string_view,4> repeated{"--local-base-port","41000","--local-base-port","43000"};
    assert(!parse_endpoint_options(EndpointRole::controller,repeated));
    const std::array<std::string_view,1> missing{"--peer-base-port"};assert(!parse_endpoint_options(EndpointRole::controllee,missing));
    assert(!parse_endpoint_options(static_cast<EndpointRole>(99),{}));
}
