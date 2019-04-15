#include "multi-paxos-service-impl.h"

namespace keyvaluestore {

using grpc::Status;

// Logic upon receiving a Prepare message.
// Role: Acceptor
Status MultiPaxosServiceImpl::Prepare(grpc::ServerContext* context,
                                      const PrepareRequest* request,
                                      PromiseResponse* response) {
  std::string key = request->get_key();
  int round = request->get_round();
  int propose_id = request->get_propose_id();
  response->set_round(round);
  response->set_propose_id(propose_id);
  auto* value_status = kv_db_->GetValueStatus(key);
  auto it = value_status->paxos_logs.find(round);
  if (it != value_status->paxos_logs.end()) {
    auto paxos_log = it->second;
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
  kv_db_->SetValueStatus(key, round, propose_id);
  return Status::OK;
}

// Logic upon receiving a Propose message.
// Role: Acceptor
Status MultiPaxosServiceImpl::Propose(grpc::ServerContext* context,
                                      const ProposeRequest* request,
                                      AcceptResponse* response) {
  std::string key = request->get_key();
  int round = request->get_round();
  int propose_id = request->get_propose_id();
  auto* value_status = kv_db_->GetValueStatus(key);
  auto it = value_status->paxos_logs.find(round);
  if (it != value_status->paxos_logs.end()) {
    auto paxos_log = it->second;
    // Will NOT accept ProposeRequests with propose_id < promised_id.
    if (paxos_log.promised_id > propose_id) {
      return Status(grpc::StatusCode::ABORTED,
                  "Proposal ID is too low.";
    } else {
      // Respond with acceptance and update accepted proposal in db.
      auto type = request->get_type();
      std::string value = request->get_value();
      response->set_round(round);
      response->set_propose_id(propose_id);
      response->set_type(type);
      response->set_value(value);
      kv_db_->SetValueStatus(key, round, propose_id, type, value);
    }
  }
  return Status::OK;
}

// Logic upon receiving an Inform message.
// Role: Learner
Status MultiPaxosServiceImpl::Inform(grpc::ServerContext* context,
                                     const InformRequest* request,
                                     EmptyMessage* response) {
  std::string key = request->get_key();
  auto acceptance = request->get_acceptance();
  int round = acceptance.get_round();
  auto type = acceptance.get_type();
  // Update Paxos log in db.
  kv_db_->SetValueStatus(key, round, acceptance.get_propose_id(), type,
                         acceptance.get_value());
  // Obtain updated value status from db.
  auto* value_status = kv_db_->GetValueStatus(key);
  // Will NOT execute operation if it's not the latest round.
  if (round < value_status->paxos_logs.rbegin()->first) {
    return Status(grpc::StatusCode::ABORTED,
                  "Operation overwritten by others.";
  }
  // Execute operation.
  switch (acceptance.get_type()) {
    case SET:
      KeyValueDataBase::ValueMutator val_m = kv_db_->GetValueMutator(key);
      val_m.SetValue(acceptance.get_value());
      break;
    case DELET:
      KeyValueDataBase::ValueMutator val_m = kv_db_->GetValueMutator(key);
      val_m.DeleteEntry();
      break;
    default:
      break;
  }
  return Status::OK;
}
}  // namespace keyvaluestore
