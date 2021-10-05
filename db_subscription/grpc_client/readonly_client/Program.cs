using System;
using Grpc.Core;
using DbSubscription;

namespace readonly_client
{
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length != 1)
            {
                Console.Error.WriteLine("Usage: readonly_client key");
                return;            
            }
            var channel = new Channel("localhost:12345", ChannelCredentials.Insecure);
            var client = new Readonly.ReadonlyClient(channel);
            Console.WriteLine(client.Query(new DBKey() {
                Name = args[0]
            }));
        }
    }
}
