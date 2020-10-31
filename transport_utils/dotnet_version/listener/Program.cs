using System;
using System.Text;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using Microsoft.Extensions.CommandLineUtils;
using PeterO.Cbor;

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
            var count = 0;
            var exporter = RealTimeAppUtils<ClockEnv>.pureExporter<ByteDataWithTopic>(
                (x) => {
                    ++count;
                    switch (printMode)
                    {
                        case PrintMode.Length:
                            env.log(LogLevel.Info, $"Topic {x.topic}: {x.content.Length} bytes");
                            break;
                        case PrintMode.String:
                            env.log(LogLevel.Info, $"Topic {x.topic}: {Encoding.UTF32.GetString(x.content)}");
                            break;
                        case PrintMode.Cbor:
                            env.log(LogLevel.Info, $"Topic {x.topic}: {CBORObject.DecodeFromBytes(x.content)}");
                            break;
                        case PrintMode.None:
                        default:
                            break;
                    }
                }
                , false
            );
            r.exportItem(exporter, r.importItem(importer));
            if (!captureFile.Equals(""))
            {
                var fileExporter = FileUtils<ClockEnv>.byteDataWithTopicOutput(
                    captureFile
                    , filePrefix : new ByteData(new byte[] {0x01, 0x23, 0x45, 0x67})
                    , recordPrefix : new ByteData(new byte[] {0x76, 0x54, 0x32, 0x10})
                );
                r.exportItem(fileExporter, r.importItem(importer));
            }
            if (summaryPeriod != 0)
            {
                var now = env.now();
                var timerImporter = ClockImporter<ClockEnv>.createRecurringClockConstImporter<int>(
                    now
                    , now.AddDays(1)
                    , summaryPeriod*1000
                    , 0
                );
                var summaryExporter = RealTimeAppUtils<ClockEnv>.pureExporter<int>(
                    (x) => {
                        env.log(LogLevel.Info, $"Read {count} messages so far");
                    }
                    , false
                );
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
