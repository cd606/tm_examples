#include <tm_kit/basic/simple_shared_chain/ChainReader.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainWriter.hpp>

struct TransferRequest {
    std::string from, to;
    uint16_t amount;
};
struct State {
    uint32_t a, b;
    int32_t a_pending, b_pending;
    uint16_t pendingRequestCount;
};
struct Process {};

using ChainItem = std::variant<
    TransferRequest, Process
>;

class InMemoryChain {
public:
    using ItemType = ChainItem;
    bool fetchNext(ChainItem *current) {
        return false;
    }
    bool append(ChainItem *current, ChainItem &&toBeWritten) {
        return false;
    }
};

class StateFolder {
public:
    State initialize(void *) {
        return State {};
    } 
    State fold(State const &lastState, ChainItem const &newInfo) {
        return lastState;
    }
};

int main(int argc, char **argv) {
    return 0;
}