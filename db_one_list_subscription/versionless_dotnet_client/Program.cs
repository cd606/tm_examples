﻿using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using PeterO.Cbor;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using Microsoft.Extensions.CommandLineUtils;

namespace dotnet_client
{
    [CborWithFieldNames]
    class DBKey
    {
        public string name {get; set;}
    }
    [CborWithFieldNames]
    class DBData
    {
        public Int32 amount {get; set;}
        public double stat {get; set;}
    }

    [CborWithFieldNames]
    class DBDelta
    {
        public List<DBKey> deletes {get; set;}
        public List<(DBKey,DBData)> inserts_updates {get; set;}
    }
}

namespace dotnet_client
{
    using GS = GeneralSubscriber<NoVersion, VoidStruct, NoVersion, Dictionary<DBKey,DBData>, NoVersion, DBDelta>;
    using TI = TransactionInterface<NoVersion, VoidStruct, NoVersion, Dictionary<DBKey,DBData>, UInt32, NoVersion, DBDelta>;
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
            public uint old_count;
            public string id;
        }

        const string gsFacilityChannel = "transaction_server_components/subscription_handler";
        const string tiFacilityChannel = "transaction_server_components/transaction_handler";

        void runFacility<InT,OutT>(ClockEnv env, InT input, string facilityChannelName)
        {
            var heartbeat = MultiTransportImporter<ClockEnv>.FetchTypedFirstUpdateAndDisconnect(
                env : env 
                , decoder : (b) => CborDecoder<Heartbeat>.Decode(CBORObject.DecodeFromBytes(b))
                , address : "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                , topicStr : "versionless_db_one_list_subscription_server.heartbeat"
                , predicate : (h) => new Regex("versionless_db_one_list_subscription_server").IsMatch(h.sender_description) && h.facility_channels.ContainsKey(facilityChannelName)
            ).GetAwaiter().GetResult();
            string address = "";
            heartbeat.content.facility_channels.TryGetValue(facilityChannelName, out address);
            var facility = MultiTransportFacility<ClockEnv>.CreateFacility<InT,OutT>(
                encoder : (x) => CborEncoder<InT>.Encode(x).EncodeToBytes()
                , decoder : (o) => CborDecoder<OutT>.Decode(CBORObject.DecodeFromBytes(o))
                , address : address
                , identityAttacher: ClientSideIdentityAttacher.SimpleIdentityAttacher("dotnet_client")
            );
            var keyInput = RealTimeAppUtils<ClockEnv>.constFirstPushKeyImporter<InT>(input);
            var exporter = RealTimeAppUtils<ClockEnv>.simpleExporter<KeyedData<InT,OutT>>(
                (d) => {
                    env.log(LogLevel.Info, $"Got output: {CborEncoder<OutT>.Encode(d.timedData.value.data)}");
                    if (d.timedData.finalFlag)
                    {
                        env.log(LogLevel.Info, "Got final output, exiting");
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
        void runGS(ClockEnv env, GS.Input input)
        {
            runFacility<GS.Input,GS.Output>(env, input, gsFacilityChannel);
        }

        void runTI(ClockEnv env, TI.Transaction input)
        {
            runFacility<TI.Transaction,TI.TransactionResponse>(env, input, tiFacilityChannel);
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
                        , oldDataSummary = data.old_count
                        , dataDelta = new DBDelta() {
                            deletes = new List<DBKey>()
                            , inserts_updates = new List<(DBKey, DBData)>() {
                                (
                                    new DBKey() {
                                        name = data.name
                                    }
                                    , new DBData() {
                                        amount = data.amount 
                                        , stat = data.stat
                                    }
                                )
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
                        , oldDataSummary = data.old_count
                        , dataDelta = new DBDelta() {
                            deletes = new List<DBKey>() { new DBKey() {name = data.name} }
                            , inserts_updates = new List<(DBKey,DBData)>() {}
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
            runGS(env, new GS.Input() {
                data = Variant<GS.Subscription, GS.Unsubscription, GS.ListSubscriptions, GS.UnsubscribeAll, GS.SnapshotRequest>
                    .From3(
                        new GS.ListSubscriptions()
                    )
            }); 
        }
        void Snapshot(ClockEnv env) {
            runGS(env, new GS.Input() {
                data = Variant<GS.Subscription, GS.Unsubscription, GS.ListSubscriptions, GS.UnsubscribeAll, GS.SnapshotRequest>
                    .From5(
                        new GS.SnapshotRequest() {
                            keys = new List<VoidStruct>() {new VoidStruct()}
                        }
                    )
            });
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
