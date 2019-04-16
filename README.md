Project 4  by Jiahan Zhu
===================================

This project utilizes gRPC as the framework for client and server communication.

To build and run the project, please first install gRPC and Protocol Buffers v3 on your system.


# Install gRPC
Follow the instructions here: https://github.com/grpc/grpc/blob/master/BUILDING.md

# Install Protocol Buffers v3
Follow the instructions here: https://github.com/protocolbuffers/protobuf/blob/master/src/README.md

# Build
run command `make` from folder keyvaluestore/

# Run the server
`python run_server.py <NUM_REPLICAS>`. 
The server addresses can be accessed locally as  `0.0.0.0`, where the port numbers will start from `8000` and increment by 1 for each of the replica. E.g., `num_replicas==5` will bring up five servers, `0.0.0.0:8000`, `0.0.0.0:8001`, `0.0.0.0:8002`, `0.0.0.0:8003`, and `0.0.0.0:8004`.

You can run individual servers by,
`./server <SERVER_ADDR> <NUM_REPLICAS> <ADDR_OF_REPLICA_1> <ADDR_OF_REPLICA_1> <ADDR_OF_REPLICA_2>..."`
(Note: The server itself should be included in <ADDR_OF_REPLICA>s)

In the simplest case where we only need one server, the script looks like,
`./server <SERVER_ADDR> <ADDR_OF_REPLICA_1>"` with <ADDR_OF_REPLICA_1> = <SERVER_ADDR>



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
* Each server is assigned a priority and the one with the highest priority among all active servers plays the role of Leader. This strategy makes sure when the current leader is down, servers will know who is the next leader.
* Only Leader is allowed to propose, i.e., any server will forward its client requests to Leader, and let Leader propose for them.
* After accepting a proposal, acceptors will respond with their acceptances to Leader, whose role now is a distinguished learner responsible for informing other learners when a value has been chosen.
* GET is handled by Leader, but does NOT go through PAXOS.

 

## Technical Impression





