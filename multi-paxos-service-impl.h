#ifndef MULTI_PAXOS_SERVICE_IMPL_H
#define MULTI_PAXOS_SERVICE_IMPL_H

#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "keyvaluestore.grpc.pb.h"
#include "kv-database.h"
#include "paxos-stubs-map.h"
#include "time_log.h"

namespace keyvaluestore {

class MultiPaxosServiceImpl final : public MultiPaxos::Service {
 public:
  MultiPaxosServiceImpl(PaxosStubsMap* paxos_stubs_map, KeyValueDataBase* kv_db,
                        const std::string& my_paxos_address);
  grpc::Status Initialize();

  // Get the corresponding value for a given key.
  grpc::Status GetValue(grpc::ServerContext* context, const GetRequest* request,
                        GetResponse* response) override;
  // Put a (key, value) pair into the store.
  grpc::Status PutPair(grpc::ServerContext* context, const PutRequest* request,
                       EmptyMessage* response) override;
  // Delete the corresponding pair from the store for a given key.
  grpc::Status DeletePair(grpc::ServerContext* context,
                          const DeleteRequest* request,
                          EmptyMessage* response) override;
  // Update coordinator when old coordinator is unavailable.
  grpc::Status ElectCoordinator(grpc::ServerContext* context,
                                const ElectCoordinatorRequest* request,
                                EmptyMessage* response) override;
  // Get the current coordinator.
  grpc::Status GetCoordinator(grpc::ServerContext* context,
                              const EmptyMessage* request,
                              GetCoordinatorResponse* response) override;

  // Paxos phase 1. Coordinator -> Acceptor.
  grpc::Status Prepare(grpc::ServerContext* context,
                       const PrepareRequest* request,
                       PromiseResponse* response) override;
  // Paxos phase 2. Coordinator -> Acceptor.
  grpc::Status Propose(grpc::ServerContext* context,
                       const ProposeRequest* request,
                       AcceptResponse* response) override;
  // Paxos phase 3. Coordinator -> Learner.
  grpc::Status Inform(grpc::ServerContext* context,
                      const InformRequest* request,
                      EmptyMessage* response) override;

  // Test if the server is available.
  grpc::Status Ping(grpc::ServerContext* context, const EmptyMessage* request,
                    EmptyMessage* response) override;

  // After brought up again, a server will catch up with others' logs.
  grpc::Status Recover(grpc::ServerContext* context,
                       const EmptyMessage* request,
                       RecoverResponse* response) override;

 private:
  void SetProposeValue(const ElectCoordinatorRequest& set_cdnt_req,
                       ProposeRequest* propose_req);
  void SetProposeValue(const PutRequest& put_req, ProposeRequest* propose_req);
  void SetProposeValue(const DeleteRequest& del_req,
                       ProposeRequest* propose_req);
  template <typename Request>
  grpc::Status RunPaxos(const Request& req);
  grpc::Status GetCoordinator();
  grpc::Status ElectNewCoordinator();
  grpc::Status GetRecovery();
  bool RandomFail(double fail_rate);

  const std::string my_paxos_address_;
  KeyValueDataBase* kv_db_;
  PaxosStubsMap* paxos_stubs_map_;
  std::shared_mutex log_mtx_;
};

}  // namespace keyvaluestore

#endif