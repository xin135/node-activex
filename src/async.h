//-------------------------------------------------------------------------------------------------------
// Project: NodeActiveX
// Author: Yuri Dursin
// Description: Asynchronius call support
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "utils.h"

class job_t {
public:
	Persistent<Promise::Resolver> promise;
	CComPtr<IDispatch> disp;
	VarArguments args;
	CComVariant result;
	LONG dispid, flags;
	HRESULT hrcode;

	typedef void process(const job_t &job);
	typedef std::function<process> process_ptr;
	process_ptr on_result;

	job_t(IDispatch *disp_, DISPID dispid_, LONG flags_): 
		disp(disp_), dispid(dispid_), flags(flags_), hrcode(E_PENDING)
	{}

	void execute() {
		UINT argcnt = args.items.size();
		VARIANT *argptr = (argcnt > 0) ? (VARIANT*)&args.items.front() : nullptr;

		// Don`t works
		hrcode = DispInvoke((IDispatch*)disp, (DISPID)dispid, argcnt, argptr, (VARIANT*)&result, (WORD)flags);

		if (on_result) on_result(*this);
	}
};
typedef std::shared_ptr<job_t> job_ptr;

class job_processor_t {
public:

	void start() {
		if (thread) stop();
		terminated = false;
		thread.reset(new std::thread(&job_processor_t::process_main, this));
	}

	void stop() {
		if (!thread) return;
		terminated = true;
		thread->join();
		thread.reset();
	}

	void push(const job_ptr &job) {
		if (!thread || terminated) {
			job->execute();
		}
		else {
			lock_t lck(safe);
			queue.push_back(job);
			condvar.notify_one();
		}
	}

protected:
	typedef std::unique_lock<std::mutex> lock_t;
	typedef std::deque<job_ptr> queue_t;
	std::unique_ptr<std::thread> thread;
	std::condition_variable condvar;
	std::atomic<bool> terminated;
	std::mutex safe;
	queue_t queue;

	bool process_next() {
		job_ptr job;
		{
			lock_t lck(safe);
			if (!queue.empty()) {
				job = queue.front();
				queue.pop_front();
			}
		}
		if (!job) return false;
		job->execute();
		return true;
	}

	void process_main() {
		std::mutex mtx;
		CoInitialize(0);
		while (!terminated) {
			if (!process_next()) {
				lock_t lck(mtx);
				condvar.wait(lck);
			}
		}
		CoUninitialize();
	}
};
typedef std::shared_ptr<job_processor_t> job_processor_ptr;
extern job_processor_ptr job_processor;