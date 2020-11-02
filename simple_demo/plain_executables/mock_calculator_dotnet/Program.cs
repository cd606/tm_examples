using System;
using System.IO;
using System.Net;
using System.Diagnostics;
using System.Threading;
using System.Collections.Generic;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using ProtoBuf;

namespace mock_calculator_dotnet
{
    [ProtoContract]
    class CalculateCommand
    {
        [ProtoMember(1)]
        public int id {get; set;}
        [ProtoMember(2)]
        public double value {get; set;}
    }
    [ProtoContract]
    class CalculateResult
    {
        [ProtoMember(1)]
        public int id {get; set;}
        [ProtoMember(2)]
        public double value {get; set;}
    }
    class CalculateFacility : AbstractOnOrderFacility<ClockEnv, (string, CalculateCommand), CalculateResult>
    {
        public override void start(ClockEnv env)
        {
        }
        public override void handle(TimedDataWithEnvironment<ClockEnv, Key<(string, CalculateCommand)>> data)
        {
            var thisID = data.timedData.value.id;
            var thisEnv = data.environment;
            var cmd = data.timedData.value.key.Item2;
            publish(new TimedDataWithEnvironment<ClockEnv, Key<CalculateResult>>(
                thisEnv
                , new WithTime<Key<CalculateResult>>(
                    thisEnv.now()
                    , new Key<CalculateResult>(
                        thisID
                        , new CalculateResult() {id = cmd.id, value=2.0*cmd.value}
                    )
                    , false
                )
            ));
            new Thread(() => {
                Thread.Sleep(2000);
                publish(new TimedDataWithEnvironment<ClockEnv, Key<CalculateResult>>(
                    thisEnv
                    , new WithTime<Key<CalculateResult>>(
                        thisEnv.now()
                        , new Key<CalculateResult>(
                            thisID
                            , new CalculateResult() {id = cmd.id, value=-1.0}
                        )
                        , true
                    )
                ));
            }).Start();
        }
    }
    class Program
    {
        static void Main(string[] args)
        {
            var env = new ClockEnv();
            var r = new Runner<ClockEnv>(env);
            //var serviceAddr = "rabbitmq://127.0.0.1::guest:guest:test_queue";
            var serviceAddr = "redis://127.0.0.1:6379:::test_queue";
            var calculateFacility = new CalculateFacility();
            MultiTransportFacility<ClockEnv>.WrapOnOrderFacility<string,CalculateCommand,CalculateResult>(
                r : r 
                , facility : calculateFacility
                , decoder : (x) => Serializer.Deserialize<CalculateCommand>(new MemoryStream(x))
                , encoder : (x) => {
                    var s = new MemoryStream();
                    Serializer.Serialize<CalculateResult>(s, x);
                    return s.ToArray();
                }
                , address : serviceAddr
                , identityChecker : ServerSideIdentityChecker<string>.SimpleIdentityChecker()
            );
            var heartbeatAddr = "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]";
            var heartbeatPublisher = MultiTransportExporter<ClockEnv>.CreateTypedExporter<Heartbeat>(
                encoder : (h) => h.asCborObject().EncodeToBytes()
                , address : heartbeatAddr
            );
            var heartbeatID = Guid.NewGuid().ToString();
            var host = Dns.GetHostName();
            var pid = Process.GetCurrentProcess().Id;
            var broadcastChannels = new Dictionary<string, List<string>>();
            var facilityChannels = new Dictionary<string, string>();
            facilityChannels.Add("calculator facility", serviceAddr);
            var details = new Dictionary<string, (string, string)>();
            details.Add("program", ("Good", ""));
            var timerImporter = ClockImporter<ClockEnv>.createRecurringClockImporter<TypedDataWithTopic<Heartbeat>>(
                start : env.now()
                , end : env.now().AddDays(1)
                , periodMs : 1000
                , gen : (t) => {
                    return new TypedDataWithTopic<Heartbeat>(
                        "simple_demo.plain_executables.calculator.heartbeat"
                        , new Heartbeat() {
                            uuid_str = heartbeatID
                            , timestamp = env.now().ToUnixTimeMilliseconds()*1000
                            , host = host
                            , pid = pid 
                            , sender_description = "simple_demo plain Calculator"
                            , broadcast_channels = broadcastChannels
                            , facility_channels = facilityChannels
                            , details = details
                        }
                    );
                }
            );
            r.exportItem(heartbeatPublisher, r.importItem(timerImporter));
            r.finalize();
            RealTimeAppUtils<ClockEnv>.terminateAfterDuration(
                env, TimeSpan.FromDays(1)
            );
        }
    }
}
