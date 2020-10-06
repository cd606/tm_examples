#include <mutex>
#include <cstring>
#include <ctime>

#include <sodium.h>

#include "EncHelper.hpp"

class EncHelperImpl {
private:
    unsigned char key_[EncHelper::KeyLength/8];
    std::mutex mutex_;
public:
    EncHelperImpl() : key_(), mutex_() {
    }
    ~EncHelperImpl() {}
    void setKey(std::array<unsigned char,EncHelper::KeyLength/8> const &key) {
        std::lock_guard<std::mutex> _(mutex_);
        std::memcpy(key_, key.data(), EncHelper::KeyLength/8*sizeof(unsigned char));
    }
    dev::cd606::tm::basic::ByteData encode(dev::cd606::tm::basic::ByteData &&data) {
        std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
        randombytes_buf(nonce.data(), crypto_secretbox_NONCEBYTES);
        auto l = data.content.length();
        dev::cd606::tm::basic::ByteData ret;
        ret.content.resize(
            crypto_secretbox_NONCEBYTES
            +l
            +crypto_secretbox_MACBYTES
        );
        auto *p = reinterpret_cast<unsigned char *>(ret.content.data());
        if (crypto_secretbox_easy(
            &p[crypto_secretbox_NONCEBYTES]
            , reinterpret_cast<unsigned char const *>(data.content.data())
            , l
            , nonce.data()
            , key_
            ) == 0) 
        {
            std::memcpy(p, nonce.data(), crypto_secretbox_NONCEBYTES);
            return ret;
        }
        else 
        {
            return dev::cd606::tm::basic::ByteData {};
        }
    }
    std::optional<dev::cd606::tm::basic::ByteData> decode(dev::cd606::tm::basic::ByteData &&data) {
        auto l = data.content.length();
        if (l < crypto_secretbox_NONCEBYTES+crypto_secretbox_MACBYTES) {
            return std::nullopt;
        }
        dev::cd606::tm::basic::ByteData ret;
        ret.content.resize(l-crypto_secretbox_NONCEBYTES-crypto_secretbox_MACBYTES);
        auto *p = reinterpret_cast<unsigned char const *>(data.content.data());
        if (crypto_secretbox_open_easy(
            reinterpret_cast<unsigned char *>(ret.content.data())
            , &p[crypto_secretbox_NONCEBYTES]
            , l-crypto_secretbox_NONCEBYTES
            , p 
            , key_
        ) == 0) {
            return ret;
        } else {
            return std::nullopt;
        }
    }
};

EncHelper::EncHelper() : impl_(std::make_unique<EncHelperImpl>()) {}
EncHelper::~EncHelper() {}
EncHelper::EncHelper(EncHelper &&) = default;
EncHelper &EncHelper::operator=(EncHelper &&) = default;
std::array<unsigned char,EncHelper::KeyLength/8> EncHelper::keyFromString(std::string const &s) {
    static_assert(crypto_generichash_BYTES >= EncHelper::KeyLength/8, "generic hash bytes not long enough");
    unsigned char hash_res[crypto_generichash_BYTES];
    crypto_generichash(
        hash_res, crypto_generichash_BYTES
        , reinterpret_cast<unsigned char const *>(s.data()), s.length()
        , 0, 0
    );
    std::array<unsigned char,EncHelper::KeyLength/8> ret;
    std::memcpy(ret.data(), hash_res, EncHelper::KeyLength/8);
    return ret;
}
void EncHelper::setKey(std::array<unsigned char,EncHelper::KeyLength/8> const &key) {
    impl_->setKey(key);
}
dev::cd606::tm::basic::ByteData EncHelper::encode(dev::cd606::tm::basic::ByteData &&data) {
    return impl_->encode(std::move(data));
}
std::optional<dev::cd606::tm::basic::ByteData> EncHelper::decode(dev::cd606::tm::basic::ByteData &&data) {
    return impl_->decode(std::move(data));
}