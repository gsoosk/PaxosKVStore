#include <cstdlib>

#include "multi-paxos-service-impl.h"

namespace keyvaluestore {

using grpc::ClientContext;
using grpc::ServerContext;
using grpc::Status;
using keyvaluestore::AcceptResponse;
using keyvaluestore::DeleteRequest;
using keyvaluestore::EmptyMessage;
using keyvaluestore::GetRequest;
using keyvaluestore::GetResponse;
using keyvaluestore::InformRequest;
using keyvaluestore::MultiPaxos;
using keyvaluestore::OperationType;
using keyvaluestore::PaxosLog;
using keyvaluestore::PrepareRequest;
using keyvaluestore::PromiseResponse;
using keyvaluestore::ProposeRequest;
using keyvaluestore::PutRequest;
using keyvaluestore::RecoverResponse;

// Construction method.
MultiPaxosServiceImpl::MultiPaxosServiceImpl(
    PaxosStubsMap* paxos_stubs_map, KeyValueDataBase* kv_db,
    const std::string& my_paxos_address, double fail_rate)
    : paxos_stubs_map_(paxos_stubs_map),
      kv_db_(kv_db),
      my_paxos_address_(my_paxos_address),
      fail_rate_(fail_rate) {}

// Find Coordinator and recover data from Coordinator on construction.
Status MultiPaxosServiceImpl::Initialize() {
  // Try to get Coordinator address from other replicas.
  Status get_status = GetCoordinator();
  auto* coordinator_stub = paxos_stubs_map_->GetCoordinatorStub();
  // If not successful, start an election for Coordinators.
  if (!get_status.ok()) {
    Status elect_status = ElectNewCoordinator();
    if (!elect_status.ok()) {
      return Status(
          grpc::StatusCode::ABORTED,
          "ElectNewCoordinator Failed: " + elect_status.error_message());
    }
  }
  Status recover_status = GetRecovery();
  if (!recover_status.ok()) {
    return Status(grpc::StatusCode::ABORTED,
                  "GetRecovery Failed: " + recover_status.error_message());
  }
  return Status::OK;
}

// Get the corresponding value for a given key
Status MultiPaxosServiceImpl::GetValue(ServerContext* context,
                                       const GetRequest* request,
                                       GetResponse* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  std::string key = request->key();
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Received Forwarded Request: Get [key: " << key << "]."
             << std::endl;
  }
  if (key == "coordinator") {
    return Status(grpc::StatusCode::ABORTED, "Illegal keyword");
  }
  assert(kv_db_ != nullptr);
  std::string value;
  bool get_success = kv_db_->GetValue(key, &value);
  if (!get_success) {
    return Status(grpc::StatusCode::NOT_FOUND, "Key not found.");
  }
  response->set_value(value);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Returning GetResponse: [key: " << key
             << ", value: " << response->value() << "]." << std::endl;
  }
  return Status::OK;
}

Status MultiPaxosServiceImpl::PutPair(grpc::ServerContext* context,
                                      const PutRequest* request,
                                      EmptyMessage* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  std::string key = request->key();
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Received Forwarded Request: Put [key: " << key
             << ", value: " << request->value() << "]." << std::endl;
  }
  if (key == "coordinator") {
    return Status(grpc::StatusCode::ABORTED, "Illegal keyword");
  }
  // Run a Paxos instance to reach consensus on the operation.
  Status put_status = RunPaxos(*request);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Returning Response to Request: Put [key: " << key
             << ", value: " << request->value() << "]." << std::endl;
  }
  return put_status;
}

Status MultiPaxosServiceImpl::DeletePair(grpc::ServerContext* context,
                                         const DeleteRequest* request,
                                         EmptyMessage* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  std::string key = request->key();
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Received Forwarded Request: Delete [key: " << key << "]."
             << std::endl;
  }
  if (key == "coordinator") {
    return Status(grpc::StatusCode::ABORTED, "Illegal keyword");
  }
  // Run a Paxos instance to reach consensus on the operation.
  Status delete_status = RunPaxos(*request);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Returning Response to Request: Delete [key: " << key << "]."
             << std::endl;
  }
  return delete_status;
}

