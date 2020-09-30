#ifndef SIGNATURE_AND_ENC_BASED_IDENTITY_CHECKER_COMPONENT_HPP_
#define SIGNATURE_AND_ENC_BASED_IDENTITY_CHECKER_COMPONENT_HPP_

#include <tm_kit/transport/AbstractIdentityCheckerComponent.hpp>
#include "simple_demo/security_logic/SignatureHelper.hpp"
#include "simple_demo/security_logic/EncHook.hpp"

#include <unordered_map>
#include <mutex>

template <class Req>
class ClientSideSignatureAndEncBasedIdentityAttacherComponent 
    : public dev::cd606::tm::transport::ClientSideAbstractIdentityAttacherComponent<std::string, Req>
{
private:
    SignHelper signer_;
    EncHook enc_;
public:
    ClientSideSignatureAndEncBasedIdentityAttacherComponent() : signer_(), enc_() {}
    ClientSideSignatureAndEncBasedIdentityAttacherComponent(std::string const &name, std::array<unsigned char, 64> const &privateKey) : signer_(name, privateKey), enc_() {}
    ClientSideSignatureAndEncBasedIdentityAttacherComponent(ClientSideSignatureAndEncBasedIdentityAttacherComponent const &) = delete;
    ClientSideSignatureAndEncBasedIdentityAttacherComponent &operator=(ClientSideSignatureAndEncBasedIdentityAttacherComponent const &) = delete;
    ClientSideSignatureAndEncBasedIdentityAttacherComponent(ClientSideSignatureAndEncBasedIdentityAttacherComponent &&) = default;
    ClientSideSignatureAndEncBasedIdentityAttacherComponent &operator=(ClientSideSignatureAndEncBasedIdentityAttacherComponent &&) = default;
    ~ClientSideSignatureAndEncBasedIdentityAttacherComponent() = default;
    void set_enc_key(std::array<unsigned char, 32> const &encKey) {
        enc_.setKey(encKey);
    }
    virtual dev::cd606::tm::basic::ByteData attach_identity(dev::cd606::tm::basic::ByteData &&d) override final {
        return signer_.sign(enc_.encode(std::move(d)));
    }
};

template <class Req>
class ServerSideSignatureAndEncBasedIdentityCheckerComponent 
    : public dev::cd606::tm::transport::ServerSideAbstractIdentityCheckerComponent<std::string, Req>
{
private:
    VerifyHelper verifier_;
    std::unordered_map<std::string, std::unique_ptr<EncHook>> enc_;
    std::mutex mutex_;
public:
    ServerSideSignatureAndEncBasedIdentityCheckerComponent() : verifier_() {}
    ServerSideSignatureAndEncBasedIdentityCheckerComponent(ServerSideSignatureAndEncBasedIdentityCheckerComponent const &) = delete;
    ServerSideSignatureAndEncBasedIdentityCheckerComponent &operator=(ServerSideSignatureAndEncBasedIdentityCheckerComponent const &) = delete;
    ServerSideSignatureAndEncBasedIdentityCheckerComponent(ServerSideSignatureAndEncBasedIdentityCheckerComponent &&) = default;
    ServerSideSignatureAndEncBasedIdentityCheckerComponent &operator=(ServerSideSignatureAndEncBasedIdentityCheckerComponent &&) = default;
    ~ServerSideSignatureAndEncBasedIdentityCheckerComponent() = default;
    void add_identity_and_key(std::string const &name, std::array<unsigned char, 32> const &publicKey) {
        verifier_.addKey(name, publicKey);
    }
    void set_enc_key_for_identity(std::string const &name, std::array<unsigned char, 32> const &encKey) {
        std::lock_guard<std::mutex> _(mutex_);
        auto iter = enc_.find(name);
        if (iter == enc_.end()) {
            iter = enc_.insert({
                name
                , std::make_unique<EncHook>()
            }).first;
        }
        iter->second->setKey(encKey);
    }
    virtual std::optional<std::tuple<std::string,dev::cd606::tm::basic::ByteData>> check_identity(dev::cd606::tm::basic::ByteData &&d) override final {
        auto v = verifier_.verify(std::move(d));
        if (!v) {
            return std::nullopt;
        }
        auto const &identity = std::get<0>(*v);
        EncHook *encHook = nullptr;
        {
            std::lock_guard<std::mutex> _(mutex_);
            auto iter = enc_.find(identity);
            if (iter == enc_.end()) {
                return std::nullopt;
            }
            encHook = iter->second.get();
        }
        if (encHook) {
            auto d = encHook->decode(std::move(std::get<1>(*v)));
            if (!d) {
                return std::nullopt;
            }
            return std::tuple<std::string,dev::cd606::tm::basic::ByteData> {
                std::move(std::get<0>(*v))
                , std::move(*d)
            };
        } else {
            return std::nullopt;
        }
    }
};
#endif