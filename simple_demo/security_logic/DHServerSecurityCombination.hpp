#ifndef DH_SERVER_SECURITY_COMBINATION_HPP_
#define DH_SERVER_SECURITY_COMBINATION_HPP_

#include "simple_demo/security_logic/EncHook.hpp"
#include "simple_demo/security_logic/DHHelper.hpp"
#include "simple_demo/security_logic/SignatureAndEncBasedIdentityCheckerComponent.hpp"

#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>

#include <boost/hana/functional/curry.hpp>

#include <array>
#include <unordered_map>
#include <mutex>

using namespace dev::cd606::tm;

template <
    class R
    , class CommandToBeSecured
    , class OnOrderFacilityTransport
    , std::enable_if_t<
        std::is_base_of_v<
            basic::real_time_clock::ClockComponent
            ,typename R::EnvironmentType>            
        &&
        std::is_base_of_v<
            ServerSideSignatureAndEncBasedIdentityCheckerComponent<CommandToBeSecured>
            ,typename R::EnvironmentType>
        &&
        std::is_base_of_v<
            dev::cd606::tm::transport::security::ServerSideSignatureBasedIdentityCheckerComponent<DHHelperCommand>
            ,typename R::EnvironmentType>
        ,int
        > = 0
>
void DHServerSideCombination(
    R &r
    , std::array<unsigned char, 64> privateKey
    , transport::ConnectionLocator const &dhQueueLocator
) {
    using M = typename R::AppType;

    auto dh = std::make_shared<DHServerHelper>(
        boost::hana::curry<3>(
            std::mem_fn(&ServerSideSignatureAndEncBasedIdentityCheckerComponent<CommandToBeSecured>::set_enc_key_for_identity)
        )(r.environment())
    );
    r.preservePointer(dh);

    auto facility = M::template liftPureOnOrderFacility<std::tuple<std::string, DHHelperCommand>>(
        boost::hana::curry<2>(std::mem_fn(&DHServerHelper::process))(dh.get())
    );
    r.registerOnOrderFacility("dh_server_facility", facility);

    auto signHook = transport::security::SignatureHookFactoryComponent<void> {
        privateKey
    }.defaultHook();
    //this hook is used to receive DH request
    //The reason why it is an empty hook is that the ServerSideIdentityChecker already
    //verifies the request, and currently no further hook is needed
    auto emptyHook = transport::EmptyIncomingBroadcastHookFactoryComponent<void>().defaultHook();

    OnOrderFacilityTransport::template wrapOnOrderFacility
        <DHHelperCommand, DHHelperReply>(
          r 
        , facility
        , dhQueueLocator
        , "dh_server_wrapper_"
        , transport::ByteDataHookPair {
            signHook, emptyHook
        } //the outgoing data is signed, the incoming data does not need extra hook
    );
}

#endif