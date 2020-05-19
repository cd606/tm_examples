#ifndef DH_SERVER_SECURITY_COMBINATION_HPP_
#define DH_SERVER_SECURITY_COMBINATION_HPP_

#include "simple_demo/security_logic/AESHook.hpp"
#include "simple_demo/security_logic/DHHelper.hpp"
#include "simple_demo/security_logic/SignatureBasedIdentityCheckerComponent.hpp"
#include "simple_demo/security_logic/SignatureAndAESBasedIdentityCheckerComponent.hpp"

#include <boost/hana/functional/curry.hpp>

#include <array>
#include <unordered_map>
#include <mutex>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

using namespace dev::cd606::tm;

template <
    class R
    , class CommandToBeSecured
    , class ImporterExporterTransport
    , class OnOrderFacilityTransport
    , std::enable_if_t<
        std::is_base_of_v<
            basic::real_time_clock::ClockComponent
            ,typename R::EnvironmentType>            
        &&
        std::is_base_of_v<
            transport::rabbitmq::RabbitMQComponent
            ,typename R::EnvironmentType>
        &&
        std::is_base_of_v<
            ServerSideSignatureAndAESBasedIdentityCheckerComponent<CommandToBeSecured>
            ,typename R::EnvironmentType>
        &&
        std::is_base_of_v<
            ServerSideSignatureBasedIdentityCheckerComponent<DHHelperCommand>
            ,typename R::EnvironmentType>
        ,int
        > = 0
>
void DHServerSideCombination(
    R &r
    , std::array<unsigned char, 64> privateKey
    , std::string const &dhQueueLocator
    , std::string const &dhRestartExchangeLocator
    , std::string const &dhRestartTopic
) {
    using Env = typename R::EnvironmentType;
    using M = typename R::MonadType;

    auto dh = std::make_shared<DHServerHelper>(
        boost::hana::curry<3>(
            std::mem_fn(&ServerSideSignatureAndAESBasedIdentityCheckerComponent<CommandToBeSecured>::set_aes_key_for_identity)
        )(r.environment())
    );
    r.preservePointer(dh);

    auto facility = M::template liftPureOnOrderFacility<std::tuple<std::string, DHHelperCommand>>(
        boost::hana::curry<2>(std::mem_fn(&DHServerHelper::process))(dh.get())
    );
    r.registerOnOrderFacility("dh_server_facility", facility);

    auto signer = std::make_shared<SignHelper>("", privateKey);
    r.preservePointer(signer);
    transport::WireToUserHook emptyHook = {
        [](basic::ByteData &&d) -> std::optional<basic::ByteData> {
            return std::move(d);
        }
    };
    transport::UserToWireHook signHook = {
        boost::hana::curry<2>(std::mem_fn(&SignHelper::sign))(signer.get())
    };

    OnOrderFacilityTransport::template WithIdentity<std::string>::template wrapOnOrderFacility
        <DHHelperCommand, DHHelperReply>(
          r 
        , facility
        , transport::ConnectionLocator::parse(dhQueueLocator)
        , "dh_server_wrapper_"
        , transport::ByteDataHookPair {
            signHook, emptyHook
        } //the outgoing data is signed, the incoming data does not need extra hook
    );

    auto oneShot = basic::real_time_clock::ClockImporter<Env>::template createOneShotClockConstImporter<basic::TypedDataWithTopic<DHHelperRestarted>>(
        r.environment()->now()+std::chrono::milliseconds(100)
        , basic::TypedDataWithTopic<DHHelperRestarted> {dhRestartTopic, DHHelperRestarted {}}
    );

    auto restartPublisher = ImporterExporterTransport::template createTypedExporter
        <DHHelperRestarted>(
        transport::ConnectionLocator::parse(dhRestartExchangeLocator)
        , signHook
    );
    r.registerExporter("restartPublisher", restartPublisher);
    r.exportItem(restartPublisher, r.importItem("dh_create_restart_message", oneShot));
}

#endif