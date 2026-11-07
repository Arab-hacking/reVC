#ifndef REVC_CUSTOM_BACKGROUNDASSETQUEUE_H
#define REVC_CUSTOM_BACKGROUNDASSETQUEUE_H

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>

// One CPU-only worker for custom archive reads, .mod preparation and .btx
// decoding. No RenderWare objects, game model info or GPU calls may be touched
// by a job. Results are handed back to the game thread. The number of queued
// jobs and *completed* results is bounded; even when the game is paused the
// worker cannot accumulate a whole map pack of decoded images in memory.
namespace customcache {

template <typename T>
class BackgroundAssetQueue
{
public:
	struct Completed {
		std::string key;
		std::shared_ptr<T> value; // null is a completed but failed decode
	};

private:
	struct Job {
		std::string key;
		uint64_t ticket;
		bool priority;
		std::function<std::shared_ptr<T>()> run;
	};
	struct Finished {
		Completed result;
		uint64_t ticket;
	};

	mutable std::mutex mutex;
	std::condition_variable changed;
	std::deque<Job> waiting;
	std::deque<Finished> finished;
	std::map<std::string, uint64_t> pending;
	std::thread worker;
	const size_t maxPending;
	const size_t maxFinished;
	uint64_t nextTicket;
	bool stopping;
	bool unavailable;

	void Run(void)
	{
		for(;;){
			Job job;
			{
				std::unique_lock<std::mutex> lock(mutex);
				changed.wait(lock, [this]{ return stopping || (!waiting.empty() && finished.size() < maxFinished); });
				if(stopping)
					return;
				job = std::move(waiting.front());
				waiting.pop_front();
			}
			std::shared_ptr<T> value;
			try { value = job.run(); }
			catch(...) { value.reset(); } // errors are reported by the game thread
			{
				std::unique_lock<std::mutex> lock(mutex);
				changed.wait(lock, [this, &job]{
					std::map<std::string, uint64_t>::const_iterator it = pending.find(job.key);
					return stopping || it == pending.end() || it->second != job.ticket || finished.size() < maxFinished;
				});
				if(stopping)
					return;
				std::map<std::string, uint64_t>::const_iterator it = pending.find(job.key);
				if(it != pending.end() && it->second == job.ticket){
					Finished result;
					result.result.key = job.key;
					result.result.value = std::move(value);
					result.ticket = job.ticket;
					finished.push_back(std::move(result));
				}
			}
			changed.notify_all();
		}
	}

public:
	BackgroundAssetQueue(size_t maxJobs, size_t maxResults)
		: maxPending(maxJobs), maxFinished(maxResults), nextTicket(0), stopping(false), unavailable(false) {}
	BackgroundAssetQueue(const BackgroundAssetQueue&) = delete;
	BackgroundAssetQueue& operator=(const BackgroundAssetQueue&) = delete;
	~BackgroundAssetQueue(void) { Stop(); }

	bool Schedule(const std::string &key, const std::function<std::shared_ptr<T>()> &job, bool priority = false)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if(stopping || unavailable)
			return false;
		if(pending.find(key) != pending.end()){
			// A later high-priority request can promote an earlier background
			// prefetch, without decoding that same file twice.
			if(priority)
				for(typename std::deque<Job>::iterator it = waiting.begin(); it != waiting.end(); ++it)
					if(it->key == key && !it->priority){
						Job promoted = std::move(*it);
						waiting.erase(it);
						promoted.priority = true;
						typename std::deque<Job>::iterator firstNormal = waiting.begin();
						while(firstNormal != waiting.end() && firstNormal->priority) ++firstNormal;
						waiting.insert(firstNormal, std::move(promoted));
						break;
					}
			return true; // queued, running or waiting for main-thread consumption
		}
		if(pending.size() >= maxPending || maxPending == 0 || maxFinished == 0)
			return false;
		if(!worker.joinable()){
			try { worker = std::thread(&BackgroundAssetQueue::Run, this); }
			catch(const std::exception&) { unavailable = true; return false; }
		}
		Job item;
		item.key = key;
		item.ticket = ++nextTicket;
		item.priority = priority;
		item.run = job;
		pending[key] = item.ticket;
		if(priority){
			typename std::deque<Job>::iterator firstNormal = waiting.begin();
			while(firstNormal != waiting.end() && firstNormal->priority) ++firstNormal;
			waiting.insert(firstNormal, std::move(item));
		}else
			waiting.push_back(std::move(item));
		changed.notify_all();
		return true;
	}

	bool IsPending(const std::string &key) const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return pending.find(key) != pending.end();
	}

	bool Pop(Completed &result)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if(finished.empty())
			return false;
		Finished item = std::move(finished.front());
		finished.pop_front();
		std::map<std::string, uint64_t>::iterator it = pending.find(item.result.key);
		if(it != pending.end() && it->second == item.ticket)
			pending.erase(it);
		result = std::move(item.result);
		changed.notify_all();
		return true;
	}

	// Used only by the game's explicit blocking load paths (startup, scripts).
	// Ordinary streaming never waits on CPU preparation in a frame.
	void WaitForResult(void)
	{
		std::unique_lock<std::mutex> lock(mutex);
		changed.wait(lock, [this]{ return !finished.empty() || pending.empty() || stopping || unavailable; });
	}

	void Cancel(const std::string &key)
	{
		std::lock_guard<std::mutex> lock(mutex);
		pending.erase(key);
		for(typename std::deque<Job>::iterator it = waiting.begin(); it != waiting.end(); ){
			if(it->key == key) it = waiting.erase(it);
			else ++it;
		}
		for(typename std::deque<Finished>::iterator it = finished.begin(); it != finished.end(); ){
			if(it->result.key == key) it = finished.erase(it);
			else ++it;
		}
		changed.notify_all();
	}

	void Stop(void)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			stopping = true;
			waiting.clear();
			finished.clear();
			pending.clear();
		}
		changed.notify_all();
		if(worker.joinable())
			worker.join(); // the archive index must not be freed until this returns
		std::lock_guard<std::mutex> lock(mutex);
		stopping = false;
		unavailable = false;
	}

	size_t PendingCount(void) const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return pending.size();
	}
};

} // namespace customcache

#endif
