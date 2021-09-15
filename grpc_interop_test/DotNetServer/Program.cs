using System;
using System.Threading.Tasks;
using GrpcInteropTest;
using Grpc.Core;

namespace DotNetServer
{
    class TestServiceImpl : TestService.TestServiceBase
    {
        public override async Task Test(TestRequest request, IServerStreamWriter<TestResponse> responseStream, ServerCallContext context)
        {
            if (request == null)
            {
                return;
            }
            if (request.DoubleListParam == null)
            {
                await responseStream.WriteAsync(
                    new TestResponse()
                );
                return;
            }
            var chunkSize = Math.Max(1, request.IntParam);
            for (uint ii=0; ii<request.DoubleListParam.Count; ii += chunkSize)
            {
                var resp = new TestResponse();
                for (uint jj=0; jj<chunkSize && ii+jj<request.DoubleListParam.Count; ++jj)
                {
                    resp.StringResp.Add(request.DoubleListParam[(int) (ii+jj)].ToString());
                }
                await responseStream.WriteAsync(resp);
                await Task.Delay(100);
            }
        }
        public override async Task<SimpleResponse> SimpleTest(SimpleRequest req, ServerCallContext context) {
            var resp = new SimpleResponse();
            resp.Resp = req.Input*2;
            await Task.Delay(1);
            return resp;
        }
    }
    class Program
    {
        static void Main(string[] args)
        {
            var server = new Server()
            {
                Services = { TestService.BindService(new TestServiceImpl()) },
                Ports = { new ServerPort("127.0.0.1", 34567, ServerCredentials.Insecure)} 
            };
            server.Start();
            Console.ReadKey();
            server.ShutdownAsync().Wait();
        }
    }
}