Status MultiPaxosServiceImpl::ElectCoordinator(
    grpc::ServerContext* context, const ElectCoordinatorRequest* request,
    EmptyMessage* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Received ElectCoordinator Request: [coordinator: "
             << request->coordinator() << "]." << std::endl;
  }
  // Run a Paxos instance to reach consensus on the operation.
  Status set_status = RunPaxos(*request);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Returning Response to Request: ElectCoordinator [coordinator: "
             << request->coordinator() << "]." << std::endl;
  }
  return set_status;
}

Status MultiPaxosServiceImpl::GetCoordinator(grpc::ServerContext* context,
                                             const EmptyMessage* request,
                                             GetCoordinatorResponse* response) {
  if (context->IsCancelled()) {
    return Status(grpc::StatusCode::CANCELLED,
                  "Deadline exceeded or Client cancelled, abandoning.");
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Received GetCoordinator Request." << std::endl;
  }
  assert(paxos_stubs_map_ != nullptr);
  std::string coordinator = paxos_stubs_map_->GetCoordinator();
  if (coordinator.empty()) {
    return Status(grpc::StatusCode::NOT_FOUND, "Coordinator not found.");
  }
  response->set_coordinator(coordinator);
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Returning GetCoordinatorResponse: [coordinator: "
             << response->coordinator() << "]." << std::endl;
  }
  return Status::OK;
}

Status MultiPaxosServiceImpl::Ping(grpc::ServerContext* context,
                                   const EmptyMessage* request,
                                   EmptyMessage* response) {
  // {
  //   std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
  //   TIME_LOG << "[" << my_paxos_address_ << "] "
  //            << "Received Ping Request. Returning Response." << std::endl;
  // }
  return Status::OK;
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
  // Update round id in db.
  kv_db_->AddPaxosLog(key, round);
  response->set_round(round);
  response->set_propose_id(propose_id);
  PaxosLog paxos_log = kv_db_->GetPaxosLog(key, round);
  std::stringstream promise_msg;
  promise_msg << "[Promised] [key: " << key << ", round: " << round
              << ", propose_id: " << propose_id;
  // Will NOT accept PrepareRequests with propose_id <= promised_id.
  if (paxos_log.promised_id() >= propose_id) {
    return Status(grpc::StatusCode::ABORTED,
                  "Aborted. Proposal ID is too low.");
  } else if (key != "coordinator" && RandomFail()) {
    std::stringstream fail_msg;
    fail_msg << "[Rejected] Acceptor random-failed on Prepare[key: " << key
             << ", round: " << round << ", propose_id: " << propose_id
             << "]. (fail_rate=" << fail_rate_ << ")";
    {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "[" << my_paxos_address_ << "] " << fail_msg.str()
               << std::endl;
    }
    return Status(grpc::StatusCode::ABORTED, fail_msg.str());
  } else if (paxos_log.accepted_id() > 0) {
    // Piggyback accepted proposal information in response.
    response->set_accepted_id(paxos_log.accepted_id());
    response->set_type(paxos_log.accepted_type());
    response->set_value(paxos_log.accepted_value());
    promise_msg << ", accepted_id: " << paxos_log.accepted_id()
                << ", type: " << paxos_log.accepted_type()
                << ", value: " << paxos_log.accepted_value();
  }
  promise_msg << "].";
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] " << promise_msg.str()
             << std::endl;
  }
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
  std::stringstream accept_msg;
  // Will NOT accept ProposeRequests with propose_id < promised_id.
  if (paxos_log.promised_id() > propose_id) {
    return Status(grpc::StatusCode::ABORTED,
                  "Aborted. Proposal ID is too low.");
  } else if (key != "coordinator" && RandomFail()) {
    std::stringstream fail_msg;
    fail_msg << "[Rejected] Acceptor random-failed on Propose[key: " << key
             << ", round: " << round << ", propose_id: " << propose_id
             << "]. (fail_rate=" << fail_rate_ << ")";
    {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "[" << my_paxos_address_ << "] " << fail_msg.str()
               << std::endl;
    }
    return Status(grpc::StatusCode::ABORTED, fail_msg.str());
  } else {
    // Respond with acceptance and update accepted proposal in db.
    auto type = request->type();
    std::string value = request->value();
    response->set_round(round);
    response->set_propose_id(propose_id);
    response->set_type(type);
    response->set_value(value);
    kv_db_->AddPaxosLog(key, round, propose_id, type, value);
    accept_msg << "[Accepted] [key: " << key << ", round: " << round
               << ", propose_id: " << propose_id << ", type: " << type
               << ", value: " << value << "].";
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] " << accept_msg.str()
             << std::endl;
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
                  "Aborted. Operation overwritten by others.");
  }
  // Execute operation.
  switch (acceptance.type()) {
    case OperationType::SET:
      kv_db_->SetValue(key, acceptance.value());
      {
        std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
        TIME_LOG << "[" << my_paxos_address_ << "] "
                 << "[Success] Set " << key << ":" << acceptance.value() << "."
                 << std::endl;
      }
      break;
    case OperationType::DELETE:
      kv_db_->DeleteEntry(key);
      {
        std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
        TIME_LOG << "[" << my_paxos_address_ << "] "
                 << "[Success] Deleted " << key << "." << std::endl;
      }
      break;
    case OperationType::SET_COORDINATOR:
      // kv_db_->SetValue("coordinator", acceptance.value());
      paxos_stubs_map_->SetCoordinator(acceptance.value());
      {
        std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
        TIME_LOG << "[" << my_paxos_address_ << "] "
                 << "[Success] Set Coordinator to [" << acceptance.value()
                 << "]." << std::endl;
      }
      break;
    default:
      break;
  }
  return Status::OK;
}

