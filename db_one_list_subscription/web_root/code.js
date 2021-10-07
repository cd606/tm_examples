let currentInfo = {
    version : 0
    , data : []
    , subscription_ws : {}
    , transaction_ws : {}
}

function updateTable() {
    $("#DataTable tbody").empty();
    for (const [key, value] of Object.entries(currentInfo.data)) {
        $("#DataTable").find("tbody").append(
            $("<tr><td>"+key+"</td><td>"+value[0]+"</td><td>"+value[1]+"</td></tr>")
        )
    }
}

function wrapData(data) {
    return window.cbor.encode([uuidv4(), window.cbor.encode(["browser_client", window.cbor.encode(data)])]);
}

function start() {
    currentInfo.subscription_ws = new WebSocket("ws://localhost:56790");
    currentInfo.subscription_ws.binaryType = 'arraybuffer';
    currentInfo.subscription_ws.onopen = function(event) {
        let cmd = [0, {keys: [0]}];
        currentInfo.subscription_ws.send(wrapData(cmd));
    }
    currentInfo.subscription_ws.onmessage = function(event) {
        let data = window.cbor.decode(event.data);
        let innerData = window.cbor.decode(data[1][1]);
        if (innerData[0] == 2) {
            //subscription update
            currentInfo.version = innerData[1].version;
            let updateSize = innerData[1].data.length;
            for (let ii = 0; ii < updateSize; ++ii) {
                let oneUpdate = innerData[1].data[ii];
                if (oneUpdate[0] == 0) {
                    //full
                    currentInfo.data = oneUpdate[1].data[0];
                } else {
                    //delta
                    let dataDelta = oneUpdate[1][2];
                    for (const delKey of dataDelta.deletes) {
                        delete(currentInfo.data[delKey]);
                    }
                    for (const item of dataDelta.inserts_updates) {
                        currentInfo.data[item[0]] = item[1];
                    }
                }
            }
            updateTable();
        }
    }
    currentInfo.transaction_ws = new WebSocket("ws://localhost:56789");
    currentInfo.transaction_ws.binaryType = 'arraybuffer';
    currentInfo.transaction_ws.onmessage = function(event) {
    };

    $("#InsUpdBtn").click(function() {
        let name = $("#NameField").val().trim();
        let amtStr = $("#AmountField").val().trim();
        let statStr = $("#StatField").val().trim();
        if (name.length == 0 || amtStr.length == 0 || statStr.length == 0) {
            return;
        }
        let amt = parseInt(amtStr);
        let stat = parseFloat(statStr);
        cmd = [
            1
            , {
                key : 0
                , oldVersionSlice : [currentInfo.version]
                , oldDataSummary : [Object.keys(currentInfo.data).length]
                , dataDelta : {
                    deletes: []
                    , inserts_updates : [[
                        name
                        , [amt, stat]
                    ]]
                }
            }
        ];
        currentInfo.transaction_ws.send(wrapData(cmd));
    });
    $("#DelBtn").click(function() {
        let name = $("#NameField").val().trim();
        if (name.length == 0) {
            return;
        }
        cmd = [
            1
            , {
                key : 0
                , oldVersionSlice : [currentInfo.version]
                , oldDataSummary : [Object.keys(currentInfo.data).length]
                , dataDelta : {
                    deletes: [name]
                    , inserts_updates : []
                }
            }
        ];
        currentInfo.transaction_ws.send(wrapData(cmd));
    });
}