#ifndef DH_CLIENT_SECURITY_COMBINATION_HPP_
#define DH_CLIENT_SECURITY_COMBINATION_HPP_

#include "simple_demo/security_logic/EncHook.hpp"
#include "simple_demo/security_logic/DHHelper.hpp"
#include "simple_demo/security_logic/SignatureAndEncBasedIdentityCheckerComponent.hpp"
#include "simple_demo/security_logic/EncAndSignHookFactory.hpp"

#include <boost/hana/functional/curry.hpp>

#include <array>
#include <unordered_map>
#include <mutex>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/security/SignatureBasedIdentityCheckerComponent.hpp>
#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>

using namespace dev::cd606::tm;

template <
    class R
    , class CommandToBeSecured
    , class ResponseToBeSecured
    , std::enable_if_t<
        std::is_base_of_v<
            basic::real_time_clock::ClockComponent
            ,typename R::EnvironmentType>            
        &&
        std::is_base_of_v<
            ClientSideSignatureAndEncBasedIdentityAttacherComponent<CommandToBeSecured>
            ,typename R::EnvironmentType>
        &&
        std::is_base_of_v<
            dev::cd606::tm::transport::security::ClientSideSignatureBasedIdentityAttacherComponent<DHHelperCommand>
            ,typename R::EnvironmentType>
        ,int
        > = 0
>
auto DHClientSideCombination(
    R &r
    , std::array<unsigned char, 32> serverPublicKey
    , transport::MultiTransportBroadcastListenerAddSubscription const &heartbeatChannelSpec
    , std::regex const &serverNameRE
    , std::string const &nameOfFacilityToBeSecuredOnServerSide
    , std::string const &heartbeatDecryptionKey
)
    -> typename R::template FacilitioidConnector<CommandToBeSecured,ResponseToBeSecured>
{
    using Env = typename R::EnvironmentType;

    Env *env = r.environment();

    auto dhClientHelper = std::make_shared<DHClientHelper>(
        boost::hana::curry<2>(
            std::mem_fn(&ClientSideSignatureAndEncBasedIdentityAttacherComponent<CommandToBeSecured>::set_encdec_keys)
        )(env)
    );
    auto dhClientHelperMutex = std::make_shared<std::mutex>();

    r.preservePointer(dhClientHelper);
    r.preservePointer(dhClientHelperMutex);

    auto heartbeatHook = VerifyAndDecHookFactoryComponent<transport::HeartbeatMessage>(heartbeatDecryptionKey, serverPublicKey).defaultHook();
  
    auto facilities =
        transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::template SetupRemoteFacilities<
            std::tuple<
                std::tuple<std::string, DHHelperCommand, DHHelperReply>
            >
            , std::tuple<
                std::tuple<std::string, CommandToBeSecured, ResponseToBeSecured>
            >
        >::run(
            r 
            , heartbeatChannelSpec
            , serverNameRE
            , {
                "dh_server_facility", nameOfFacilityToBeSecuredOnServerSide
            }
            , std::chrono::seconds(3)
            , std::chrono::seconds(5)
            , {
                [env,dhClientHelper,dhClientHelperMutex]() -> DHHelperCommand {
                    env->log(infra::LogLevel::Info, "Creating DH client command");
                    std::lock_guard<std::mutex> _(*dhClientHelperMutex);
                    dhClientHelper->reset(); //this forces a regeneration of key
                    return dhClientHelper->buildCommand();
                }
            }
            , {
                [dhClientHelper,dhClientHelperMutex](DHHelperCommand const &, DHHelperReply const &data) -> bool {
                    std::lock_guard<std::mutex> _(*dhClientHelperMutex);
                    dhClientHelper->process(data);
                    return true;
                }
            }
            , {
                "dhClient", "facility"
            }
            , "facilities"
            , heartbeatHook
        );

    return std::get<0>(std::get<1>(facilities));
}

#endif