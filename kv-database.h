#ifndef KV_DATABASE_H
#define KV_DATABASE_H

#include <grpcpp/grpcpp.h>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>
#include "keyvaluestore.grpc.pb.h"

namespace keyvaluestore {

struct ValueStatus {
  ValueStatus();
  struct PaxosLog {
    int promised_id;
    int accepted_id;
    OperationType accepted_type;
    string accepted_value;
  };
  string value;
  std::shared_mutex key_mutex;
  std::map<int, PaxosLog> paxos_logs;
};

// An in-memory implementation of a key-value database.
//
// Thread-safe.
class KeyValueDataBase {
 public:
  // A scoped mutator that,
  // (1) Can be used to mutate the corresponding value safely.
  // (2) Performs the actual unlock upon destruction.
  class ValueMutator {
   public:
    ValueMutator(const ValueMutator&) = delete;
    ValueMutator operator=(const ValueMutator&) = delete;
    ValueMutator(ValueMutator&&) = default;
    ValueMutator& operator=(ValueMutator&&) = default;
    ~ValueMutator();

    // Return whether the value is found.
    bool GetValue(std::string* value);
    // Returns true if the value is overwritten, false if the key-val
    // pair is newly added.
    bool SetValue(const std::string& val);
    // Returns true if the deletion actually happens, false if the key
    // didn't exist.
    bool DeleteEntry();

   private:
    ValueMutator(const std::string& key, const std::string& lock_key,
                 KeyValueDataBase* kv_db)
        : key_(key), lock_key_(lock_key), kv_db_(kv_db) {}
    const std::string key_;
    const std::string lock_key_;
    KeyValueDataBase* kv_db_;
    friend class KeyValueDataBase;
  };

  // Locks a given key-value pair. The lock will be auto-released when
  // the return value is out of scope.
  ValueStatus* GetValueStatus(const std::string& key);
  void SetValueStatus(const std::string& key, int round, int promised_id);
  void SetValueStatus(const std::string& key, int round, int accepted_id,
                      OperationType accepted_type, string accepted_value);
  ValueMutator GetValueMutator(const std::string& key);

 private:
  std::map<std::string, ValueStatus> data_map_;
  std::shared_mutex data_mtx_;
  friend class ValueMutator;
};

}  // namespace keyvaluestore

#endif