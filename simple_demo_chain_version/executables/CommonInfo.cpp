#include "CommonInfo.hpp"

#include <tm_kit/infra/ChronoUtils.hpp>
#include <string>
#include <sstream>

namespace simple_demo_chain_version {
    std::string theChainLocator() {
        std::string today = dev::cd606::tm::infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
        std::ostringstream chainLocatorOss;
        //chainLocatorOss << "in_shared_memory://::::" << today << "-simple-demo-chain[size=" << (100*1024*1024) << ",useNotification=true]";
        chainLocatorOss << "in_shared_memory://::::" << today << "-simple-demo-chain[size=" << (100*1024*1024) << "]";
        return chainLocatorOss.str();
    }
}