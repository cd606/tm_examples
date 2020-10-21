import * as blessed from 'blessed'
import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as cbor from 'cbor'
import * as proto from 'protobufjs'

proto.load('../proto/defs.proto').then(function(root) {
    let inputT = root.lookupType('simple_demo_chain_version.ConfigureCommand');
    let outputT = root.lookupType('simple_demo_chain_version.ConfigureResult');
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

    type E = TMBasic.ClockEnv;
    let heartbeatImporter = TMTransport.RemoteComponents.createTypedImporter<E,TMTransport.RemoteComponents.Heartbeat>(
        (d : Buffer) => cbor.decode(d)
        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , "simple_demo_chain_version.#.heartbeat"
    );
    type ConfigureCommand = {enabled : boolean};
    type ConfigureResult = {enabled? : boolean};
    let facility = new TMTransport.RemoteComponents.DynamicFacilityProxy<E,ConfigureCommand,ConfigureResult>(
        (t : ConfigureCommand) => Buffer.from(inputT.encode(t).finish())
        , (d : Buffer) => outputT.decode(d) as ConfigureResult
        , {
            address : null
            , identityAttacher : TMTransport.RemoteComponents.Security.simpleIdentityAttacher("ConsoleEnabler.ts")
        }
    );
    let heartbeatAction = TMInfra.RealTimeApp.Utils.liftMaybe<E,TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>,boolean>(
        (h : TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>) => {
            if (h.content.sender_description == "simple_demo_chain_version MainLogic") {
                if (h.content.facility_channels.hasOwnProperty("main_program/cfgFacility")) {
                    let channelInfoFromHeartbeat = h.content.facility_channels["main_program/cfgFacility"];
                    facility.changeAddress(channelInfoFromHeartbeat);
                }
                let status = h.content.details.calculation_status.info as string;
                return (status == 'enabled');
            } else {
                return null;
            }
        }
    );
    let configureImporter = new TMInfra.RealTimeApp.Utils.TriggerImporter<E,TMInfra.Key<ConfigureCommand>>();
    let statusExporter = TMInfra.RealTimeApp.Utils.pureExporter<E,boolean>(
        (enabled : boolean) => {
            if (enabled) {
                label.content = 'Enabled';
                label.style.fg = 'green';
            } else {
                label.content = 'Disabled';
                label.style.fg = 'red';
            }
            screen.render();
        }
    );
    let configureResultAction = TMInfra.RealTimeApp.Utils.liftPure<E,TMInfra.KeyedData<ConfigureCommand,ConfigureResult>,boolean>(
        (d : TMInfra.KeyedData<ConfigureCommand,ConfigureResult>) => {
            if (d.data.enabled === null || d.data.enabled === undefined) {
                return false;
            } else {
                return d.data.enabled;
            }
        }
    )

    enable.on('press', () => {
        configureImporter.trigger(TMInfra.keyify({enabled: true}));
    });
    disable.on('press', () => {
        configureImporter.trigger(TMInfra.keyify({enabled: false}));
    });

    let r = new TMInfra.RealTimeApp.Runner<E>(new TMBasic.ClockEnv());
    r.exportItem(statusExporter, r.execute(heartbeatAction, r.importItem(heartbeatImporter)));
    r.placeOrderWithFacility(r.importItem(configureImporter), facility, r.actionAsSink(configureResultAction));
    r.exportItem(statusExporter, r.actionAsSource(configureResultAction));
    r.finalize();
}