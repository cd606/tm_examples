#include <mutex>
#include <cstring>
#include <unordered_map>

//The implementation is from https://github.com/msotoodeh/curve25519.git
#include <curve25519/C++/ed25519.h>

#include "SignatureHelper.hpp"

class SignHelperImpl {
private:
    std::string name_;
    ED25519Private signer_;
public:
    SignHelperImpl(std::string const &name, std::array<unsigned char, 64> const &privateKey) : 
        name_(name), signer_(privateKey.data(), 64) {}
    ~SignHelperImpl() {}
    dev::cd606::tm::basic::ByteData sign(dev::cd606::tm::basic::ByteData &&data) {       
        std::size_t origLen = data.content.length();
        std::string outputContent { std::move(data.content) };
        outputContent.resize(origLen+64);
        auto const *p = reinterpret_cast<unsigned char const *>(outputContent.c_str());
        auto *signArea = const_cast<unsigned char *>(p+origLen);
        signer_.SignMessage(p, origLen, signArea);
        return dev::cd606::tm::basic::ByteData {std::move(outputContent)};
    }
};

SignHelper::SignHelper() : impl_() {}
SignHelper::SignHelper(std::string const &name, std::array<unsigned char, 64> const &privateKey)
    : impl_(std::make_unique<SignHelperImpl>(name, privateKey))
    {}
SignHelper::~SignHelper() {}
SignHelper::SignHelper(SignHelper &&) = default;
SignHelper &SignHelper::operator=(SignHelper &&) = default;
dev::cd606::tm::basic::ByteData SignHelper::sign(dev::cd606::tm::basic::ByteData &&data) {
    if (impl_) {
        return impl_->sign(std::move(data));
    } else {
        return std::move(data);
    }
}

class VerifyHelperImpl {
private:
    std::unordered_map<std::string, std::unique_ptr<ED25519Public>> verifiers_;
    std::mutex mutex_;
public:
    VerifyHelperImpl() : verifiers_(), mutex_() {}
    ~VerifyHelperImpl() {}
    void addKey(std::string const &name, std::array<unsigned char, 32> const &publicKey) {
        std::lock_guard<std::mutex> _(mutex_);
        verifiers_.insert({
            name
            , std::make_unique<ED25519Public>(publicKey.data())
        });
    }
    std::optional<std::tuple<std::string,dev::cd606::tm::basic::ByteData>> verify(dev::cd606::tm::basic::ByteData &&data) {       
        std::size_t origLen = data.content.length();
        if (origLen < 64) {
            return std::nullopt;
        }
        dev::cd606::tm::basic::ByteData output {std::move(data)};
        origLen -= 64;
        bool result = false;
        auto const *p = reinterpret_cast<const unsigned char *>(output.content.c_str());
        auto const *q = p+origLen;
        std::string name;
        {
            std::lock_guard<std::mutex> _(mutex_);
            for (auto const &item : verifiers_) {
                result = result || item.second->VeifySignature(p, origLen, q);
                if (result) {
                    name = item.first;
                    break;
                }
            }
        }
        if (result) {
            output.content.resize(origLen);
            return std::tuple<std::string, dev::cd606::tm::basic::ByteData> {std::move(name), std::move(output)};
        } else {
            return std::nullopt;
        }
    }
};

VerifyHelper::VerifyHelper() : impl_(std::make_unique<VerifyHelperImpl>()) {}
VerifyHelper::~VerifyHelper() {}
VerifyHelper::VerifyHelper(VerifyHelper &&) = default;
VerifyHelper &VerifyHelper::operator=(VerifyHelper &&) = default;
void VerifyHelper::addKey(std::string const &name, std::array<unsigned char, 32> const &publicKey) {
    impl_->addKey(name,publicKey);
}
std::optional<std::tuple<std::string,dev::cd606::tm::basic::ByteData>> VerifyHelper::verify(dev::cd606::tm::basic::ByteData &&data) {
    return impl_->verify(std::move(data));
}