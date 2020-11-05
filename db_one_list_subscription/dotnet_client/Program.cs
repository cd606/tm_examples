using System;
using System.Collections.Generic;
using PeterO.Cbor;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using RabbitMQ.Client;
using RabbitMQ.Client.Events;
using Microsoft.Extensions.CommandLineUtils;

namespace dotnet_client
{
    [CborWithoutFieldNames]
    class DBKey
    {
        public string name {get; set;}
    }
    [CborWithoutFieldNames]
    class DBData
    {
        public Int32 amount {get; set;}
        public double stat {get; set;}
    }
    [CborWithFieldNames]
    class DBItem
    {
        public DBKey key {get; set;}
        public DBData data {get; set;}
    }
    [CborWithFieldNames]
    class DBDeltaDelete
    {
        public List<DBKey> keys {get; set;}
    }
    [CborWithFieldNames]
    class DBDeltaInsertUpdate
    {
        public List<DBItem> items {get; set;}
    }
    [CborWithFieldNames]
    class DBDelta
    {
        public DBDeltaDelete deletes {get; set;}
        public DBDeltaInsertUpdate inserts_updates {get; set;}
    }
}

namespace dotnet_client
{
    using GS = GeneralSubscriber<Int64, VoidStruct, Int64, Dictionary<DBKey,DBData>, Int64, DBDelta>;
    using TI = TransactionInterface<Int64, VoidStruct, Int64, Dictionary<DBKey,DBData>, UInt32, Int64, DBDelta>;
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

        const string gsAddress = "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue";
        const string tiAddress = "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue";

        void runGS(ClockEnv env, GS.Input input)
        {
            var facility = MultiTransportFacility<ClockEnv>.CreateFacility<GS.Input,GS.Output>(
                encoder : (x) => CborEncoder<GS.Input>.Encode(x).EncodeToBytes()
                , decoder : (o) => {
                    return CborDecoder<GS.Output>.Decode(CBORObject.DecodeFromBytes(o));
                }
                , address : gsAddress
                , identityAttacher: ClientSideIdentityAttacher.SimpleIdentityAttacher("dotnet_client")
            );
            var keyInput = RealTimeAppUtils<ClockEnv>.constFirstPushKeyImporter<GS.Input>(input);
            var exporter = RealTimeAppUtils<ClockEnv>.simpleExporter<KeyedData<GS.Input,GS.Output>>(
                (d) => {
                    env.log(LogLevel.Info, $"Got GS Output: {d.timedData.value.data.asCborObject()}");
                    if (d.timedData.finalFlag)
                    {
                        env.log(LogLevel.Info, "Got final GS output, exiting");
                        env.exit();
                    }
                }
                , false
            );
            var r = new Runner<ClockEnv>(env);
            r.placeOrderWithFacility(r.importItem(keyInput), facility, r.exporterAsSink(exporter));
            r.finalize();
            RealTimeAppUtils<ClockEnv>.runForever(env);
        }

