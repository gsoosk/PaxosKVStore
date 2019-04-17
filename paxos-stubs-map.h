#ifndef PAXOS_STUBS_MAP_H
#define PAXOS_STUBS_MAP_H

#include <grpcpp/grpcpp.h>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

namespace keyvaluestore {
using PaxosStubs = std::map<std::string, std::unique_ptr<MultiPaxos::Stub>>;

// Stores a map from address to Paxos stubs.
// Thread-safe.
class PaxosStubsMap {
 public:
  PaxosStubsMap(PaxosStubs stubs) : stubs_(std::move(stubs)) {}
  std::string GetCoordinator();
  bool SetCoordinator(const std::string& coordinator);
  MultiPaxos::Stub* GetCoordinatorStub();
  MultiPaxos::Stub* GetStub(const std::string& address);
  std::map<std::string, MultiPaxos::Stub*> GetPaxosStubs();

 private:
  PaxosStubs stubs_;
  std::shared_mutex stubs_mtx_;
  std::string coordinator_;
  std::shared_mutex coordinator_mtx_;
};

}  // namespace keyvaluestore

#endif