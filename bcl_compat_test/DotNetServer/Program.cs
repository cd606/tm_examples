﻿using System;
using System.IO;
using System.Collections.Generic;
using ProtoBuf;
using Dev.CD606.TM.Infra;
using Dev.CD606.TM.Infra.RealTimeApp;
using Dev.CD606.TM.Basic;
using Dev.CD606.TM.Transport;

namespace DotNetServer
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
    class Facility : AbstractOnOrderFacility<ClockEnv, Query, Result> 
    {
        public override void start(ClockEnv env)
        {
        }
        public override void handle(TimedDataWithEnvironment<ClockEnv, Key<Query>> data)
        {
            Console.WriteLine(data.timedData.value.key.ID);
            Console.WriteLine(data.timedData.value.key.Value);
            foreach (var f in data.timedData.value.key.FloatArr)
            {
                Console.WriteLine($"\t{f}");
            }
            Console.WriteLine(data.timedData.value.key.TS);
            Console.WriteLine(data.timedData.value.key.DT.Kind);
            if (data.timedData.value.key.DT.Kind == DateTimeKind.Utc) {
                Console.WriteLine(data.timedData.value.key.DT.ToLocalTime());
            } else {
                Console.WriteLine(data.timedData.value.key.DT);
            }
            publish(new TimedDataWithEnvironment<ClockEnv, Key<Result>>(
                data.environment
                , new WithTime<Key<Result>>(
                    data.environment.now()
                    , new Key<Result>(
                        data.timedData.value.id
                        , new Result {
                            ID = data.timedData.value.key.ID
                            , Value = data.timedData.value.key.Value*2.0m
                            , Messages = new List<string> {data.timedData.value.key.Description}
                            , TS = data.timedData.value.key.TS
                            , DT = DateTime.Now
                        }
                    )
                    , true
                )
            ));
        }
    }
    class Program
    {
        //private const string facilityLocator = "redis://127.0.0.1:6379:::bcl_test_queue";
        private const string facilityLocator = "rabbitmq://127.0.0.1::guest:guest:bcl_test_queue";
        static void Main(string[] args)
        {
            var env = new ClockEnv();
            var runner = new Runner<ClockEnv>(env);
            var facility = new Facility();
            Serializer.PrepareSerializer<Query>();
            Serializer.PrepareSerializer<Result>();
            MultiTransportFacility<ClockEnv>.WrapOnOrderFacility(
                r : runner 
                , facility : facility 
                , decoder: (byte[] x) => {
                    var q = Serializer.Deserialize<Query>(new MemoryStream(x));
                    return q;
                }
                , encoder: (Result r) => {
                    var s = new MemoryStream();
                    Serializer.Serialize<Result>(s, r);
                    return s.ToArray();
                }
                , address: facilityLocator
            );
            Console.WriteLine("Starting server");
            runner.finalize();
            while (true) {
                System.Threading.Thread.Sleep(10000);
            }
        }
    }
}
