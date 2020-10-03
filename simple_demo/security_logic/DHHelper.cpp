#include "DHHelper.hpp"

//The implementation is from https://github.com/msotoodeh/curve25519.git
//#include <curve25519/C++/x25519.h>
//Now we move to libsodium
#include <sodium.h>

static_assert(crypto_kx_SESSIONKEYBYTES >= 32, "libsodium kx session key length is less than 32");

class DHServerHelperImpl {
private:
    std::function<void(FacilityKeyPairForIdentity const &)> localRegistryUpdater_;
public:
    DHServerHelperImpl(std::function<void(FacilityKeyPairForIdentity const &)> localRegistryUpdater)
        : localRegistryUpdater_(localRegistryUpdater) {}
    ~DHServerHelperImpl() {}
    DHHelperReply process(std::tuple<std::string, DHHelperCommand> &&input) {
        unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
        unsigned char server_sk[crypto_kx_SECRETKEYBYTES];
        unsigned char server_rx[crypto_kx_SESSIONKEYBYTES];
        unsigned char server_tx[crypto_kx_SESSIONKEYBYTES];

        crypto_kx_keypair(server_pk, server_sk);
        if (crypto_kx_server_session_keys(
            server_rx, server_tx, server_pk, server_sk
            , reinterpret_cast<const unsigned char *>(std::get<1>(input).clientPub.content.c_str())
        ) != 0) {
            return DHHelperReply {};
        }
        std::array<unsigned char, 32> sharedOutgoingKey, sharedIncomingKey;
        std::memcpy(sharedOutgoingKey.data(), server_tx, 32);
        std::memcpy(sharedIncomingKey.data(), server_rx, 32);
        localRegistryUpdater_(FacilityKeyPairForIdentity {
            std::get<0>(input)
            , sharedOutgoingKey
            , sharedIncomingKey
        });
        DHHelperReply ret;
        ret.serverPub = dev::cd606::tm::basic::ByteData {std::string {server_pk, server_pk+crypto_kx_PUBLICKEYBYTES}};
        return ret;

        /*
        X25519Private x;
        std::array<unsigned char, 32> sharedKey;
        x.CreateSharedKey(
            reinterpret_cast<const unsigned char *>(std::get<1>(input).clientPub.content.c_str())
            , sharedKey.data()
            , 32
        );
        localRegistryUpdater_(std::get<0>(input), sharedKey);
        DHHelperReply ret;
        const unsigned char *pub = x.GetPublicKey(nullptr);
        ret.serverPub = dev::cd606::tm::basic::ByteData { std::string {pub, pub+32} };
        return ret;
        */
    }
};

DHServerHelper::DHServerHelper(std::function<void(FacilityKeyPairForIdentity const &)> localRegistryUpdater)
    : impl_(std::make_unique<DHServerHelperImpl>(localRegistryUpdater)) {}
DHServerHelper::~DHServerHelper() {}
    
DHHelperReply DHServerHelper::process(std::tuple<std::string, DHHelperCommand> &&input) {
    return impl_->process(std::move(input));
}

class DHClientHelperImpl {
private:
    std::function<void(FacilityKeyPair const &)> localKeyUpdater_;
    unsigned char pk_[crypto_kx_PUBLICKEYBYTES];
    unsigned char sk_[crypto_kx_SECRETKEYBYTES];
    //X25519Private x_;
public:
    DHClientHelperImpl(std::function<void(FacilityKeyPair const &)> localKeyUpdater)
        : localKeyUpdater_(localKeyUpdater), pk_(), sk_()/*, x_()*/ 
    {
        crypto_kx_keypair(pk_, sk_);
    }
    ~DHClientHelperImpl() {}
    void reset() {
        //x_ = X25519Private {};
        crypto_kx_keypair(pk_, sk_);
    }
    DHHelperCommand buildCommand() {
        /*
        const unsigned char *pub = x_.GetPublicKey(nullptr);
        return DHHelperCommand {
            dev::cd606::tm::basic::ByteData { std::string {pub, pub+32} }
        };
        */
        return DHHelperCommand {
            dev::cd606::tm::basic::ByteData { std::string {pk_, pk_+crypto_kx_PUBLICKEYBYTES} }
        };
    }
    void process(DHHelperReply const &input) {
        /*
        std::array<unsigned char, 32> sharedKey;
        x_.CreateSharedKey(
            reinterpret_cast<const unsigned char *>(input.serverPub.content.c_str())
            , sharedKey.data()
            , 32
        );
        localKeyUpdater_(sharedKey);
        */
        unsigned char client_rx[crypto_kx_SESSIONKEYBYTES];
        unsigned char client_tx[crypto_kx_SESSIONKEYBYTES];
        if (crypto_kx_client_session_keys(
            client_rx, client_tx, pk_, sk_
            , reinterpret_cast<const unsigned char *>(input.serverPub.content.c_str())
        ) != 0) {
            return;
        }
        std::array<unsigned char, 32> sharedOutgoingKey, sharedIncomingKey;
        std::memcpy(sharedOutgoingKey.data(), client_tx, 32);
        std::memcpy(sharedIncomingKey.data(), client_rx, 32);
        localKeyUpdater_(FacilityKeyPair {
            sharedOutgoingKey
            , sharedIncomingKey
        });
    }
    std::function<void(FacilityKeyPair const &)> localKeyUpdater() const {
        return localKeyUpdater_;
    }
};

DHClientHelper::DHClientHelper(std::function<void(FacilityKeyPair const &)> localKeyUpdater)
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