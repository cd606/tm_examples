#include "EnablerGUIDataFlow.hpp"

#include <tm_kit/infra/TerminationController.hpp>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: oneshot_enable_client [on|off]\n";
        return 1;
    }
    std::string s = argv[1];
    if (s != "on" && s != "off") {
        std::cerr << "Usage: oneshot_enable_client [on|off]\n";
        return 1;
    }
    bool enable = (s == "on");

    TheEnvironment env;
    env.CustomizedExitControlComponent::operator=(
        CustomizedExitControlComponent(
            []() {
                ::exit(0);
            }
        )
    );
    R r(&env);

    enablerOneShotDataFlow(
        r
        , "oneshot_enabler"
        , enable
    );

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });
}