// Server side of keyvaluestore.
#include "kv-store-service-impl.h"

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

// Get the corresponding value for a given key
Status KeyValueStoreServiceImpl::GetValue(ServerContext* context,
                                          const GetRequest* request,
                                          GetResponse* response) {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Received GetRequest. request.key=" << request->key()
             << std::endl;
  }
  std::string lock_key = GetTransactionId().DebugString();
  assert(kv_db_ != nullptr);
  Status lock_status = kv_db_->TryLock(lock_key, request->key());
  if (!lock_status.ok()) return lock_status;
  KeyValueDataBase::ValueMutator val_mutator =
      kv_db_->Unlock(lock_key, request->key());
  std::string val;
  if (!val_mutator.GetValue(&val)) {
    return Status(grpc::StatusCode::NOT_FOUND, "Key not found.");
  }
  response->set_value(val);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Returned GetResponse." << response->DebugString() << std::endl;
  }
  return Status::OK;
}

TransactionId KeyValueStoreServiceImpl::GetTransactionId() {
  TransactionId id;
  id.set_server_id(server_id_);
  std::unique_lock<std::shared_mutex> writer_lock(counter_mtx_);
  id.set_local_trans_id(counter_++);
  return id;
}

void KeyValueStoreServiceImpl::AddRequestToVoteRequest(
    const DeleteRequest& del_req, VoteRequest* vote_req) {
  *vote_req->mutable_del_request() = del_req;
}
void KeyValueStoreServiceImpl::AddRequestToVoteRequest(
    const PutRequest& put_req, VoteRequest* vote_req) {
  *vote_req->mutable_put_request() = put_req;
}

Status KeyValueStoreServiceImpl::ExecuteCommit(
    const CommitRequest& commit_req) {
  std::stringstream error_msg;
  for (int i = 0; i < participants_.size(); ++i) {
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(1000);
    context.set_deadline(deadline);
    EmptyMessage commit_resp;
    // Here we can use the stub's newly available method we just added.
    Status commit_status =
        participants_[i]->Commit(&context, commit_req, &commit_resp);
    if (!commit_status.ok()) {
      error_msg << "participant #" << i << " returns error on Commit, "
                << commit_status.error_message() << ". CommitRequest is, "
                << commit_req.DebugString() << std::endl;
    }
  }
  if (error_msg.str().empty()) return Status::OK;
  return Status(grpc::StatusCode::INTERNAL, error_msg.str());
}

template <typename Request>
Status KeyValueStoreServiceImpl::RequestFlow(const Request& req) {
  std::stringstream error_msg;
  TransactionId trans_id = GetTransactionId();
  VoteRequest vote_req;
  *vote_req.mutable_trans_id() = trans_id;
  AddRequestToVoteRequest(req, &vote_req);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Sending vote_req=" << vote_req.DebugString() << " to "
             << participants_.size() << " participants." << std::endl;
  }
  for (int i = 0; i < participants_.size(); ++i) {
    ClientContext context;
    VoteResponse vote_resp;
    // Here we can use the stub's newly available method we just added.
    Status vote_status = participants_[i]->Vote(&context, vote_req, &vote_resp);
    if (!vote_status.ok()) {
      error_msg << "participant #" << i << " returns error on Vote, "
                << vote_status.error_message() << std::endl;
    } else if (vote_resp.decision() != VoteDecision::COMMIT) {
      error_msg << "participant #" << i << " decides to abort. Reason, "
                << vote_resp.abort_reason() << std::endl;
    }
  }
  if (!error_msg.str().empty()) {
    {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "About to abort because " << error_msg.str() << std::endl;
    }
    CommitRequest commit_req;
    *commit_req.mutable_trans_id() = trans_id;
    commit_req.set_decision(VoteDecision::ABORT);
    Status abort_status = ExecuteCommit(commit_req);
    return Status(grpc::StatusCode::ABORTED,
                  "Aborted. " + error_msg.str() +
                      " Abort status: " + abort_status.error_message());
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "All participants are good. About to commit trans_id:"
             << trans_id.DebugString() << std::endl;
  }
  // Okay let's commit.
  CommitRequest commit_req;
  *commit_req.mutable_trans_id() = trans_id;
  commit_req.set_decision(VoteDecision::COMMIT);
  // This could only fail due to participant unavailability, possibly due to
  // crashes. When that happens, the culprit participant should be guaranteed to
  // recover commit.
  return ExecuteCommit(commit_req);
}

}  // namespace keyvaluestore
