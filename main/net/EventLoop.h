// EventLoop.h
// Created by Lixin on 2020.02.11
// Version 0.2

#ifndef MAIN_NET_EVENTLOOP_H
#define MAIN_NET_EVENTLOOP_H

class Channel;
class Poller;
class TimerQueue;

// Reactor,one thread one loop
// interface class

#include <atomic>
#include <functional>
#include <vector>

#include <boost/any.hpp>

#include "main/base/Mutex.h"
#include "main/base.CurrentThread.h"
#include "main/base/Timestamp.h"
#include "main/net/Callbacks.h"
#include "main/net/TimerId.h"

namespace main{

namespace net{
	
class EventLoop : noncopyable
{
public:
	typedef std::fuction<void()> Functor;

	EventLoop();
	~EventLoop();

	//Loops forever
	void loop();
	//Quits loop
	void quit();

	Timerstamp pollReturnTime() const {return pollReturnTime_;}

	int64_t iteration() const {return tireation_;}

private:
	void abortNotInLoopThread();
	void handleRead(); // waked up;
	void doPendingFunctors();

	void printActiveChannels() const; // DEBUG 

	typedef std::vector<Channel*> ChannelList;

	bool looping_; // atomic
	std::atomic<bool> quit_;
	bool eventHandling_; // atomic;
	bool callingPendingFunctors_; // atomic
	int64_t iterarion_;
	const pid_t threadId_;
	Timestamp pollReturnTime_;
	std::unique_ptr<Poller> poller_;
	std::unique_ptr<TimerQueue> timerQueue_;
	int wakeunFd_;
	std::unique_ptr<Channel> wakeupChannel_;
	boost::any context_;

	ChannelList activeChannels_;
	Channel* currentActiveChannel_;

	mutable MutexLock mutex_;
	std::vector<Functor> pendingFunctors_ GUARDED_BY(mutex_);
};

}// namespace net 
}// namespace main

#endif //MAIN_NET_EVENT_LOOP_H

/*
//Version 0.1
#include <boost/utility.hpp>
#include <pthread.h>

#include "Channel.h"
#include "Poller.h"
#include "TimerQueue.h"


class EventLoop{
public:
	EventLoop();
	~EventLoop();
	
	void loop();
	static EventLoop* getEventLoopOfCurrentThread();
private:
	const pid_t threadId_;
	bool isInLoopThread();
	bool assertInLoopThread();
}
*/
