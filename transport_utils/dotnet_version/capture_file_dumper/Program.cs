using System;
using System.IO;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using PeterO.Cbor;
using Microsoft.Extensions.CommandLineUtils;

namespace capture_file_dumper
{
    class Program
    {
        static int run(string fileName, string printMode, RecordFileUtils.TopicCaptureFileRecordReaderOption option)
        {
            using (var f = new FileStream(fileName, FileMode.Open))
            using (var r = new BinaryReader(f))
            {
                foreach (var x in RecordFileUtils.GenericRecordDataSource<RecordFileUtils.TopicCaptureFileRecord>(
                    r
                    , new RecordFileUtils.TopicCaptureFileRecordReader(option)
                ))
                {
                    string dataPart;
                    switch (printMode)
                    {
                        case "string":
                            dataPart = System.Text.Encoding.UTF8.GetString(x.Data);
                            break;
                        case "cbor":
                            dataPart = $"{CBORObject.DecodeFromBytes(x.Data)}";
                            break;
                        case "none":
                            dataPart = "(not printed)";
                            break;
                        case "bytes":
                            dataPart = $"{x.Data}";
                            break;
                        case "length":
                        default:
                            dataPart = $"{x.Data.Length} bytes";
                            break;
                    }
                    Console.WriteLine($"[{x.TimeString}] Topic {x.Topic} : {dataPart} (isFinal : {x.IsFinal})");
                }
            }
            return 0;
        }
        static void Main(string[] args)
        {
            CommandLineApplication app = new CommandLineApplication(
                throwOnUnexpectedArg : true
            );
            var fileOption = app.Option(
                "-f|--file <file>"
                , "file to dump"
                , CommandOptionType.SingleValue
            );
            var printModeOption = app.Option(
                "-p|--printMode <printMode>"
                , "print mode (length|string|cbor|none|bytes"
                , CommandOptionType.SingleValue
            );
            var fileMagicLenOption = app.Option(
                "-F|--fileMagicLength <length>"
                , "file magic length (default: 4)"
                , CommandOptionType.SingleValue
            );
            var recordMagicLenOption = app.Option(
                "-R|--recordMagicLength <length>"
                , "record magic length (default: 4)"
                , CommandOptionType.SingleValue
            );
            var timeFieldLenOption = app.Option(
                "-T|--timeFieldLength <length>"
                , "time field length (default: 8)"
                , CommandOptionType.SingleValue
            );
            var topicLengthFieldLenOption = app.Option(
                "-O|--topicLengthFieldLength <length>"
                , "topic length field length (default: 4)"
                , CommandOptionType.SingleValue
            );
            var dataLengthFieldLenOption = app.Option(
                "-D|--dataLengthFieldLength <length>"
                , "data length field length (default: 4)"
                , CommandOptionType.SingleValue
            );
            var hasFinalFlagOption = app.Option(
                "-I|--hasFinalFlag <hasFlag>"
                , "has final flag (default: true)"
                , CommandOptionType.SingleValue
            );
            var timeUnitOption = app.Option(
                "-U|--timeUnit <timeUnit>"
                , "second|millisecond|microsecond (default: microsecond)"
                , CommandOptionType.SingleValue
            );
            app.HelpOption("-? | -h | --help");
            app.OnExecute(() => {
                if (!fileOption.HasValue())
                {
                    Console.Error.WriteLine("Please provide file name");
                    return 0;
                }
                var file = fileOption.Value();
                var printMode = printModeOption.HasValue() ? printModeOption.Value() : "length";
                var option = new RecordFileUtils.TopicCaptureFileRecordReaderOption();
                if (fileMagicLenOption.HasValue())
                {
                    option.FileMagicLength = ushort.Parse(fileMagicLenOption.Value());
                }
                if (recordMagicLenOption.HasValue())
                {
                    option.RecordMagicLength = ushort.Parse(recordMagicLenOption.Value());
                }
                if (timeFieldLenOption.HasValue())
                {
                    option.TimeFieldLength = ushort.Parse(timeFieldLenOption.Value());
                }
                if (topicLengthFieldLenOption.HasValue())
                {
                    option.TopicLengthFieldLength = ushort.Parse(topicLengthFieldLenOption.Value());
                }
                if (dataLengthFieldLenOption.HasValue())
                {
                    option.DataLengthFieldLength = ushort.Parse(dataLengthFieldLenOption.Value());
                }
                if (hasFinalFlagOption.HasValue())
                {
                    option.HasFinalFlagField = (hasFinalFlagOption.Value() != "false");
                }
                if (timeUnitOption.HasValue())
                {
                    switch (timeUnitOption.Value())
                    {
                        case "second":
                            option.TimePrecision = RecordFileUtils.TopicCaptureFileRecordReaderOption.TimePrecisionLevel.Second;
                            break;
                        case "millisecond":
                            option.TimePrecision = RecordFileUtils.TopicCaptureFileRecordReaderOption.TimePrecisionLevel.Millisecond;
                            break;
                        case "microsecond":
                        default:
                            option.TimePrecision = RecordFileUtils.TopicCaptureFileRecordReaderOption.TimePrecisionLevel.Microsecond;
                            break;
                    }
                }
                return run(file, printMode, option);
            });
            app.Execute(args);
        }
    }
}
