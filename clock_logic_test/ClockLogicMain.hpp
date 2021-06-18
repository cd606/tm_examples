#ifndef CLOCK_LOGIC_TEST_CLOCK_LOGIC_MAIN_HPP_
#define CLOCK_LOGIC_TEST_CLOCK_LOGIC_MAIN_HPP_

#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/AppClockHelper.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>
#include "FacilityLogic.hpp"
#include <sstream>

namespace dev { namespace cd606 { namespace tm { namespace clock_logic_test_app {

    template <class R>
    void clockLogicMain(R &r, std::ostream &fileOutput) {
        using M = typename R::AppType;
        using ClockImporterExporter = typename basic::AppClockHelper<M>::Importer;
        using ClockOnOrderFacility = typename basic::AppClockHelper<M>::Facility;
        using TheEnvironment = typename R::EnvironmentType;
        using FileComponent = typename basic::template ByteDataWithTopicRecordFileImporterExporter<M>;

        using namespace dev::cd606::tm::infra;

        auto importer1 = ClockImporterExporter::template createRecurringClockImporter<std::string>(
            withtime_utils::parseLocalTime("2020-01-01T10:00:00.0121")
            , withtime_utils::parseLocalTime("2020-01-01T10:01:00.012")
            , std::chrono::seconds(5)
            , [](typename TheEnvironment::TimePointType const &tp) {
                return "RECURRING "+withtime_utils::localTimeString(tp);
            }
        );
        auto importer2 = ClockImporterExporter::template createRecurringClockImporter<std::string>(
            withtime_utils::parseLocalTime("2020-01-01T10:00:12")
            , withtime_utils::parseLocalTime("2020-01-01T10:00:28")
            , std::chrono::seconds(16)
            , [](typename TheEnvironment::TimePointType const &tp) {
                return "ONESHOT "+withtime_utils::localTimeString(tp);
            }
        );
        auto importer3 = ClockImporterExporter::template createOneShotClockImporter<std::string>(
            withtime_utils::parseLocalTime("2020-01-01T10:00:27")
            , [](typename TheEnvironment::TimePointType const &tp) {
                return "ONESHOT "+withtime_utils::localTimeString(tp);
            }
        );

        auto facility = M::localOnOrderFacility(new Facility<M>());
        r.registerLocalOnOrderFacility("facility", facility);

        auto addTopic = basic::SerializationActions<M>::template addConstTopic<std::string>("data.main");
        auto serialize = basic::SerializationActions<M>::template serialize<std::string>();
        
        auto fileSink = FileComponent::template createExporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
            fileOutput,
            {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67},
            {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
        );

        auto clockFacility = basic::AppRunnerUtilComponents<R>::template clockBasedFacility<std::string,std::string>(
            [](std::string &&) -> std::vector<std::chrono::system_clock::duration> {
                return {
                    std::chrono::milliseconds(150)
                    , std::chrono::seconds(1)
                    , std::chrono::milliseconds(5250)
                };
            }
            , [](typename TheEnvironment::TimePointType const &tp, typename TheEnvironment::DurationType const &, std::size_t thisIdx, std::size_t total, std::string &&inputStr) -> std::vector<std::string> {
                std::ostringstream oss;
                oss << std::string("CALLBACK (") << thisIdx << " out of " << total << ") " << withtime_utils::localTimeString(tp);
                return {oss.str()};
            }
            , "clockFacility"
        );

        infra::DeclarativeGraph<R>("", {
            {"recurring", importer1}
            , {"oneShot1", importer2}
            , {"oneShot2", importer3}
            , {"print", [](typename M::template InnerData<std::string> &&s) {
                std::ostringstream oss;
                oss << s.timedData;
                s.environment->log(LogLevel::Info, oss.str());
            } }
            , {"print2", [](typename M::template InnerData<typename M::template KeyedData<std::string,std::string>> &&s) {
                std::ostringstream oss;
                oss << s.timedData;
                s.environment->log(LogLevel::Info, oss.str());
            } }
            , {"converter", withtime_utils::keyify<std::string, typename M::EnvironmentType>}
            , {"clockFacilityOutput", [](typename M::template KeyedData<std::string,std::string> &&s2) -> std::string {
                return s2.key.key()+" --- "+s2.data;
            } }
            , {"genKey", [](std::string &&s) -> typename M::template Key<std::string> {
                return M::keyify(std::move(s));
            } }
            , {"fileSink", fileSink}
            , {"serialize", serialize}
            , {"addTopic", addTopic}
            , {"recurring", "addTopic"}
            , {"addTopic", "serialize"}
            , {"serialize", "fileSink"}
            , {"recurring", "print"}
            , {"oneShot1", "converter"}
            , {"converter", "facility", "print2"}
            , {"clockFacilityOutput", "print"}
            , {"recurring", "genKey"}
            , {"oneShot2", "facility"}
        })(r);
        clockFacility(
            r
            , r.template sourceByName<typename M::template Key<std::string>>("genKey")
            , r.template sinkByName<typename M::template KeyedData<std::string,std::string>>("clockFacilityOutput")
        );
    }   

} } } }

#endif