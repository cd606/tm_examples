#include <mutex>
#include <cstring>
#include <ctime>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

//The implementation is from https://github.com/SergeyBel/AES.git
#include <AES/AES.h>

#include "AESHook.hpp"

class AESHookImpl {
private:
    boost::random::mt19937 gen_;
    boost::random::uniform_int_distribution<unsigned char> dist_;
    AES aes_;
    unsigned char key_[AESHook::KeyLength/8];
    bool hasKey_;
    std::mutex mutex_;
public:
    AESHookImpl() : gen_(), dist_((unsigned char) 0, (unsigned char) 255), aes_(AESHook::KeyLength), key_(), hasKey_(false), mutex_() {
        gen_.seed(std::time(nullptr));
    }
    ~AESHookImpl() {}
    void setKey(std::array<unsigned char,AESHook::KeyLength/8> const &key) {
        std::lock_guard<std::mutex> _(mutex_);
        std::memcpy(key_, key.data(), AESHook::KeyLength/8*sizeof(unsigned char));
        hasKey_ = true;
    }
    dev::cd606::tm::basic::ByteData encode(dev::cd606::tm::basic::ByteData &&data) {
        auto input = std::make_unique<unsigned char[]>(data.content.length());
        std::memcpy(input.get(), data.content.c_str(), data.content.length());

        unsigned char iv[16];
        for (size_t ii=0; ii<16; ++ii) {
            iv[ii] = dist_(gen_);
        }

        unsigned char keyCopy[AESHook::KeyLength/8];
        {
            std::lock_guard<std::mutex> _(mutex_);
            std::memcpy(keyCopy, key_, AESHook::KeyLength/8*sizeof(unsigned char));
        }

        unsigned encLen;
        
        std::unique_ptr<unsigned char[]> encRes { aes_.EncryptCBC(
            input.get()
            , data.content.length()
            , keyCopy
            , iv
            , encLen
        ) };
        auto res = std::make_unique<unsigned char[]>(encLen+24);
        uint64_t origLen = data.content.length();
        std::memcpy(res.get(), &origLen, 8);
        std::memcpy(res.get()+8, iv, 16);
        std::memcpy(res.get()+24, encRes.get(), encLen);
        return dev::cd606::tm::basic::ByteData {
            std::string { res.get(), res.get()+(encLen+24) }
        };
    }
    std::optional<dev::cd606::tm::basic::ByteData> decode(dev::cd606::tm::basic::ByteData &&data) {
        if (data.content.length() < 24) {
            return std::nullopt;
        }

        auto input = std::make_unique<unsigned char[]>(data.content.length());
        std::memcpy(input.get(), data.content.c_str(), data.content.length());

        unsigned char keyCopy[AESHook::KeyLength/8];
        {
            std::lock_guard<std::mutex> _(mutex_);
            std::memcpy(keyCopy, key_, AESHook::KeyLength/8*sizeof(unsigned char));
        }

        std::unique_ptr<unsigned char[]> decRes { aes_.DecryptCBC(
            input.get()+24
            , data.content.length()-24
            , keyCopy
            , input.get()+8
        ) };
        uint64_t origLen;
        std::memcpy(&origLen, input.get(), 8);
        return dev::cd606::tm::basic::ByteData {
            std::string { decRes.get(), decRes.get()+origLen }
        };
    }
};

AESHook::AESHook() : impl_(std::make_unique<AESHookImpl>()) {}
AESHook::~AESHook() {}
AESHook::AESHook(AESHook &&) = default;
AESHook &AESHook::operator=(AESHook &&) = default;
std::array<unsigned char,AESHook::KeyLength/8> AESHook::keyFromString(std::string const &s) {
    if (s.length() < AESHook::KeyLength/8) {
        return keyFromString(s+std::string(AESHook::KeyLength/8-s.length(), ' '));
    } else {
        std::array<unsigned char,AESHook::KeyLength/8> ret;
        std::memcpy(ret.data(), s.c_str(), AESHook::KeyLength/8);
        return ret;
    }
}
void AESHook::setKey(std::array<unsigned char,AESHook::KeyLength/8> const &key) {
    impl_->setKey(key);
}
dev::cd606::tm::basic::ByteData AESHook::encode(dev::cd606::tm::basic::ByteData &&data) {
    return impl_->encode(std::move(data));
}
std::optional<dev::cd606::tm::basic::ByteData> AESHook::decode(dev::cd606::tm::basic::ByteData &&data) {
    return impl_->decode(std::move(data));
}