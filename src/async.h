//-------------------------------------------------------------------------------------------------------
// Project: NodeActiveX
// Author: Yuri Dursin
// Description: Asynchronius call support
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "utils.h"

//-------------------------------------------------------------------------------------------------------

class job_t {
public:
	Persistent<Promise::Resolver> promise;
	CComPtr<IDispatch> disp;
	VarArguments args;
	CComVariant result;
	LONG dispid, flags;
	HRESULT hrcode;

	job_t(Isolate *isolate, IDispatch *disp_, DISPID dispid_, LONG flags_):
		disp(disp_), dispid(dispid_), flags(flags_), hrcode(E_PENDING),
		promise(isolate, Promise::Resolver::New(isolate))
	{}

	void execute() {
		UINT argcnt = args.items.size();
		VARIANT *argptr = (argcnt > 0) ? (VARIANT*)&args.items.front() : nullptr;

		// Don`t works
		hrcode = S_OK; // DispInvoke((IDispatch*)disp, (DISPID)dispid, argcnt, argptr, (VARIANT*)&result, (WORD)flags);
	}

	void process() {
		auto isolate = v8::Isolate::GetCurrent();
		v8::HandleScope scope(isolate);
		auto context = isolate->GetCurrentContext();
		//auto global = context->Global();
		Local<Promise::Resolver> resolver = promise.Get(isolate);

		// Don`t works
		if SUCCEEDED(hrcode) resolver->Resolve(context, Variant2Value(isolate, result, true));
		else resolver->Reject(context, Win32Error(isolate, hrcode, L"Async Invocation"));
	}
};
typedef std::shared_ptr<job_t> job_ptr;

class job_processor_t {
public:

	void start() {
		if (thread) stop();
		terminated = false;
		memset(&async, 0, sizeof(async));
		async.data = this;
		uv_async_init(uv_default_loop(), &async, async_process);
		thread.reset(new std::thread(&job_processor_t::process_main, this));
	}

	void stop() {
		if (!thread) return;
		terminated = true;
		thread->join();
		thread.reset();
		uv_close((uv_handle_t*)&async, async_done);
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
	queue_t queue, async_queue;
	std::unique_ptr<std::thread> thread;
	std::condition_variable condvar;
	std::atomic<bool> terminated;
	std::mutex safe, async_safe;
	uv_async_t async;

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
		{
			lock_t lck(async_safe);
			async_queue.push_back(job);
		}
		uv_async_send(&async);
		return true;
	}

	static void async_process(uv_async_t* handle) {
		auto self = static_cast<job_processor_t*>(handle->data);
		while (!self->async_process_next());
	}

	bool async_process_next() {
		job_ptr job;
		{
			lock_t lck(async_safe);
			if (!async_queue.empty()) {
				job = async_queue.front();
				async_queue.pop_front();
			}
		}
		if (job) {
			job->process();
			return true;
		}
		return false;
	}

	static void async_done(uv_handle_t* handle) {
		//auto self = static_cast<job_processor_t*>(handle->data);
		//std::lock_guard<std::mutex> lck(self->async_safe);
		//self->async.data = nullptr;
	}

};
typedef std::shared_ptr<job_processor_t> job_processor_ptr;
extern job_processor_ptr job_processor;

//-------------------------------------------------------------------------------------------------------
