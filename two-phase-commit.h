#ifndef TWO_PHASE_COMMIT_H
#define TWO_PHASE_COMMIT_H

#include <string>
#include <vector>
#include "keyvaluestore.grpc.pb.h"
#include "kv-database.h"

namespace keyvaluestore {

class TwoPhaseCommitServiceImpl final : public TwoPhaseCommit::Service {
 public:
  explicit TwoPhaseCommitServiceImpl(KeyValueDataBase* kv_db) : kv_db_(kv_db) {}

  grpc::Status Vote(grpc::ServerContext* context, const VoteRequest* request,
                    VoteResponse* response) override;

  grpc::Status Commit(grpc::ServerContext* context,
                      const CommitRequest* request,
                      EmptyMessage* response) override;

 private:
  static std::string GetKey(const VoteRequest& request);
  static std::string GetLockKey(const VoteRequest& request);
  static std::string GetLockKey(const CommitRequest& request);

  std::unordered_map<std::string, VoteRequest> cached_vote_req_;
  std::shared_mutex cache_mutex_;

  KeyValueDataBase* kv_db_;
};

}  // namespace keyvaluestore

#endif