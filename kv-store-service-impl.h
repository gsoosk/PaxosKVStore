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
#include "paxos-stubs-map.h"
#include "time_log.h"

namespace keyvaluestore {

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final : public KeyValueStore::Service {
 public:
  KeyValueStoreServiceImpl(PaxosStubsMap* paxos_stubs_map,
                           const std::string& keyvaluestore_address,
                           const std::string& my_paxos_address)
      : paxos_stubs_map_(paxos_stubs_map),
        keyvaluestore_address_(keyvaluestore_address),
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
  grpc::Status ForwardToCoordinator(grpc::ClientContext* cc,
                                    MultiPaxos::Stub* stub,
                                    const GetRequest& request,
                                    GetResponse* response);
  grpc::Status ForwardToCoordinator(grpc::ClientContext* cc,
                                    MultiPaxos::Stub* stub,
                                    const PutRequest& request,
                                    EmptyMessage* response);
  grpc::Status ForwardToCoordinator(grpc::ClientContext* cc,
                                    MultiPaxos::Stub* stub,
                                    const DeleteRequest& request,
                                    EmptyMessage* response);
  template <typename Request, typename Response>
  grpc::Status RequestFlow(const Request& request, Response* response);
  grpc::Status ElectNewCoordinator();

  const std::string keyvaluestore_address_;
  const std::string my_paxos_address_;
  PaxosStubsMap* paxos_stubs_map_;
  std::shared_mutex log_mtx_;
};

}  // namespace keyvaluestore

#endif