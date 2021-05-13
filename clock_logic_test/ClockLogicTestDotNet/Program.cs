using System;
using System.Collections.Generic;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;

namespace ClockLogicTestDotNet
{
    class Facility<Env> : AbstractOnOrderFacility<Env,string,string> where Env : EnvBase
    {
        private string logic(string queryKey, string dataInput)
        {
            return $"Reply to '{queryKey}' is '{dataInput}'";
        }
        private string outsideInput = null;
        private List<Key<string>> queryInput = new List<Key<string>>();
        public Facility() {}
        public override void start(Env env)
        {
        }
        public void setOutsideInput(TimedDataWithEnvironment<Env,string> s)
        {
            outsideInput = s.timedData.value;
            foreach (var q in queryInput)
            {
                publish(new TimedDataWithEnvironment<Env, Key<string>>(
                    s.environment
                    , new WithTime<Key<string>>(
                        s.timedData.timePoint
                        , new Key<string>(
                            q.id
                            , logic(q.key, s.timedData.value)
                        )
                        , true
                    )
                ));
            }
            queryInput.Clear();
        }
        public override void handle(TimedDataWithEnvironment<Env, Key<string>> data)
        {
            if (outsideInput == null)
            {
                queryInput.Add(data.timedData.value);
            }
            else
            {
                publish(new TimedDataWithEnvironment<Env, Key<string>>(
                    data.environment
                    , new WithTime<Key<string>>(
                        data.timedData.timePoint
                        , new Key<string>(
                            data.timedData.value.id
                            , logic(data.timedData.value.key, outsideInput)
                        )
                        , true
                    )
                ));
            }
        }
    }
    class Program
    {
        static void Main(string[] args)
        {
            var env = new ClockEnv(
                ClockEnv.clockSettingsWithStartPointCorrespondingToNextAlignment(
                    1 
                    , new DateTimeOffset(new DateTime(2020,1,1,10,0,0))
                    , 2.0
                )
            );
            var r = new Runner<ClockEnv>(env);
            var importer1 = ClockImporter<ClockEnv>.createRecurringClockImporter<string>(
                new DateTimeOffset(new DateTime(2020,1,1,10,0,0,121))
                , new DateTimeOffset(new DateTime(2020,1,1,10,1,0,12))
                , 5000
                , (DateTimeOffset d) => $"RECURRING {env.formatTime(d)}"
            );
            var importer2 = ClockImporter<ClockEnv>.createRecurringClockImporter<string>(
                new DateTimeOffset(new DateTime(2020,1,1,10,0,12))
                , new DateTimeOffset(new DateTime(2020,1,1,10,0,28))
                , 16000
                , (DateTimeOffset d) => $"RECURRING 2 {env.formatTime(d)}"
            );
            var importer3 = ClockImporter<ClockEnv>.createOneShotClockImporter<string>(
                new DateTimeOffset(new DateTime(2020,1,1,10,0,27))
                , (DateTimeOffset d) => $"ONESHOT {env.formatTime(d)}"
            );
            var converter = RealTimeAppUtils<ClockEnv>.liftPure(
                (string s) => InfraUtils.keyify(s)
                , false
            );
            var facility = new Facility<ClockEnv>();
            var exporter = RealTimeAppUtils<ClockEnv>.pureExporter(
                (string s) => {
                    env.log(LogLevel.Info, s);
                }
                , false
            );
            var exporter2 = RealTimeAppUtils<ClockEnv>.pureExporter(
                (KeyedData<string,string> s) => {
                    env.log(LogLevel.Info, $"id={s.key.id}, input={s.key.key}, output={s.data}");
                }
                , false
            );
            var clockFacility = ClockOnOrderFacility<ClockEnv>.createClockCallback<string,string>(
                (DateTimeOffset d, int thisIdx, int total) => 
                    $"CALLBACK ({thisIdx} out of {total}) {env.formatTime(d)}"
            );
            var clockFacilityInput = RealTimeAppUtils<ClockEnv>.liftPure(
                (string s) => InfraUtils.keyify(new ClockOnOrderFacilityInput<string>(
                    s, new List<TimeSpan> {TimeSpan.FromMilliseconds(150), TimeSpan.FromMilliseconds(1000), TimeSpan.FromMilliseconds(5250)}
                ))
                , false
            );
            var clockFacilityOutput = RealTimeAppUtils<ClockEnv>.liftPure(
                (KeyedData<ClockOnOrderFacilityInput<string>,string> d) => 
                    $"{d.key.key.inputData} --- {d.data}"
                , false
            );
            var feedItemToFacility = RealTimeAppUtils<ClockEnv>.simpleExporter(
                (TimedDataWithEnvironment<ClockEnv, string> s) => {
                    facility.setOutsideInput(s);
                }
                , false
            );

            r.exportItem(exporter, r.importItem(importer1));
            r.placeOrderWithFacility(
                r.execute(converter, r.importItem(importer2))
                , facility
                , r.exporterAsSink(exporter2)
            );
            r.placeOrderWithFacility(
                r.execute(clockFacilityInput, r.importItem(importer1))
                , clockFacility
                , r.actionAsSink(clockFacilityOutput)
            );
            r.exportItem(feedItemToFacility, r.importItem(importer3));
            r.exportItem(exporter, r.actionAsSource(clockFacilityOutput));

            var addTopicAndSerialize = RealTimeAppUtils<ClockEnv>.liftPure(
                (string s) => new ByteDataWithTopic(
                    "main.data"
                    , System.Text.Encoding.UTF8.GetBytes(s)
                )
                , false
            );
            var fileSink = FileUtils<ClockEnv>.byteDataWithTopicOutput(
                (args.Length>0? args[0] : "clockData.dat")
                , new ByteData(
                    new byte[] {0x01, 0x23, 0x45, 0x67}
                )
                , new ByteData(
                    new byte[] {0x76, 0x54, 0x32, 0x10}
                )
            );
            r.exportItem(fileSink, r.execute(addTopicAndSerialize, r.importItem(importer1)));

            r.finalize();
            RealTimeAppUtils<ClockEnv>.terminateAtTimePoint(
                env
                , env.virtualToActual(new DateTimeOffset(new DateTime(2020,1,1,10,2,0)))
            );
        }
    }
}