Status MultiPaxosServiceImpl::Recover(grpc::ServerContext* context,
                                      const EmptyMessage* request,
                                      RecoverResponse* response) {
  auto kv_map = kv_db_->GetDataMap();
  auto paxos_logs_map = kv_db_->GetPaxosLogsMap();
  *response->mutable_kv_map() = google::protobuf::Map<std::string, std::string>(
      kv_map.begin(), kv_map.end());
  // for (const auto& kv : kv_map) {
  //   const std::string& key = kv.first;
  //   const auto& paxos_logs = kv_db_->GetPaxosLogs(key);
  //   *(*response->mutable_paxos_logs())[key].mutable_logs() =
  //       google::protobuf::Map<int, PaxosLog>(paxos_logs.begin(),
  //                                            paxos_logs.end());
  // }
  for (const auto& paxos_logs : paxos_logs_map) {
    const std::string& key = paxos_logs.first;
    *(*response->mutable_paxos_logs())[key].mutable_logs() =
        google::protobuf::Map<int, PaxosLog>(paxos_logs.second.begin(),
                                             paxos_logs.second.end());
  }
  return Status::OK;
}

void MultiPaxosServiceImpl::SetProposeValue(
    const ElectCoordinatorRequest& set_cdnt_req, ProposeRequest* propose_req) {
  propose_req->set_type(OperationType::SET_COORDINATOR);
  propose_req->set_value(set_cdnt_req.coordinator());
}
void MultiPaxosServiceImpl::SetProposeValue(const PutRequest& put_req,
                                            ProposeRequest* propose_req) {
  propose_req->set_type(OperationType::SET);
  propose_req->set_value(put_req.value());
}
void MultiPaxosServiceImpl::SetProposeValue(const DeleteRequest& del_req,
                                            ProposeRequest* propose_req) {
  propose_req->set_type(OperationType::DELETE);
}

