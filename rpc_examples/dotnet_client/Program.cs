using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;
using PeterO.Cbor;

namespace dotnet_client
{
    [CborWithoutFieldNames]
    class Input 
    {
        public Int32 x {get; set;}
        public string y {get; set;}
    }
    [CborWithoutFieldNames]
    class Output 
    {
        public string result {get; set;}
    }
    class Program
    {
        static async Task Main(string[] args)
        {
            var env = new ClockEnv();
            var r = new SynchronousRunner<ClockEnv>(env);

            var descriptors = new List<(string,string,bool)> {
                ("redis://127.0.0.1:6379:::rpc_example_simple", "SIMPLE RPC", false)
                , ("redis://127.0.0.1:6379:::rpc_example_client_stream", "CLIENT STREAM RPC", true)
                , ("redis://127.0.0.1:6379:::rpc_example_server_stream", "SERVER STREAM RPC", false)
                , ("redis://127.0.0.1:6379:::rpc_example_both_stream", "BOTH STREAM RPC", true)
            };

            foreach (var desc in descriptors)
            {
                Console.WriteLine($"============={desc.Item2}====================");
                var streamer = r.facilityStreamer(
                    MultiTransportFacility<ClockEnv>.CreateFacility<Input,Output>(
                        (x) => CborEncoder<Input>.Encode(x).EncodeToBytes()
                        , (b) => CborDecoder<Output>.Decode(CBORObject.DecodeFromBytes(b))
                        , desc.Item1
                    )
                );
                streamer.Send(new Input() {x=5, y="abc"});
                if (desc.Item3)
                {
                    streamer.Send(new Input() {x=-1, y="bcd"});
                    streamer.Send(new Input() {x=-2, y="cde"});
                    streamer.Send(new Input() {x=-3, y="def"});
                    streamer.Send(new Input() {x=-4, y="efg"});
                }
                await foreach (var output in streamer.Results())
                {
                    Console.WriteLine(output.timedData.value.data.result);
                }
            }
        }
    }
}
