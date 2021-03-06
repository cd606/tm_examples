#ifndef MAIN_PROGRAM_CHAIN_DATA_READER_HPP_
#define MAIN_PROGRAM_CHAIN_DATA_READER_HPP_

#include "simple_demo_chain_version/chain_data/ChainData.hpp"

#include <tm_kit/infra/ChronoUtils.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainReader.hpp>

namespace simple_demo_chain_version { namespace main_program_logic {
    class TrivialChainDataFolder : public basic::simple_shared_chain::TrivialChainDataFetchingFolder<ChainData> {
    public:
        static std::chrono::system_clock::time_point extractTime(std::optional<ChainData> const &st) {
            if (st) {
                return infra::withtime_utils::epochDurationToTime<std::chrono::milliseconds>((*st).timestamp);
            } else {
                return infra::withtime_utils::epochDurationToTime<std::chrono::milliseconds>(0);
            }
        }
    };
} }

#endif