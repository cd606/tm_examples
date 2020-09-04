#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainReader.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainWriter.hpp>
#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>

using namespace dev::cd606::tm;

#define TransferRequestFields \
    ((std::string, from)) \
    ((std::string, to)) \
    ((uint16_t, amount))

TM_BASIC_CBOR_CAPABLE_STRUCT(TransferRequest, TransferRequestFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(TransferRequest, TransferRequestFields);

using Process = basic::ConstType<1>;

using ChainItemData = std::variant<TransferRequest, Process>;
using ChainItemID = std::array<unsigned char, 16>;

struct State {
    uint32_t a, b;
    int32_t a_pending, b_pending;
    uint16_t pendingRequestCount;
};

using ChainItem = basic::CBOR<std::variant<
    TransferRequest, Process
>>;

class InMemoryChain {
public:
    struct Item {
        ChainItemData data;
        std::atomic<Item *> next;

        Item() : data(), next(nullptr) {}
        Item(Item const &x) : data(x.data), next(x.next.load()) {}
        Item &operator=(Item const &x) {
            if (this == &x) {
                return *this;
            }
            data = x.data;
            next.store(x.next.load());
            return *this;
        }
    };
    using ItemType = Item;
private:
    Item start_;
public:
    InMemoryChain() : start_() {}
    Item head(void *) {
        return start_;
    }
    bool fetchNext(Item *current) {
        if (!current) {
            return false;
        }
        auto *p = current->next.load(std::memory_order_acquire);
        if (p) {
            *current = *p;
            return true;
        } else {
            return false;
        }
    }
    bool append(Item *current, Item *toBeWritten) {
        if (!current) {
            return false;
        }
        Item *oldVal = nullptr;
        return current->next.compare_exchange_strong(oldVal, toBeWritten, std::memory_order_release, std::memory_order_acquire);
    }
};

class StateFolder {
public:
    using ResultType = State;
    State initialize(void *) {
        return State {};
    } 
    State fold(State const &lastState, InMemoryChain::Item const &newInfo) {
        return lastState;
    }
};

class RequestHandler {
public:
    using ResponseType = bool;
    using InputType = int;
    void initialize(void *) {

    }
    InMemoryChain::Item *handleInput(InputType &&, State const &, bool *) {
        return nullptr;
    }
};

void histRun() {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>>,
        transport::CrossGuidComponent
    >;
    using A = infra::SinglePassIterationApp<TheEnvironment>;

    TheEnvironment env;
    InMemoryChain chain;

    basic::simple_shared_chain::ChainWriter<A, InMemoryChain, StateFolder, RequestHandler> w(&chain);
    basic::simple_shared_chain::ChainReader<A, InMemoryChain, StateFolder> r(&env, &chain);
}

int main(int argc, char **argv) {
    histRun();
    return 0;
}