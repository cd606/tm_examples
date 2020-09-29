#ifndef ENABLER_GUI_DATA_FLOW_HPP_
#define ENABLER_GUI_DATA_FLOW_HPP_

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListener.hpp>

#include "defs.pb.h"

using namespace dev::cd606::tm;
using namespace simple_demo;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
    transport::BoostUUIDComponent,
    transport::ClientSideSimpleIdentityAttacherComponent<std::string,ConfigureCommand>,
    transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<TheEnvironment>;
using R = infra::AppRunner<M>;

extern void enablerGUIDataFlow(
    R &r
    , std::string const &clientName
    , R::Sourceoid<ConfigureCommand> configureSource
    , R::Sinkoid<bool> statusSink
);

#endif