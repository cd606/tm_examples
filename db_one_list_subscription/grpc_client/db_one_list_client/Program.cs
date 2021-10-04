using System;
using System.Threading.Tasks;
using Microsoft.Extensions.CommandLineUtils;
using Grpc.Core;
using DbOneListSubscription;

namespace db_one_list_client
{
    class Program
    {
        enum Command {
            Subscribe
            , Update
            , Delete
            , Unsubscribe
            , List
            , Snapshot
            , Unknown
        };
        struct Data {
            public string name;
            public int amount;
            public double stat;
            public long old_version;
            public uint old_count;
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
                        input.Input.Subscription.Keys.Add(new GSKey());
                        var res = client.Subscription(input).ResponseStream;
                        while (await res.MoveNext())
                        {
                            Console.WriteLine(res.Current);
                        }
                    }
                    break;
                case Command.Update:
                    {
                        var input = new TITransaction();
                        input.Input = new TITransactionVariants();
                        input.Input.UpdateAction = new UpdateAction();
                        input.Input.UpdateAction.OldVersionSlice = data.old_version;
                        input.Input.UpdateAction.OldDataSummary = data.old_count;
                        input.Input.UpdateAction.DataDelta = new DBDelta();   
                        input.Input.UpdateAction.DataDelta.InsertsUpdates.Add(
                            data.name, new DBData() {Amount = data.amount, Stat = data.stat}
                        );
                        var res = client.Transaction(input);
                        Console.WriteLine(res);
                    }
                    break;
                case Command.Delete:
                    {
                        var input = new TITransaction();
                        input.Input = new TITransactionVariants();
                        input.Input.UpdateAction = new UpdateAction();
                        input.Input.UpdateAction.OldVersionSlice = data.old_version;
                        input.Input.UpdateAction.OldDataSummary = data.old_count;
                        input.Input.UpdateAction.DataDelta = new DBDelta();  
                        input.Input.UpdateAction.DataDelta.Deletes.Add(data.name);
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
                        input.Input.SnapshotRequest.Keys.Add(new GSKey());
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
                , "the command to send (subscribe|update|delete|unsubscribe|list|snapshot)"
                , CommandOptionType.SingleValue
            );
            CommandOption nameOption = app.Option(
                "-n|--name <name>"
                , "the name to add/update"
                , CommandOptionType.SingleValue
            );
            CommandOption amountOption = app.Option(
                "-a|--amount <amount>"
                , "the amount to add/update"
                , CommandOptionType.SingleValue
            );
            CommandOption statOption = app.Option(
                "-s|--stat <stat>"
                , "the stat to add/update"
                , CommandOptionType.SingleValue
            );
            CommandOption oldVersionOption = app.Option(
                "-v|--old_version <version>"
                , "the old version to update/delete"
                , CommandOptionType.SingleValue
            );
            CommandOption oldCountOption = app.Option(
                "-C|--old_count <count>"
                , "the old list count for verification in update/delete"
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
                if (amountOption.HasValue()) {
                    data.amount = int.Parse(amountOption.Value());
                }
                if (statOption.HasValue()) {
                    data.stat = double.Parse(statOption.Value());
                }
                if (oldVersionOption.HasValue()) {
                    data.old_version = long.Parse(oldVersionOption.Value());
                }
                if (oldCountOption.HasValue()) {
                    data.old_count = uint.Parse(oldCountOption.Value());
                }
                if (idOption.HasValue()) {
                    data.id = idOption.Value();
                }
                switch (cmdOption.Value()) {
                case "subscribe":
                    await new Program().Run(Command.Subscribe, data);
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
