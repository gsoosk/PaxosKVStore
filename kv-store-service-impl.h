#ifndef KV_STORE_SERVICE_IMPL_H
#define KV_STORE_SERVICE_IMPL_H

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "keyvaluestore.grpc.pb.h"
#include "time_log.h"

namespace keyvaluestore {

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final : public KeyValueStore::Service {
 public:
  KeyValueStoreServiceImpl(PaxosStubsMap* paxos_stubs_map,
                           const std::string& my_paxos_address)
      : paxos_stubs_map_(paxos_stubs_map),
        my_paxos_address_(my_paxos_address) {}

  // Get the corresponding value for a given key
  grpc::Status GetValue(grpc::ServerContext* context, const GetRequest* request,
                        GetResponse* response) override;

  // Put a (key, value) pair into the store
  grpc::Status PutPair(grpc::ServerContext* context, const PutRequest* request,
                       EmptyMessage* response) override;

  // Delete the corresponding pair from the store for a given key
  grpc::Status DeletePair(grpc::ServerContext* context,
                          const DeleteRequest* request,
                          EmptyMessage* response) override;

 private:
  Status ForwardToCoordinator(ClientContext* cc, MultiPaxos::Stub* stub,
                              const GetRequest& request, GetResponse* response);
  Status ForwardToCoordinator(ClientContext* cc, MultiPaxos::Stub* stub,
                              const PutRequest& request,
                              EmptyMessage* response);
  Status ForwardToCoordinator(ClientContext* cc, MultiPaxos::Stub* stub,
                              const DeleteRequest& request,
                              EmptyMessage* response);
  template <typename Request, typename Response>
  Status RequestFlow(const Request& request, Response* response);
  Status GetCoordinator();
  Status ElectNewCoordinator();
  const std::string my_paxos_address_;
  PaxosStubsMap* paxos_stubs_map_;
  std::shared_mutex log_mtx_;
};

}  // namespace keyvaluestore

#endif