template <typename Request>
Status MultiPaxosServiceImpl::RunPaxos(const Request& req) {
  std::string key = req.key();
  int round = kv_db_->GetLatestRound(key) + 1;
  int propose_id = 1;
  auto paxos_stubs = paxos_stubs_map_->GetPaxosStubs();
  // Ping.
  std::set<std::string> live_paxos_stubs;
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Sending PingRequest to " << paxos_stubs.size() << " Acceptors."
             << std::endl;
  }
  for (const auto& stub : paxos_stubs) {
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(500);
    context.set_deadline(deadline);
    EmptyMessage ping_req, ping_resp;
    Status ping_status = stub.second->Ping(&context, ping_req, &ping_resp);
    if (ping_status.ok()) {
      live_paxos_stubs.insert(stub.first);
    }
  }
  if (live_paxos_stubs.empty()) {
    return Status(grpc::StatusCode::ABORTED,
                  "Aborted. Can't connect to any PaxosStub.");
  }
  int num_of_acceptors = live_paxos_stubs.size();
  // Prepare.
  PrepareRequest prepare_req;
  prepare_req.set_key(key);
  prepare_req.set_round(round);
  prepare_req.set_propose_id(propose_id);

  int num_of_promised = 0;
  int accepted_id = 0;
  OperationType accepted_type = OperationType::NOT_SET;
  std::string accepted_value;
  // {
  //   std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
  //   TIME_LOG << "[" << my_paxos_address_ << "] "
  //            << "Sending PrepareRequest [key: " << prepare_req.key()
  //            << ", round: " << prepare_req.round()
  //            << ", propose_id: " << prepare_req.propose_id() << "] to "
  //            << num_of_acceptors << " Acceptors." << std::endl;
  // }
  for (const std::string& addr : live_paxos_stubs) {
    const auto& stub = paxos_stubs[addr];
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);
    PromiseResponse promise_resp;
    Status promise_status = stub->Prepare(&context, prepare_req, &promise_resp);
    if (!promise_status.ok()) {
      // std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      // TIME_LOG << "[" << my_paxos_address_ << "] "
      //          << "  Acceptor " << addr
      //          << " rejected Prepare: " << promise_status.error_message()
      //          << std::endl;
    } else {
      // {
      //   std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      //   TIME_LOG << "[" << my_paxos_address_ << "] "
      //            << "  Acceptor " << addr << " promised." << std::endl;
      // }
      num_of_promised++;
      if (promise_resp.accepted_id() > accepted_id) {
        accepted_id = promise_resp.accepted_id();
        accepted_type = promise_resp.type();
        accepted_value = promise_resp.value();
      }
    }
  }
  std::stringstream quorum_msg;
  quorum_msg << "[key: " << prepare_req.key()
             << ", round: " << prepare_req.round()
             << ", propose_id: " << prepare_req.propose_id()
             << "]: " << num_of_promised << " Promise, "
             << num_of_acceptors - num_of_promised << " Reject.";
  if (num_of_promised <= num_of_acceptors / 2) {
    {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "[" << my_paxos_address_ << "] "
               << "[Failed QUORUM] on " << quorum_msg.str() << std::endl;
    }
    return Status(grpc::StatusCode::ABORTED,
                  "[Failed QUORUM] on " + quorum_msg.str());
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "[Reached QUORUM] on " << quorum_msg.str() << std::endl;
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
  // {
  //   std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
  //   TIME_LOG << "[" << my_paxos_address_ << "] "
  //            << "Sending ProposeRequest [key: " << propose_req.key()
  //            << ", round: " << propose_req.round()
  //            << ", propose_id: " << propose_req.propose_id()
  //            << ", type: " << propose_req.type()
  //            << ", value: " << propose_req.value() << "] to "
  //            << num_of_acceptors << " Acceptors." << std::endl;
  // }
  int num_of_accepted = 0;
  AcceptResponse acceptance;
  for (const std::string& addr : live_paxos_stubs) {
    const auto& stub = paxos_stubs[addr];
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);
    AcceptResponse accept_resp;
    Status accept_status = stub->Propose(&context, propose_req, &accept_resp);
    if (!accept_status.ok()) {
      // std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      // TIME_LOG << "[" << my_paxos_address_ << "] "
      //          << "  Acceptor " << addr
      //          << " rejected Propose: " << accept_status.error_message()
      //          << std::endl;
    } else {
      // {
      //   std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      //   TIME_LOG << "[" << my_paxos_address_ << "] "
      //            << "  Acceptor " << addr << " accepted." << std::endl;
      // }
      num_of_accepted++;
      acceptance = accept_resp;
    }
  }
  std::stringstream consensus_msg;
  consensus_msg << "[key: " << propose_req.key()
                << ", round: " << propose_req.round()
                << ", propose_id: " << propose_req.propose_id()
                << ", value: " << propose_req.value()
                << "]: " << num_of_accepted << " Accept, "
                << num_of_acceptors - num_of_accepted << " Reject.";
  if (num_of_accepted <= num_of_acceptors / 2) {
    {
      std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      TIME_LOG << "[" << my_paxos_address_ << "] "
               << "[Failed CONSENSUS] on " << consensus_msg.str() << std::endl;
    }
    return Status(grpc::StatusCode::ABORTED,
                  "[Failed CONSENSUS] on " + consensus_msg.str());
  }
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "[Reached CONSENSUS] on " << consensus_msg.str() << std::endl;
  }
  // Inform Learners.
  InformRequest inform_req;
  inform_req.set_key(key);
  *inform_req.mutable_acceptance() = acceptance;
  // {
  //   std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
  //   TIME_LOG << "[" << my_paxos_address_ << "] "
  //            << "Informed " << num_of_acceptors << " Learners:"
  //            << " [key: " << inform_req.key()
  //            << ", type: " << inform_req.acceptance().type()
  //            << ", value: " << inform_req.acceptance().value() << "]."
  //            << std::endl;
  // }
  for (const std::string& addr : live_paxos_stubs) {
    const auto& stub = paxos_stubs[addr];
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);
    EmptyMessage inform_resp;
    Status inform_status = stub->Inform(&context, inform_req, &inform_resp);
    if (!inform_status.ok()) {
      // std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      // TIME_LOG << "[" << my_paxos_address_ << "] "
      //          << "  Learner " << addr
      //          << " returned error: " << inform_status.error_message()
      //          << std::endl;
    } else {
      // std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
      // TIME_LOG << "[" << my_paxos_address_ << "] "
      //          << "  Learner " << addr << " returned OK." << std::endl;
    }
  }
  return Status::OK;
}

