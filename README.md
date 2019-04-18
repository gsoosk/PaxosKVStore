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
It takes one argument (besides `server`) containing the following fields:
* `my_addr` will be used for listening for client requests.
* `my_paxos` will be used for listening for Paxos messages from other servers.
* `fail_rate` is the rate at which the server randomly fails as an Acceptor.
* `(repeated) replica`s are Paxos Addresses of all server replicas, which will be used for communication during Paxos runs. The address of `my_paxos` should be included as a replica.
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
## Features Overview
* Servers reach fault-tolerant consensus by running Multi-Paxos runs.
* Clients may generate requests to any of the replicas at any time.
* Any server may be down and restarted at any time. Data recovery(through replication) happens each time a server comes back to live.
* Servers always forward client requests to Coordinator, and let Coordinator handle/propose for them.
* GET is handled by Coordinator, but will NOT go through Paxos.
* Coordinator is elected via Paxos runs. Each server may start a Coordinator election, self-nominating, when they find Coordinator is unavailable or not elected yet.
* Prior to each Paxos run, Coordinator pings all replicas to determine the number of live Acceptors. Majority vote occurs across live Acceptors only.
* Acceptors send acceptances to Coordinator. Coordinator informs all Learners. (Instead of Acceptors sending acceptance to Learners directly.)
* Acceptors are set to randomly fail at a percentage.
* Servers are multi-threaded and don't queue requests.
* The datastore is thread-safe.

## Assignment Overview
The design for the RPC interfaces is in the `keyvaluestore.proto` file.  
Each server exports two RPC services, KeyValueStore and MultiPaxos, at two different ports.  
KeyValueStore is open to clients and respond to `GET`/`PUT`/`DELETE` requests.  
MultiPaxos is used for server-to-server communication. A KeyValueStore-Service forwards client requests to the MultiPaxos-Service of Coordinator. On `GET` requests, Coordinator looks up it's own datastore and respons. On other requests, Coordinator starts a new round of Paxos run.  
Roles in a Paxos run: Coordinator(Proposer), Acceptor, Learner.  
* A typical Paxos run has two phases: Prepare and Propose. Since our key value store service requires continuous multi Paxos runs, some optimization was applied to meet project requirements.  
I divided a Paxos run into four phases: Ping, Prepare, Propose, and Inform.  
  ** Ping: Coordinator pings every Acceptor to determine the set of live Acceptors (live_set). A Quorum is defined as more than half of live_set's size.
  ** Prepare: Coordinator sends a PrepareRequest to each Acceptor in live_set. Acceptor decides whether to promise based on the proposal_id. Acceptor is set to fail randomly at a given fail_rate.
  ** Propose: If Prepare phase reached Quorum, Coordinator sends a ProposeRequest to each Acceptor in live_set. Acceptor decides whether to accept based on the proposal_id. Acceptor is set to fail randomly at a given fail_rate.
  ** Inform: If Propose phase reached Consensus, Coordinator forwards the accepted proposal to Learners. Learner executes the operation in the accepted proposal.

To ensure any server can catch up with other replicas after it's brought up, it goes through an Initialize stage once it's started.
* Initialize: Server contact other replicas to know who is Coordinator. It then sends request to Coordinator



## Technical Impression
This project is very interesting and challenging. I spent a lot of time on it and it's worth it. I've learned so much thoughout the whole process of understanding the scope, designing the system, and implementing it.  
It took me quite some efforts to understand and clarify the scope of this project. At the very beginning I only had a rough idea of how it should be like. By asking questions like "What's the pros and cons to having a Coordinator", "How should I store logs for past Paxos runs so that it can help Acceptor make decisions, and easy to recover via replication", and going through many edge cases, my design got more reasonable and more detailed through iterations.
I also made some changes in design after I started to implement it. For example, I didn't plan to do a strict Coordinator election at the beginning. I planned to assign each replica an id to determine their priority when it comes to Coordinator "election". However, after I completed other parts of the project, I realized that Coordinator election can be done by runnning a Paxos instance, just like PUT(key, value) requests. This change was done by adding just a few lines of code.



