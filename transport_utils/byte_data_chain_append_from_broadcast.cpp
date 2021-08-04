#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainDataImporterExporter.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SharedChainCreator.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>

#include <iostream>
#include <fstream>

#include <tclap/CmdLine.h>

using namespace dev::cd606::tm;

void run(std::string const &inputChannelStr, std::string const &outputChainLocatorStr, std::string const &topicStr) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >,
        transport::CrossGuidComponent,
        transport::AllNetworkTransportComponents,
        transport::AllChainComponents
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    auto sharedChainCreator = std::make_shared<transport::SharedChainCreator<M>>();
    r.preservePointer(sharedChainCreator);

    auto importerSource = transport::MultiTransportBroadcastListenerManagingUtils<R>::
        oneByteDataBroadcastListener(
            r, "importer", inputChannelStr, topicStr
        );
    infra::GenericComponentLiftAndRegistration _(r);
    auto removeTopic = _("removeTopic", [topicStr](basic::ByteDataWithTopic &&x) -> basic::ByteData {
        return {std::move(x.content)};
    });
    auto sink = basic::simple_shared_chain::createChainDataSink<
        R, basic::ByteData 
    >(
        r 
        , sharedChainCreator->template writerFactory<
            basic::ByteData
            , basic::simple_shared_chain::EmptyStateChainFolder
            , basic::simple_shared_chain::SimplyPlaceOnChainInputHandler<basic::ByteData>
            , void //no idle logic
            , true //force separate data storage if possible
        >(&env, outputChainLocatorStr)
        , "output_chain"
    );

    r.connect(r.execute(removeTopic, std::move(importerSource)), sink);
    
    r.finalize();
    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });
}

int main(int argc, char **argv) {
    TCLAP::CmdLine cmd("Byte data chain publisher", ' ', "0.0.1");
    TCLAP::ValueArg<std::string> inputArg("i", "input", "a channel descriptor", true, "", "string");
    TCLAP::ValueArg<std::string> outputArg("o", "output", "a chain locator", true, "", "string");
    TCLAP::ValueArg<std::string> topicArg("t", "topic", "topic name", true, "", "string");

    cmd.add(inputArg);
    cmd.add(outputArg);    
    cmd.add(topicArg);    
    
    cmd.parse(argc, argv);

    auto inputChannelStr = inputArg.getValue();
    if (inputChannelStr == "") {
        std::cerr << "Please provide input channel\n";
        return 1;
    }
    auto outputChainLocatorStr = outputArg.getValue();
    if (outputChainLocatorStr == "") {
        std::cerr << "Please provide output chain locator\n";
        return 1;
    }
    auto topicStr = topicArg.getValue();
    if (topicStr == "") {
        std::cerr << "Please provide topic\n";
        return 1;
    }
   
    run(inputChannelStr, outputChainLocatorStr, topicStr);

    return 0;
}