Status MultiPaxosServiceImpl::GetCoordinator() {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Sending Requests to get Coordinator address." << std::endl;
  }
  assert(paxos_stubs_map_ != nullptr);
  auto stubs = paxos_stubs_map_->GetPaxosStubs();
  std::set<std::string> coordinators;
  std::set<std::string> live_paxos_stubs;
  for (const auto& stub : stubs) {
    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(1000);
    context.set_deadline(deadline);
    EmptyMessage get_cdnt_req;
    GetCoordinatorResponse get_cdnt_resp;
    Status get_status =
        stub.second->GetCoordinator(&context, get_cdnt_req, &get_cdnt_resp);
    if (get_status.ok()) {
      live_paxos_stubs.insert(stub.first);
      if (!get_cdnt_resp.coordinator().empty()) {
        coordinators.insert(get_cdnt_resp.coordinator());
      }
    }
  }
  if (coordinators.size() != 1) {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Failed to get Coordinator addresses." << std::endl;
    return Status(grpc::StatusCode::ABORTED,
                  "Failed to get Coordinator addresses.");
  }
  if (live_paxos_stubs.find(*coordinators.begin()) == live_paxos_stubs.end()) {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Coordinator is unavailable." << std::endl;
    return Status(grpc::StatusCode::ABORTED, "Coordinator is unavailable.");
  }
  paxos_stubs_map_->SetCoordinator(*coordinators.begin());
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "[Success] Set Coordinator addresses to ["
             << *coordinators.begin() << "]." << std::endl;
  }
  return Status::OK;
}

