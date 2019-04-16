#include "paxos-stubs-map.h"

namespace keyvaluestore {

using PaxosStubs = std::map<std::string, std::unique_ptr<MultiPaxos::Stub>>;

std::string PaxosStubsMap::GetCoordinator() {
  std::shared_lock<std::shared_mutex> reader_lock(coordinator_mtx_);
  return coordinator_;
}

void PaxosStubsMap::SetCoordinator(const std::string& coordinator) {
  std::unique_lock<std::shared_mutex> writer_lock(servers_mtx_);
  coordinator_ = coordinator;
}

MultiPaxos::Stub* PaxosStubsMap::GetCoordinatorStub() {
  std::shared_lock<std::shared_mutex> reader_lock(coordinator_mtx_);
  std::shared_lock<std::shared_mutex> reader_lock(stubs_mtx_);
  return stubs_[coordinator_].get();
}

std::map<std::string, MultiPaxos::Stub*> PaxosStubsMap::GetPaxosStubs() {
  std::map<std::string, MultiPaxos::Stub*> paxos_stubs;
  std::shared_lock<std::shared_mutex> reader_lock(stubs_mtx_);
  for (const auto& kv : stubs_) {
    paxos_stubs[kv.first] = kv.second.get();
  }
  return paxos_stubs;
}

}  // namespace keyvaluestore