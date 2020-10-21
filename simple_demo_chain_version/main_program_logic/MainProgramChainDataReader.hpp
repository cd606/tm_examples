#ifndef MAIN_PROGRAM_CHAIN_DATA_READER_HPP_
#define MAIN_PROGRAM_CHAIN_DATA_READER_HPP_

#include "simple_demo_chain_version/chain_data/ChainData.hpp"

#include <tm_kit/infra/ChronoUtils.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainReader.hpp>

namespace simple_demo_chain_version { namespace main_program_logic {
    template <class Env, class Chain>
    class TrivialChainDataFolder : public basic::simple_shared_chain::FolderUsingPartialHistoryInformation {
    public:
        using ResultType = ChainData;
        static ResultType initialize(Env *env, Chain *chain) {
            return ChainData {};
        }
        static ResultType fold(ResultType const &state, typename Chain::ItemType const &item) {
            static_assert(std::is_same_v<typename Chain::DataType, ChainData>);
            ChainData const *p = Chain::extractData(item);
            if (!p) {
                return state;
            }
            return *p;
        }
        static std::chrono::system_clock::time_point extractTime(ResultType const &st) {
            return infra::withtime_utils::epochDurationToTime<std::chrono::milliseconds>(st.timestamp);
        }
    };

    template <class M, class Chain>
    using MainProgramChainDataReader = basic::simple_shared_chain::ChainReader<
        M, Chain, TrivialChainDataFolder<typename M::EnvironmentType, Chain>, void
    >;
} }

#endif