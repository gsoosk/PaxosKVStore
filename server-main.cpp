// Server side of keyvaluestore.

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "kv-database.h"
#include "kv-store-service-impl.h"
#include "multi-paxos-service-impl.h"
#include "time_log.h"

void StartService(const std::string& server_address, grpc::Service* service) {
  grpc::ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance to communicate with clients. In this
  // case, it corresponds to an *synchronous* service.
  builder.RegisterService(service);
  // Finally assemble the server.
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  // Wait for the server to shutdown.
  TIME_LOG << "Server running: " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  // Set server address.
  if (argc <= 2) {
    std::cerr << "Usage: server-main [addr] [paxos_address] "
                 "[addr_of_paxos_service_1] [addr_of_paxos_service_2]..."
              << std::endl;
    return -1;
  }
  std::map<std::string, std::unique_ptr<keyvaluestore::MultiPaxos::Stub>>
      participants;
  for (int i = 2; i < argc; ++i) {
    const std::string paxos_address = std::string(argv[i]);
    participants[paxos_address] =
        std::make_unique<keyvaluestore::MultiPaxos::Stub>(grpc::CreateChannel(
            paxos_address, grpc::InsecureChannelCredentials()));
    TIME_LOG << "Adding " << paxos_address << " to the participant list."
             << std::endl;
  }
  keyvaluestore::PaxosStubsMap paxos_stubs_map(std::move(participants));
  keyvaluestore::KeyValueDataBase kv_db;
  const std::string keyvaluestore_address = std::string(argv[1]);
  const std::string my_paxos_address = std::string(argv[2]);
  keyvaluestore::KeyValueStoreServiceImpl keyvaluestore_service(
      &paxos_stubs_map, my_paxos_address);
  keyvaluestore::MultiPaxosServiceImpl multi_paxos_service(&paxos_stubs_map,
                                                           &kv_db);
  // Starts KeyValueStoreService in a detached thread.
  std::thread keyvaluestore_thread(StartService, keyvaluestore_address,
                                   &keyvaluestore_service);
  TIME_LOG << "KeyValueStoreService listening on " << keyvaluestore_address
           << std::endl;
  // Starts MultiPaxosService in a detached thread.
  std::thread multi_paxos_thread(StartService, my_paxos_address,
                                 &multi_paxos_service);
  TIME_LOG << "MultiPaxosService listening on " << my_paxos_address
           << std::endl;
  keyvaluestore_thread.join();
  multi_paxos_thread.join();
  TIME_LOG << "Shutting down!" << std::endl;
  return 0;
}
