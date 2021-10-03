using System;
using Grpc.Core;
using DbOneListSubscription;

namespace readonly_client
{
    class Program
    {
        static void Main(string[] args)
        {
            var channel = new Channel("localhost:12345", ChannelCredentials.Insecure);
            var client = new Readonly.ReadonlyClient(channel);
            Console.WriteLine(client.Query(new DBQuery()));
        }
    }
}
