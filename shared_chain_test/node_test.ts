import {EtcdSharedChain} from '../../tm_transport/node_lib/TMTransport'
import {v4 as uuidv4} from "uuid"

let config = EtcdSharedChain.defaultEtcdSharedChainConfiguration();
config.chainPrefix = "shared_chain_test";
config.dataPrefix = "shared_chain_test_data";
config.extraDataPrefix = "shared_chain_test_extra_data";
config.saveDataOnSeparateStorage = true;
config.headKey = '2020-01-01-head';
config.duplicateFromRedis = true;
config.automaticallyDuplicateToRedis = true;
config.redisTTLSeconds = 5;

let chain = new EtcdSharedChain(config);

(async () => {
    await chain.start([1, {}]);
    let count = 0;
    while (true) {
        let x = await chain.next();
        if (!x) {
            break;
        }
        ++count;
    }
    console.log(count);
    console.log(chain.currentValue());
    let id1 = chain.currentValue().id;
    let id2 = uuidv4();
    console.log(await chain.idIsAlreadyOnChain(id1));
    console.log(await chain.idIsAlreadyOnChain(id2));
    await chain.append(id2, [1, {}]);
    console.log(chain.currentValue());
    console.log(await chain.idIsAlreadyOnChain(id1));
    console.log(await chain.idIsAlreadyOnChain(id2));
    await chain.close();
})();
