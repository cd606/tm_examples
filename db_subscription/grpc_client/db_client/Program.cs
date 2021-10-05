using System;
using System.Threading.Tasks;
using Microsoft.Extensions.CommandLineUtils;
using Grpc.Core;
using DbSubscription;

namespace db_client
{
    class Program
    {
        enum Command {
            Subscribe
            , Insert
            , Update
            , Delete
            , Unsubscribe
            , List
            , Snapshot
            , Unknown
        };
        struct Data {
            public string name;
            public int value1;
            public string value2;
            public long old_version;
            public int old_value1;
            public string old_value2;
            public string id;
        }
        async Task Run(Command command, Data data)
        {
            var channel = new Channel("localhost:12345", ChannelCredentials.Insecure);
            var client = new Main.MainClient(channel);
            switch (command)
            {
                case Command.Subscribe:
                    {
                        var input = new GSInput();
                        input.Input = new GSInputVariants();
                        input.Input.Subscription = new Subscription();
                        input.Input.Subscription.Keys.Add(data.name);
                        var res = client.Subscription(input).ResponseStream;
                        while (await res.MoveNext())
                        {
                            Console.WriteLine(res.Current);
                        }
                    }
                    break;
                case Command.Insert:
                    {
                        var input = new TITransaction();
                        input.Input = new TITransactionVariants();
                        input.Input.InsertAction = new InsertAction() {
                            Key = data.name
                            , Data = new DBData() {Value1 = data.value1, Value2 = data.value2}
                        };
                        var res = client.Transaction(input);
                        Console.WriteLine(res);
                    }
                    break;
                case Command.Update:
                    {
                        var input = new TITransaction();
                        input.Input = new TITransactionVariants();
                        input.Input.UpdateAction = new UpdateAction() {
                            Key = data.name
                            , OldVersionSlice = data.old_version
                            , OldDataSummary = new DBData() {Value1 = data.old_value1, Value2 = data.old_value2}
                            , DataDelta = new DBData() {Value1 = data.value1, Value2 = data.value2}
                        };
                        var res = client.Transaction(input);
                        Console.WriteLine(res);
                    }
                    break;
                case Command.Delete:
                    {
                        var input = new TITransaction();
                        input.Input = new TITransactionVariants();
                        input.Input.DeleteAction = new DeleteAction() {
                            Key = data.name
                            , OldVersionSlice = data.old_version
                            , OldDataSummary = new DBData() {Value1 = data.old_value1, Value2 = data.old_value2}
                        };
                        var res = await client.TransactionAsync(input);
                        Console.WriteLine(res);
                    }
                    break;
                case Command.Unsubscribe:
                    {
                        var input = new GSInput();
                        input.Input = new GSInputVariants();
                        if (data.id == null || data.id.Equals("") || data.id.Equals("all")) 
                        {
                            input.Input.UnsubscribeAll = new UnsubscribeAll();
                        }
                        else 
                        {
                            input.Input.Unsubscription = new Unsubscription();
                            input.Input.Unsubscription.OriginalSubscriptionID = data.id;
                        }
                        var res = client.Subscription(input).ResponseStream;
                        while (await res.MoveNext())
                        {
                            Console.WriteLine(res.Current);
                        }
                    }
                    break;
                case Command.List:
                    {
                        var input = new GSInput();
                        input.Input = new GSInputVariants();
                        input.Input.ListSubscriptions = new ListSubscriptions();
                        var res = client.Subscription(input).ResponseStream;
                        while (await res.MoveNext())
                        {
                            Console.WriteLine(res.Current);
                        }
                    }
                    break;
                case Command.Snapshot:
                    {
                        var input = new GSInput();
                        input.Input = new GSInputVariants();
                        input.Input.SnapshotRequest = new SnapshotRequest();
                        input.Input.SnapshotRequest.Keys.Add(data.name);
                        var res = client.Subscription(input).ResponseStream;
                        while (await res.MoveNext())
                        {
                            Console.WriteLine(res.Current);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        static void Main(string[] args)
        {
            CommandLineApplication app = new CommandLineApplication(
                throwOnUnexpectedArg: true
            );
            CommandOption cmdOption = app.Option(
                "-c|--command <cmd>"
                , "the command to send (subscribe|insert|update|delete|unsubscribe|list|snapshot)"
                , CommandOptionType.SingleValue
            );
            CommandOption nameOption = app.Option(
                "-n|--name <name>"
                , "the name to add/update"
                , CommandOptionType.SingleValue
            );
            CommandOption value1Option = app.Option(
                "-1|--value1 <amount>"
                , "the value1 to add/update"
                , CommandOptionType.SingleValue
            );
            CommandOption value2Option = app.Option(
                "-2|--value2 <stat>"
                , "the value2 to add/update"
                , CommandOptionType.SingleValue
            );
            CommandOption oldVersionOption = app.Option(
                "-v|--old_version <version>"
                , "the old version to update/delete"
                , CommandOptionType.SingleValue
            );
            CommandOption oldValue1Option = app.Option(
                "-3|--oldValue1 <amount>"
                , "the old value1 to update/delete"
                , CommandOptionType.SingleValue
            );
            CommandOption oldValue2Option = app.Option(
                "-4|--oldValue2 <stat>"
                , "the old value2 to update/delete"
                , CommandOptionType.SingleValue
            );
            CommandOption idOption = app.Option(
                "-i|--id <id>"
                , "the original ID for unsubscription"
                , CommandOptionType.SingleValue
            );
            app.HelpOption("-? | -h | --help");
            app.OnExecute(async () => {
                if (!cmdOption.HasValue()) {
                    Console.Error.WriteLine("Please provide command");
                    return 1;
                }
                Data data = new Data();
                if (nameOption.HasValue()) {
                    data.name = nameOption.Value();
                }
                if (value1Option.HasValue()) {
                    data.value1 = int.Parse(value1Option.Value());
                }
                if (value2Option.HasValue()) {
                    data.value2 = value2Option.Value();
                }
                if (oldVersionOption.HasValue()) {
                    data.old_version = long.Parse(oldVersionOption.Value());
                }
                if (oldValue1Option.HasValue()) {
                    data.old_value1 = int.Parse(oldValue1Option.Value());
                }
                if (oldValue2Option.HasValue()) {
                    data.old_value2 = oldValue2Option.Value();
                }
                if (idOption.HasValue()) {
                    data.id = idOption.Value();
                }
                switch (cmdOption.Value()) {
                case "subscribe":
                    await new Program().Run(Command.Subscribe, data);
                    break;
                case "insert":
                    await new Program().Run(Command.Insert, data);
                    break;
                case "update":
                    await new Program().Run(Command.Update, data);
                    break;
                case "delete":
                    await new Program().Run(Command.Delete, data);
                    break;
                case "unsubscribe":
                    await new Program().Run(Command.Unsubscribe, data);
                    break;
                case "list":
                    await new Program().Run(Command.List, data);
                    break;
                case "snapshot":
                    await new Program().Run(Command.Snapshot, data);
                    break;
                default:
                    Console.Error.WriteLine($"Unknown command {cmdOption.Value()}");
                    break;
                }
                return 0;
            });
            app.Execute(args);
        }
    }
}
