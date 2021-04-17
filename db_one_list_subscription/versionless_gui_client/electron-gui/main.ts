import {app, BrowserWindow, Menu, ipcMain} from 'electron'
import * as TMBasic from '../../../../tm_basic/node_lib/TMBasic'
import * as TMInfra from '../../../../tm_infra/node_lib/TMInfra'
import * as GuiDataFlow from '../GuiDataFlow'
import * as path from 'path'

let dataFlowLogicOutput : GuiDataFlow.LogicOutput = null
let win

async function startSubscription() {
  dataFlowLogicOutput = await GuiDataFlow.guiDataFlow({
    dataHandler : (d : TMInfra.TimedDataWithEnvironment<TMBasic.ClockEnv,GuiDataFlow.LocalData>) => {
      win.webContents.send(
        "fromMain"
        , {
          what : "data update"
          , data : Array.from(d.timedData.value.entries()).sort(function (a, b) {
            if (a[0] < b[0]) {
              return -1;
            } else if (a[0] > b[0]) {
              return 1;
            } else {
              return 0;
            }
          })
        }
      );
    }
    , unsubscribeConfirmedHandler : (d : TMInfra.TimedDataWithEnvironment<TMBasic.ClockEnv,GuiDataFlow.UnsubscribeConfirmed>) => {
      if (process.platform !== 'darwin') {
        app.quit()
      }
    }
  }, new TMBasic.ClockEnv(null, "electron-gui.log"));
}

function unsubscribeAndQuit() {
  if (dataFlowLogicOutput != null) {
    dataFlowLogicOutput.guiExitEventFeeder({});
  } else {
    if (process.platform !== 'darwin') {
      app.quit()
    }
  }
}

function createWindow () {
  win = new BrowserWindow({
    width: 800,
    height: 600,
    title: "Versionless Electron GUI",
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
            unsubscribeAndQuit();
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
  unsubscribeAndQuit();
});

ipcMain.on('toMain', (_event, args) => {
  if (args.hasOwnProperty("action")) {
    if (args["action"] === "insert_update") {
      dataFlowLogicOutput.tiInputFeeder(TMInfra.keyify([
        TMBasic.Transaction.TransactionInterface.TransactionSubtypes.UpdateAction
        , {
            key : GuiDataFlow.theKey
            , oldVersionSlice : []
            , oldDataSummary : [args.old_data_size]
            , dataDelta : {
                deletes: []
                , inserts_updates : [[
                  {'name': args.name}
                  , {'amount': args.amount, 'stat': args.stat}
                ]]
            }
        }
      ]));
    } else if (args["action"] === "delete") {
      dataFlowLogicOutput.tiInputFeeder(TMInfra.keyify([
        TMBasic.Transaction.TransactionInterface.TransactionSubtypes.UpdateAction
        , {
            key : GuiDataFlow.theKey
            , oldVersionSlice : []
            , oldDataSummary : [args.old_data_size]
            , dataDelta : {
              deletes: [{'name': args.name}]
              , inserts_updates : []
            }
        }
      ]));
    }
  }
});