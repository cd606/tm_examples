using System;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using Microsoft.Extensions.CommandLineUtils;

namespace relayer
{    
    class Program
    {
        void run(string incomingAddress, string outgoingAddr, int summaryPeriod)
        {
            var env = new ClockEnv();
            var r = new Runner<ClockEnv>(env);
            
            var importer = MultiTransportImporter<ClockEnv>.CreateImporter(
                incomingAddress, ""
            );
            var exporter = MultiTransportExporter<ClockEnv>.CreateExporter(
                outgoingAddr
            );
            r.exportItem(exporter, r.importItem(importer));
            if (summaryPeriod != 0)
            {
                var count = 0;
                var countingExporter = RealTimeAppUtils<ClockEnv>.pureExporter<ByteDataWithTopic>(
                    (x) => {
                        ++count;
                    }
                    , false
                );
                var now = env.now();
                var timerImporter = ClockImporter<ClockEnv>.createRecurringClockConstImporter<int>(
                    now
                    , now.AddDays(1)
                    , summaryPeriod*1000
                    , 0
                );
                var summaryExporter = RealTimeAppUtils<ClockEnv>.pureExporter<int>(
                    (x) => {
                        env.log(LogLevel.Info, $"Relayed {count} messages so far");
                    }
                    , false
                );
                r.exportItem(countingExporter, r.importItem(importer));
                r.exportItem(summaryExporter, r.importItem(timerImporter));
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
            CommandOption incomingAddressOption = app.Option(
                "-i|--incomingAddress <address>"
                , "Address to listen to (PROTOCOL://LOCATOR)"
                , CommandOptionType.SingleValue
            );
            CommandOption outgoingAddressOption = app.Option(
                "-o|--outgoingAddress <address>"
                , "Address to publish on (PROTOCOL://LOCATOR)"
                , CommandOptionType.SingleValue
            );
            CommandOption summaryPeriodOption = app.Option(
                "-s|--summaryPeriod <number-of-seconds>"
                , "How often to print summary (default: 0 = don't print summary)"
                , CommandOptionType.SingleValue
            );
            app.HelpOption("-? | -h | --help");
            app.OnExecute(() => {
                if (!incomingAddressOption.HasValue())
                {
                    Console.Error.WriteLine("Please provide incoming address");
                    return 1;
                }
                if (!outgoingAddressOption.HasValue())
                {
                    Console.Error.WriteLine("Please provide outgoing address");
                    return 1;
                }
                var incomingAddr = incomingAddressOption.Value();
                var outgoingAddr = outgoingAddressOption.Value();
                var summaryPeriod = 1;
                if (summaryPeriodOption.HasValue())
                {
                    summaryPeriod = int.Parse(summaryPeriodOption.Value());
                }
                new Program().run(incomingAddr, outgoingAddr, summaryPeriod);
                return 0;
            });
            app.Execute(args);
        }
    }
}
