#ifndef SIGNATURE_AND_AES_BASED_IDENTITY_CHECKER_COMPONENT_HPP_
#define SIGNATURE_AND_AES_BASED_IDENTITY_CHECKER_COMPONENT_HPP_

#include <tm_kit/transport/AbstractIdentityCheckerComponent.hpp>
#include "simple_demo/security_logic/SignatureHelper.hpp"
#include "simple_demo/security_logic/AESHook.hpp"

#include <unordered_map>
#include <mutex>

template <class Req>
class ClientSideSignatureAndAESBasedIdentityAttacherComponent 
    : public dev::cd606::tm::transport::ClientSideAbstractIdentityAttacherComponent<std::string, Req>
{
private:
    SignHelper signer_;
    AESHook aes_;
public:
    ClientSideSignatureAndAESBasedIdentityAttacherComponent() : signer_(), aes_() {}
    ClientSideSignatureAndAESBasedIdentityAttacherComponent(std::string const &name, std::array<unsigned char, 64> const &privateKey) : signer_(name, privateKey), aes_() {}
    ClientSideSignatureAndAESBasedIdentityAttacherComponent(ClientSideSignatureAndAESBasedIdentityAttacherComponent const &) = delete;
    ClientSideSignatureAndAESBasedIdentityAttacherComponent &operator=(ClientSideSignatureAndAESBasedIdentityAttacherComponent const &) = delete;
    ClientSideSignatureAndAESBasedIdentityAttacherComponent(ClientSideSignatureAndAESBasedIdentityAttacherComponent &&) = default;
    ClientSideSignatureAndAESBasedIdentityAttacherComponent &operator=(ClientSideSignatureAndAESBasedIdentityAttacherComponent &&) = default;
    ~ClientSideSignatureAndAESBasedIdentityAttacherComponent() = default;
    void set_aes_key(std::array<unsigned char, 16> const &aesKey) {
        aes_.setKey(aesKey);
    }
    virtual dev::cd606::tm::basic::ByteData attach_identity(dev::cd606::tm::basic::ByteData &&d) override final {
        return signer_.sign(aes_.encode(std::move(d)));
    }
};

template <class Req>
class ServerSideSignatureAndAESBasedIdentityCheckerComponent 
    : public dev::cd606::tm::transport::ServerSideAbstractIdentityCheckerComponent<std::string, Req>
{
private:
    VerifyHelper verifier_;
    std::unordered_map<std::string, std::unique_ptr<AESHook>> aes_;
    std::mutex mutex_;
public:
    ServerSideSignatureAndAESBasedIdentityCheckerComponent() : verifier_() {}
    ServerSideSignatureAndAESBasedIdentityCheckerComponent(ServerSideSignatureAndAESBasedIdentityCheckerComponent const &) = delete;
    ServerSideSignatureAndAESBasedIdentityCheckerComponent &operator=(ServerSideSignatureAndAESBasedIdentityCheckerComponent const &) = delete;
    ServerSideSignatureAndAESBasedIdentityCheckerComponent(ServerSideSignatureAndAESBasedIdentityCheckerComponent &&) = default;
    ServerSideSignatureAndAESBasedIdentityCheckerComponent &operator=(ServerSideSignatureAndAESBasedIdentityCheckerComponent &&) = default;
    ~ServerSideSignatureAndAESBasedIdentityCheckerComponent() = default;
    void add_identity_and_key(std::string const &name, std::array<unsigned char, 32> const &publicKey) {
        verifier_.addKey(name, publicKey);
    }
    void set_aes_key_for_identity(std::string const &name, std::array<unsigned char, 16> const &aesKey) {
        std::lock_guard<std::mutex> _(mutex_);
        auto iter = aes_.find(name);
        if (iter == aes_.end()) {
            iter = aes_.insert({
                name
                , std::make_unique<AESHook>()
            }).first;
        }
        iter->second->setKey(aesKey);
    }
    virtual std::optional<std::tuple<std::string,dev::cd606::tm::basic::ByteData>> check_identity(dev::cd606::tm::basic::ByteData &&d) override final {
        auto v = verifier_.verify(std::move(d));
        if (!v) {
            return std::nullopt;
        }
        auto const &identity = std::get<0>(*v);
        AESHook *AESHook = nullptr;
        {
            std::lock_guard<std::mutex> _(mutex_);
            auto iter = aes_.find(identity);
            if (iter == aes_.end()) {
                return std::nullopt;
            }
            AESHook = iter->second.get();
        }
        if (AESHook) {
            auto d = AESHook->decode(std::move(std::get<1>(*v)));
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