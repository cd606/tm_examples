#include <mutex>
#include <cstring>
#include <unordered_map>

//The implementation is from https://github.com/msotoodeh/curve25519.git
//#include <curve25519/C++/ed25519.h>
//Now we move to libsodium
#include <sodium.h>

#include "SignatureHelper.hpp"

static_assert(crypto_sign_SECRETKEYBYTES == 64, "libsodium private key length is not 64");
static_assert(crypto_sign_PUBLICKEYBYTES == 32, "libsodium public key length is not 32");
static_assert(crypto_sign_BYTES == 64, "libsodium signature length is not 64");

class SignHelperImpl {
private:
    std::string name_;
    //ED25519Private signer_;
    std::array<unsigned char, 64> privateKey_;
public:
    SignHelperImpl(std::string const &name, std::array<unsigned char, 64> const &privateKey) : 
        //name_(name), signer_(privateKey.data(), 64) {}
        name_(name), privateKey_(privateKey) {}
    ~SignHelperImpl() {}
    dev::cd606::tm::basic::ByteData sign(dev::cd606::tm::basic::ByteData &&data) {       
        std::array<unsigned char, 64> signature;
        /*
        signer_.SignMessage(
            reinterpret_cast<unsigned char const *>(data.content.c_str())
            , data.content.length()
            , signature.data()
        );
        */
        crypto_sign_detached(
            signature.data()
            , 0
            , reinterpret_cast<unsigned char const *>(data.content.c_str())
            , data.content.length()
            , privateKey_.data()
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
    //std::unordered_map<std::string, std::unique_ptr<ED25519Public>> verifiers_;
    std::unordered_map<std::string, std::array<unsigned char, 32>> publicKeys_;
    std::mutex mutex_;
public:
    VerifyHelperImpl() : /*verifiers_()*/publicKeys_(), mutex_() {}
    ~VerifyHelperImpl() {}
    void addKey(std::string const &name, std::array<unsigned char, 32> const &publicKey) {
        std::lock_guard<std::mutex> _(mutex_);
        /*
        verifiers_.insert({
            name
            , std::make_unique<ED25519Public>(publicKey.data())
        });
        */
        publicKeys_.insert({name, publicKey});
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
        std::size_t l = signedData.content.length();
        std::string name;
        {
            std::lock_guard<std::mutex> _(mutex_);
            //for (auto const &item : verifiers_) {
            for (auto const &item : publicKeys_) {
                //result = result || item.second->VeifySignature(p, signedData.content.length(), q);
                result = 
                    result ||
                    (
                        crypto_sign_verify_detached(q, p, l, item.second.data()) == 0
                    );
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