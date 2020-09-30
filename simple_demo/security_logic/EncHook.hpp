#ifndef ENC_HOOK_HPP_
#define ENC_HOOK_HPP_

#include <array>
#include <memory>
#include <cstddef>

#include <optional>
#include <tm_kit/basic/ByteData.hpp>

class EncHookImpl;

class EncHook {
private:
    std::unique_ptr<EncHookImpl> impl_;
public:
    //when we move to libsodium, the key length is no longer 16 bytes but 32 bytes
    //static constexpr std::size_t KeyLength = 128;
    static constexpr std::size_t KeyLength = 256;
    EncHook();
    ~EncHook();
    EncHook(EncHook const &) = delete;
    EncHook &operator=(EncHook const &) = delete;
    EncHook(EncHook &&);
    EncHook &operator=(EncHook &&);
    static std::array<unsigned char,KeyLength/8> keyFromString(std::string const &s);
    void setKey(std::array<unsigned char,KeyLength/8> const &key);
    dev::cd606::tm::basic::ByteData encode(dev::cd606::tm::basic::ByteData &&);
    std::optional<dev::cd606::tm::basic::ByteData> decode(dev::cd606::tm::basic::ByteData &&);
};

#endif