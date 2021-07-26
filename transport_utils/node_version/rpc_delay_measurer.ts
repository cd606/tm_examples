import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as yargs from 'yargs'
import * as cbor from 'cbor'

type FacilityInput = [number, number];
type FacilityOutput = [number];

async function runServer(
    serviceDescriptor : string 
    , heartbeatDescriptor : string
    , heartbeatTopic : string
    , heartbeatIdentity : string
) {
    let env = new TMBasic.ClockEnv();
    let r = new TMInfra.RealTimeApp.Runner<TMBasic.ClockEnv>(env);
    let facility = TMInfra.RealTimeApp.Utils.liftPureOnOrderFacility<TMBasic.ClockEnv,FacilityInput,FacilityOutput>(
        (x : FacilityInput) => [x[1]*2]
    );
    TMTransport.RemoteComponents.wrapFacility(
        r
        , facility 
        , (x : FacilityOutput) => cbor.encode(x) as Buffer
        , (d : Buffer) => cbor.decode(d) as FacilityInput
        , {
            address : serviceDescriptor
        }
    );
    if (heartbeatDescriptor != '') {
        var h = new TMTransport.RemoteComponents.HeartbeatPublisher(
            heartbeatDescriptor, heartbeatTopic, heartbeatIdentity, 2000
        );
        h.registerFacility('facility', serviceDescriptor);
        h.addToRunner(r);
    }
    r.finalize();
}

async function runClient(
    serviceDescriptor : string 
    , heartbeatDescriptor : string
    , heartbeatTopic : string
    , heartbeatIdentity : string
    , repeatTimes : number
) {
    let env = new TMBasic.ClockEnv();
    let r = new TMInfra.RealTimeApp.Runner<TMBasic.ClockEnv>(env);
    r.finalize();
    let firstTimeStamp = 0;
    let count = 0;
    let recvCount = 0;
    let importerCount = 0;
    let delayedImporter = TMInfra.RealTimeApp.Utils.delayedImporter<TMBasic.ClockEnv,string,number>(
        TMInfra.RealTimeApp.Utils.uniformSimpleImporter(
            async function (env : TMBasic.ClockEnv) {
                ++importerCount;
                return [
                    (importerCount < repeatTimes)
                    , {
                        environment : env 
                        , timedData : {
                            timePoint : env.now()
                            , value : importerCount 
                            , finalFlag : (importerCount >= repeatTimes)
                        }
                    }
                ];
            }
        )
    );
    let keyify = TMInfra.RealTimeApp.Utils.kleisli<TMBasic.ClockEnv,number,TMInfra.Key<FacilityInput>>(
        function (x) {
            return {
                environment : x.environment
                , timedData : {
                    timePoint : x.timedData.timePoint
                    , value : TMInfra.keyify([x.timedData.timePoint.getTime(), x.timedData.value])
                    , finalFlag : x.timedData.finalFlag
                }
            }
        }
    );
    let exporter = TMInfra.RealTimeApp.Utils.simpleExporter<TMBasic.ClockEnv,TMInfra.KeyedData<FacilityInput,FacilityOutput>>(
        (x) => {
            let now = x.timedData.timePoint.getTime();
            count += now-x.timedData.value.key.key[0];
            ++recvCount;
            if (x.timedData.value.key.key[1] == 1) {
                firstTimeStamp = x.timedData.value.key.key[0];
            }
            if (x.timedData.value.key.key[1] >= repeatTimes) {
                console.log(`average delay of ${recvCount} is ${count*1.0/recvCount} milliseconds`);
                console.log(`total time for ${recvCount} is ${now-firstTimeStamp} milliseconds`);
                process.exit(0);
            }
        }
    );
    if (serviceDescriptor != '') {
        let facility = TMTransport.RemoteComponents.createFacilityProxy<TMBasic.ClockEnv,FacilityInput,FacilityOutput>(
            (x) => cbor.encode(x) as Buffer 
            , (d) => cbor.decode(d) as FacilityOutput 
            , {
                address : serviceDescriptor
            }
        );
        let importer = TMInfra.RealTimeApp.Utils.constFirstPushImporter<TMBasic.ClockEnv,string>('');
        r.placeOrderWithFacility(
            r.execute(keyify, r.execute(delayedImporter, r.importItem(importer)))
            , facility 
            , r.exporterAsSink(exporter)
        );
    } else {
        let dynamicFacility = new TMTransport.RemoteComponents.DynamicFacilityProxy<TMBasic.ClockEnv,FacilityInput,FacilityOutput>(
            (x) => cbor.encode(x) as Buffer 
            , (d) => cbor.decode(d) as FacilityOutput 
            , {
                address : ''
            }
        );
        let heartbeatImporter = TMTransport.RemoteComponents.createTypedImporter<TMBasic.ClockEnv,TMTransport.RemoteComponents.Heartbeat>(
            (d) => cbor.decode(d) as TMTransport.RemoteComponents.Heartbeat
            , heartbeatDescriptor
            , heartbeatTopic
        );
        let parser = TMInfra.RealTimeApp.Utils.liftMaybe<TMBasic.ClockEnv,TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>,string>(
            function (data) {
                if (data.content.sender_description == heartbeatIdentity) {
                    let s = data.content.facility_channels['facility'];
                    dynamicFacility.changeAddress(s);
                    return s;
                } else {
                    return null;
                }
            }
        );
        r.placeOrderWithFacility(
            r.execute(keyify, r.execute(delayedImporter, r.execute(parser, r.importItem(heartbeatImporter))))
            , dynamicFacility
            , r.exporterAsSink(exporter)
        );
    }
    r.finalize();
}

