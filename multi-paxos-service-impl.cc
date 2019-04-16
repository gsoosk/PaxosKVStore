#include "multi-paxos-service-impl.h"

namespace keyvaluestore {

using grpc::Status;

// Get the corresponding value for a given key
Status KeyValueStoreServiceImpl::GetValue(ServerContext* context,
                                          const GetRequest* request,
                                          GetResponse* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Received Forwarded Request: Get [key: " << request->key()
             << "]." << std::endl;
  }
  assert(kv_db_ != nullptr);
  std::string value;
  bool get_success = kv_db_->GetValue(&value);
  if (!get_success) {
    return Status(grpc::StatusCode::NOT_FOUND, "Key not found.");
  }
  response->set_value(value);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Returning Response to Request: Get [key: " << request->key()
             << "]." << std::endl;
  }
  return Status::OK;
}

Status KeyValueStoreServiceImpl::PutPair(grpc::ServerContext* context,
                                         const PutRequest* request,
                                         EmptyMessage* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Received Forwarded Request: Put [key: " << request->key()
             << ", value: " << request->value() << "]." << std::endl;
  }
  // Run a Paxos instance to reach consensus on the operation.
  Status put_status = RunPaxos(*request);
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
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Received Forwarded Request: Delete [key: " << request->key()
             << "]." << std::endl;
  }
  // Run a Paxos instance to reach consensus on the operation.
  Status delete_status = RunPaxos(*request);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Returning Response to Request: Delete [key: " << request->key()
             << "]." << std::endl;
  }
  return delete_status;
}

// Logic upon receiving a Prepare message.
// Role: Acceptor
Status MultiPaxosServiceImpl::Prepare(grpc::ServerContext* context,
                                      const PrepareRequest* request,
                                      PromiseResponse* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  std::string key = request->key();
  int round = request->round();
  int propose_id = request->propose_id();
  response->set_round(round);
  response->set_propose_id(propose_id);
  PaxosLog paxos_log = kv_db_->GetPaxosLog(key, round);
  // Will NOT accept PrepareRequests with propose_id <= promised_id.
  if (paxos_log.promised_id >= propose_id) {
      return Status(grpc::StatusCode::ABORTED,
                  "Aborted. Proposal ID is too low.";
  } else if (paxos_log.accepted_id > 0) {
    // Piggyback accepted proposal information in response.
    response->set_accepted_id(paxos_log.accepted_id);
    response->set_type(paxos_log.accepted_type);
    response->set_value(paxos_log.accepted_value);
  }
  // Update promised id in db.
  kv_db_->AddPaxosLog(key, round, propose_id);
  return Status::OK;
}

// Logic upon receiving a Propose message.
// Role: Acceptor
Status MultiPaxosServiceImpl::Propose(grpc::ServerContext* context,
                                      const ProposeRequest* request,
                                      AcceptResponse* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  std::string key = request->key();
  int round = request->round();
  int propose_id = request->propose_id();
  PaxosLog paxos_log = kv_db_->GetPaxosLog(key, round);
  // Will NOT accept ProposeRequests with propose_id < promised_id.
  if (paxos_log.promised_id > propose_id) {
      return Status(grpc::StatusCode::ABORTED,
                  "Aborted. Proposal ID is too low.";
  } else {
    // Respond with acceptance and update accepted proposal in db.
    auto type = request->type();
    std::string value = request->value();
    response->set_round(round);
    response->set_propose_id(propose_id);
    response->set_type(type);
    response->set_value(value);
    kv_db_->AddPaxosLog(key, round, propose_id, type, value);
  }
  return Status::OK;
}

// Logic upon receiving an Inform message.
// Role: Learner
Status MultiPaxosServiceImpl::Inform(grpc::ServerContext* context,
                                     const InformRequest* request,
                                     EmptyMessage* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  std::string key = request->key();
  auto acceptance = request->acceptance();
  // Update Paxos log in db.
  kv_db_->AddPaxosLog(key, acceptance.round(), acceptance.propose_id(),
                      acceptance.type(), acceptance.value());
  // Will NOT execute operation if it's not the latest round.
  if (acceptance.round() < kv_db_->GetLatestRound(key)) {
    return Status(grpc::StatusCode::ABORTED,
                  "Aborted. Operation overwritten by others.";
  }
  // Execute operation.
  switch (acceptance.type()) {
    case OperationType::SET:
      kv_db_->SetValue(key, acceptance.value());
      break;
    case OperationType::DELETE:
      kv_db_->DeleteEntry(key);
      break;
    default:
      break;
  }
  return Status::OK;
}

void MultiPaxosServiceImpl::SetProposeValue(const PutRequest& put_req,
                                            ProposeRequest* propose_req) {
  *propose_req->set_type(OperationType::SET);
  *propose_req->set_value(put_req.value());
}
void MultiPaxosServiceImpl::SetProposeValue(const DeleteRequest& del_req,
                                            ProposeRequest* propose_req) {
  *propose_req->set_type(OperationType::DELETE);
}

template <typename Request>
Status MultiPaxosServiceImpl::RunPaxos(const Request& req) {
  std::string key = req.key();
  int round = kv_db_->GetLatestRound(key) + 1;
  int propose_id = 1;
  auto paxos_stubs = paxos_stubs_map_->GetPaxosStubs();
  int num_of_acceptors = paxos_stubs.size();

  // Prepare.
  PrepareRequest prepare_req;
  prepare_req.set_key(key);
  prepare_req.set_round(round);
  prepare_req.set_propose_id(propose_id);

  int num_of_promised = 0;
  int accepted_id = 0;
  OperationType accepted_type = OperationType::NOTSET;
  std::string accepted_value = "";
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Sending PrepareRequest [key: " << prepare_req.key()
             << ", round: " << prepare_req.round()
             << ", propose_id: " << prepare_req.propose_id() << "] to "
             << num_of_acceptors << " Acceptors." << std::endl;
  }
  for (const auto& stub : paxos_stubs) {
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);
    PromiseResponse promise_resp;
    Status promise_status =
        stub.second->Prepare(&context, prepare_req, &promise_resp);
    if (!promise_status.ok()) {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "Acceptor " << stub.first
               << " rejected Prepare: " << promise_status.error_message()
               << std::endl;
    } else {
      num_of_promised++;
      if (promise_resp.accepted_id() > accepted_id) {
        accepted_id = promise_resp.accepted_id();
        accepted_type = promise_resp.type();
        accepted_value = promise_resp.value();
      }
    }
  }
  if (num_of_promised <= num_of_acceptors / 2) {
    std::stringstream abort_msg;
    abort_msg << "Received " << num_of_promised << " Promises from "
              << num_of_acceptors << " Acceptors. "
              << "Failed to reach Quorum." << std::endl;
    {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "Proposal aborted. " << abort_msg.str();
    }
    return Status(grpc::StatusCode::ABORTED, "Aborted. " + abort_msg.str();
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Quorum reached on [key: " << prepare_req.key()
             << ", round: " << prepare_req.round()
             << ", propose_id: " << prepare_req.propose_id() << "]"
             << std::endl;
  }

  // Propose.
  ProposeRequest propose_req;
  propose_req.set_key(key);
  propose_req.set_round(round);
  propose_req.set_propose_id(propose_id);
  if (accepted_id > 0) {
    propose_req.set_type(accepted_type);
    propose_req.set_value(accepted_value);
  } else {
    SetProposeValue(req, &propose_req);
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Sending ProposeRequest [key: " << propose_req.key()
             << ", round: " << propose_req.round()
             << ", propose_id: " << propose_req.propose_id()
             << ", type: " << propose_req.type()
             << ", value: " << propose_req.value() << "] to "
             << num_of_acceptors << " Acceptors." << std::endl;
  }
  int num_of_accepted = 0;
  AcceptResponse acceptance;
  for (const auto& stub : paxos_stubs) {
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);
    AcceptResponse accept_resp;
    Status promise_status =
        stub.second->Propose(&context, propose_req, &accept_resp);
    if (!promise_status.ok()) {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "Acceptor " << stub.first
               << " rejected Propose: " << promise_status.error_message()
               << std::endl;
    } else {
      num_of_accepted++;
      acceptance = accept_resp;
    }
  }

  if (num_of_accepted <= num_of_acceptors / 2) {
    std::stringstream abort_msg;
    abort_msg << "Received " << num_of_accepted << " Acceptance from "
              << num_of_acceptors << " Acceptors. "
              << "Failed to reach Consensus." << std::endl;
    {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "Proposal aborted. " << abort_msg.str();
    }
    return Status(grpc::StatusCode::ABORTED, "Aborted. " + abort_msg.str();
  }
  // Inform Learners.
  InformRequest inform_req;
  inform_req.set_key(key);
  *inform_req->mutable_acceptance() = acceptance;
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "Sending InformRequest [key: " << inform_req.key()
             << ", type: " << inform_req.acceptance().type()
             << ", value: " << inform_req.acceptance().value() << "] to "
             << num_of_acceptors << " Learners." << std::endl;
  }
  for (const auto& stub : paxos_stubs) {
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);
    EmptyMessage inform_resp;
    Status inform_status =
        stub.second->Inform(&context, inform_req, &inform_resp);
    if (!inform_status.ok()) {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "Learner " << stub.first
               << " returned error: " << inform_status.error_message()
               << std::endl;
    }
  }
  return Status::OK;
}

}  // namespace keyvaluestore
