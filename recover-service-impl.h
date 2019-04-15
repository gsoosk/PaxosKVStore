#ifndef RECOVER_SERVICE_IMPL_H
#define RECOVER_SERVICE_IMPL_H

#include <string>
#include <vector>
#include "keyvaluestore.grpc.pb.h"
#include "kv-database.h"

namespace keyvaluestore {

class RecoverServiceImpl final : public Recover::Service {
 public:
  explicit RecoverServiceImpl(KeyValueDataBase* kv_db) : kv_db_(kv_db) {}

  grpc::Status Greet(grpc::ServerContext* context, const Greeting* request,
                     EmptyMessage* response) override;

 private:
  static std::string GetKey(const PrepareRequest& request);

  KeyValueDataBase* kv_db_;
};

}  // namespace keyvaluestore

#endif