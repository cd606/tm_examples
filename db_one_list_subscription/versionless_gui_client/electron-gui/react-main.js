class DataTableDisplay extends React.Component {
    state = {
        value : []
    }
    constructor() {
        super();
    }
    render() {
        return (
        <div>
            <table class="center">
                <thead>
                    <tr>
                        <th>Name</th>
                        <th>Amount</th>
                        <th>Stat</th>
                    </tr>
                </thead>
                <tbody>
                    {
                        this.state.value.map((value, _index) => {
                            return (
                                <tr>
                                    <td>{value[0]}</td>
                                    <td>{value[1].amount}</td>
                                    <td>{value[1].stat}</td>
                                </tr>
                            );
                        })
                    }
                </tbody>
            </table>
            <hr/>
            <div class="control-div">
                Name: <input id="name" type="text"></input>
                Amount: <input id="amount" type="text"></input>
                Stat: <input id="stat" type="text"></input>
            </div>
            <div class="control-div">
                <button onClick={() => this.handleInsertUpdate()}>Insert/Update</button>
                <button onClick={() => this.handleDelete()}>Delete</button>
            </div>
        </div>
        );
    }
    setValue(v) {
        this.setState({value: v});
    }
    handleInsertUpdate() {
        let name = document.getElementById("name").value.trim();
        let amount = document.getElementById("amount").value.trim();
        let stat = document.getElementById("stat").value.trim();
        if (name !== "" && amount !== "" && stat !== "") {
            let amountInt = parseInt(amount);
            let statFloat = parseFloat(stat);
            let old_data_size = this.state.value.length;
            if (!isNaN(amountInt) && !isNaN(statFloat)) {
                window.api.send("toMain", {
                    action: "insert_update"
                    , name : name
                    , amount : amountInt
                    , stat : statFloat
                    , old_data_size : old_data_size
                });
            }
        }
    }
    handleDelete() {
        let name = document.getElementById("name").value.trim();
        if (name !== "") {
            let old_data_size = this.state.value.length;
            window.api.send("toMain", {
                action: "delete"
                , name : name
                , old_data_size : old_data_size
            });
        }
    }
}

let dataTableDisplay = React.createRef();

ReactDOM.render(<DataTableDisplay ref={dataTableDisplay} />, document.getElementById('root'))

window.api.receive(
    "fromMain", (data) => {
        if (data.hasOwnProperty("what") && data.what === "data update") {
            dataTableDisplay.current.setValue(data.data);
        }
    }
)