yargs
    .scriptName('rpc_delay_measurer')
    .usage('$0 <options>')
    .option('mode', {
        describe : 'server or client'
        , type : 'string'
        , nargs : 1
        , demandOption : true
    })
    .option('serviceDescriptor', {
        describer : 'service descriptor'
        , type : 'string'
        , nargs : 1
        , demandOption : false 
        , default : ''
    })
    .option('heartbeatDescriptor', {
        describer : 'heartbeat descriptor'
        , type : 'string'
        , nargs : 1
        , demandOption : false 
        , default : ''
    })
    .option('heartbeatTopic', {
        describer : 'heartbeat topic'
        , type : 'string'
        , nargs : 1
        , demandOption : false 
        , default : 'tm.examples.heartbeats'
    })
    .option('heartbeatIdentity', {
        describer : 'heartbeat identity'
        , type : 'string'
        , nargs : 1
        , demandOption : false 
        , default : 'rpc_delay_measurer'
    })
    .option('repeatTimes', {
        describer : 'repeat times'
        , type : 'number'
        , nargs : 1
        , demandOption : false 
        , default : 1000
    })
    ;

enum Mode {
    Server, Client
};

let mode : Mode;
let modeStr = yargs.argv.mode as string;
if (modeStr == 'server') {
    mode = Mode.Server;
} else if (modeStr == 'client') {
    mode = Mode.Client;
} else {
    console.error(`Unknown mode string '${mode}', must be server or client`);
    process.exit(0);
}
let serviceDescriptor = yargs.argv.serviceDescriptor as string;
let heartbeatDescriptor = yargs.argv.heartbeatDescriptor as string;
if (mode == Mode.Server && serviceDescriptor == '') {
    console.error("In server mode, service descriptor must be provided");
    process.exit(0);
} else if (mode == Mode.Client && serviceDescriptor == '' && heartbeatDescriptor == '') {
    console.error("In client mode, either service descriptor or heartbeat descriptor must be provided");
    process.exit(0);
}
let heartbeatTopic = yargs.argv.heartbeatTopic as string;
let heartbeatIdentity = yargs.argv.heartbeatIdentity as string;
let repeatTimes = yargs.argv.repeatTimes as number;

if (mode == Mode.Server) {
    runServer(serviceDescriptor, heartbeatDescriptor, heartbeatTopic, heartbeatIdentity);
} else {
    runClient(serviceDescriptor, heartbeatDescriptor, heartbeatTopic, heartbeatIdentity, repeatTimes);
}