#ifndef DH_FACILITY_HPP_
#define DH_FACILITY_HPP_

#include <memory>
#include <array>
#include <functional>
#include <tm_kit/basic/ByteData.hpp>

#include "simple_demo/security_logic/SignatureAndEncBasedIdentityCheckerComponent.hpp"

struct DHHelperCommand {
    dev::cd606::tm::basic::ByteData clientPub;
    void SerializeToString(std::string *s) const {
        *s = clientPub.content;
    }
    bool ParseFromString(std::string const &s) {
        clientPub = {s};
        return true;
    }
};
struct DHHelperReply {
    dev::cd606::tm::basic::ByteData serverPub;
    void SerializeToString(std::string *s) const {
        *s = serverPub.content;
    }
    bool ParseFromString(std::string const &s) {
        serverPub = {s};
        return true;
    }
};

class DHServerHelperImpl;

class DHServerHelper {
private:
    std::unique_ptr<DHServerHelperImpl> impl_;
public:
    DHServerHelper(ServerSideSignatureAndEncBasedIdentityCheckerComponentBase *);
    ~DHServerHelper();
    
    DHHelperReply process(std::tuple<std::string, DHHelperCommand> &&input);
};

class DHClientHelperImpl;

class DHClientHelper {
private:
    std::unique_ptr<DHClientHelperImpl> impl_;
public:
    DHClientHelper(ClientSideSignatureAndEncBasedIdentityAttacherComponentBase *);
    ~DHClientHelper();
    
    DHHelperCommand resetAndBuildCommand();
    void process(DHHelperReply const &input);
};

#endif