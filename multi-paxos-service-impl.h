#ifndef MULTI_PAXOS_SERVICE_IMPL_H
#define MULTI_PAXOS_SERVICE_IMPL_H

#include <string>
#include <vector>
#include "keyvaluestore.grpc.pb.h"
#include "kv-database.h"

namespace keyvaluestore {

class MultiPaxosServiceImpl final : public MultiPaxos::Service {
 public:
  explicit MultiPaxosServiceImpl(KeyValueDataBase* kv_db) : kv_db_(kv_db) {}

  grpc::Status Prepare(grpc::ServerContext* context,
                       const PrepareRequest* request,
                       PromiseResponse* response) override;

  grpc::Status Propose(grpc::ServerContext* context,
                       const ProposeRequest* request,
                       AcceptResponse* response) override;
  grpc::Status Inform(grpc::ServerContext* context,
                      const InformRequest* request,
                      EmptyMessage* response) override;

 private:
  KeyValueDataBase* kv_db_;
};

}  // namespace keyvaluestore

#endif