#ifndef DH_FACILITY_HPP_
#define DH_FACILITY_HPP_

#include <memory>
#include <array>
#include <functional>
#include <tm_kit/basic/ByteData.hpp>

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
struct DHHelperRestarted {
    void SerializeToString(std::string *s) const {
        *s = "";
    }
    bool ParseFromString(std::string const &s) {
        return true;
    }
};

class DHServerHelperImpl;

class DHServerHelper {
private:
    std::unique_ptr<DHServerHelperImpl> impl_;
public:
    DHServerHelper(std::function<void(std::string const &, std::array<unsigned char, 16> const &)> localRegistryUpdater);
    ~DHServerHelper();
    
    DHHelperReply process(std::tuple<std::string, DHHelperCommand> &&input);
};

class DHClientHelperImpl;

class DHClientHelper {
private:
    std::unique_ptr<DHClientHelperImpl> impl_;
public:
    DHClientHelper(std::function<void(std::array<unsigned char, 16> const &)> localKeyUpdater);
    ~DHClientHelper();
    
    void reset();
    DHHelperCommand buildCommand();
    void process(DHHelperReply const &input);
};

#endif