﻿using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Grpc.Core;
using GrpcInteropTest;

namespace DotNetClient
{
    class Program
    {
        static async Task Main(string[] args)
        {
            var channel = new Channel("127.0.0.1:34567", ChannelCredentials.Insecure);
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
            Console.WriteLine(client.SimpleTest(simpleReq));
        }
    }
}
