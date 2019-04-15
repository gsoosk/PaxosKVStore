#ifndef KV_STORE_SERVICE_IMPL_H
#define KV_STORE_SERVICE_IMPL_H

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "keyvaluestore.grpc.pb.h"
#include "kv-database.h"
#include "time_log.h"

namespace keyvaluestore {

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final : public KeyValueStore::Service {
 public:
  KeyValueStoreServiceImpl(
      const std::string& server_id,
      std::vector<std::unique_ptr<TwoPhaseCommit::Stub>> participants,
      KeyValueDataBase* kv_db)
      : server_id_(server_id),
        participants_(std::move(participants)),
        kv_db_(kv_db) {}
  // Get the corresponding value for a given key
  grpc::Status GetValue(grpc::ServerContext* context, const GetRequest* request,
                        GetResponse* response) override;

  // Put a (key, value) pair into the store
  grpc::Status PutPair(grpc::ServerContext* context, const PutRequest* request,
                       EmptyMessage* response) override {
    return RequestFlow(*request);
  }

  // Delete the corresponding pair from the store for a given key
  grpc::Status DeletePair(grpc::ServerContext* context,
                          const DeleteRequest* request,
                          EmptyMessage* response) override {
    return RequestFlow(*request);
  }

 private:
  TransactionId GetTransactionId();

  void AddRequestToVoteRequest(const DeleteRequest& del_req,
                               VoteRequest* vote_req);
  void AddRequestToVoteRequest(const PutRequest& put_req,
                               VoteRequest* vote_req);

  grpc::Status ExecuteCommit(const CommitRequest& commit_req);

  template <typename Request>
  grpc::Status RequestFlow(const Request& req);

  const std::string server_id_;
  std::vector<std::unique_ptr<TwoPhaseCommit::Stub>> participants_;

  int counter_ = 0;
  std::shared_mutex counter_mtx_;

  KeyValueDataBase* kv_db_;

  std::shared_mutex log_mtx_;
};

}  // namespace keyvaluestore

#endif