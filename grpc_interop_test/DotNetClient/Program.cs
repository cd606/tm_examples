﻿using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.IO;
using Grpc.Core;
using GrpcInteropTest;

namespace DotNetClient
{
    class Program
    {
        static async Task Main(string[] args)
        {
            var channel = new Channel("localhost:34567", 
                (
                    (args.Length >= 1 && args[0].Equals("ssl"))
                    ?
                    (new SslCredentials(
                        File.ReadAllText("../DotNetServer/server.crt")
                        , new KeyCertificatePair(
                            File.ReadAllText("./client.crt")
                            , File.ReadAllText("./client.key")
                        )
                    ))
                    : ChannelCredentials.Insecure
                )
            );
            var client = new TestService.TestServiceClient(channel);
            var request = new TestRequest();
            request.IntParam = 2;
            request.DoubleListParam.AddRange(new List<double> {1.0,2.1,3.2,4.3,5.4,6.5,7.6});
            var s = client.Test(request).ResponseStream;
            while (await s.MoveNext())
            {
                var val = s.Current;
                Console.WriteLine(val);
            }
            var simpleReq = new SimpleRequest();
            simpleReq.Input = 1;
            simpleReq.Name = "abc";
            simpleReq.Name2 = "def";
            simpleReq.AnotherInput.AddRange(new uint[] {0,1,0,0,1});
            simpleReq.MapInput.Add(0, "");
            simpleReq.MapInput.Add(10, "abc");
            Console.WriteLine(client.SimpleTest(simpleReq));
            simpleReq = new SimpleRequest();
            simpleReq.Input = 2;
            simpleReq.Val = 0.1f;
            simpleReq.Name2 = "ghi";
            simpleReq.AnotherInput.AddRange(new uint[] {1,2,3,0,1,0});
            simpleReq.MapInput.Add(1, "bcd");
            simpleReq.MapInput.Add(2, "cde");
            Console.WriteLine(client.SimpleTest(simpleReq));
        }
    }
}
