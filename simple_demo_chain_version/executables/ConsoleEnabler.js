"use strict";
exports.__esModule = true;
var blessed = require("blessed");
var TMInfra = require("../../../tm_infra/node_lib/TMInfra");
var TMBasic = require("../../../tm_basic/node_lib/TMBasic");
var TMTransport = require("../../../tm_transport/node_lib/TMTransport");
var cbor = require("cbor");
var proto = require("protobufjs");
proto.load('../proto/defs.proto').then(function (root) {
    var inputT = root.lookupType('simple_demo_chain_version.ConfigureCommand');
    var outputT = root.lookupType('simple_demo_chain_version.ConfigureResult');
    run(inputT, outputT);
});
function run(inputT, outputT) {
    var screen = blessed.screen({
        smartCSR: true,
        title: 'Plain Console Enabler'
    });
    var label = blessed.box({
        parent: screen,
        top: '20%',
        left: 'center',
        width: '150',
        content: 'Disconnected',
        style: {
            fg: 'white'
        },
        focusable: false
    });
    var form = blessed.form({
        parent: screen,
        top: '60%',
        left: '10%',
        width: '80%',
        height: 5,
        border: {
            type: 'line'
        },
        style: {
            border: {
                fg: 'blue'
            }
        },
        keys: true
    });
    var enable = blessed.button({
        parent: form,
        left: '10%',
        width: '30%',
        height: 3,
        content: 'Enable',
        border: {
            type: 'line'
        },
        style: {
            fg: 'green',
            border: {
                fg: 'blue'
            }, focus: {
                bg: 'white'
            }
        },
        align: 'center'
    });
    var disable = blessed.button({
        parent: form,
        left: '60%',
        width: '30%',
        height: 3,
        content: 'Disable',
        border: {
            type: 'line'
        },
        style: {
            fg: 'red',
            border: {
                fg: 'blue'
            },
            focus: {
                bg: 'white'
            }
        },
        align: 'center'
    });
    form.focus();
    screen.key(['escape', 'q', 'C-c'], function (_ch, _key) {
        return process.exit(0);
    });
    screen.render();
    var heartbeatImporter = TMTransport.RemoteComponents.createTypedImporter(function (d) { return cbor.decode(d); }, "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]", "simple_demo_chain_version.#.heartbeat");
    var facility = new TMTransport.RemoteComponents.DynamicFacilityProxy(function (t) { return Buffer.from(inputT.encode(t).finish()); }, function (d) { return outputT.decode(d); }, {
        address: null,
        identityAttacher: TMTransport.RemoteComponents.Security.simpleIdentityAttacher("ConsoleEnabler.ts")
    });
    var heartbeatAction = TMInfra.RealTimeApp.Utils.liftMaybe(function (h) {
        if (h.content.sender_description == "simple_demo_chain_version MainLogic") {
            if (h.content.facility_channels.hasOwnProperty("main_program/cfgFacility")) {
                var channelInfoFromHeartbeat = h.content.facility_channels["main_program/cfgFacility"];
                facility.changeAddress(channelInfoFromHeartbeat);
            }
            var status_1 = h.content.details.calculation_status.info;
            return (status_1 == 'enabled');
        }
        else {
            return null;
        }
    });
    var configureImporter = new TMInfra.RealTimeApp.Utils.TriggerImporter();
    var statusExporter = TMInfra.RealTimeApp.Utils.pureExporter(function (enabled) {
        if (enabled) {
            label.content = 'Enabled';
            label.style.fg = 'green';
        }
        else {
            label.content = 'Disabled';
            label.style.fg = 'red';
        }
        screen.render();
    });
    var configureResultAction = TMInfra.RealTimeApp.Utils.liftPure(function (d) {
        if (d.data.enabled === null || d.data.enabled === undefined) {
            return false;
        }
        else {
            return d.data.enabled;
        }
    });
    enable.on('press', function () {
        configureImporter.trigger(TMInfra.keyify({ enabled: true }));
    });
    disable.on('press', function () {
        configureImporter.trigger(TMInfra.keyify({ enabled: false }));
    });
    var r = new TMInfra.RealTimeApp.Runner(new TMBasic.ClockEnv());
    r.exportItem(statusExporter, r.execute(heartbeatAction, r.importItem(heartbeatImporter)));
    r.placeOrderWithFacility(r.importItem(configureImporter), facility, r.actionAsSink(configureResultAction));
    r.exportItem(statusExporter, r.actionAsSource(configureResultAction));
    r.finalize();
}
