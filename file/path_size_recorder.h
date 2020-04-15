//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "port/port.h"
#include "util/mutexlock.h"
#include "rocksdb/status.h"
#include "include/rocksdb/env.h"
#include "include/rocksdb/options.h"

namespace rocksdb {

class PathSizeRecorder {
public:
    PathSizeRecorder(Env* env) : env_(env) {}

    void AddCfPaths(uint32_t cfd_id, const std::vector<DbPath>& cf_paths) {
        if (cf_paths.empty())
            return;
        MutexLock l(&mu_);
        std::vector<Path> paths;
        for (auto cf_path : cf_paths) {
            // check whether db_path is tracked
            if (paths_global_id_.find(cf_path.path) == paths_global_id_.end()) {
                // a new cf_path
                uint32_t new_path_id = GetCurrentPathId();
                paths.push_back(Path(new_path_id, cf_path.target_size));
                paths_global_id_[cf_path.path] = new_path_id;
                paths_size_.push_back(GlobalPath(0, cf_path.target_size));
            } else {
                assert(paths_global_id_[cf_path.path] < paths_size_.size());
                paths_size_[paths_global_id_[cf_path.path]].global_path_capacity_ 
                    += cf_path.target_size;
                paths.push_back(Path(paths_global_id_[cf_path.path], cf_path.target_size));
            }
        }
        cfd_paths_.insert(std::make_pair(cfd_id, std::move(paths)));
    }

    void OnAddFile(const std::string& file_path, uint32_t cfd_id, uint32_t path_id) {
        uint64_t file_size;
        Status s = env_->GetFileSize(file_path, &file_size);
        if (s.ok()) {
            MutexLock l(&mu_);
            auto tracked_file = tracked_files_.find(file_path);
            uint32_t global_path_id;
            if (tracked_file != tracked_files_.end()) {
                // File was added before, we will just update the size
                SstFile& sstfile = tracked_file->second;
                assert(sstfile.cfd_id_ == cfd_id && sstfile.local_path_id_ == path_id);
                auto path = cfd_paths_.find(sstfile.cfd_id_);
                assert(path != cfd_paths_.end());
                // Update local path size
                uint64_t& local_path_size = (path->second)[path_id].cfd_local_path_size_;
                assert(local_path_size >= sstfile.file_size_);
                local_path_size -= sstfile.file_size_;
                local_path_size += file_size;
                // Update global path size
                global_path_id = (path->second)[path_id].global_path_id_;
                assert(paths_size_[global_path_id].global_path_size_ >= sstfile.file_size_);
                paths_size_[global_path_id].global_path_size_ -= sstfile.file_size_;
                paths_size_[global_path_id].global_path_size_ += file_size; 
            } else {
                auto path = cfd_paths_.find(cfd_id);
                // cfd_paths is updated before OnAddFile is called
                assert(path != cfd_paths_.end());
                // Update local path size
                uint64_t& local_path_size = (path->second)[path_id].cfd_local_path_size_;
                local_path_size += file_size;
                // Update global path size
                global_path_id = (path->second)[path_id].global_path_id_;
                paths_size_[global_path_id].global_path_size_ += file_size; 
            }
            tracked_files_.insert(
                std::make_pair(file_path, SstFile(cfd_id, path_id, global_path_id, file_size)));
        }
    }

    void OnDeleteFile(const std::string& file_path) {
        MutexLock l(&mu_);
        auto tracked_file = tracked_files_.find(file_path);
        if (tracked_file == tracked_files_.end()) {
            // File is not tracked
            return;
        }
        SstFile& sstfile = tracked_file->second;
        auto path = cfd_paths_.find(sstfile.cfd_id_);
        assert(path != cfd_paths_.end());
        // Update local path size
        uint64_t& local_path_size = (path->second)[sstfile.local_path_id_].cfd_local_path_size_;
        assert(local_path_size >= sstfile.file_size_);
        local_path_size -= sstfile.file_size_;
        // Update global path size
        uint32_t global_path_id = (path->second)[sstfile.local_path_id_].global_path_id_;
        paths_size_[global_path_id].global_path_size_ -= sstfile.file_size_;
        tracked_files_.erase(tracked_file);
    }

    std::vector<std::pair<uint64_t, uint64_t>> 
    GetLocalPathSizeAndCapacity(uint32_t cfd_id) {
        MutexLock l(&mu_);
        std::vector<std::pair<uint64_t, uint64_t>> result;
        auto paths = cfd_paths_.find(cfd_id);
        if (paths == cfd_paths_.end())
            return result;
        for (auto& path : paths->second) {
            result.push_back(
                std::make_pair(path.cfd_local_path_size_, path.path_capacity_));
        }
        return result;
    }

    std::vector<std::pair<uint64_t, uint64_t>> 
    GetGlobalPathSizeAndCapacity(uint32_t cfd_id) {
        MutexLock l(&mu_);
        std::vector<std::pair<uint64_t, uint64_t>> result;
        auto paths = cfd_paths_.find(cfd_id);
        if (paths == cfd_paths_.end())
            return result;
        for (auto& path : paths->second) {
            uint32_t global_path_id = path.global_path_id_;
            result.push_back(
                std::make_pair(paths_size_[global_path_id].global_path_size_, 
                               paths_size_[global_path_id].global_path_capacity_));
        }
        return result;
    }

    // global_path_id_ is a inner auto-increased number for path_id.
    // cfd_local_path_size_ is the size that is maintained by a ColumnFamilyData.
    // path_capacity_ is initialized by ImmutableOptions.cf_paths.
    struct Path {
        uint32_t global_path_id_;
        uint64_t cfd_local_path_size_;
        uint64_t path_capacity_;

        Path(uint32_t path_id, uint64_t capacity)
            : global_path_id_(path_id), cfd_local_path_size_(0), path_capacity_(capacity) {}
    };

    struct GlobalPath {
        uint64_t global_path_size_;
        uint64_t global_path_capacity_;

        GlobalPath(uint64_t global_path_size, uint64_t global_path_capacity)
            : global_path_size_(global_path_size), global_path_capacity_(global_path_capacity) {}
    };

    // A SstFile is owned by a ColumnFamilyData numbered cfd_id_.
    // local_path_id_ is the path number in Options.
    // global_path_id_ is a inner auto-increased number for path_id.
    struct SstFile {
        uint32_t cfd_id_;
        uint32_t local_path_id_;
        uint32_t global_path_id_;
        uint64_t file_size_;

        SstFile(uint32_t cfd_id, uint32_t local_path_id,
                uint32_t global_path_id, uint64_t file_size)
            : cfd_id_(cfd_id), local_path_id_(local_path_id), 
              global_path_id_(global_path_id), file_size_(file_size) {}
    };

private:
    inline uint32_t GetCurrentPathId() { return static_cast<uint32_t>(paths_size_.size()); }

    Env* env_;
    // Mutex to protect
    port::Mutex mu_;
    // A map from ColumnFamilyData ID to a list of Path.
    std::unordered_map<uint32_t, std::vector<Path>> cfd_paths_;
    // A map from path name to global path id.
    // In order to figure out whether a path is tracked.
    std::unordered_map<std::string, uint32_t> paths_global_id_;
    // size of all paths indexed by global path id.
    std::vector<GlobalPath> paths_size_;
    // A map containing all tracked files and there path id & sizes
    //  file_path => SstFile.
    std::unordered_map<std::string, SstFile> tracked_files_;
};

} // namespace rocksdb