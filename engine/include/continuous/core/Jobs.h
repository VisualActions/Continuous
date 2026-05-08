// Continuous Engine - job system / task graph.
//
// Why this design: we want simple parallel-for and DAG-style dependent jobs
// without leaning on TBB or OpenMP. A handful of worker threads pull from a
// shared lock-protected MPMC queue (good enough at the scale of a game frame -
// dozens to low thousands of jobs/sec, not millions). Jobs can declare
// dependencies as a list of parent JobHandles that must finish first; the
// scheduler decrements a child's pending-parent counter on each parent
// completion and enqueues the child once it hits zero.
//
// Wait semantics: wait(handle) parks the caller until the job completes;
// while parked the caller participates in stealing work to avoid deadlock if
// all worker threads are inside nested waits.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace cn::jobs {

struct Job; // forward

struct JobHandle {
    Job* job{nullptr};
    bool valid() const { return job != nullptr; }
};

using JobFn = std::function<void()>;

// ----------------------------------------------------------------------------
// Job system.
// ----------------------------------------------------------------------------
class CN_API System {
public:
    System() = default;
    ~System() { shutdown(); }
    CN_NONCOPYABLE(System);

    void init(u32 worker_count = 0); // 0 = auto = hardware_concurrency-1
    void shutdown();

    // Schedule a job that runs on a worker. Optional list of parents - this
    // job will not be enqueued until every parent finishes. The returned
    // handle is owned by the system; it stays valid until wait()/wait_all().
    JobHandle schedule(JobFn fn, std::initializer_list<JobHandle> deps = {});

    // Parallel for: split [begin,end) into roughly worker_count*4 chunks, each
    // chunk submitted as one job. Returns a handle that completes when all
    // chunks have completed.
    JobHandle parallel_for(usize begin, usize end, usize grain,
                           std::function<void(usize, usize)> body);

    // Block until job completes. Caller participates in execution.
    void wait(JobHandle h);

    // Block until all queued work drains.
    void wait_all();

    u32 worker_count() const { return static_cast<u32>(workers_.size()); }

private:
    void worker_loop_(u32 idx);
    bool execute_one_(); // try to pop and run one job. returns true if did.

    void link_parent_(Job* child, Job* parent);
    void on_complete_(Job* j);

    std::vector<std::thread>         workers_;
    std::mutex                       mu_;
    std::condition_variable          cv_;
    std::deque<Job*>                 ready_;
    std::vector<std::unique_ptr<Job>> alive_;
    std::atomic<u32>                 pending_{0};
    std::atomic<bool>                stop_{false};
    std::atomic<u32>                 next_id_{1};
};

CN_API System& global();

} // namespace cn::jobs
