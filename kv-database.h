#ifndef KV_DATABASE_H
#define KV_DATABASE_H

#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace keyvaluestore {

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
  grpc::Status TryLock(const std::string& lock_key, const std::string& key);

  // Unlocks a `key` locked by `lock_key`. Returns a object that,
  // (1) Can be used to mutate the corresponding value safely.
  // (2) Performs the actual unlock upon destruction, so that subsequent
  // TryLock attempts on that `key` can succeed.
  ValueMutator Unlock(const std::string& lock_key, const std::string& key);

 private:
  // Key is lock_key. Value is the key in the in_memory_map_.
  std::unordered_map<std::string, std::string> lock_key_map_;
  // Same content as above, but key-value relations are reversed.
  std::unordered_map<std::string, std::string> rev_lock_key_map_;
  std::shared_mutex lock_map_mtx_;

  std::unordered_map<std::string, std::string> data_map_;
  std::shared_mutex data_mtx_;
  friend class ValueMutator;
};

}  // namespace keyvaluestore

#endif