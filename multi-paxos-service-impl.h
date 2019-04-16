#ifndef MULTI_PAXOS_SERVICE_IMPL_H
#define MULTI_PAXOS_SERVICE_IMPL_H

#include <string>
#include <vector>
#include "keyvaluestore.grpc.pb.h"
#include "kv-database.h"

namespace keyvaluestore {

class MultiPaxosServiceImpl final : public MultiPaxos::Service {
 public:
  explicit MultiPaxosServiceImpl(PaxosStubsMap* paxos_stubs_map,
                                 KeyValueDataBase* kv_db)
      : paxos_stubs_map_(paxos_stubs_map), kv_db_(kv_db) {}

  // Get the corresponding value for a given key
  grpc::Status GetValue(grpc::ServerContext* context, const GetRequest* request,
                        GetResponse* response) override;

  // Put a (key, value) pair into the store
  grpc::Status PutPair(grpc::ServerContext* context, const PutRequest* request,
                       EmptyMessage* response) override;

  // Delete the corresponding pair from the store for a given key   grpc::Status
  DeletePair(grpc::ServerContext* context, const DeleteRequest* request,
             EmptyMessage* response)
      override;  // Paxos phase 1. Coordinator -> Acceptor.   grpc::Status
  Prepare(grpc::ServerContext* context, const PrepareRequest* request,
          PromiseResponse* response)
      override;  // Paxos phase 2. Coordinator -> Acceptor.   grpc::Status
  Propose(grpc::ServerContext* context, const ProposeRequest* request,
          AcceptResponse* response)
      override;  // Paxos phase 3. Coordinator -> Learner.   grpc::Status
  Inform(grpc::ServerContext* context, const InformRequest* request,
         EmptyMessage* response) override;

 private:
  void SetProposeValue(const PutRequest& put_req, ProposeRequest* propose_req);
  void SetProposeValue(const DeleteRequest& del_req,
                       ProposeRequest* propose_req);
  template <typename Request>
  Status RunPaxos(const Request& req);
  KeyValueDataBase* kv_db_;
  PaxosStubsMap* paxos_stubs_map_;
  std::shared_mutex log_mtx_;
};

}  // namespace keyvaluestore

#endif