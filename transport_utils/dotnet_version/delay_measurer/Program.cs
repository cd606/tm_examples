using System;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using Microsoft.Extensions.CommandLineUtils;
using PeterO.Cbor;
using Here;

namespace delay_measurer
{    
    class Program
    {
        void runSender(string address, uint intervalMs, uint bytes, int summaryPeriod)
        {
            var env = new ClockEnv();
            var r = new Runner<ClockEnv>(env);
            var publisher = MultiTransportExporter<ClockEnv>.CreateTypedExporter<(int,long,byte[])>(
                encoder : (x) => CBORObject.NewArray()
                                    .Add(x.Item1)
                                    .Add(x.Item2)
                                    .Add(x.Item3)
                                    .EncodeToBytes()
                , address: address
            );
            var counter = 0;
            var dataToSend = new byte[bytes];
            var source = ClockImporter<ClockEnv>.createRecurringClockImporter<TypedDataWithTopic<(int,long,byte[])>>(
                start : env.now()
                , end : env.now().AddDays(1)
                , periodMs: intervalMs
                , gen : (currentTime) => new TypedDataWithTopic<(int, long, byte[])>(
                    topic : "test.data"
                    , content : (++counter, env.now().ToUnixTimeMilliseconds()*1000, dataToSend)
                )
            );
            r.exportItem(publisher, r.importItem(source));
            if (summaryPeriod > 0)
            {
                var summarySource = ClockImporter<ClockEnv>.createRecurringClockImporter<string>(
                    start : env.now()
                    , end : env.now().AddDays(1)
                    , periodMs: summaryPeriod*1000
                    , gen : (currentTime) => $"sent {counter} messages"
                );
                var summaryExporter = RealTimeAppUtils<ClockEnv>.pureExporter<string>(
                    (x) => {
                        env.log(LogLevel.Info, x);
                    }
                    , false
                );
                r.exportItem(summaryExporter, r.importItem(summarySource));
            }
            r.finalize();
            RealTimeAppUtils<ClockEnv>.terminateAfterDuration(
                env, TimeSpan.FromDays(1)
            );
        }
        struct Stats
        {
            public int count;
            public double totalDelay;
            public double totalDelaySq;
            public int minID;
            public int maxID;
            public long minDelay;
            public long maxDelay;
        }
        void runReceiver(string address, int summaryPeriod)
        {
            var env = new ClockEnv();
            var r = new Runner<ClockEnv>(env);
            var source = MultiTransportImporter<ClockEnv>.CreateTypedImporter<(int,long,byte[])>(
                decoder : (x) => {
                    var cborObj = CBORObject.DecodeFromBytes(x);
                    if (cborObj.Type != CBORType.Array || cborObj.Count != 3)
                    {
                        return Option.None;
                    }
                    return (cborObj[0].AsNumber().ToInt32Checked()
                        , cborObj[1].AsNumber().ToInt64Checked()
                        , cborObj[2].ToObject<byte[]>()
                    );
                }
                , address : address
                , topicStr : "test.data"
            );
            var stats = new Stats {
                count = 0
                , totalDelay = 0.0
                , totalDelaySq = 0.0
                , minID = 0
                , maxID = 0
                , minDelay = -1000000
                , maxDelay = 0
            };
            var statCalc = RealTimeAppUtils<ClockEnv>.pureExporter<TypedDataWithTopic<(int,long,byte[])>>(
                (x) => {
                    lock (this)
                    {
                        var now = env.now().ToUnixTimeMilliseconds()*1000;
                        var delay = now-x.content.Item2;
                        var id = x.content.Item1;
                        ++(stats.count);
                        stats.totalDelay += 1.0*delay;
                        stats.totalDelaySq += 1.0*delay*delay;
                        if (stats.minID == 0 || stats.minID > id)
                        {
                            stats.minID = id;
                        }
                        if (stats.maxID < id)
                        {
                            stats.maxID = id;
                        }
                        if (stats.minDelay > delay || stats.minDelay < -100000)
                        {
                            stats.minDelay = delay;
                        }
                        if (stats.maxDelay < delay)
                        {
                            stats.maxDelay = delay;
                        }
                    }
                }
                , false
            );
            r.exportItem(statCalc, r.importItem(source));
            if (summaryPeriod > 0)
            {
                var summarySource = ClockImporter<ClockEnv>.createRecurringClockConstImporter<VoidStruct>(
                    start : env.now()
                    , end : env.now().AddDays(1)
                    , periodMs: summaryPeriod*1000
                    , t : new VoidStruct()
                );
                var summaryExporter = RealTimeAppUtils<ClockEnv>.pureExporter<VoidStruct>(
                    (x) => {
                        lock (this)
                        {
                            var mean = 0.0;
                            var sd = 0.0;
                            var missed = 0;
                            if (stats.count > 0) 
                            {
                                mean = stats.totalDelay/stats.count;
                                missed = stats.maxID-stats.minID+1-stats.count;
                            }
                            if (stats.count > 1)
                            {
                                sd = Math.Sqrt((stats.totalDelaySq-mean*mean*stats.count)/(stats.count-1));
                            }
                            env.log(LogLevel.Info, $"Got {stats.count} messages, mean delay {mean} micros, std delay {sd} micros, missed {missed} messages, min delay {stats.minDelay} micros, max delay {stats.maxDelay} micros");
                        }
                    }
                    , false
                );
                r.exportItem(summaryExporter, r.importItem(summarySource));
            }
            r.finalize();
            RealTimeAppUtils<ClockEnv>.terminateAfterDuration(
                env, TimeSpan.FromDays(1)
            );
        }

