#include "DHHelper.hpp"

#include <sodium.h>

static_assert(crypto_kx_SESSIONKEYBYTES >= 32, "libsodium kx session key length is less than 32");

class DHServerHelperImpl {
private:
    ServerSideSignatureAndEncBasedIdentityCheckerComponentBase *checker_;
public:
    DHServerHelperImpl(ServerSideSignatureAndEncBasedIdentityCheckerComponentBase *checker)
        : checker_(checker) {}
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
        checker_->set_encdec_keys(FacilityKeyPairForIdentity {
            std::get<0>(input)
            , sharedOutgoingKey
            , sharedIncomingKey
        });
        DHHelperReply ret;
        ret.serverPub = dev::cd606::tm::basic::ByteData {std::string {server_pk, server_pk+crypto_kx_PUBLICKEYBYTES}};
        return ret;
    }
};

DHServerHelper::DHServerHelper(ServerSideSignatureAndEncBasedIdentityCheckerComponentBase *checker)
    : impl_(std::make_unique<DHServerHelperImpl>(checker)) {}
DHServerHelper::~DHServerHelper() {}
    
DHHelperReply DHServerHelper::process(std::tuple<std::string, DHHelperCommand> &&input) {
    return impl_->process(std::move(input));
}

class DHClientHelperImpl {
private:
    ClientSideSignatureAndEncBasedIdentityAttacherComponentBase *attacher_;
    unsigned char pk_[crypto_kx_PUBLICKEYBYTES];
    unsigned char sk_[crypto_kx_SECRETKEYBYTES];
    std::mutex mutex_;
public:
    DHClientHelperImpl(ClientSideSignatureAndEncBasedIdentityAttacherComponentBase *attacher)
        : attacher_(attacher), pk_(), sk_(), mutex_()
    {
    }
    ~DHClientHelperImpl() {}
    DHHelperCommand resetAndBuildCommand() {
        std::lock_guard<std::mutex> _(mutex_);
        crypto_kx_keypair(pk_, sk_);
        return DHHelperCommand {
            dev::cd606::tm::basic::ByteData { std::string {pk_, pk_+crypto_kx_PUBLICKEYBYTES} }
        };
    }
    void process(DHHelperReply const &input) {
        std::lock_guard<std::mutex> _(mutex_);
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
        attacher_->set_encdec_keys(FacilityKeyPair {
            sharedOutgoingKey
            , sharedIncomingKey
        });
    }
};

DHClientHelper::DHClientHelper(ClientSideSignatureAndEncBasedIdentityAttacherComponentBase *attacher)
    : impl_(std::make_unique<DHClientHelperImpl>(attacher)) {}
DHClientHelper::~DHClientHelper() {}
    
DHHelperCommand DHClientHelper::resetAndBuildCommand() {
    return impl_->resetAndBuildCommand();
}
void DHClientHelper::process(DHHelperReply const &input) {
    impl_->process(input);
}