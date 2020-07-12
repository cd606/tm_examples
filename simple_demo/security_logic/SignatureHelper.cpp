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
        std::array<unsigned char, 64> signature;
        signer_.SignMessage(
            reinterpret_cast<unsigned char const *>(data.content.c_str())
            , data.content.length()
            , signature.data()
        );
        std::tuple<dev::cd606::tm::basic::ByteData, dev::cd606::tm::basic::ByteData const *> t {dev::cd606::tm::basic::ByteData {std::string(reinterpret_cast<char const *>(signature.data()), signature.size())}, &data};
        auto res = dev::cd606::tm::basic::bytedata_utils::RunCBORSerializerWithNameList<
            std::tuple<dev::cd606::tm::basic::ByteData, dev::cd606::tm::basic::ByteData const *>
            , 2
        >::apply(t, {"signature", "data"});
        return dev::cd606::tm::basic::ByteData {std::string(reinterpret_cast<char const *>(res.data()), res.size())};
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
         auto res = dev::cd606::tm::basic::bytedata_utils::RunCBORDeserializerWithNameList<
            std::tuple<dev::cd606::tm::basic::ByteData, dev::cd606::tm::basic::ByteData>
            , 2
        >::apply(std::string_view {data.content}, 0, {"signature", "data"});
        if (!res) {
            return std::nullopt;
        }
        if (std::get<1>(*res) != data.content.length()) {
            return std::nullopt;
        }
        auto const &dataWithSignature = std::get<0>(*res);
        auto const &signature = std::get<0>(dataWithSignature);
        auto const &signedData = std::get<1>(dataWithSignature);
        if (signature.content.length() != 64) {
            return std::nullopt;
        }

        bool result = false;
        
        auto const *p = reinterpret_cast<const unsigned char *>(signedData.content.c_str());
        auto const *q = reinterpret_cast<const unsigned char *>(signature.content.c_str());
        std::string name;
        {
            std::lock_guard<std::mutex> _(mutex_);
            for (auto const &item : verifiers_) {
                result = result || item.second->VeifySignature(p, signedData.content.length(), q);
                if (result) {
                    name = item.first;
                    break;
                }
            }
        }
        if (result) {
            return std::tuple<std::string, dev::cd606::tm::basic::ByteData> {std::move(name), std::move(signedData)};
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