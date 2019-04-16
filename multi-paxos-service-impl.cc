#include "multi-paxos-service-impl.h"

namespace keyvaluestore {

using grpc::Status;

// Logic upon receiving a Prepare message.
// Role: Acceptor
Status MultiPaxosServiceImpl::Prepare(grpc::ServerContext* context,
                                      const PrepareRequest* request,
                                      PromiseResponse* response) {
  std::string key = request->key();
  int round = request->round();
  int propose_id = request->propose_id();
  response->set_round(round);
  response->set_propose_id(propose_id);
  auto paxos_logs = kv_db_->GetPaxosLogs(key);
  auto it = paxos_logs.find(round);
  if (it != paxos_logs.end()) {
    const PaxosLog& paxos_log = it->second;
    // Will NOT accept PrepareRequests with propose_id <= promised_id.
    if (paxos_log.promised_id >= propose_id) {
      return Status(grpc::StatusCode::ABORTED,
                  "Proposal ID is too low.";
    } else if (paxos_log.accepted_id > 0) {
      // Piggyback accepted proposal information in response.
      response->set_accepted_id(paxos_log.accepted_id);
      response->set_type(paxos_log.accepted_type);
      response->set_value(paxos_log.accepted_value);
    }
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
  std::string key = request->key();
  int round = request->round();
  int propose_id = request->propose_id();
  auto paxos_logs = kv_db_->GetPaxosLogs(key);
  auto it = paxos_logs.find(round);
  if (it != paxos_logs.end()) {
    const PaxosLog& paxos_log = it->second;
    // Will NOT accept ProposeRequests with propose_id < promised_id.
    if (paxos_log.promised_id > propose_id) {
      return Status(grpc::StatusCode::ABORTED,
                  "Proposal ID is too low.";
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
  }
  return Status::OK;
}

// Logic upon receiving an Inform message.
// Role: Learner
Status MultiPaxosServiceImpl::Inform(grpc::ServerContext* context,
                                     const InformRequest* request,
                                     EmptyMessage* response) {
  std::string key = request->key();
  auto acceptance = request->acceptance();
  // Update Paxos log in db.
  kv_db_->AddPaxosLog(key, acceptance.round(), acceptance.propose_id(),
                      acceptance.type(), acceptance.value());
  // Obtain updated value status from db.
  auto paxos_logs = kv_db_->GetPaxosLogs(key);
  // Will NOT execute operation if it's not the latest round.
  if (acceptance.round() < paxos_logs.rbegin()->first) {
    return Status(grpc::StatusCode::ABORTED,
                  "Operation overwritten by others.";
  }
  // Execute operation.
  switch (acceptance.type()) {
    case OperationType::SET:
      kv_db_->SetValue(key, acceptance.value());
      break;
    case OperationType::DELET:
      kv_db_->DeleteEntry(key);
      break;
    default:
      break;
  }
  return Status::OK;
}
}  // namespace keyvaluestore
