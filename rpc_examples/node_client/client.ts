import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as _ from 'lodash'
import * as cbor from 'cbor'

type Input = [number,string];
type Output = [string];
type Descriptor = [string, string, boolean];

(async () => {
    let env = new TMBasic.ClockEnv();
    let r = new TMInfra.RealTimeApp.SynchronousRunner<TMBasic.ClockEnv>(env);
    let descriptors : Descriptor[] = [
        ["redis://127.0.0.1:6379:::rpc_example_simple", "SIMPLE RPC", false]
        , ["redis://127.0.0.1:6379:::rpc_example_client_stream", "CLIENT STREAM RPC", true]
        , ["redis://127.0.0.1:6379:::rpc_example_server_stream", "SERVER STREAM RPC", false]
        , ["redis://127.0.0.1:6379:::rpc_example_both_stream", "BOTH STREAM RPC", true]
    ];
    await Promise.all(_.map(descriptors, async (desc) => {
        let streamer = r.facilityStreamer(
            TMTransport.RemoteComponents.createFacilityProxy<TMBasic.ClockEnv,Input,Output>(
                (x) => cbor.encode(x) as Buffer 
                , (d) => cbor.decode(d) as Output 
                , {
                    address : desc[0]
                }
            )
        );
        streamer.send([5, "abc"]);
        if (desc[2]) {
            streamer.send([-1, "bcd"]);
            streamer.send([-2, "cde"]);
            streamer.send([-3, "def"]);
            streamer.send([-4, "efg"]);
        }
        let first = true;
        for await (const output of streamer.read()) {
            if (first) {
                console.log(`=============${desc[1]}====================`);
                first = false;
            }
            console.log(output.timedData.value.data[0]);
        }
    }));
    process.exit();
})();