Status MultiPaxosServiceImpl::ElectNewCoordinator() {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Sending Request to elect Coordinator via Paxos." << std::endl;
  }
  assert(paxos_stubs_map_ != nullptr);
  auto* my_paxos_stub = paxos_stubs_map_->GetStub(my_paxos_address_);
  ClientContext context;
  auto deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
  context.set_deadline(deadline);
  ElectCoordinatorRequest set_cdnt_req;
  set_cdnt_req.set_key("coordinator");
  set_cdnt_req.set_coordinator(my_paxos_address_);
  EmptyMessage set_cdnt_resp;
  return my_paxos_stub->ElectCoordinator(&context, set_cdnt_req,
                                         &set_cdnt_resp);
}

Status MultiPaxosServiceImpl::GetRecovery() {
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Sending RecoverRequest to Coordinator." << std::endl;
  }
  assert(paxos_stubs_map_ != nullptr);
  auto* coordinator_stub = paxos_stubs_map_->GetCoordinatorStub();
  ClientContext context;
  auto deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
  context.set_deadline(deadline);
  EmptyMessage recover_req;
  RecoverResponse recover_resp;
  Status recover_status =
      coordinator_stub->Recover(&context, recover_req, &recover_resp);
  if (!recover_status.ok()) {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Failed to get Recovery from Coordinator." << std::endl;
    return Status(grpc::StatusCode::ABORTED,
                  "Failed to get Recovery from Coordinator.");
  }

  for (const auto& kv : recover_resp.kv_map()) {
    kv_db_->SetValue(kv.first, kv.second);
  }
  for (const auto& entry : recover_resp.paxos_logs()) {
    const std::string& key = entry.first;
    const auto& paxos_logs = entry.second.logs();
    for (const auto& log : paxos_logs) {
      kv_db_->AddPaxosLog(key, log.first, log.second.promised_id(),
                          log.second.accepted_id(), log.second.accepted_type(),
                          log.second.accepted_value());
    }
  }
  auto tmp_kv_db = kv_db_->GetDataMap();
  auto tmp_paxos_keys = kv_db_->GetPaxosLogKeys();
  {
    std::unique_lock<std::shared_mutex> writer_lock(log_mtx_);
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Received recovery snapshot:" << std::endl;
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "KV Data Map:" << std::endl;
    for (const auto& kv : tmp_kv_db) {
      TIME_LOG << "[" << my_paxos_address_ << "] "
               << "  [key: " << kv.first << ", value: " << kv.second << "]"
               << std::endl;
    }
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "Paxos Logs:" << std::endl;
    for (const auto& key : tmp_paxos_keys) {
      TIME_LOG << "[" << my_paxos_address_ << "] "
               << "  [key: " << key
               << ", last round: " << kv_db_->GetLatestRound(key) << "]."
               << std::endl;
    }
    TIME_LOG << "[" << my_paxos_address_ << "] "
             << "[Success] Recovered data and paxos logs." << std::endl;
  }
  return Status::OK;
}

// fail_rate_ >= 1 will work as fail_rate_ == 1
// fail_rate_ <= 0 will work as fail_rate_ == 0
bool MultiPaxosServiceImpl::RandomFail() {
  double rand_num = rand();
  if (rand_num / RAND_MAX < fail_rate_) return true;
  return false;
}

}  // namespace keyvaluestore
