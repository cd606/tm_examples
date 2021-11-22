using System;
using System.IO;
using System.Collections.Generic;
using System.Threading.Tasks;
using ProtoBuf;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;

namespace DotNetClient
{
    [ProtoContract]
    public class Query 
    {
        [ProtoMember(1)]
        public Guid ID {get; set;}
        [ProtoMember(2)]
        public Decimal Value {get; set;}
        [ProtoMember(6)]
        public string Description {get; set;}
        [ProtoMember(5)]
        public List<float> FloatArr {get; set;}
        [ProtoMember(7)]
        public TimeSpan TS {get; set;}
        [ProtoMember(8)]
        public DateTime DT {get; set;}
    }
    [ProtoContract]
    public class Result
    {
        [ProtoMember(1)]
        public Guid ID {get; set;}
        [ProtoMember(3)]
        public Decimal Value {get; set;}
        [ProtoMember(2)]
        public List<string> Messages {get; set;}
        [ProtoMember(4)]
        public TimeSpan TS {get; set;}
        [ProtoMember(5)]
        public DateTime DT {get; set;}
    }
    class Program
    {
        //private const string facilityLocator = "redis://127.0.0.1:6379:::bcl_test_queue";
        private const string facilityLocator = "rabbitmq://127.0.0.1::guest:guest:bcl_test_queue";
        static async Task Main(string[] args)
        {
            Query q = new Query {
                ID = Guid.NewGuid()
                , Value = -0.00000000123m
                , Description = "Test Query"
                , FloatArr = new List<float> {1.0f, 2.0f}
                , TS = TimeSpan.FromMilliseconds(12345)
                , DT = DateTime.Now
            };
            Result r = await MultiTransportFacility<ClockEnv>.OneShot<Query,Result>(
                env: new ClockEnv()
                , input: new Key<Query>(q)
                , encoder: (Query q) => {
                    var s = new MemoryStream();
                    Serializer.Serialize<Query>(s, q);
                    return s.ToArray();
                }
                , decoder: (byte[] b) => Serializer.Deserialize<Result>(new MemoryStream(b))
                , address : facilityLocator
            );     
            Console.WriteLine(q.ID);
            Console.WriteLine(q.Value);
            Console.WriteLine(r.ID);
            Console.WriteLine(r.Value);
            Console.WriteLine(r.TS);
            if (r.DT.Kind == DateTimeKind.Utc) {
                Console.WriteLine(r.DT.ToLocalTime());
            } else {
                Console.WriteLine(r.DT);
            }
            /*
            Console.WriteLine(Serializer.GetProto<Query>());
            Console.WriteLine(Serializer.GetProto<Result>());
            */
        }
    }
}
