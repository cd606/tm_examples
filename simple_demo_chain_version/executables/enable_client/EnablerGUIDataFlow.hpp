#ifndef ENABLER_GUI_DATA_FLOW_HPP_
#define ENABLER_GUI_DATA_FLOW_HPP_

#include "simple_demo_chain_version/enable_server_data/EnableServerTransactionData.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListener.hpp>

using namespace dev::cd606::tm;
using namespace simple_demo_chain_version::enable_server;

class CustomizedExitControlComponent {
private:
    std::function<void()> f_;
public:
    CustomizedExitControlComponent() : f_() {}
    CustomizedExitControlComponent(std::function<void()> const &f) : f_(f) {}
    CustomizedExitControlComponent &operator=(CustomizedExitControlComponent &&) = default;
    void exit() {
        f_();
    }
};

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    CustomizedExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
    transport::CrossGuidComponent,
    transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>,
    transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::Transaction>,
    transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<TheEnvironment>;
using R = infra::AppRunner<M>;

extern void enablerGUIDataFlow(
    R &r
    , std::string const &clientName
    , R::Sourceoid<bool> const &configureSource
    , R::Sinkoid<bool> const &statusSink
    , std::optional<R::Source<basic::VoidStruct>> const &exitSource
);

extern void enablerOneShotDataFlow(
    R &r
    , std::string const &clientName
    , bool enableCommand
);

#endif