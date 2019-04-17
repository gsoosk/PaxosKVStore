// Server side of keyvaluestore.
#include "kv-store-service-impl.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
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
  Status get_status = RequestFlow(*request, response);
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
  Status put_status = RequestFlow(*request, response);
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
  Status delete_status = RequestFlow(*request, response);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Returning Response to Request: Delete [key: " << request->key()
             << "]." << std::endl;
  }
  return delete_status;
}

// Forward GetRequest to Coordinator.
Status KeyValueStoreServiceImpl::ForwardToCoordinator(ClientContext* cc,
                                                      MultiPaxos::Stub* stub,
                                                      const GetRequest& request,
                                                      GetResponse* response) {
  return stub->GetValue(cc, request, response);
}
// Forward PutRequest to Coordinator.
Status KeyValueStoreServiceImpl::ForwardToCoordinator(ClientContext* cc,
                                                      MultiPaxos::Stub* stub,
                                                      const PutRequest& request,
                                                      EmptyMessage* response) {
  return stub->PutPair(cc, request, response);
}
// Forward DeleteRequest to Coordinator.
Status KeyValueStoreServiceImpl::ForwardToCoordinator(
    ClientContext* cc, MultiPaxos::Stub* stub, const DeleteRequest& request,
    EmptyMessage* response) {
  return stub->DeletePair(cc, request, response);
}

template <typename Request, typename Response>
Status KeyValueStoreServiceImpl::RequestFlow(const Request& request,
                                             Response* response) {
  assert(paxos_stubs_map_ != nullptr);
  auto* coordinator_stub = paxos_stubs_map_.GetCoordinatorStub();

  // If Coordinator is not set, i.e., this server has just been brought up.
  if (coordinator_stub == nullptr) {
    // Try to get Coordinator address from other replicas.
    Status get_status = GetCoordinator();
    // If not successful, start an election for coordinators.
    if (!get_status.ok()) {
      Status election_status = ElectNewCoordinator();
    }
    coordinator_stub = paxos_stubs_map_.GetCoordinatorStub();
    // If Coordinator is still NULL, return an error.
    if (coordinator_stub == nullptr) {
      return Status(
          election_status.error_code(),
          "Failed to elect a Coordinator. " + election_status.error_message());
    }
  }
  ClientContext cc;
  auto deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
  cc.set_deadline(deadline);
  // Forward request to Coordinator.
  Status forward_status =
      ForwardToCoordinator(&cc, coordinator_stub, request, response);

  // Elect a new Coordinator if the current one is unavailable.
  if (forward_status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    Status election_status = ElectNewCoordinator();
    coordinator_stub = paxos_stubs_map_.GetCoordinatorStub();
    if (!election_status.ok() || coordinator_stub == nullptr) {
      return Status(
          election_status.error_code(),
          "Can't reach Coordinator. Failed to elect a new Coordinator. " +
              election_status.error_message());
    }
    // Forward request to new Coordinator.
    ClientContext new_cc;
    forward_status =
        ForwardToCoordinator(&new_cc, coordinator_stub, request, response);
    if (!forward_status.ok()) {
      return Status(forward_status.error_code(),
                    "Failed to communicate with Coordinator. " +
                        forward_status.error_message());
    }
  }
  return forward_status;
}

Status KeyValueStoreServiceImpl::GetCoordinator() {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Sending Requests to get Coordinator address." << std::endl;
  }
  assert(paxos_stubs_map_ != nullptr);
  auto stubs = paxos_stubs_map_.GetPaxosStubs();
  std::set<std::string> coordinators;
  for (const auto& stub : stubs) {
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(1000);
    context.set_deadline(deadline);
    EmptyMessage get_cdnt_req;
    GetCoordinatorResponse get_cdnt_resp;
    Status get_status =
        stub.second->GetCoordinator(&context, get_cdnt_req, &get_cdnt_resp);
    if (get_status.ok() && !get_cdnt_resp->coordinator().empty()) {
      coordinators.insert(get_cdnt_resp->coordinator());
    }
  }
  if (coordinators.size() != 1) {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Failed to get Coordinator addresses." << std::endl;
    return Status(grpc::StatusCode::ABORT,
                  "Failed to get Coordinator addresses.");
  }
  paxos_stubs_map_.SetCoordinator(*coordinators.begin());
  return Status::OK;
}

Status KeyValueStoreServiceImpl::ElectNewCoordinator() {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Sending Request to elect Coordinator via Paxos." << std::endl;
  }
  assert(paxos_stubs_map_ != nullptr);
  auto* my_paxos_stub = paxos_stubs_map_.GetStub(my_paxos_address_);
  ClientContext context;
  auto deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
  context.set_deadline(deadline);
  SetCoordinatorRequest set_cdnt_req;
  set_cdnt_req.set_key("coordinator");
  set_cdnt_req.set_coordinator(my_paxos_address_);
  EmptyMessage set_cdnt_resp;
  return stub.second->SetCoordinator(&context, set_cdnt_req, &set_cdnt_resp);
}

}  // namespace keyvaluestore
