#ifndef PROGRESS_REPORTER_HPP_
#define PROGRESS_REPORTER_HPP_

#include "simple_demo_chain_version/chain_data/ChainData.hpp"
#include "simple_demo_chain_version/main_program_logic/MainProgramFacilityInputHandler.hpp"

namespace simple_demo_chain_version { namespace main_program_logic {
    class ProgressReporter {
    public:
        static std::vector<std::string> reportProgress(
            std::tuple<std::string, std::optional<ChainData>> &&latestChainData
        );
    };
} }

#endif