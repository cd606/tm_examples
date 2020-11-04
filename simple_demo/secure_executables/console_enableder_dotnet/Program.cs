using System;
using System.Text;
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
        static byte[] serverPublicKey = new byte[] {
            0xDA,0xA0,0x15,0xD4,0x33,0xE8,0x92,0xC9,0xF2,0x96,0xA1,0xF8,0x1F,0x79,0xBC,0xF4,
            0x2D,0x7A,0xDE,0x48,0x03,0x47,0x16,0x0C,0x57,0xBD,0x1F,0x45,0x81,0xB5,0x18,0x2E 
        };
        static byte[] signatureSecretKey = new byte[] {
            0x89,0xD9,0xE6,0xED,0x17,0xD6,0x7B,0x30,0xE6,0x16,0xAC,0xB4,0xE6,0xD0,0xAD,0x47,
            0xD5,0x0C,0x6E,0x5F,0x11,0xDF,0xB1,0x9F,0xFE,0x4D,0x23,0x2A,0x0D,0x45,0x84,0x8E
        };
        static string decryptKeyString = "testkey";
        private byte[] decryptKey;
        Program()
        {
            this.decryptKey = Sodium.GenericHash.Hash(decryptKeyString, (byte[]) null, 32);
        }
        Option<byte[]> verifyAndDecode(byte[] input)
        {
            var cborObj = CBORObject.DecodeFromBytes(input);
            if (cborObj.Type != CBORType.Map)
            {
                return Option.None;
            }
            var sig = cborObj["signature"].ToObject<byte[]>();
            var data = cborObj["data"].ToObject<byte[]>();
            if (!Sodium.PublicKeyAuth.VerifyDetached(sig, data, serverPublicKey))
            {
                return Option.None;
            }
            var nonce = new byte[24];
            var realData = new byte[data.Length-24];
            Array.Copy(data, 0, nonce, 0, 24);
            Array.Copy(data, 24, realData, 0, realData.Length);
            var decrypted = Sodium.SecretBox.Open(realData, nonce, decryptKey);
            if (decrypted != null)
            {
                return decrypted;
            }
            return Option.None;
        }
        void run()
        {
            Application.Init();
            var top = Application.Top;

            var window = new Window() {
                X = 0, Y = 0, Width = Dim.Fill(), Height = Dim.Fill(), Title = "Console Enabler (Secure)"
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
                , identityAttacher : ClientSideIdentityAttacher.SignatureBasedIdentityAttacher(signatureSecretKey)
            );
            var heartbeatSource = MultiTransportImporter<ClockEnv>.CreateTypedImporter<Heartbeat>(
                decoder : (x) => CborDecoder<Heartbeat>.Decode(CBORObject.DecodeFromBytes(x))
                , address : "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                , topicStr : "simple_demo.secure_executables.#.heartbeat"
                , hook : new WireToUserHook(this.verifyAndDecode)
            );
            var heartbeatAction = RealTimeAppUtils<ClockEnv>.liftMaybe<TypedDataWithTopic<Heartbeat>,bool>(
                (TypedDataWithTopic<Heartbeat> h) => {
                    if (h.content.sender_description.Equals("simple_demo secure MainLogic")) {
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
        static void Main(string[] args)
        {
            new Program().run();
        }
    }
}
