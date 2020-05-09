#include "FacilityLogic.hpp"

using namespace dev::cd606::tm::infra;
using namespace std::chrono;

namespace dev { namespace cd606 { namespace tm { namespace clock_logic_test_app {

    class FacilityLogicImpl {
    public:
        std::string result(std::string const &queryKey, std::string const &dataInput) {
            return "Reply to '"+queryKey+"' is '"+dataInput+"'";
        }
    };

    FacilityLogic::FacilityLogic() : impl_(std::make_unique<FacilityLogicImpl>()) {}
    FacilityLogic::~FacilityLogic() {}
    std::string FacilityLogic::result(std::string const &queryKey, std::string const &dataInput) {
        return impl_->result(queryKey, dataInput);
    }

} } } }