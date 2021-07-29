#ifndef RPC_SERVER_HPP_
#define RPC_SERVER_HPP_

#include <tm_kit/infra/GenericLift.hpp>
#include <unordered_map>

#include "RpcInterface.hpp"

namespace rpc_examples {
    template <class R>
    auto simpleFacility()
        -> typename R::template OnOrderFacilityPtr<Input,Output>
    {
        using namespace dev::cd606::tm;
        using GL = typename infra::GenericLift<typename R::AppType>;
        return GL::lift(infra::LiftAsFacility{}, [](Input &&input) -> Output {
            return {input.y+":"+std::to_string(input.x)};
        });
    }
    template <class R>
    auto clientStreamFacility()
        -> typename R::template OnOrderFacilityPtr<Input,Output>
    {
        using M = typename R::AppType;
        class Facility : public M::template AbstractOnOrderFacility<Input,Output> {
        private:
            struct PerIDData {
                std::string res;
                int remainingCount;
            };
            std::unordered_map<typename M::EnvironmentType::IDType, PerIDData> remainingInputs_;
        public:
            Facility() : remainingInputs_() {}
            void handle(typename M::template InnerData<
                typename M::template Key<Input>
            > &&input) {
                auto id = input.timedData.value.id();
                auto const &realInput = input.timedData.value.key();
                auto iter = remainingInputs_.find(id);
                if (iter == remainingInputs_.end()) {
                    iter = remainingInputs_.insert({
                        id, PerIDData {
                            realInput.y+":"+std::to_string(realInput.x)
                            , realInput.x-1
                        }
                    }).first;
                } else {
                    iter->second.res += ","+realInput.y+":"+std::to_string(realInput.x);
                    --iter->second.remainingCount;
                }
                if (iter->second.remainingCount <= 0) {
                    this->publish(
                        input.environment
                        , typename M::template Key<Output> {
                            id, {std::move(iter->second.res)}
                        }
                        , true //this is the last (and only) one output for this input
                    );
                    remainingInputs_.erase(iter);
                }
            }
        };
        return M::template fromAbstractOnOrderFacility(new Facility());
    }
    template <class R>
    auto serverStreamFacility()
        -> typename R::template OnOrderFacilityPtr<Input,Output>
    {
        using M = typename R::AppType;
        class Facility : public M::template AbstractOnOrderFacility<Input,Output> {
        public:
            Facility() {}
            void handle(typename M::template InnerData<
                typename M::template Key<Input>
            > &&input) {
                auto id = input.timedData.value.id();
                auto const &realInput = input.timedData.value.key();
                int resultCount = std::max(1,realInput.x);
                Output o {realInput.y+":"+std::to_string(realInput.x)};
                for (int ii=1; ii<=resultCount; ++ii) {
                    this->publish(
                        input.environment
                        , typename M::template Key<Output> {
                            id, o
                        }
                        , (ii == resultCount)
                    );
                }
            }
        };
        return M::template fromAbstractOnOrderFacility(new Facility());
    }
    template <class R>
    auto bothStreamFacility()
        -> typename R::template OnOrderFacilityPtr<Input,Output>
    {
        using M = typename R::AppType;
        class Facility : public M::template AbstractOnOrderFacility<Input,Output> {
        private:
            struct PerIDData {
                std::vector<Output> res;
                int remainingCount;
            };
            std::unordered_map<typename M::EnvironmentType::IDType, PerIDData> remainingInputs_;
        public:
            Facility() : remainingInputs_() {}
            void handle(typename M::template InnerData<
                typename M::template Key<Input>
            > &&input) {
                auto id = input.timedData.value.id();
                auto const &realInput = input.timedData.value.key();
                auto iter = remainingInputs_.find(id);
                if (iter == remainingInputs_.end()) {
                    iter = remainingInputs_.insert({
                        id, PerIDData {
                            {Output {realInput.y+":"+std::to_string(realInput.x)}}
                            , realInput.x-1
                        }
                    }).first;
                } else {
                    iter->second.res.push_back(Output {realInput.y+":"+std::to_string(realInput.x)});
                    --iter->second.remainingCount;
                }
                if (iter->second.remainingCount <= 0) {
                    int sz = iter->second.res.size();
                    for (int ii=0; ii<sz; ++ii) {
                        this->publish(
                            input.environment
                            , typename M::template Key<Output> {
                                id, std::move(iter->second.res[ii])
                            }
                            , (ii==sz-1)
                        );
                    }
                    remainingInputs_.erase(iter);
                }
            }
        };
        return M::template fromAbstractOnOrderFacility(new Facility());
    }
}

#endif