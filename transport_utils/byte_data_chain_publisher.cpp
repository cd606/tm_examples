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
#include <tm_kit/transport/MultiTransportTouchups.hpp>

#include <iostream>
#include <fstream>

#include <tclap/CmdLine.h>

using namespace dev::cd606::tm;

class TrivialByteDataChainFolder : public basic::simple_shared_chain::TrivialChainDataFetchingFolder<basic::ByteData> {
private:
    std::chrono::system_clock::time_point tp_;
public:
    std::chrono::system_clock::time_point extractTime(std::optional<basic::ByteData> const &st) {
        if (st) {
            tp_ += std::chrono::microseconds(1);
        }
        return tp_;
    }
};

void run(std::string const &inputChainLocatorStr, std::string const &outputChannelStr, std::string const &topicStr) {
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

    transport::multi_transport_touchups::PublisherTouchup<R, basic::ByteData> _touchup(
        r, {outputChannelStr}
    );

    auto sharedChainCreator = std::make_shared<transport::SharedChainCreator<M>>();
    r.preservePointer(sharedChainCreator);
    auto chainDataSource = basic::simple_shared_chain::createChainDataSource<
        R, basic::ByteData
    >(
        r 
        , sharedChainCreator->template readerFactory<
            basic::ByteData
            , TrivialByteDataChainFolder
            , void //no trigger type
            , void //no result transformer
            , true //force separate data storage if possible
        >(
            &env
            , inputChainLocatorStr
        )
        , "input_chain"
    );

    infra::GenericComponentLiftAndRegistration _(r);
    auto addTopic = _("addTopic", [topicStr](basic::ByteData &&x) -> basic::ByteDataWithTopic {
        return {topicStr, std::move(x.content)};
    });
    r.execute(addTopic, std::move(chainDataSource));
    
    r.finalize();
    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });
}

int main(int argc, char **argv) {
    TCLAP::CmdLine cmd("Byte data chain publisher", ' ', "0.0.1");
    TCLAP::ValueArg<std::string> inputArg("i", "input", "a chain locator", true, "", "string");
    TCLAP::ValueArg<std::string> outputArg("o", "output", "a channel descriptor", true, "", "string");
    TCLAP::ValueArg<std::string> topicArg("t", "topic", "topic name", true, "", "string");

    cmd.add(inputArg);
    cmd.add(outputArg);    
    cmd.add(topicArg);    
    
    cmd.parse(argc, argv);

    auto inputChainLocatorStr = inputArg.getValue();
    if (inputChainLocatorStr == "") {
        std::cerr << "Please provide input chain locator\n";
        return 1;
    }
    auto outputChannelStr = outputArg.getValue();
    if (outputChannelStr == "") {
        std::cerr << "Please provide output channel\n";
        return 1;
    }
    auto topicStr = topicArg.getValue();
    if (topicStr == "") {
        std::cerr << "Please provide topic\n";
        return 1;
    }
   
    run(inputChainLocatorStr, outputChannelStr, topicStr);

    return 0;
}