        void runTI(ClockEnv env, TI.Transaction input)
        {
            var facility = MultiTransportFacility<ClockEnv>.CreateFacility<TI.Transaction,TI.TransactionResponse>(
                encoder : (x) => CborEncoder<TI.Transaction>.Encode(x).EncodeToBytes()
                , decoder : (o) => {
                    return CborDecoder<TI.TransactionResponse>.Decode(CBORObject.DecodeFromBytes(o));
                }
                , address : tiAddress
                , identityAttacher: ClientSideIdentityAttacher.SimpleIdentityAttacher("dotnet_client")
            );
            var keyInput = RealTimeAppUtils<ClockEnv>.constFirstPushKeyImporter<TI.Transaction>(input);
            var exporter = RealTimeAppUtils<ClockEnv>.simpleExporter<KeyedData<TI.Transaction,TI.TransactionResponse>>(
                (d) => {
                    env.log(LogLevel.Info, $"Got TI Output: {CborEncoder<TI.TransactionResponse>.Encode(d.timedData.value.data)}");
                    if (d.timedData.finalFlag)
                    {
                        env.log(LogLevel.Info, "Got final TI output, exiting");
                        env.exit();
                    }
                }
                , false
            );
            var r = new Runner<ClockEnv>(env);
            r.placeOrderWithFacility(r.importItem(keyInput), facility, r.exporterAsSink(exporter));
            r.finalize();
            RealTimeAppUtils<ClockEnv>.runForever(env);
        }
        void Subscribe(ClockEnv env) {
            runGS(env, new GS.Input() {
                data = Variant<GS.Subscription, GS.Unsubscription, GS.ListSubscriptions, GS.UnsubscribeAll, GS.SnapshotRequest>
                    .From1(
                        new GS.Subscription() {
                            keys = new List<VoidStruct>() {new VoidStruct()}
                        }
                    )
            });
        }
        void Update(ClockEnv env, Data data) {
            runTI(env, new TI.Transaction() {
                data = Variant<TI.InsertAction, TI.UpdateAction, TI.DeleteAction>
                    .From2(new TI.UpdateAction() {
                        key = new VoidStruct()
                        , oldVersionSlice = data.old_version
                        , oldDataSummary = data.old_count
                        , dataDelta = new DBDelta() {
                            deletes = new DBDeltaDelete() { keys = new List<DBKey>() }
                            , inserts_updates = new DBDeltaInsertUpdate() {
                                items = new List<DBItem>() {
                                    new DBItem() {
                                        key = new DBKey() {name = data.name}
                                        , data = new DBData() {
                                            amount = data.amount 
                                            , stat = data.stat
                                        }
                                    }
                                }
                            }
                        }
                    })
            });
        }
        void Delete(ClockEnv env, Data data) {
            runTI(env, new TI.Transaction() {
                data = Variant<TI.InsertAction, TI.UpdateAction, TI.DeleteAction>
                    .From2(new TI.UpdateAction() {
                        key = new VoidStruct()
                        , oldVersionSlice = data.old_version
                        , oldDataSummary = data.old_count
                        , dataDelta = new DBDelta() {
                            deletes = new DBDeltaDelete() { 
                                keys = new List<DBKey>() {
                                    new DBKey() {name = data.name}
                                } 
                            }
                            , inserts_updates = new DBDeltaInsertUpdate() {
                                items = new List<DBItem>()
                            }
                        }
                    })
            });
        }
        void Unsubscribe(ClockEnv env, Data data) {
            if (data.id == null || data.id.Equals("") || data.id.Equals("all")) {
                runGS(env, new GS.Input() {
                    data = Variant<GS.Subscription, GS.Unsubscription, GS.ListSubscriptions, GS.UnsubscribeAll, GS.SnapshotRequest>
                        .From4(
                            new GS.UnsubscribeAll()
                        )
                });
            } else {
                runGS(env, new GS.Input() {
                    data = Variant<GS.Subscription, GS.Unsubscription, GS.ListSubscriptions, GS.UnsubscribeAll, GS.SnapshotRequest>
                        .From2(
                            new GS.Unsubscription() {
                                originalSubscriptionID = data.id
                            }
                        )
                });
            }
        }
        void List(ClockEnv env, Data data) {
            var input = new GS.Input() {
                data = Variant<GS.Subscription, GS.Unsubscription, GS.ListSubscriptions, GS.UnsubscribeAll, GS.SnapshotRequest>
                    .From3(
                        new GS.ListSubscriptions()
                    )
            }; 
            runGS(env, input);
        }
        void Snapshot(ClockEnv env) {
            var input = new GS.Input() {
                data = Variant<GS.Subscription, GS.Unsubscription, GS.ListSubscriptions, GS.UnsubscribeAll, GS.SnapshotRequest>
                    .From5(
                        new GS.SnapshotRequest() {
                            keys = new List<VoidStruct>() {new VoidStruct()}
                        }
                    )
            };
            runGS(env, input);
        }
        void Run(ClockEnv env, Command cmd, Data data) {
            switch (cmd) {
            case Command.Subscribe:
                Subscribe(env);
                break;
            case Command.Update:
                Update(env, data);
                break;
            case Command.Delete:
                Delete(env, data);
                break;
            case Command.Unsubscribe:
                Unsubscribe(env, data);
                break;
            case Command.List:
                List(env, data);
                break;
            case Command.Snapshot:
                Snapshot(env);
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
                "-c|--cmd <cmd>"
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
            app.OnExecute(() => {
                if (!cmdOption.HasValue()) {
                    Console.Error.WriteLine("Please provide command");
                    return 1;
                }
                var env = new ClockEnv();
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
                    new Program().Run(env, Command.Subscribe, data);
                    break;
                case "update":
                    new Program().Run(env, Command.Update, data);
                    break;
                case "delete":
                    new Program().Run(env, Command.Delete, data);
                    break;
                case "unsubscribe":
                    new Program().Run(env, Command.Unsubscribe, data);
                    break;
                case "list":
                    new Program().Run(env, Command.List, data);
                    break;
                case "snapshot":
                    new Program().Run(env, Command.Snapshot, data);
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
