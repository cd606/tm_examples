#ifndef SIGNATURE_AND_ENC_BASED_IDENTITY_CHECKER_COMPONENT_HPP_
#define SIGNATURE_AND_ENC_BASED_IDENTITY_CHECKER_COMPONENT_HPP_

#include <tm_kit/transport/AbstractIdentityCheckerComponent.hpp>
#include <tm_kit/transport/security/SignatureHelper.hpp>
#include "simple_demo/security_logic/EncHelper.hpp"

#include <unordered_map>
#include <mutex>

struct FacilityKeyPair {
    std::array<unsigned char, 32> outgoingKey;
    std::array<unsigned char, 32> incomingKey;
};
struct FacilityKeyPairForIdentity {
    std::string identity;
    std::array<unsigned char, 32> outgoingKey;
    std::array<unsigned char, 32> incomingKey;
};

class ClientSideSignatureAndEncBasedIdentityAttacherComponentBase
{
public:
    virtual ~ClientSideSignatureAndEncBasedIdentityAttacherComponentBase() {}
    virtual void set_encdec_keys(FacilityKeyPair const &keys) = 0;
};

template <class Req>
class ClientSideSignatureAndEncBasedIdentityAttacherComponent 
    : public dev::cd606::tm::transport::ClientSideAbstractIdentityAttacherComponent<std::string, Req>
    , public ClientSideSignatureAndEncBasedIdentityAttacherComponentBase
{
private:
    dev::cd606::tm::transport::security::SignatureHelper::Signer signer_;
    EncHelper outgoing_, incoming_;
public:
    ClientSideSignatureAndEncBasedIdentityAttacherComponent() : signer_(), outgoing_(), incoming_() {}
    ClientSideSignatureAndEncBasedIdentityAttacherComponent(std::array<unsigned char, 64> const &privateKey) : signer_(privateKey), outgoing_(), incoming_() {}
    ClientSideSignatureAndEncBasedIdentityAttacherComponent(ClientSideSignatureAndEncBasedIdentityAttacherComponent const &) = delete;
    ClientSideSignatureAndEncBasedIdentityAttacherComponent &operator=(ClientSideSignatureAndEncBasedIdentityAttacherComponent const &) = delete;
    ClientSideSignatureAndEncBasedIdentityAttacherComponent(ClientSideSignatureAndEncBasedIdentityAttacherComponent &&) = default;
    ClientSideSignatureAndEncBasedIdentityAttacherComponent &operator=(ClientSideSignatureAndEncBasedIdentityAttacherComponent &&) = default;
    virtual ~ClientSideSignatureAndEncBasedIdentityAttacherComponent() = default;
    void set_encdec_keys(FacilityKeyPair const &keys) override final {
        outgoing_.setKey(keys.outgoingKey);
        incoming_.setKey(keys.incomingKey);
    }
    virtual dev::cd606::tm::basic::ByteData attach_identity(dev::cd606::tm::basic::ByteData &&d) override final {
        return signer_.sign(outgoing_.encode(std::move(d)));
    }
    virtual std::optional<dev::cd606::tm::basic::ByteData> process_incoming_data(dev::cd606::tm::basic::ByteData &&d) override final {
        return incoming_.decode(std::move(d));
    }
};

class ServerSideSignatureAndEncBasedIdentityCheckerComponentBase {
public:
    virtual ~ServerSideSignatureAndEncBasedIdentityCheckerComponentBase() {}
    virtual void set_encdec_keys(FacilityKeyPairForIdentity const &keys)  = 0;
};

template <class Req>
class ServerSideSignatureAndEncBasedIdentityCheckerComponent 
    : public dev::cd606::tm::transport::ServerSideAbstractIdentityCheckerComponent<std::string, Req>
    , public ServerSideSignatureAndEncBasedIdentityCheckerComponentBase
{
private:
    struct EncDec {
        EncHelper outgoing;
        EncHelper incoming;
    };
    dev::cd606::tm::transport::security::SignatureHelper::Verifier verifier_;
    std::unordered_map<std::string, std::unique_ptr<EncDec>> encDecs_;
    std::mutex mutex_;
public:
    ServerSideSignatureAndEncBasedIdentityCheckerComponent() : verifier_(), encDecs_(), mutex_() {}
    ServerSideSignatureAndEncBasedIdentityCheckerComponent(ServerSideSignatureAndEncBasedIdentityCheckerComponent const &) = delete;
    ServerSideSignatureAndEncBasedIdentityCheckerComponent &operator=(ServerSideSignatureAndEncBasedIdentityCheckerComponent const &) = delete;
    ServerSideSignatureAndEncBasedIdentityCheckerComponent(ServerSideSignatureAndEncBasedIdentityCheckerComponent &&) = default;
    ServerSideSignatureAndEncBasedIdentityCheckerComponent &operator=(ServerSideSignatureAndEncBasedIdentityCheckerComponent &&) = default;
    virtual ~ServerSideSignatureAndEncBasedIdentityCheckerComponent() = default;
    void add_identity_and_key(std::string const &name, std::array<unsigned char, 32> const &publicKey) {
        verifier_.addKey(name, publicKey);
    }
    void set_encdec_keys(FacilityKeyPairForIdentity const &keys) override final {
        std::lock_guard<std::mutex> _(mutex_);
        auto iter = encDecs_.find(keys.identity);
        if (iter == encDecs_.end()) {
            iter = encDecs_.insert({
                keys.identity
                , std::make_unique<EncDec>()
            }).first;
        }
        iter->second->outgoing.setKey(keys.outgoingKey);
        iter->second->incoming.setKey(keys.incomingKey);
    }
    virtual std::optional<std::tuple<std::string,dev::cd606::tm::basic::ByteData>> check_identity(dev::cd606::tm::basic::ByteData &&d) override final {
        auto v = verifier_.verify(std::move(d));
        if (!v) {
            return std::nullopt;
        }
        auto const &identity = std::get<0>(*v);
        EncDec *encDec = nullptr;
        {
            std::lock_guard<std::mutex> _(mutex_);
            auto iter = encDecs_.find(identity);
            if (iter == encDecs_.end()) {
                return std::nullopt;
            }
            encDec = iter->second.get();
        }
        if (encDec) {
            auto d = encDec->incoming.decode(std::move(std::get<1>(*v)));
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
    virtual dev::cd606::tm::basic::ByteData process_outgoing_data(std::string const &identity, dev::cd606::tm::basic::ByteData &&d) override final {
        EncDec *encDec = nullptr;
        {
            std::lock_guard<std::mutex> _(mutex_);
            auto iter = encDecs_.find(identity);
            if (iter == encDecs_.end()) {
                return dev::cd606::tm::basic::ByteData {};
            }
            encDec = iter->second.get();
        }
        if (encDec) {
            return encDec->outgoing.encode(std::move(d));
        } else {
            return dev::cd606::tm::basic::ByteData {};
        }
    }
};
#endif