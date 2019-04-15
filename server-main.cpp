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
#include "time_log.h"
#include "two-phase-commit.h"

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
    std::cerr
        << "Usage: server-main [addr] [addr_of_two_phase_service] "
           "[addr_of_two_phase_service_1] [addr_of_two_phase_service_2]..."
           "[address_2] ..."
        << std::endl;
    return -1;
  }
  std::vector<std::unique_ptr<keyvaluestore::TwoPhaseCommit::Stub>>
      participants;
  for (int i = 2; i < argc; ++i) {
    participants.push_back(
        std::make_unique<keyvaluestore::TwoPhaseCommit::Stub>(
            grpc::CreateChannel(std::string(argv[i]),
                                grpc::InsecureChannelCredentials())));
    TIME_LOG << "Adding " << std::string(argv[i]) << " to the participant list."
             << std::endl;
  }
  keyvaluestore::KeyValueDataBase kv_db;
  const std::string keyvaluestore_address = std::string(argv[1]);
  const std::string two_phase_address = std::string(argv[2]);
  keyvaluestore::KeyValueStoreServiceImpl keyvaluestore_service(
      keyvaluestore_address, std::move(participants), &kv_db);
  keyvaluestore::TwoPhaseCommitServiceImpl two_phase_commit_service(&kv_db);
  // Starts KeyValueStoreService in a detached thread.
  std::thread keyvaluestore_thread(StartService, keyvaluestore_address,
                                   &keyvaluestore_service);
  TIME_LOG << "KeyValueStoreService listening on " << keyvaluestore_address
           << std::endl;
  // Starts TwoPhaseCommitService in a detached thread.
  std::thread two_phase_commit_thread(StartService, two_phase_address,
                                      &two_phase_commit_service);
  TIME_LOG << "TwoPhaseCommitService listening on " << two_phase_address
           << std::endl;
  keyvaluestore_thread.join();
  two_phase_commit_thread.join();
  TIME_LOG << "Shutting down!" << std::endl;
  return 0;
}
