#ifndef AES_HOOK_HPP_
#define AES_HOOK_HPP_

#include <array>
#include <memory>
#include <cstddef>

#include <optional>
#include <tm_kit/basic/ByteData.hpp>

class AESHookImpl;

class AESHook {
private:
    std::unique_ptr<AESHookImpl> impl_;
public:
    static constexpr std::size_t KeyLength = 128;
    AESHook();
    ~AESHook();
    AESHook(AESHook const &) = delete;
    AESHook &operator=(AESHook const &) = delete;
    AESHook(AESHook &&);
    AESHook &operator=(AESHook &&);
    static std::array<unsigned char,KeyLength/8> keyFromString(std::string const &s);
    void setKey(std::array<unsigned char,KeyLength/8> const &key);
    dev::cd606::tm::basic::ByteData encode(dev::cd606::tm::basic::ByteData &&);
    std::optional<dev::cd606::tm::basic::ByteData> decode(dev::cd606::tm::basic::ByteData &&);
};

#endif