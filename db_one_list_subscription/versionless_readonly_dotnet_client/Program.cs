using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Text.RegularExpressions;
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

}

namespace dotnet_client
{
    class Program
    {
        static async Task<Dictionary<DBKey,DBData>> snapshot()
        {
            return await MultiTransportFacility<ClockEnv>.OneShotByHeartbeat<VoidStruct,Dictionary<DBKey,DBData>>(
                env: new ClockEnv()
                , input: new Key<VoidStruct>(new VoidStruct())
                , encoder: (x) => CborEncoder<VoidStruct>.Encode(x).EncodeToBytes()
                , decoder: (o) => CborDecoder<Dictionary<DBKey,DBData>>.Decode(CBORObject.DecodeFromBytes(o))
                , heartbeatAddress : "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                , heartbeatTopicStr : "read_only_db_one_list_server.heartbeat"
                , heartbeatSenderRE : new Regex("read_only_db_one_list_server")
                , facilityChannelName : "queryFacility"
            );
        }
        static async Task Main(string[] args)
        {
            var s = await snapshot();
            foreach (var item in s)
            {
                Console.WriteLine($"{item.Key.name}: {item.Value.amount}, {item.Value.stat}");
            }
        }
    }
}
