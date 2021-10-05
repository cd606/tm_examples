function start() {
    var ctx = document.getElementById('myChart').getContext('2d');
    var myChart = new Chart(ctx, {
        type: 'bar'
        , data: {
            labels: []
            , datasets: [{
                label: 'Data Value'
                , data: []
                , backgroundColor: 'red'
                , borderColor: 'red'
            }]
        }
        , options: {
            tooltips: {
                enabled: false
            }
        }
    });
    $.post("/key_query", JSON.stringify({"request":{}}), function(data) {
        let decryptKey = sodium.crypto_generichash(sodium.crypto_generichash_BYTES, sodium.from_string(data.response));
        let ws = new WebSocket("wss://localhost:56789");
        ws.binaryType = 'arraybuffer';
        ws.onmessage = function(event) {
            let rawData = new Uint8Array(event.data);
            let data = window.cbor.decode(rawData);
            if (data.length != 2) {
                return;
            }
            let innerRawData = data[1];
            let innerData = window.cbor.decode(sodium.crypto_secretbox_open_easy(
                innerRawData.slice(sodium.crypto_secretbox_NONCEBYTES)
                , innerRawData.slice(0, sodium.crypto_secretbox_NONCEBYTES)
                , decryptKey
            ));
            document.getElementById('value').innerText = 'Value: '+innerData.value;
            if (myChart.data.labels.length < 500) {
                myChart.data.labels.push('');
                myChart.data.datasets[0].data.push(parseFloat(innerData.value));
            } else {
                myChart.data.labels.shift(0);
                myChart.data.datasets[0].data.shift(0);
                myChart.data.labels.push('');
                myChart.data.datasets[0].data.push(parseFloat(innerData.value));
            }
            myChart.update();
        }
    });
}