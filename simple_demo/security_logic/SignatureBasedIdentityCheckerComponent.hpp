#ifndef SIGNATURE_BASED_IDENTITY_CHECKER_COMPONENT_HPP_
#define SIGNATURE_BASED_IDENTITY_CHECKER_COMPONENT_HPP_

#include <tm_kit/transport/AbstractIdentityCheckerComponent.hpp>
#include "simple_demo/security_logic/SignatureHelper.hpp"

template <class Req>
class ClientSideSignatureBasedIdentityAttacherComponent 
    : public dev::cd606::tm::transport::ClientSideAbstractIdentityAttacherComponent<std::string, Req>
{
private:
    SignHelper signer_;
public:
    ClientSideSignatureBasedIdentityAttacherComponent() : signer_() {}
    ClientSideSignatureBasedIdentityAttacherComponent(std::string const &name, std::array<unsigned char, 64> const &privateKey) : signer_(name, privateKey) {}
    ClientSideSignatureBasedIdentityAttacherComponent(ClientSideSignatureBasedIdentityAttacherComponent const &) = delete;
    ClientSideSignatureBasedIdentityAttacherComponent &operator=(ClientSideSignatureBasedIdentityAttacherComponent const &) = delete;
    ClientSideSignatureBasedIdentityAttacherComponent(ClientSideSignatureBasedIdentityAttacherComponent &&) = default;
    ClientSideSignatureBasedIdentityAttacherComponent &operator=(ClientSideSignatureBasedIdentityAttacherComponent &&) = default;
    ~ClientSideSignatureBasedIdentityAttacherComponent() = default;
    virtual dev::cd606::tm::basic::ByteData attach_identity(dev::cd606::tm::basic::ByteData &&d) override final {
        return signer_.sign(std::move(d));
    }
};

template <class Req>
class ServerSideSignatureBasedIdentityCheckerComponent 
    : public dev::cd606::tm::transport::ServerSideAbstractIdentityCheckerComponent<std::string, Req>
{
private:
    VerifyHelper verifier_;
public:
    ServerSideSignatureBasedIdentityCheckerComponent() : verifier_() {}
    ServerSideSignatureBasedIdentityCheckerComponent(ServerSideSignatureBasedIdentityCheckerComponent const &) = delete;
    ServerSideSignatureBasedIdentityCheckerComponent &operator=(ServerSideSignatureBasedIdentityCheckerComponent const &) = delete;
    ServerSideSignatureBasedIdentityCheckerComponent(ServerSideSignatureBasedIdentityCheckerComponent &&) = default;
    ServerSideSignatureBasedIdentityCheckerComponent &operator=(ServerSideSignatureBasedIdentityCheckerComponent &&) = default;
    ~ServerSideSignatureBasedIdentityCheckerComponent() = default;
    void add_identity_and_key(std::string const &name, std::array<unsigned char, 32> const &publicKey) {
        verifier_.addKey(name, publicKey);
    }
    virtual std::optional<std::tuple<std::string,dev::cd606::tm::basic::ByteData>> check_identity(dev::cd606::tm::basic::ByteData &&d) override final {
        return verifier_.verify(std::move(d));
    }
};
#endif