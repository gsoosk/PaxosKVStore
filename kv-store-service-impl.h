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
  KeyValueStoreServiceImpl(PaxosStubsMap* paxos_stubs_map)
      : paxos_stubs_map_(paxos_stubs_map) {}

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
  void FindNewCoordinator();
  PaxosStubsMap* paxos_stubs_map_;
  std::shared_mutex log_mtx_;
};

}  // namespace keyvaluestore

#endif