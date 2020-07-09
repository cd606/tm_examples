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
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>

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
            ClientSideSignatureAndAESBasedIdentityAttacherComponent<CommandToBeSecured>
            ,typename R::EnvironmentType>
        &&
        std::is_base_of_v<
            ClientSideSignatureBasedIdentityAttacherComponent<DHHelperCommand>
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
            std::mem_fn(&ClientSideSignatureAndAESBasedIdentityAttacherComponent<CommandToBeSecured>::set_aes_key)
        )(env)
    );
    auto dhClientHelperMutex = std::make_shared<std::mutex>();

    r.preservePointer(dhClientHelper);
    r.preservePointer(dhClientHelperMutex);
    
    auto verifier = std::make_shared<VerifyHelper>();
    r.preservePointer(verifier);
    verifier->addKey("", serverPublicKey);
    transport::WireToUserHook verifyHook = {
        [verifier,env](basic::ByteData &&d) -> std::optional<basic::ByteData> {
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

    auto aes = std::make_shared<AESHook>();
    r.preservePointer(aes);
    aes->setKey(AESHook::keyFromString(heartbeatDecryptionKey));

    auto heartbeatHook = transport::composeWireToUserHook(
        verifyHook
        , transport::WireToUserHook {
            boost::hana::curry<2>(std::mem_fn(&AESHook::decode))(aes.get())
        }
    );
  
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
            , [emptyHook,verifyHook](std::string const &, transport::ConnectionLocator const &)
                -> std::optional<transport::ByteDataHookPair>
                {
                    return transport::ByteDataHookPair {
                        emptyHook, verifyHook
                    };
                }
        );

    return std::get<0>(std::get<1>(facilities));
}

#endif