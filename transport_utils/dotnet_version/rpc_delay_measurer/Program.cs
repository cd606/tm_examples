using System;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using PeterO.Cbor;
using Microsoft.Extensions.CommandLineUtils;

namespace rpc_delay_measurer
{
    [CborWithoutFieldNames]
    class FacilityInput 
    {
        public Int64 timestamp {get; set;}
        public Int32 data {get; set;}
    }
    [CborWithoutFieldNames]
    class FacilityOutput
    {
        public Int32 data {get; set;}
    }
    enum Mode 
    {
        Server 
        , Client
    }
    class Program
    {
        static void runServer(string serviceDescriptor, string heartbeatDescriptor, string heartbeatTopic, string heartbeatIdentity)
        {
            var env = new ClockEnv();
            var r = new Runner<ClockEnv>(env);
            var facility = RealTimeAppUtils<ClockEnv>.liftPureOnOrderFacility(
                (FacilityInput x) => new FacilityOutput() {data = x.data*2}
                , false
            );
            MultiTransportFacility<ClockEnv>.WrapOnOrderFacility(
                r 
                , facility 
                , (bytes) => CborDecoder<FacilityInput>.Decode(CBORObject.DecodeFromBytes(bytes))
                , (output) => CborEncoder<FacilityOutput>.Encode(output).EncodeToBytes()
                , serviceDescriptor
            );
            if (heartbeatDescriptor != null)
            {
                var heartbeatPublisher = new HeartbeatPublisher(
                    heartbeatDescriptor, heartbeatTopic, heartbeatIdentity, TimeSpan.FromSeconds(2)
                );
                heartbeatPublisher.RegisterFacility("facility", serviceDescriptor);
                heartbeatPublisher.AddToRunner(r);
            }
            r.finalize();
            RealTimeAppUtils<ClockEnv>.runForever(env);
        }
        static void runClient(string serviceDescriptor, string heartbeatDescriptor, string heartbeatTopic, string heartbeatIdentity, int repeatTimes)
        {
            var env = new ClockEnv();
            var r = new Runner<ClockEnv>(env);
            Int64 firstTimeStamp = 0;
            Int64 count = 0;
            Int64 recvCount = 0;
            int importerCount = 0;
            var delayedImporter = RealTimeAppUtils<ClockEnv>.delayedImporter<string, int>(
                RealTimeAppUtils<ClockEnv>.uniformSimpleImporter<int>(
                    (env) => {
                        ++importerCount;
                        return (
                            importerCount < repeatTimes
                            , new TimedDataWithEnvironment<ClockEnv, int>(
                                env
                                , new WithTime<int>(
                                    env.now()
                                    , importerCount 
                                    , (importerCount>=repeatTimes)
                                )
                            )
                        );
                    }
                )
            );
            var keyify = RealTimeAppUtils<ClockEnv>.kleisli<int,Key<FacilityInput>>(
                (TimedDataWithEnvironment<ClockEnv,int> input) => new TimedDataWithEnvironment<ClockEnv, Key<FacilityInput>>(
                    input.environment 
                    , new WithTime<Key<FacilityInput>>(
                        input.timedData.timePoint
                        , new Key<FacilityInput>(
                            new FacilityInput() {timestamp = input.timedData.timePoint, data = input.timedData.value}
                        )
                        , input.timedData.finalFlag
                    )
                )
                , false
            );
            var exporter = RealTimeAppUtils<ClockEnv>.simpleExporter(
                (TimedDataWithEnvironment<ClockEnv,KeyedData<FacilityInput,FacilityOutput>> data) => {
                    var now = data.timedData.timePoint;
                    count += now-data.timedData.value.key.key.timestamp;
                    ++recvCount;
                    if (data.timedData.value.key.key.data == 1) {
                        firstTimeStamp = data.timedData.value.key.key.timestamp;
                    }
                    if (data.timedData.value.key.key.data >= repeatTimes)
                    {
                        env.log(LogLevel.Info, $"average delay of {recvCount} calls is {count*1.0/recvCount} milliseconds");
                        env.log(LogLevel.Info, $"total time for {recvCount} calls is {now-firstTimeStamp} milliseconds");
                        env.exit();
                    }
                }
                , false
            );
            
            if (serviceDescriptor != null) 
            {
                var facility = MultiTransportFacility<ClockEnv>.CreateFacility<FacilityInput,FacilityOutput>(
                    (x) => CborEncoder<FacilityInput>.Encode(x).EncodeToBytes()
                    , (b) => CborDecoder<FacilityOutput>.Decode(CBORObject.DecodeFromBytes(b))
                    , serviceDescriptor
                );
                var importer = RealTimeAppUtils<ClockEnv>.constFirstPushImporter<string>("");
                r.placeOrderWithFacility(
                    r.execute(keyify, r.execute(delayedImporter, r.importItem(importer)))
                    , facility 
                    , r.exporterAsSink(exporter)
                );
            }
            else 
            {
                var dynamicFacility = MultiTransportFacility<ClockEnv>.CreateDynamicFacility<FacilityInput,FacilityOutput>(
                    (x) => CborEncoder<FacilityInput>.Encode(x).EncodeToBytes()
                    , (b) => CborDecoder<FacilityOutput>.Decode(CBORObject.DecodeFromBytes(b))
                );
                var importer = MultiTransportImporter<ClockEnv>.CreateTypedImporter<Heartbeat>(
                    (b) => CborDecoder<Heartbeat>.Decode(CBORObject.DecodeFromBytes(b))
                    , heartbeatDescriptor
                    , heartbeatTopic
                );
                var parser = RealTimeAppUtils<ClockEnv>.liftMaybe(
                    (TypedDataWithTopic<Heartbeat> h) => {
                        if (h.content.sender_description.Equals(heartbeatIdentity))
                        {
                            var s = h.content.facility_channels["facility"];
                            dynamicFacility.changeAddress(s);
                            return Here.Option<string>.Some(s);
                        }
                        else
                        {
                            return Here.Option<string>.None;
                        }
                    }
                    , false
                );
                r.placeOrderWithFacility(
                    r.execute(keyify, r.execute(delayedImporter, r.execute(parser, r.importItem(importer))))
                    , dynamicFacility
                    , r.exporterAsSink(exporter)
                );
            }
            r.finalize();
            RealTimeAppUtils<ClockEnv>.runForever(env);
        }
        static void Main(string[] args)
        {
            CommandLineApplication app = new CommandLineApplication(
                throwOnUnexpectedArg: true
            );
            CommandOption modeOption = app.Option(
                "-m|--mode <mode>"
                , "Mode of operation (server|client)"
                , CommandOptionType.SingleValue
            );
            CommandOption serviceDescriptorOption = app.Option(
                "-d|--serviceDescriptor <descriptor>"
                , "Service descriptor"
                , CommandOptionType.SingleValue
            );
            CommandOption heartbeatDescriptorOption = app.Option(
                "-H|--heartbeatDescriptor <descriptor>"
                , "Heartbeat descriptor"
                , CommandOptionType.SingleValue
            );
            CommandOption heartbeatTopicOption = app.Option(
                "-T|--heartbeatTopic <topic>"
                , "Heartbeat topic (default: \"tm.examples.heartbeats\")"
                , CommandOptionType.SingleValue
            );
            CommandOption heartbeatIdentityOption = app.Option(
                "-I|--heartbeatIdentity <identity>"
                , "Heartbeat identity (default: \"rpc_delay_measurer\")"
                , CommandOptionType.SingleValue
            );
            CommandOption repeatTimesOption = app.Option(
                "-r|--repeatTimes <identity>"
                , "repeat times (default: 1000)"
                , CommandOptionType.SingleValue
            );
            app.HelpOption("-? | -h | --help");
            app.OnExecute(() => {
                if (!modeOption.HasValue()) 
                {
                    Console.Error.WriteLine("Please provide mode");
                    return 1;
                }
                var modeStr = modeOption.Value();
                Mode mode = Mode.Server;
                if (modeStr.Equals("server")) 
                {
                    mode = Mode.Server;
                }
                else if (modeStr.Equals("client"))
                {
                    mode = Mode.Client;
                }
                else 
                {
                    Console.Error.WriteLine("Mode can only be server or client");
                    return 1;
                }
                if (!serviceDescriptorOption.HasValue() && mode == Mode.Server) 
                {
                    Console.Error.WriteLine("Please provide service descriptor for server");
                    return 1;
                }
                string serviceDescriptor = null;
                if (serviceDescriptorOption.HasValue()) 
                {
                    serviceDescriptor = serviceDescriptorOption.Value();
                }
                string heartbeatDescriptor = null;
                if (heartbeatDescriptorOption.HasValue())
                {
                    heartbeatDescriptor = heartbeatDescriptorOption.Value();
                }
                if (serviceDescriptor == null && heartbeatDescriptor == null)
                {
                    Console.Error.WriteLine("Please provice either service descriptor or heartbeat descriptor for client");
                    return 1;
                }
                string heartbeatTopic = "tm.examples.heartbeats";
                if (heartbeatTopicOption.HasValue())
                {
                    heartbeatTopic = heartbeatTopicOption.Value();
                }
                string heartbeatIdentity = "rpc_delay_measurer";
                if (heartbeatIdentityOption.HasValue())
                {
                    heartbeatIdentity = heartbeatIdentityOption.Value();
                }
                int repeatTimes = 1000;
                if (repeatTimesOption.HasValue())
                {
                    try 
                    {
                        repeatTimes = int.Parse(repeatTimesOption.Value());
                    }
                    catch (FormatException)
                    {
                        Console.Error.WriteLine("Bad repeat times format");
                        return 1;
                    }
                }
                if (mode == Mode.Server)
                {
                    runServer(serviceDescriptor, heartbeatDescriptor, heartbeatTopic, heartbeatIdentity);
                }
                else 
                {
                    runClient(serviceDescriptor, heartbeatDescriptor, heartbeatTopic, heartbeatIdentity, repeatTimes);
                }
                return 0;
            }); 
            app.Execute(args);
        }
    }
}
