using System;
using System.IO;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using ProtoBuf;
using PeterO.Cbor;
using Here;
using Terminal.Gui;

namespace console_enabler_dotnet
{
    [ProtoContract]
    class ConfigureCommand
    {
        [ProtoMember(1)]
        public bool enabled {get; set;}
    }
    [ProtoContract]
    class ConfigureResult
    {
        [ProtoMember(1)]
        public bool enabled {get; set;}
    }
    class Program
    {
        static void Main(string[] args)
        {
            Application.Init();
            var top = Application.Top;

            var window = new Window() {
                X = 0, Y = 0, Width = Dim.Fill(), Height = Dim.Fill(), Title = "Console Enabler (Plain)"
            };
            top.Add(window);

            var display = new Label("Disconnected") {
                X = 50, Y = 5
            };
            var enableBtn = new Button("Enable") {
                X = 30, Y = 10
            };
            var disableBtn = new Button("Disable") {
                X = 70, Y = 10
            };
            window.Add(display, enableBtn, disableBtn);
            top.KeyPress += (args) => {
                if (args.KeyEvent.KeyValue == (int) ConsoleKey.Escape)
                {
                    top.Running = false;
                }
            };

            var env = new ClockEnv();
            var r = new Runner<ClockEnv>(env);

            var facility = MultiTransportFacility<ClockEnv>.CreateDynamicFacility<ConfigureCommand,ConfigureResult>(
                encoder : (x) => {
                    var s = new MemoryStream();
                    Serializer.Serialize<ConfigureCommand>(s, x);
                    return s.ToArray();
                }
                , decoder : (x) => Serializer.Deserialize<ConfigureResult>(new MemoryStream(x))
                , identityAttacher : ClientSideIdentityAttacher.SimpleIdentityAttacher("dotnet_console_enabler")
            );
            var heartbeatSource = MultiTransportImporter<ClockEnv>.CreateTypedImporter<Heartbeat>(
                decoder : (x) => new Heartbeat().fromCborObject(CBORObject.DecodeFromBytes(x))
                , address : "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                , topicStr : "simple_demo.plain_executables.#.heartbeat"
            );
            var heartbeatAction = RealTimeAppUtils<ClockEnv>.liftMaybe<TypedDataWithTopic<Heartbeat>,bool>(
                (TypedDataWithTopic<Heartbeat> h) => {
                    if (h.content.sender_description.Equals("simple_demo plain MainLogic")) {
                        if (h.content.facility_channels.TryGetValue("cfgFacility", out string channelInfo)) {
                            facility.changeAddress(channelInfo);
                        }
                        return h.content.details["calculation_status"].info.Equals("enabled");
                    } else {
                        return Option.None;
                    }
                }
                , false
            );
            var statusExporter = RealTimeAppUtils<ClockEnv>.pureExporter<bool>(
                (x) => {
                    Application.MainLoop.Invoke (() => {
                        display.Text = (x?"Enabled":"Disabled");
                    });
                }
                , false
            );
            var commandImporter = RealTimeAppUtils<ClockEnv>.triggerImporter<ConfigureCommand>();
            var keyify = RealTimeAppUtils<ClockEnv>.liftPure(
                (ConfigureCommand cmd) => InfraUtils.keyify(cmd)
                , false
            );
            var resultExporter = RealTimeAppUtils<ClockEnv>.pureExporter<KeyedData<ConfigureCommand,ConfigureResult>>(
                (x) => {
                    Application.MainLoop.Invoke (() => {
                        display.Text = (x.data.enabled?"Enabled":"Disabled");
                    });
                }
                , false
            );
            r.exportItem(statusExporter, r.execute(heartbeatAction, r.importItem(heartbeatSource)));
            r.placeOrderWithFacility(r.execute(keyify, r.importItem(commandImporter)), facility, r.exporterAsSink(resultExporter));
            
            enableBtn.Clicked += () => {
                commandImporter.trigger(new ConfigureCommand() {
                    enabled = true
                });
            };
            disableBtn.Clicked += () => {
                commandImporter.trigger(new ConfigureCommand() {
                    enabled = false
                });
            };

            r.finalize();

            Application.Run();
        }
    }
}
