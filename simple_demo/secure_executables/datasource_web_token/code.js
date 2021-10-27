var allData = {
    myChart : {}
};

function start() {
    var ctx = document.getElementById('myChart').getContext('2d');
    allData.myChart = new Chart(ctx, {
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
    $('#login').on('click', function() {
        var username = $('#username').val().trim();
        var password = $('#password').val().trim();
        if (username === '' || password === '') {
            return;
        }
        $.post("/__API_AUTHENTICATION", JSON.stringify({
            "username": username
            , "password": password
        }), function(data) {
            handleToken(data);
        });
    });
}

function handleToken(token) {
    $('#login-div').hide();
    $('#chart-div').show();
    $.ajaxSetup({
        beforeSend: function (xhr) {
            xhr.setRequestHeader("Authorization", "Bearer "+token);
        }
    });
    $.post("/key_query", JSON.stringify({"request":{}}), function(data) {
        let decryptKey = sodium.crypto_generichash(sodium.crypto_generichash_BYTES, sodium.from_string(data.response));
        let ws = new WebSocket("wss://localhost:56789/data");
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
            if (allData.myChart.data.labels.length < 500) {
                allData.myChart.data.labels.push('');
                allData.myChart.data.datasets[0].data.push(parseFloat(innerData.value));
            } else {
                allData.myChart.data.labels.shift(0);
                allData.myChart.data.datasets[0].data.shift(0);
                allData.myChart.data.labels.push('');
                allData.myChart.data.datasets[0].data.push(parseFloat(innerData.value));
            }
            allData.myChart.update();
        }
    });
}