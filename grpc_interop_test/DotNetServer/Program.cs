using System;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.IO;
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
                    resp.StringResp.Add("");
                }
                await responseStream.WriteAsync(resp);
                await Task.Delay(100);
            }
        }
        public override async Task<SimpleResponse> SimpleTest(SimpleRequest req, ServerCallContext context) {
            var resp = new SimpleResponse();
            resp.Resp = req.Input*2;
            switch (req.ReqOneofCase) {
            case SimpleRequest.ReqOneofOneofCase.Name:
                resp.NameResp = req.Name+":resp";
                break;
            case SimpleRequest.ReqOneofOneofCase.Val:
                resp.ValResp = req.Val*2.0f;
                break;
            default:
                break;
            }
            resp.Name2Resp = req.Name2+":resp";
            resp.AnotherInputBack.AddRange(req.AnotherInput);
            await Task.Delay(1);
            Console.WriteLine($"Req={req},Resp={resp}");
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
                Ports = { new ServerPort("0.0.0.0", 34567, 
                    (
                        (args.Length >= 1 && args[0].Equals("ssl"))
                        ?
                        (new SslServerCredentials(
                            new List<KeyCertificatePair> {
                                new KeyCertificatePair(
                                    File.ReadAllText("./server.crt")
                                    , File.ReadAllText("./server.key")
                                )
                            }
                        ))
                        :
                        ServerCredentials.Insecure
                    )
                )} 
            };
            server.Start();
            Console.ReadKey();
            server.ShutdownAsync().Wait();
        }
    }
}
