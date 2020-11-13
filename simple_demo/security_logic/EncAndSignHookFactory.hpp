#ifndef ENC_AND_SIGN_HOOK_FACTORY_HPP_
#define ENC_AND_SIGN_HOOK_FACTORY_HPP_

#include <tm_kit/transport/AbstractHookFactoryComponent.hpp>
#include <tm_kit/transport/security/SignatureHelper.hpp>
#include "simple_demo/security_logic/EncHelper.hpp"

template <class T>
class EncAndSignHookFactoryComponent : public dev::cd606::tm::transport::AbstractOutgoingHookFactoryComponent<T> {
private:
    std::string encKey_;
    std::array<unsigned char, 64> signKey_;
public:
    EncAndSignHookFactoryComponent() : encKey_(), signKey_() {}
    EncAndSignHookFactoryComponent(std::string const &encKey, std::array<unsigned char, 64> const &signKey)
        : encKey_(encKey), signKey_(signKey) {}
    virtual ~EncAndSignHookFactoryComponent() {}
    virtual std::optional<dev::cd606::tm::transport::UserToWireHook> defaultHook() override final {
        auto encHelper = std::make_shared<EncHelper>();
        encHelper->setKey(EncHelper::keyFromString(encKey_));
        auto signer = std::make_shared<dev::cd606::tm::transport::security::SignatureHelper::Signer>(signKey_);
        return dev::cd606::tm::transport::composeUserToWireHook(
            { [encHelper](dev::cd606::tm::basic::ByteData &&d) {
                return encHelper->encode(std::move(d));
            } }
            , { [signer](dev::cd606::tm::basic::ByteData &&d) {
                return signer->sign(std::move(d));
            } }
        );
    }
};

template <class T>
class VerifyAndDecHookFactoryComponent : public dev::cd606::tm::transport::AbstractIncomingHookFactoryComponent<T> {
private:
    std::string decKey_;
    std::array<unsigned char, 32> verifyKey_;
public:
    VerifyAndDecHookFactoryComponent() : decKey_(), verifyKey_() {}
    VerifyAndDecHookFactoryComponent(std::string const &decKey, std::array<unsigned char, 32> const &verifyKey)
        : decKey_(decKey), verifyKey_(verifyKey) {}
    virtual ~VerifyAndDecHookFactoryComponent() {}
    virtual std::optional<dev::cd606::tm::transport::WireToUserHook> defaultHook() override final {
        auto decHelper = std::make_shared<EncHelper>();
        decHelper->setKey(EncHelper::keyFromString(decKey_));
        auto verifier = std::make_shared<dev::cd606::tm::transport::security::SignatureHelper::Verifier>();
        verifier->addKey("", verifyKey_);
        return dev::cd606::tm::transport::composeWireToUserHook(
            { [verifier](dev::cd606::tm::basic::ByteDataView const &d) -> std::optional<dev::cd606::tm::basic::ByteData> {
                auto res = verifier->verify(d);
                if (res) {
                    return std::move(std::get<1>(*res));
                } else {
                    return std::nullopt;
                }
            } }
            , { [decHelper](dev::cd606::tm::basic::ByteDataView const &d) {
                return decHelper->decode(d);
            } }
        );
    }
};


template <class T>
class EncHookFactoryComponent : public dev::cd606::tm::transport::AbstractOutgoingHookFactoryComponent<T> {
private:
    std::string encKey_;
public:
    EncHookFactoryComponent() : encKey_() {}
    EncHookFactoryComponent(std::string const &encKey)
        : encKey_(encKey) {}
    virtual ~EncHookFactoryComponent() {}
    virtual std::optional<dev::cd606::tm::transport::UserToWireHook> defaultHook() override final {
        auto encHelper = std::make_shared<EncHelper>();
        encHelper->setKey(EncHelper::keyFromString(encKey_));
        return dev::cd606::tm::transport::UserToWireHook { 
            [encHelper](dev::cd606::tm::basic::ByteData &&d) {
                return encHelper->encode(std::move(d));
            } 
        };
    }
};

template <class T>
class DecHookFactoryComponent : public dev::cd606::tm::transport::AbstractIncomingHookFactoryComponent<T> {
private:
    std::string decKey_;
public:
    DecHookFactoryComponent() : decKey_() {}
    DecHookFactoryComponent(std::string const &decKey)
        : decKey_(decKey) {}
    virtual ~DecHookFactoryComponent() {}
    void setDecKey(std::string const &k) {
        decKey_ = k;
    }
    virtual std::optional<dev::cd606::tm::transport::WireToUserHook> defaultHook() override final {
        auto decHelper = std::make_shared<EncHelper>();
        decHelper->setKey(EncHelper::keyFromString(decKey_));
        return dev::cd606::tm::transport::WireToUserHook { 
            [decHelper](dev::cd606::tm::basic::ByteDataView const &d) {
                return decHelper->decode(d);
            } 
        };
    }
};

#endif