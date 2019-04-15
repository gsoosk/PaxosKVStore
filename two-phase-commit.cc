#include "two-phase-commit.h"

namespace keyvaluestore {

using grpc::Status;

std::string TwoPhaseCommitServiceImpl::GetKey(const VoteRequest& request) {
  std::string key;
  if (request.has_put_request()) key = request.put_request().key();
  if (request.has_del_request()) key = request.del_request().key();
  return key;
}

std::string TwoPhaseCommitServiceImpl::GetLockKey(const VoteRequest& request) {
  return request.trans_id().DebugString();
}
std::string TwoPhaseCommitServiceImpl::GetLockKey(
    const CommitRequest& request) {
  return request.trans_id().DebugString();
}

Status TwoPhaseCommitServiceImpl::Vote(grpc::ServerContext* context,
                                       const VoteRequest* request,
                                       VoteResponse* response) {
  Status lock_status = kv_db_->TryLock(GetLockKey(*request), GetKey(*request));
  if (!lock_status.ok()) {
    response->set_decision(VoteDecision::ABORT);
    response->set_abort_reason(lock_status.error_message());
    return Status::OK;
  }
  response->set_decision(VoteDecision::COMMIT);
  std::unique_lock<std::shared_mutex> writer_lock(cache_mutex_);
  cached_vote_req_[GetLockKey(*request)] = *request;
  return Status::OK;
}

Status TwoPhaseCommitServiceImpl::Commit(grpc::ServerContext* context,
                                         const CommitRequest* request,
                                         EmptyMessage* response) {
  std::string lock_key = GetLockKey(*request);
  if (request->decision() == VoteDecision::ABORT) {
    std::unique_lock<std::shared_mutex> writer_lock(cache_mutex_);
    auto iter = cached_vote_req_.find(lock_key);
    if (iter != cached_vote_req_.end()) {
      // Return value is not useful.
      kv_db_->Unlock(lock_key, GetKey(iter->second));
      cached_vote_req_.erase(lock_key);
    }
    return Status::OK;
  }
  decltype(cached_vote_req_.begin()) iter;
  {
    std::shared_lock<std::shared_mutex> reader_lock(cache_mutex_);
    iter = cached_vote_req_.find(lock_key);
  }
  assert(iter != cached_vote_req_.end());
  if (iter->second.has_put_request()) {
    KeyValueDataBase::ValueMutator val_m =
        kv_db_->Unlock(lock_key, GetKey(iter->second));
    val_m.SetValue(iter->second.put_request().value());
  }
  if (iter->second.has_del_request()) {
    KeyValueDataBase::ValueMutator val_m =
        kv_db_->Unlock(lock_key, GetKey(iter->second));
    val_m.DeleteEntry();
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(cache_mutex_);
    cached_vote_req_.erase(lock_key);
  }
  return Status::OK;
}
}  // namespace keyvaluestore
