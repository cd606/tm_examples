import {app, BrowserWindow, Menu} from 'electron'
import * as TMInfra from '../../../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../../../tm_transport/node_lib/TMTransport'
import * as path from 'path'
import * as cbor from 'cbor'
import * as proto from 'protobufjs'
import * as sodium from 'sodium-native'

let win

type E = TMBasic.ClockEnv;
interface InputData {
    value : number;
}

let secure = (process.argv.length > 2 && process.argv[2] == '--secure');

let decryptKey = Buffer.alloc(sodium.crypto_generichash_BYTES);
sodium.crypto_generichash(decryptKey, Buffer.from("input_data_key"));
decryptKey = decryptKey.slice(0, 32);

function secureDataHook(data: Buffer) : Buffer {
    let ret = Buffer.alloc(data.byteLength-sodium.crypto_secretbox_NONCEBYTES-sodium.crypto_secretbox_MACBYTES);
    if (sodium.crypto_secretbox_open_easy(
        ret
        , data.slice(sodium.crypto_secretbox_NONCEBYTES)
        , data.slice(0, sodium.crypto_secretbox_NONCEBYTES)
        , decryptKey
    )) {
        return ret;
    } else {
        return null;
    }
}

function startSubscription() {
  proto.load('../../../proto/defs.proto').then(function(root) {
      let parser = root.lookupType("simple_demo.InputData");
      let heartbeatImporter = TMTransport.RemoteComponents.createTypedImporter<E,TMTransport.RemoteComponents.Heartbeat>(
          (d : Buffer) => cbor.decode(d) as TMTransport.RemoteComponents.Heartbeat
          , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
          , (secure?
              "simple_demo.secure_executables.data_source.heartbeat"
              :"simple_demo.plain_executables.data_source.heartbeat")
      );
      let dataImporter = new TMTransport.RemoteComponents.DynamicTypedImporter<E,InputData>(
          (d : Buffer) => (parser.decode(d) as unknown) as InputData
          , null
          , null
          , (secure?secureDataHook:undefined)
      );
      let heartbeatHandler = TMInfra.RealTimeApp.Utils.pureExporter<E,TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>>(
        (x : TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>) => {
            if (x.content.sender_description == (secure?"simple_demo secure DataSource":"simple_demo DataSource")) {
                dataImporter.addSubscription(x.content.broadcast_channels["input data publisher"][0], "input.data");
            }
        }
      );
      let dataHandler = TMInfra.RealTimeApp.Utils.pureExporter<E,TMBasic.TypedDataWithTopic<InputData>>(
          (x : TMBasic.TypedDataWithTopic<InputData>) => {
              let v = x.content.value;
              win.webContents.send(
                "fromMain"
                , {
                  value: v
                }
              );
          }
      );

      let r = new TMInfra.RealTimeApp.Runner<E>(new TMBasic.ClockEnv());
      r.exportItem(heartbeatHandler, r.importItem(heartbeatImporter));
      r.exportItem(dataHandler, r.importItem(dataImporter));
      r.finalize();
  });
}

function createWindow () {
  win = new BrowserWindow({
    width: 800,
    height: 600,
    title: "Data Display",
    webPreferences: {
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js')
    }
  })

  win.loadFile('index.html');

  Menu.setApplicationMenu(Menu.buildFromTemplate([
    {
      label : "&File"
      , submenu : [
        {
          label : "E&xit"
          , click: () => {
            if (process.platform !== 'darwin') {
              app.quit()
            }
          }
        }
      ]
    }
  ]));

  win.webContents.on('did-finish-load', () => {
    startSubscription();
  });
}

app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow()
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
  }
});
