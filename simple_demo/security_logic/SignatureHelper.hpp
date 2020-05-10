#ifndef SIGNATURE_HELPER_HPP_
#define SIGNATURE_HELPER_HPP_

#include <array>
#include <memory>
#include <cstddef>
#include <string>
#include <tuple>
#include <optional>
#include <tm_kit/basic/ByteData.hpp>

class SignHelperImpl;

class SignHelper {
private:
    std::unique_ptr<SignHelperImpl> impl_;
public:
    SignHelper();
    SignHelper(std::string const &name, std::array<unsigned char, 64> const &privateKey);
    ~SignHelper();
    SignHelper(SignHelper const &) = delete;
    SignHelper &operator=(SignHelper const &) = delete;
    SignHelper(SignHelper &&);
    SignHelper &operator=(SignHelper &&);
    dev::cd606::tm::basic::ByteData sign(dev::cd606::tm::basic::ByteData &&);
};

class VerifyHelperImpl;

class VerifyHelper {
private:
    std::unique_ptr<VerifyHelperImpl> impl_;
public:
    VerifyHelper();
    ~VerifyHelper();
    VerifyHelper(VerifyHelper const &) = delete;
    VerifyHelper &operator=(VerifyHelper const &) = delete;
    VerifyHelper(VerifyHelper &&);
    VerifyHelper &operator=(VerifyHelper &&);
    void addKey(std::string const &name, std::array<unsigned char, 32> const &publicKey);
    std::optional<std::tuple<std::string,dev::cd606::tm::basic::ByteData>> verify(dev::cd606::tm::basic::ByteData &&);
};

#endif