Project 3  by Jiahan Zhu
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

The server can also be brought up manually by,
`./server <SERVER_ADDR> <ADDR_OF_TWO_PHASE_SERVICE> <ADDR_OF_TWO_PHASE_SERVICE_1> <ADDR_OF_TWO_PHASE_SERVICE_2>..."`
(and a bunch of replica servers)
In the simplest case where we only need one server, the script looks like,
`./server <SERVER_ADDR> <ADDR_OF_TWO_PHASE_SERVICE>"`



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

The design for the RPC interfaces is in the `keyvaluestore.proto` file. As compared with the previous project, the main design changes are,

1.  We have `N` (N==5) servers instead of 1.
2.  Implements the two-phase commit protocol to ensure the transactional consisteny for data updates, i.e., `put` and `delete`.

To achieve the above, each server now exports two RPC services. The first one is the original `get`/`put`/`delete`, and is open to clients. The second one is to be used as a participant, and will be called by the replica/coordinator that actually receives the `get`/`put`/`delete` requests. The replica that gets the client request becomes the coordinator, and will consult with the rest replicas via the second RPC service, to see if a consensus can be reached for a commit. The coordinator will proceed by sending a subsequent RPC call to all replicas, either to confirm the commit, or to abort the commit, depending on the outcome of the consensus.

It's assumed that no replica would ever crash, so no permanent storage was used. Both the commit preparation and the actual commit will directly operate on the data in RAM.

 

## Technical Impression

The most challenging part for me is how to design the data structure and how to separate the interfaces. This really helped me get more familiar with OOP in C++. Anther challenge is how to bring up the replicated servers so that they know each other. After some thoughts, I decided to make the C++ implementations fully describe one replicated server, and configure the cluster of servers separatedly in Python.
The improvements I can think of is to finalize the test cases. More specifically, I could have tests that send highly concurrent RPC requests and verify both the correctness and the performance of the program.





