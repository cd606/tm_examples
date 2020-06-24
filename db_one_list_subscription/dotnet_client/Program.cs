using System;
using PeterO.Cbor;
using RabbitMQ.Client;
using RabbitMQ.Client.Events;
using Microsoft.Extensions.CommandLineUtils;

namespace dotnet_client
{
    class Program
    {
        enum Command {
            Subscribe
            , Update
            , Delete
            , Unsubscribe
            , Unknown
        };
        struct Data {
            public string name;
            public int amount;
            public double stat;
            public long old_version;
            public int old_count;
            public string id;
        }

        IConnection conn = null;
        IModel chan = null;
        string replyQueue = "";
        EventingBasicConsumer consumer = null;

        void Start() {
            var factory = new ConnectionFactory {
                HostName = "127.0.0.1"
                , UserName = "guest"
                , Password = "guest"
            };
            conn = factory.CreateConnection();
            chan = conn.CreateModel();
            replyQueue = chan.QueueDeclare().QueueName;
            consumer = new EventingBasicConsumer(chan);

            consumer.Received += (model, ea) => {
                var body = ea.Body.ToArray();
                var isFinal = false;
                CBORObject cbor = null;
                if (ea.BasicProperties.ContentEncoding.Equals("with_final")) {
                    if (body.Length > 0) {
                        isFinal = (body[body.Length-1] != 0);
                        Array.Resize(ref body, body.Length-1);
                        cbor = CBORObject.DecodeFromBytes(body);
                    }
                } else {
                    isFinal = false;
                    cbor = CBORObject.DecodeFromBytes(body);
                }
                Console.Write("Got update: ");
                Console.Write(cbor);
                Console.Write($" (ID: {ea.BasicProperties.CorrelationId})");
                Console.WriteLine($" (isFinal: {isFinal})");
                if (isFinal) {
                    Environment.Exit(0);
                }
            };
        }
        void SendCommand(CBORObject cmd) {
            if (chan != null) {
                var props = chan.CreateBasicProperties();
                var id = Guid.NewGuid().ToString();
                props.CorrelationId = id;
                props.ReplyTo = replyQueue;
                props.DeliveryMode = 1;
                props.Expiration = "5000";
                props.ContentEncoding = "with_final";

                CBORObject withIdentity = CBORObject
                                            .NewArray()
                                            .Add("dotnet_client")
                                            .Add(cmd.EncodeToBytes());
                chan.BasicPublish(
                    exchange: ""
                    , routingKey: "test_db_one_list_cmd_queue"
                    , basicProperties: props
                    , body: withIdentity.EncodeToBytes()
                );
                chan.BasicConsume(
                    consumer: consumer
                    , queue: replyQueue
                    , autoAck: true
                );
                while (true) {
                    System.Threading.Thread.Sleep(1000);
                }
            }
        }
        void Subscribe() {
            SendCommand(
                CBORObject.NewArray()
                    .Add(1) //subscribe
                    .Add(CBORObject.NewMap().Add("key", 0)) //subscription object, 0 is the key (VoidStruct)
            );
        }
        void Update(Data data) {
            var updatedKey = CBORObject.NewMap().Add("name", data.name);
            var updatedValue = CBORObject.NewMap().Add("amount", data.amount).Add("stat", data.stat);
            var updatedData = 
                CBORObject.NewArray()
                    .Add(CBORObject.NewMap()
                        .Add("key", updatedKey)
                        .Add("data", updatedValue)
                    );
            var dataDelta = 
                CBORObject.NewMap()
                    .Add("deletes", CBORObject.NewMap().Add("keys", CBORObject.NewArray()))
                    .Add("inserts_updates", CBORObject.NewMap().Add("items", updatedData));
            SendCommand(
                CBORObject.NewArray()
                    .Add(0) //transaction
                    .Add(
                        CBORObject.NewArray()
                            .Add(1) //update
                            .Add(
                                CBORObject.NewMap()
                                    .Add("key", 0)
                                    .Add("old_version", data.old_version)
                                    .Add("old_data_summary", data.old_count)
                                    .Add("data_delta", dataDelta)
                                    .Add("ignore_version_check", 0)
                            )
                    )
            );
        }
        void Delete(Data data) {
            var deletedKey = CBORObject.NewMap().Add("name", data.name);
            var dataDelta = 
                CBORObject.NewMap()
                    .Add("deletes", CBORObject.NewMap().Add("keys", CBORObject.NewArray().Add(deletedKey)))
                    .Add("inserts_updates", CBORObject.NewMap().Add("items", CBORObject.NewArray()));
            SendCommand(
                CBORObject.NewArray()
                    .Add(0) //transaction
                    .Add(
                        CBORObject.NewArray()
                            .Add(1) //update
                            .Add(
                                CBORObject.NewMap()
                                    .Add("key", 0)
                                    .Add("old_version", data.old_version)
                                    .Add("old_data_summary", data.old_count)
                                    .Add("data_delta", dataDelta)
                                    .Add("ignore_version_check", 0)
                            )
                    )
            );
        }
        void Unsubscribe(Data data) {
            SendCommand(
                CBORObject.NewArray()
                    .Add(2) //unsubscribe
                    .Add(
                        CBORObject.NewMap()
                            .Add("original_subscription_id", data.id)
                            .Add("key", 0)
                    )
            );
        }
        void Run(Command cmd, Data data) {
            Start();
            switch (cmd) {
            case Command.Subscribe:
                Subscribe();
                break;
            case Command.Update:
                Update(data);
                break;
            case Command.Delete:
                Delete(data);
                break;
            case Command.Unsubscribe:
                Unsubscribe(data);
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
                , "the command to send"
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
                    data.old_count = int.Parse(oldCountOption.Value());
                }
                if (idOption.HasValue()) {
                    data.id = idOption.Value();
                }
                switch (cmdOption.Value()) {
                case "subscribe":
                    new Program().Run(Command.Subscribe, data);
                    break;
                case "update":
                    new Program().Run(Command.Update, data);
                    break;
                case "delete":
                    new Program().Run(Command.Delete, data);
                    break;
                case "unsubscribe":
                    new Program().Run(Command.Unsubscribe, data);
                    break;
                default:
                    break;
                }
                return 0;
            });
            app.Execute(args);
        }
    }
}
