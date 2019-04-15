#include "multi-paxos-service-impl.h"

namespace keyvaluestore {

using grpc::Status;
// Logic upon receiving a Greet message.
// Role: Acceptor
Status RecoverServiceImpl::Greet(grpc::ServerContext* context,
                                 const Greeting* request,
                                 EmptyMessage* response) {
  int leader_id = request->get_leader_id();
}

}  // namespace keyvaluestore
