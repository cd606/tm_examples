#include "DHHelper.hpp"

//The implementation is from https://github.com/msotoodeh/curve25519.git
#include <curve25519/C++/x25519.h>

class DHServerHelperImpl {
private:
    std::function<void(std::string const &, std::array<unsigned char, 16> const &)> localRegistryUpdater_;
public:
    DHServerHelperImpl(std::function<void(std::string const &, std::array<unsigned char, 16> const &)> localRegistryUpdater)
        : localRegistryUpdater_(localRegistryUpdater) {}
    ~DHServerHelperImpl() {}
    DHHelperReply process(std::tuple<std::string, DHHelperCommand> &&input) {
        X25519Private x;
        std::array<unsigned char, 16> sharedKey;
        x.CreateSharedKey(
            reinterpret_cast<const unsigned char *>(std::get<1>(input).clientPub.content.c_str())
            , sharedKey.data()
            , 16
        );
        localRegistryUpdater_(std::get<0>(input), sharedKey);
        DHHelperReply ret;
        const unsigned char *pub = x.GetPublicKey(nullptr);
        ret.serverPub = dev::cd606::tm::basic::ByteData { std::string {pub, pub+32} };
        return ret;
    }
};

DHServerHelper::DHServerHelper(std::function<void(std::string const &, std::array<unsigned char, 16> const &)> localRegistryUpdater)
    : impl_(std::make_unique<DHServerHelperImpl>(localRegistryUpdater)) {}
DHServerHelper::~DHServerHelper() {}
    
DHHelperReply DHServerHelper::process(std::tuple<std::string, DHHelperCommand> &&input) {
    return impl_->process(std::move(input));
}

class DHClientHelperImpl {
private:
    std::function<void(std::array<unsigned char, 16> const &)> localKeyUpdater_;
    X25519Private x_;
public:
    DHClientHelperImpl(std::function<void(std::array<unsigned char, 16> const &)> localKeyUpdater)
        : localKeyUpdater_(localKeyUpdater), x_() {}
    ~DHClientHelperImpl() {}
    void reset() {
        x_ = X25519Private {};
    }
    DHHelperCommand buildCommand() {
        const unsigned char *pub = x_.GetPublicKey(nullptr);
        return DHHelperCommand {
            dev::cd606::tm::basic::ByteData { std::string {pub, pub+32} }
        };
    }
    void process(DHHelperReply const &input) {
        std::array<unsigned char, 16> sharedKey;
        x_.CreateSharedKey(
            reinterpret_cast<const unsigned char *>(input.serverPub.content.c_str())
            , sharedKey.data()
            , 16
        );
        localKeyUpdater_(sharedKey);
    }
    std::function<void(std::array<unsigned char, 16> const &)> localKeyUpdater() const {
        return localKeyUpdater_;
    }
};

DHClientHelper::DHClientHelper(std::function<void(std::array<unsigned char, 16> const &)> localKeyUpdater)
    : impl_(std::make_unique<DHClientHelperImpl>(localKeyUpdater)) {}
DHClientHelper::~DHClientHelper() {}
    
void DHClientHelper::reset() {
    impl_->reset();
}
DHHelperCommand DHClientHelper::buildCommand() {
    return impl_->buildCommand();
}
void DHClientHelper::process(DHHelperReply const &input) {
    impl_->process(input);
}