        static void Main(string[] args)
        {
            CommandLineApplication app = new CommandLineApplication(
                throwOnUnexpectedArg: true
            );
            CommandOption modeOption = app.Option(
                "-m|--mode <mode>"
                , "Mode of operation (sender|receiver)"
                , CommandOptionType.SingleValue
            );
            CommandOption intervalOption = app.Option(
                "-i|--interval <interval>"
                , "Send interval milliseconds (sender mode only)"
                , CommandOptionType.SingleValue
            );
            CommandOption bytesOption = app.Option(
                "-b|--bytes <byte_count>"
                , "Send byte count (sender mode only)"
                , CommandOptionType.SingleValue
            );
            CommandOption addressOption = app.Option(
                "-a|--address <address>"
                , "Address to send to or listen on (PROTOCOL://LOCATOR)"
                , CommandOptionType.SingleValue
            );
            CommandOption summaryPeriodOption = app.Option(
                "-s|--summaryPeriod <number-of-seconds>"
                , "How often to print summary (default: 0 = don't print summary)"
                , CommandOptionType.SingleValue
            );
            app.HelpOption("-? | -h | --help");
            app.OnExecute(() => {
                if (!modeOption.HasValue())
                {
                    Console.Error.WriteLine("Please provide mode");
                    return 1;
                }
                if (!addressOption.HasValue())
                {
                    Console.Error.WriteLine("Please provide address");
                    return 1;
                }
                var address = addressOption.Value();
                var summaryPeriod = 1;
                if (summaryPeriodOption.HasValue())
                {
                    summaryPeriod = int.Parse(summaryPeriodOption.Value());
                }
                if (modeOption.Value().Equals("sender"))
                {
                    if (!intervalOption.HasValue()) 
                    {
                        Console.Error.WriteLine("Please provide interval for sender mode");
                        return 1;
                    }
                    if (!bytesOption.HasValue()) 
                    {
                        Console.Error.WriteLine("Please provide bytes for sender mode");
                        return 1;
                    }
                    var intervalMs = uint.Parse(intervalOption.Value());
                    var bytes = uint.Parse(bytesOption.Value());
                    new Program().runSender(address, intervalMs, bytes, summaryPeriod);
                }
                else if (modeOption.Value().Equals("receiver"))
                {
                    if (intervalOption.HasValue()) 
                    {
                        Console.Error.WriteLine("Interval option is meaningless for receiver mode");
                        return 1;
                    }
                    if (bytesOption.HasValue()) 
                    {
                        Console.Error.WriteLine("Bytes option is meaningless for receiver mode");
                        return 1;
                    }
                    new Program().runReceiver(address, summaryPeriod);
                }
                else
                {
                    Console.Error.WriteLine($"Unknown mode {modeOption.Value()}");
                    return 1;
                }
                return 0;
            });
            app.Execute(args);
        }
    }
}
