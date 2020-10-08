import * as blessed from 'blessed'
import {MultiTransportListener,MultiTransportFacilityClient, FacilityOutput} from '../../../tm_transport/node_lib/TMTransport'
import * as cbor from 'cbor'
import * as proto from 'protobufjs'
import * as Stream from 'stream'
import * as util from 'util'

proto.load('../proto/defs.proto').then(function(root) {
    let inputT = root.lookupType('simple_demo.ConfigureCommand');
    let outputT = root.lookupType('simple_demo.ConfigureResult');
    run(inputT, outputT);
});

function run(inputT : proto.Type, outputT : proto.Type) : void {
    let screen = blessed.screen({
        smartCSR: true
        , title: 'Plain Console Enabler'
    });
    let label = blessed.box({
        parent: screen
        , top: '20%'
        , left: 'center'
        , width: '150'
        , content: 'Disconnected'
        , style: {
            fg: 'white'
        }
        , focusable: false
    });
    let form = blessed.form({
        parent: screen
        , top: '60%'
        , left: '10%'
        , width: '80%'
        , height: 5
        , border: {
            type: 'line'
        }
        , style: {
            border: {
                fg: 'blue'
            }
        }
        , keys: true
    });
    let enable = blessed.button({
        parent: form
        , left: '10%'
        , width: '30%'
        , height: 3
        , content: 'Enable'
        , border: {
            type: 'line'
        }
        , style: {
            fg: 'green'
            , border: {
                fg: 'blue'
            }, focus: {
                bg: 'white'
            }
        }
        , align: 'center'
    });
    let disable = blessed.button({
        parent: form
        , left: '60%'
        , width: '30%'
        , height: 3
        , content: 'Disable'
        , border: {
            type: 'line'
        }
        , style: {
            fg: 'red'
            , border: {
                fg: 'blue'
            }
            , focus: {
                bg: 'white'
            }
        }
        , align: 'center'
    });
    form.focus();
    screen.key(['escape', 'q', 'C-c'], function(_ch, _key) {
        return process.exit(0);
    });
    screen.render();

    let cfgChannelInfo : string = null;
    let cfgStreams : [Stream.Writable, Stream.Readable] = null;
    let keyify = MultiTransportFacilityClient.keyify();
    let callbackStream = new Stream.Writable({
        write : function(chunk : FacilityOutput, _encoding, callback) {
            let parsed = outputT.decode(chunk.output);
            if (parsed.hasOwnProperty("enabled") && parsed["enabled"]) {
                label.content = 'Enabled';
                label.style.fg = 'green';
            } else {
                label.content = 'Disabled';
                label.style.fg = 'red';
            }
            screen.render();
            callback();
        }
        , objectMode : true
    });
    function setupCfgChannel() {
        (async () => {
            cfgStreams = await MultiTransportFacilityClient.facilityStream({
                address : cfgChannelInfo
                , identityAttacher : function(data : Buffer) {
                    return cbor.encode(["ConsoleEnabler.ts", data]);
                }
            });
            keyify.pipe(cfgStreams[0]);
            cfgStreams[1].pipe(callbackStream);
        })();
    }

    enable.on('press', () => {
        keyify.write(inputT.encode({enabled : true}).finish());
    });
    disable.on('press', () => {
        keyify.write(inputT.encode({enabled : false}).finish());
    })

    let heartbeatStream = MultiTransportListener.inputStream(
        "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , "simple_demo.plain_executables.#.heartbeat"
    );
    let heartbeatHandler = new Stream.Writable({
        write: function(chunk : [string, Buffer], _encoding, callback) {
            let x = cbor.decode(chunk[1]);
            if (x.hasOwnProperty("sender_description")) {
                if (x.sender_description == "simple_demo plain MainLogic") {
                    if (x.hasOwnProperty("facility_channels") && x.facility_channels.hasOwnProperty("cfgFacility")) {
                        let channelInfoFromHeartbeat = x.facility_channels["cfgFacility"];
                        if (cfgChannelInfo == null) {
                            cfgChannelInfo = channelInfoFromHeartbeat;
                            setupCfgChannel();
                        }
                    }
                    let status = x.details.calculation_status.info as string;
                    if (status == 'enabled') {
                        label.content = 'Enabled';
                        label.style.fg = 'green';
                    } else {
                        label.content = 'Disabled';
                        label.style.fg = 'red';
                    }
                    screen.render();
                }
            }
            callback();
        }
        , objectMode : true
    });
    heartbeatStream.pipe(heartbeatHandler);
}