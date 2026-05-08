#include "continuous/core/Jobs.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

namespace cn::jobs {

struct Job {
    JobFn               fn;
    std::atomic<u32>    pending_parents{0};
    std::atomic<bool>   done{false};
    std::vector<Job*>   children;
    u32                 id{0};
};

void System::init(u32 worker_count) {
    if (worker_count == 0) {
        unsigned hc = std::thread::hardware_concurrency();
        worker_count = hc > 1 ? hc - 1 : 1;
    }
    stop_.store(false);
    for (u32 i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this, i] { worker_loop_(i); });
    }
    CN_INFO("jobs", "initialized {} worker threads", worker_count);
}

void System::shutdown() {
    if (workers_.empty()) return;
    stop_.store(true);
    cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
    {
        std::lock_guard<std::mutex> lk(mu_);
        alive_.clear();
        ready_.clear();
    }
    pending_.store(0);
    CN_INFO("jobs", "shutdown");
}

JobHandle System::schedule(JobFn fn, std::initializer_list<JobHandle> deps) {
    auto j = std::make_unique<Job>();
    j->fn = std::move(fn);
    j->id = next_id_.fetch_add(1, std::memory_order_relaxed);
    Job* raw = j.get();

    {
        std::lock_guard<std::mutex> lk(mu_);
        alive_.push_back(std::move(j));
    }

    pending_.fetch_add(1, std::memory_order_relaxed);

    u32 unmet = 0;
    for (auto& d : deps) {
        if (!d.valid()) continue;
        if (!d.job->done.load(std::memory_order_acquire)) {
            link_parent_(raw, d.job);
            ++unmet;
        }
    }
    raw->pending_parents.store(unmet, std::memory_order_release);

    if (unmet == 0) {
        std::lock_guard<std::mutex> lk(mu_);
        ready_.push_back(raw);
        cv_.notify_one();
    }
    return JobHandle{raw};
}

void System::link_parent_(Job* child, Job* parent) {
    std::lock_guard<std::mutex> lk(mu_);
    if (parent->done.load(std::memory_order_acquire)) {
        // race: parent finished between caller's check and lock - decrement now.
        u32 prev = child->pending_parents.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1) ready_.push_back(child);
    } else {
        parent->children.push_back(child);
    }
}

void System::on_complete_(Job* j) {
    std::vector<Job*> to_release;
    {
        std::lock_guard<std::mutex> lk(mu_);
        j->done.store(true, std::memory_order_release);
        for (Job* c : j->children) {
            u32 prev = c->pending_parents.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 1) to_release.push_back(c);
        }
        for (Job* c : to_release) ready_.push_back(c);
        if (!to_release.empty()) cv_.notify_all();
    }
    pending_.fetch_sub(1, std::memory_order_acq_rel);
    cv_.notify_all();
}

bool System::execute_one_() {
    Job* j = nullptr;
    {
        std::unique_lock<std::mutex> lk(mu_);
        if (!ready_.empty()) {
            j = ready_.front();
            ready_.pop_front();
        }
    }
    if (!j) return false;
    j->fn();
    on_complete_(j);
    return true;
}

void System::worker_loop_(u32 idx) {
    (void)idx;
    while (!stop_.load(std::memory_order_relaxed)) {
        Job* j = nullptr;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return stop_.load() || !ready_.empty(); });
            if (stop_.load() && ready_.empty()) return;
            if (!ready_.empty()) {
                j = ready_.front();
                ready_.pop_front();
            }
        }
        if (j) {
            j->fn();
            on_complete_(j);
        }
    }
}

void System::wait(JobHandle h) {
    if (!h.valid()) return;
    Job* j = h.job;
    while (!j->done.load(std::memory_order_acquire)) {
        if (!execute_one_()) {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait_for(lk, std::chrono::microseconds(100),
                         [&] { return j->done.load() || !ready_.empty() || stop_.load(); });
        }
    }
}

void System::wait_all() {
    while (pending_.load(std::memory_order_acquire) > 0) {
        if (!execute_one_()) {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait_for(lk, std::chrono::microseconds(100),
                         [&] { return pending_.load() == 0 || !ready_.empty(); });
        }
    }
}

JobHandle System::parallel_for(usize begin, usize end, usize grain,
                               std::function<void(usize, usize)> body) {
    if (end <= begin) {
        // empty: schedule a no-op so callers can wait on a handle
        return schedule([] {}, {});
    }
    if (grain == 0) grain = 1;
    usize total = end - begin;
    usize chunks = (total + grain - 1) / grain;
    if (chunks == 1) {
        usize a = begin, b = end;
        return schedule([body, a, b] { body(a, b); }, {});
    }

    // We want to wait on a single handle, so create a "sentinel" job whose
    // dependencies are all the chunk jobs.
    std::vector<JobHandle> chunk_handles;
    chunk_handles.reserve(chunks);
    for (usize i = 0; i < chunks; ++i) {
        usize a = begin + i * grain;
        usize b = a + grain; if (b > end) b = end;
        chunk_handles.push_back(schedule([body, a, b] { body(a, b); }, {}));
    }
    // schedule sentinel with all chunk handles as deps
    auto sentinel = std::make_unique<Job>();
    sentinel->id = next_id_.fetch_add(1);
    sentinel->fn = [] {};
    Job* raw = sentinel.get();
    {
        std::lock_guard<std::mutex> lk(mu_);
        alive_.push_back(std::move(sentinel));
    }
    pending_.fetch_add(1, std::memory_order_relaxed);
    u32 unmet = 0;
    for (auto& d : chunk_handles) {
        if (!d.job->done.load(std::memory_order_acquire)) {
            link_parent_(raw, d.job);
            ++unmet;
        }
    }
    raw->pending_parents.store(unmet, std::memory_order_release);
    if (unmet == 0) {
        std::lock_guard<std::mutex> lk(mu_);
        ready_.push_back(raw);
        cv_.notify_one();
    }
    return JobHandle{raw};
}

System& global() {
    static System s;
    return s;
}

} // namespace cn::jobs
