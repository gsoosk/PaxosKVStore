// Server side of keyvaluestore.
#include "kv-store-service-impl.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "keyvaluestore.grpc.pb.h"
#include "time_log.h"

namespace keyvaluestore {

using grpc::ClientContext;
using grpc::ServerContext;
using grpc::Status;
using keyvaluestore::DeleteRequest;
using keyvaluestore::EmptyMessage;
using keyvaluestore::GetRequest;
using keyvaluestore::GetResponse;
using keyvaluestore::KeyValueStore;
using keyvaluestore::PutRequest;

Status KeyValueStoreServiceImpl::GetValue(ServerContext* context,
                                          const GetRequest* request,
                                          GetResponse* response) {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Received Request: Get [key: " << request->key() << "]."
             << std::endl;
  }
  ClientContext cc;
  auto deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
  cc.set_deadline(deadline);
  assert(paxos_stubs_map_ != nullptr);
  auto* coordinator_stub = paxos_stubs_map_.GetCoordinatorStub();
  // Forward request to Coordinator, then respond to client.
  Status get_status = coordinator_stub->GetValue(&cc, *request, response);
  if (get_status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    FindNewCoordinator();
    deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    cc.set_deadline(deadline);
    coordinator_stub = paxos_stubs_map_.GetCoordinatorStub();
    // Forward request to Coordinator, then respond to client.
    get_status = coordinator_stub->GetValue(&cc, *request, response);
    if (!get_status.ok()) {
      return Status(
          grpc::StatusCode::INTERNAL,
          "Failed to communicate with Coordinator after starting an election.");
    }
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Returning Response to Request: Get [key: " << request->key()
             << "]." << std::endl;
  }
  return get_status;
}

Status KeyValueStoreServiceImpl::PutPair(grpc::ServerContext* context,
                                         const PutRequest* request,
                                         EmptyMessage* response) {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Received Request: Put [key: " << request->key()
             << ", value: " << request->value() << "]." << std::endl;
  }
  ClientContext cc;
  auto deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
  cc.set_deadline(deadline);
  assert(paxos_stubs_map_ != nullptr);
  auto* coordinator_stub = paxos_stubs_map_.GetCoordinatorStub();
  // Forward request to Coordinator, then respond to client.
  Status put_status = coordinator_stub->PutPair(&cc, *request, response);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Returning Response to Request: Put [key: " << request->key()
             << ", value: " << request->value() << "]." << std::endl;
  }
  return put_status;
}

Status KeyValueStoreServiceImpl::DeletePair(grpc::ServerContext* context,
                                            const DeleteRequest* request,
                                            EmptyMessage* response) {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Received Request: Delete [key: " << request->key() << "]."
             << std::endl;
  }
  ClientContext cc;
  auto deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
  cc.set_deadline(deadline);
  assert(paxos_stubs_map_ != nullptr);
  auto* coordinator_stub = paxos_stubs_map_.GetCoordinatorStub();
  // Forward request to Coordinator, then respond to client.
  Status delete_status = coordinator_stub->DeletePair(&cc, *request, response);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Returning Response to Request: Delete [key: " << request->key()
             << "]." << std::endl;
  }
  return delete_status;
}

}  // namespace keyvaluestore
