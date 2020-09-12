import sys
sys.path.append("../../tm_transport/python_lib")

import TMTransport
import uuid

config = TMTransport.EtcdSharedChain.defaultEtcdSharedChainConfiguration()
config['chainPrefix'] = "shared_chain_test"
config['dataPrefix'] = "shared_chain_test_data"
config['extraDataPrefix'] = "shared_chain_test_extra_data"
config['saveDataOnSeparateStorage'] = False
config['headKey'] = '2020-01-01-head'
#config['duplicateFromRedis'] = True
#config['automaticallyDuplicateToRedis'] = True
#config['redisTTLSeconds'] = 5

chain = TMTransport.EtcdSharedChain(config)
chain.start([1, {}])
count = 0
while True:
    x = chain.next()
    if not x:
        break
    count = count+1
print(count)
print(chain.currentValue())
id1 = chain.currentValue()['id']
id2 = str(uuid.uuid4())
print(chain.idIsAlreadyOnChain(id1))
print(chain.idIsAlreadyOnChain(id2))
chain.append(id2, [1, {}])
print(chain.currentValue())
print(chain.idIsAlreadyOnChain(id1))
print(chain.idIsAlreadyOnChain(id2))
chain.saveExtraData('abc', 'test')
print(chain.loadExtraData('abc'))
print(chain.loadExtraData('def'))
chain.close()