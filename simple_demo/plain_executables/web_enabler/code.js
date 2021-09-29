function updateDisplay(enabled) {
	$("#status").text(enabled?"Enabled":"Disabled");
	$("#enable").prop('disabled', enabled);
	$("#disable").prop('disabled', !enabled);
}
function configureEnabled(enabled) {
	$.post("/configureMainLogic", JSON.stringify({"request":{"enabled":enabled}}), function(data) {
		updateDisplay(data.response.enabled);
	});
}
function start() {
	let ws = new WebSocket("ws://localhost:45678");
	ws.onmessage = function(event) {
		event.data.text().then(function(t) {
			let x = JSON.parse(t);
			updateDisplay(x.enabled);
		});
	};

	$("#enable").click(function() {
		configureEnabled(true);
	});
	$("#disable").click(function() {
		configureEnabled(false);
	});
}
