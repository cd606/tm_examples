using System;
using System.Text;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using Microsoft.Extensions.CommandLineUtils;

namespace listener
{
    enum PrintMode
    {
        Length
        , String
        , Cbor
        , None
    }
    class Program
    {
        void run(string address, string topic, PrintMode printMode, int summaryPeriod, string captureFile)
        {
            var env = new ClockEnv();
            var r = new Runner<ClockEnv>(env);
            
            var importer = MultiTransportImporter<ClockEnv>.CreateImporter(
                address, topic
            );
            var exporter = RealTimeAppUtils<ClockEnv>.pureExporter<ByteDataWithTopic>(
                (x) => {
                    switch (printMode)
                    {
                        case PrintMode.Length:
                            env.log(LogLevel.Info, $"Topic {x.topic}: {x.content.Length} bytes");
                            break;
                        case PrintMode.String:
                            env.log(LogLevel.Info, $"Topic {x.topic}: {Encoding.x.content}");
                            break;
                    }
                    
                }
                , false
            );
            r.exportItem(exporter, r.importItem(importer));
            r.finalize();
            RealTimeAppUtils<ClockEnv>.runForever(env);
        }
        static void Main(string[] args)
        {
            CommandLineApplication app = new CommandLineApplication(
                throwOnUnexpectedArg: true
            );
            CommandOption addressOption = app.Option(
                "-a|--address <address>"
                , "Address to listen to (PROTOCOL://LOCATOR)"
                , CommandOptionType.SingleValue
            );
            CommandOption topicOption = app.Option(
                "-t|--topic <topic>"
                , "Topic to listen to (default : \"\" = listen to all topics)"
                , CommandOptionType.SingleValue
            );
            CommandOption printModeOption = app.Option(
                "-m|--printMode <mode>"
                , "How to print the data (mode values: length|string|cbor|none|bytes, default: length)"
                , CommandOptionType.SingleValue
            );
            CommandOption summaryPeriodOption = app.Option(
                "-s|--summaryPeriod <number-of-seconds>"
                , "How often to print summary (default: 0 = don't print summary)"
                , CommandOptionType.SingleValue
            );
            CommandOption captureFileOption = app.Option(
                "-f|--captureFile <file-name>"
                , "The file to capture data into (default: \"\" = don't capture)"
                , CommandOptionType.SingleValue
            );
            app.HelpOption("-? | -h | --help");
            app.OnExecute(() => {
                if (!addressOption.HasValue())
                {
                    Console.Error.WriteLine("Please provide address");
                    return 1;
                }
                var addr = addressOption.Value();
                string topic = "";
                if (topicOption.HasValue())
                {
                    topic = topicOption.Value();
                }
                var printMode = PrintMode.Length;
                switch (printModeOption.Value())
                {
                    case "length":
                        printMode = PrintMode.Length;
                        break;
                    case "string":
                        printMode = PrintMode.String;
                        break;
                    case "cbor":
                        printMode = PrintMode.Cbor;
                        break;
                    case "none":
                        printMode = PrintMode.None;
                        break;
                    default:
                        Console.Error.WriteLine("Unknown print mode");
                        return 1;
                }
                var summaryPeriod = 1;
                if (summaryPeriodOption.HasValue())
                {
                    summaryPeriod = int.Parse(summaryPeriodOption.Value());
                }
                var captureFile = "";
                if (captureFileOption.HasValue())
                {
                    captureFile = captureFileOption.Value();
                }
                new Program().run(addr, topic, printMode, summaryPeriod, captureFile);
                return 0;
            });
            app.Execute(args);
        }
    }
}
