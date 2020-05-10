#ifndef DH_CLIENT_SECURITY_COMBINATION_HPP_
#define DH_CLIENT_SECURITY_COMBINATION_HPP_

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

#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQImporterExporter.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>

using namespace dev::cd606::tm;

template <
    class R
    , class CommandToBeSecured
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
            ClientSideSignatureAndAESBasedIdentityAttacherComponent<CommandToBeSecured>
            ,typename R::EnvironmentType>
        &&
        std::is_base_of_v<
            ClientSideSignatureBasedIdentityAttacherComponent<DHHelperCommand>
            ,typename R::EnvironmentType>
        ,int
        > = 0
>
void DHClientSideCombination(
    R &r
    , std::mutex &dhClientHelperMutex
    , std::unique_ptr<DHClientHelper> &dhClientHelper
    , std::array<unsigned char, 32> serverPublicKey
    , std::string const &dhQueueLocator
    , std::string const &dhRestartExchangeLocator
    , std::string const &dhRestartTopic
) {
    using Env = typename R::EnvironmentType;
    using M = typename R::MonadType;

    auto verifier = std::make_shared<VerifyHelper>();
    r.preservePointer(verifier);
    verifier->addKey("", serverPublicKey);
    transport::WireToUserHook verifyHook = {
        [verifier](basic::ByteData &&d) -> std::optional<basic::ByteData> {
            auto x = verifier->verify(std::move(d));
            if (x) {
                return std::move(std::get<1>(*x));
            } else {
                return std::nullopt;
            }
        }
    };
    transport::UserToWireHook emptyHook = {
        [](basic::ByteData &&d) -> basic::ByteData {
            return std::move(d);
        }
    };

    auto restartListener = transport::rabbitmq::template RabbitMQImporterExporter<Env>
                        ::template createTypedImporter<DHHelperRestarted>(
        transport::ConnectionLocator::parse(dhRestartExchangeLocator)
        , dhRestartTopic
        , verifyHook
    );
    r.registerImporter("restartListener", restartListener);

    Env *env = r.environment();
    auto oneShot = basic::real_time_clock::template ClockImporter<Env>
                    ::template createOneShotClockConstImporter<basic::TypedDataWithTopic<DHHelperRestarted>>(
        env->now()+std::chrono::milliseconds(100)
        , basic::TypedDataWithTopic<DHHelperRestarted> { dhRestartTopic, DHHelperRestarted {} }
    );
    r.registerImporter("oneShot", oneShot);

    auto createDHCommand = M::template liftPure<basic::TypedDataWithTopic<DHHelperRestarted>>(
        [env,&dhClientHelper,&dhClientHelperMutex](basic::TypedDataWithTopic<DHHelperRestarted> &&) {
            std::lock_guard<std::mutex> _(dhClientHelperMutex);
            dhClientHelper.reset(
                new DHClientHelper(
                    boost::hana::curry<2>(
                        std::mem_fn(&ClientSideSignatureAndAESBasedIdentityAttacherComponent<CommandToBeSecured>::set_aes_key)
                    )(env)
                )
            );
            return infra::withtime_utils::keyify<DHHelperCommand, Env>(dhClientHelper->buildCommand());
        }
    );
    auto dhHandler = M::template simpleExporter<typename M::template KeyedData<DHHelperCommand, DHHelperReply>>(
        [&dhClientHelper,&dhClientHelperMutex](typename M::template InnerData<typename M::template KeyedData<DHHelperCommand, DHHelperReply>> &&data) {
            std::lock_guard<std::mutex> _(dhClientHelperMutex);
            dhClientHelper->process(std::move(data.timedData.value.data));
        }
    );
    auto dhFacility = transport::rabbitmq::template RabbitMQOnOrderFacility<Env>
                    ::template WithIdentity<std::string>
                    ::template createTypedRPCOnOrderFacility<DHHelperCommand, DHHelperReply>(
        transport::ConnectionLocator::parse(dhQueueLocator)
        , transport::ByteDataHookPair {
            emptyHook, verifyHook
        }
    );
    r.registerAction("createDHCommand", createDHCommand);
    r.registerExporter("dhHandler", dhHandler);
    r.registerOnOrderFacility("dhFacility", dhFacility);

    r.placeOrderWithFacility(r.execute(createDHCommand, r.importItem(oneShot)), dhFacility, r.exporterAsSink(dhHandler));
    r.execute(createDHCommand, r.importItem(restartListener));
}

#endif