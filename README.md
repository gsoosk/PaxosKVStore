Project 4  by Jiahan Zhu
===================================

This project utilizes gRPC as the framework for client and server communication.

To build and run the project, please first install gRPC and Protocol Buffers v3 on your system.


# Install gRPC
Follow the instructions here: https://github.com/grpc/grpc/blob/master/BUILDING.md

# Install Protocol Buffers v3
Follow the instructions here: https://github.com/protocolbuffers/protobuf/blob/master/src/README.md

# Build
run command `make` from folder keyvaluestore-paxos/

# Run the server
### Run each server individually
You can run each server individually by, 
`server "my_addr:'<addr>' my_paxos:'<addr>' fail_rate:<double> replica:'<addr>' ... replica:'<addr>'"`
* `my_addr` will be used for listening for client requests.
* `my_paxos` will be used for listening for Paxos messages from other servers.
* `fail_rate` is the rate at which the server randomly fails as an Acceptor.
* `replica`s are Paxos Addresses of all server replicas, which will be used for communication during Paxos runs. The address of `my_paxos` should be included as a replica.
#### For example
* Server 0 :`./server "my_addr: '0.0.0.0:8000' my_paxos: '0.0.0.0:9000' fail_rate: 0.3 replica: '0.0.0.0:9000' replica: '0.0.0.0:9001' replica: '0.0.0.0:9002' replica: '0.0.0.0:9003' replica: '0.0.0.0:9004'"` 
* Server 1 :`./server "my_addr: '0.0.0.0:8001' my_paxos: '0.0.0.0:9001' fail_rate: 0.3 replica: '0.0.0.0:9000' replica: '0.0.0.0:9001' replica: '0.0.0.0:9002' replica: '0.0.0.0:9003' replica: '0.0.0.0:9004'"` 
* Server 2 :`./server "my_addr: '0.0.0.0:8002' my_paxos: '0.0.0.0:9002' fail_rate: 0.3 replica: '0.0.0.0:9000' replica: '0.0.0.0:9001' replica: '0.0.0.0:9002' replica: '0.0.0.0:9003' replica: '0.0.0.0:9004'"` 
* Server 3 :`./server "my_addr: '0.0.0.0:8003' my_paxos: '0.0.0.0:9003' fail_rate: 0.3 replica: '0.0.0.0:9000' replica: '0.0.0.0:9001' replica: '0.0.0.0:9002' replica: '0.0.0.0:9003' replica: '0.0.0.0:9004'"` 
* Server 4 :`./server "my_addr: '0.0.0.0:8004' my_paxos: '0.0.0.0:9004' fail_rate: 0.3 replica: '0.0.0.0:9000' replica: '0.0.0.0:9001' replica: '0.0.0.0:9002' replica: '0.0.0.0:9003' replica: '0.0.0.0:9004'"`   

### Run all servers at once
You can also run all of them using one command,  
`python run_server.py <NUM_REPLICAS>`.   
The server addresses can be accessed locally as  `0.0.0.0`, where the port numbers will start from `8000` and increment by 1 for each of the replica.   
E.g., `<NUM_REPLICAS>==5` is equivalent to the example above. It will bring up five servers, available to clients at `0.0.0.0:8000`, `0.0.0.0:8001`, `0.0.0.0:8002`, `0.0.0.0:8003`, and `0.0.0.0:8004`.



# Run the client
`./client`  
or  
`./client <SERVER_ADDRESS>` (for example, `./client localhost:8000`)

# Send requests from client
`GET <KEY>` (for example, `GET apple`)  
`PUT <KEY> <VALUE>` (for example, `PUT apple green`)  
`DELETE <KEY>` (for example, `DELETE apple`)  


# Executive Summary
## Assignment Overview

### Assumptions
* Any server may be down and recovered any time.
* Each server is assigned a priority and the one with the highest priority among all active servers plays the role of Coordinator. This strategy makes sure when the current Coordinator is down, servers will know who is the next Coordinator.
* Only Coordinator is allowed to propose, i.e., any server will forward its client requests to Coordinator, and let Coordinator propose for them.
* After accepting a proposal, acceptors will respond with their acceptances to Coordinator, whose role now is a distinguished learner responsible for informing other learners when a value has been chosen.
* GET is handled by Coordinator, but does NOT go through PAXOS.

 

## Technical Impression





