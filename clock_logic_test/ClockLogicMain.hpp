#ifndef CLOCK_LOGIC_TEST_CLOCK_LOGIC_MAIN_HPP_
#define CLOCK_LOGIC_TEST_CLOCK_LOGIC_MAIN_HPP_

#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include "FacilityLogic.hpp"
#include <sstream>

namespace dev { namespace cd606 { namespace tm { namespace clock_logic_test_app {

    template <class ClockImporterExporter, class ClockOnOrderFacility, class R>
    void clockLogicMain(R &r, std::ostream &fileOutput) {
        using M = typename R::MonadType;
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
        auto converter = M::template liftPure<std::string>(withtime_utils::keyify<std::string, typename M::EnvironmentType>);

        auto facility = M::localOnOrderFacility(new Facility<M>());

        auto addTopic = basic::SerializationActions<M>::template addConstTopic<std::string>("data.main");
        auto serialize = basic::SerializationActions<M>::template serialize<std::string>();
        
        auto fileSink = FileComponent::template createExporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
            fileOutput,
            {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67},
            {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
        );

        auto exporter = M::template simpleExporter<std::string>([](typename M::template InnerData<std::string> &&s) {
            std::ostringstream oss;
            oss << s;
            s.environment->log(LogLevel::Info, oss.str());
        });
        auto exporter2 = M::template simpleExporter<typename M::template KeyedData<std::string,std::string>>([](typename M::template InnerData<typename M::template KeyedData<std::string,std::string>> &&s) {
            std::ostringstream oss;
            oss << s;
            s.environment->log(LogLevel::Info, oss.str());
        });

        using ClockFacilityInput = typename ClockOnOrderFacility::template FacilityInput<std::string>;
        auto clockFacility = ClockOnOrderFacility::template createClockCallback<std::string, std::string>(
            [](typename TheEnvironment::TimePointType const &tp, std::size_t thisIdx, std::size_t total) -> std::string {
                std::ostringstream oss;
                oss << std::string("CALLBACK (") << thisIdx << " out of " << total << ") " << withtime_utils::localTimeString(tp);
                return oss.str();
            }
        );
        auto clockFacilityInput = M::template liftPure<std::string>(
            [](std::string &&s) -> typename M::template Key<ClockFacilityInput> {
                return withtime_utils::keyify<ClockFacilityInput, typename M::EnvironmentType>(ClockFacilityInput {
                    std::move(s)
                    , {
                        std::chrono::milliseconds(150)
                        , std::chrono::seconds(1)
                        , std::chrono::milliseconds(5250)
                    }
                });
            }
        );
        using ClockFacilityOutput = typename M::template KeyedData<ClockFacilityInput,std::string>;
        auto clockFacilityOutput = M::template liftPure<ClockFacilityOutput>(
            [](ClockFacilityOutput &&s2) -> std::string {
                return s2.key.key().inputData+" --- "+s2.data;
            }
        );

        r.registerImporter("recurring", importer1);
        r.registerImporter("oneShot1", importer2);
        r.registerImporter("oneShot2", importer3);
        r.registerExporter("print", exporter);
        r.registerExporter("print2", exporter2);
        r.registerAction("converter", converter);
        r.registerLocalOnOrderFacility("facility", facility);
        r.registerOnOrderFacility("clockFacility", clockFacility);
        r.registerAction("clockFacilityInput", clockFacilityInput);
        r.registerAction("clockFacilityOutput", clockFacilityOutput);

        r.exportItem(exporter, r.importItem(importer1));
        r.exportItem(
            "fileSink", fileSink
            , r.execute(
                "serialize", serialize
                , r.execute(
                    "addTopic", addTopic, r.importItem(importer1)
                )
            ));
        r.placeOrderWithLocalFacility(
            r.execute(converter, r.importItem(importer2))
            , facility
            , r.exporterAsSink(exporter2));
        r.feedItemToLocalFacility(facility, r.importItem(importer3));
        r.placeOrderWithFacility(
            r.execute(clockFacilityInput, r.importItem(importer1))
            , clockFacility
            , r.actionAsSink(clockFacilityOutput)
        );
        r.exportItem(exporter, r.actionAsSource(clockFacilityOutput));
    }   

} } } }

#endif