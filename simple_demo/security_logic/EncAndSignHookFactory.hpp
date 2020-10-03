#ifndef ENC_AND_SIGN_HOOK_FACTORY_HPP_
#define ENC_AND_SIGN_HOOK_FACTORY_HPP_

#include <tm_kit/transport/AbstractHookFactoryComponent.hpp>
#include <tm_kit/transport/security/SignatureHelper.hpp>
#include "simple_demo/security_logic/EncHook.hpp"

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
    virtual dev::cd606::tm::transport::UserToWireHook defaultHook() override final {
        auto encHook = std::make_shared<EncHook>();
        encHook->setKey(EncHook::keyFromString(encKey_));
        auto signer = std::make_shared<dev::cd606::tm::transport::security::SignatureHelper::Signer>(signKey_);
        return dev::cd606::tm::transport::composeUserToWireHook(
            { [encHook](dev::cd606::tm::basic::ByteData &&d) {
                return encHook->encode(std::move(d));
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
    virtual dev::cd606::tm::transport::WireToUserHook defaultHook() override final {
        auto decHook = std::make_shared<EncHook>();
        decHook->setKey(EncHook::keyFromString(decKey_));
        auto verifier = std::make_shared<dev::cd606::tm::transport::security::SignatureHelper::Verifier>();
        verifier->addKey("", verifyKey_);
        return dev::cd606::tm::transport::composeWireToUserHook(
            { [verifier](dev::cd606::tm::basic::ByteData &&d) -> std::optional<dev::cd606::tm::basic::ByteData> {
                auto res = verifier->verify(std::move(d));
                if (res) {
                    return std::move(std::get<1>(*res));
                } else {
                    return std::nullopt;
                }
            } }
            , { [decHook](dev::cd606::tm::basic::ByteData &&d) {
                return decHook->decode(std::move(d));
            } }
        );
    }
};


template <class T>
class EncHookFactoryComponent : public dev::cd606::tm::transport::AbstractOutgoingHookFactoryComponent<T> {
private:
    std::string encKey_;
    std::mutex mutex_;
public:
    EncHookFactoryComponent() : encKey_(), mutex_() {}
    EncHookFactoryComponent(std::string const &encKey)
        : encKey_(encKey), mutex_() {}
    EncHookFactoryComponent &operator=(EncHookFactoryComponent &&c) {
        if (this == &c) {
            return *this;
        }
        std::lock_guard<std::mutex> _(mutex_);
        std::lock_guard<std::mutex> __(c.mutex_);
        encKey_ = std::move(c.encKey_);
        return *this;
    }
    void setEncKey(std::string const &k) {
        std::lock_guard<std::mutex> _(mutex_);
        encKey_ = k;
    }
    virtual ~EncHookFactoryComponent() {}
    virtual dev::cd606::tm::transport::UserToWireHook defaultHook() override final {
        auto encHook = std::make_shared<EncHook>();
        {
            std::lock_guard<std::mutex> _(mutex_);
            encHook->setKey(EncHook::keyFromString(encKey_));
        }
        return dev::cd606::tm::transport::UserToWireHook { 
            [encHook](dev::cd606::tm::basic::ByteData &&d) {
                return encHook->encode(std::move(d));
            } 
        };
    }
};

template <class T>
class DecHookFactoryComponent : public dev::cd606::tm::transport::AbstractIncomingHookFactoryComponent<T> {
private:
    std::string decKey_;
    std::mutex mutex_;
public:
    DecHookFactoryComponent() : decKey_(), mutex_() {}
    DecHookFactoryComponent(std::string const &decKey)
        : decKey_(decKey), mutex_() {}
    DecHookFactoryComponent &operator=(DecHookFactoryComponent &&d) {
        if (this == &d) {
            return *this;
        }
        std::lock_guard<std::mutex> _(mutex_);
        std::lock_guard<std::mutex> __(d.mutex_);
        decKey_ = std::move(d.decKey_);
        return *this;
    }
    virtual ~DecHookFactoryComponent() {}
    void setDecKey(std::string const &k) {
        std::lock_guard<std::mutex> _(mutex_);
        decKey_ = k;
    }
    virtual dev::cd606::tm::transport::WireToUserHook defaultHook() override final {
        auto decHook = std::make_shared<EncHook>();
        {
            std::lock_guard<std::mutex> _(mutex_);
            decHook->setKey(EncHook::keyFromString(decKey_));
        }
        return dev::cd606::tm::transport::WireToUserHook { 
            [decHook](dev::cd606::tm::basic::ByteData &&d) {
                return decHook->decode(std::move(d));
            } 
        };
    }
};

#endif