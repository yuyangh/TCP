#pragma once

#include "common.h"
#include <unistd.h>
#include <semaphore.h>

// T: 负载类型
// block_pop: 若为true，当队列为空时，会阻塞等待。若为false，请确保pop时队列一定不为空
// block_push: 若为true，当队列为满时，会阻塞等待。若为false，请确保push时队列一定不为满
// busy_wait: 若slot存在争抢，是否忙等（否则放弃时间片）
template<typename T, bool block_pop, bool block_push, bool busy_wait = false>
class RingBuffer {
private:
	enum State {
		IDLE = 0,       // 该slot没有数据
		PUTTING,        // 有一个线程正在放入数据
		VALID,          // 该slot有数据
		TAKING,         // 有一个线程正在取出数据
	};
	
	size_t capacity;            // 容量
	T *slots;                   // 所有的slot
	State *states;              // 每一个slot的状态
	std::atomic<size_t> head;   // 逻辑上的头（不回滚）
	std::atomic<size_t> tail;   // 逻辑上的尾（不回滚）
	sem_t data_available;       // 可以取得的数据个数
	sem_t slot_available;       // 可以使用的slot个数

private:
	void wait() {
		if (!busy_wait)
			sleep(0);
	}

public:
	// capacity: 最大容量
	RingBuffer(size_t _capacity) : capacity(_capacity), head(0), tail(0) {
		assert(capacity > 0);
		slots = new T[capacity];
		states = new State[capacity];
		for (size_t i = 0; i < capacity; i++)
			states[i] = State::IDLE;
		if (block_pop)
			DO_WITH_ASSERT(sem_init(&data_available, 0, 0), _ret_ == 0);
		if (block_push)
			DO_WITH_ASSERT(sem_init(&slot_available, 0, capacity), _ret_ == 0);
	}
	
	~RingBuffer() {
		delete[] slots;
		delete[] states;
		if (block_pop)
			DO_WITH_ASSERT(sem_destroy(&data_available), _ret_ == 0);
		if (block_push)
			DO_WITH_ASSERT(sem_destroy(&slot_available), _ret_ == 0);
	}
	
	// 向队列塞入（多线程安全）
	void Push(T data) {
		if (block_push)
			DO_WITH_ASSERT(sem_wait(&slot_available), _ret_ == 0);
		auto index = (tail++) % capacity;
		auto *slot = slots + index;
		auto *state = states + index;
		// 争抢放入权
		while (!__sync_bool_compare_and_swap(state, State::IDLE, State::PUTTING))
			wait();
		*slot = data;
		// 必然有且只有一个线程（即当前线程）会修改state，所以必然一次性成功
		DO_WITH_ASSERT(__sync_lock_test_and_set(state, State::VALID), _ret_ == State::PUTTING);
		if (block_pop)
			DO_WITH_ASSERT(sem_post(&data_available), _ret_ == 0);
	}
	
	// 从队列取出（多线程安全）
	T Pop() {
		if (block_pop)
			DO_WITH_ASSERT(sem_wait(&data_available), _ret_ == 0);
		auto index = (head++) % capacity;
		auto *slot = slots + index;
		auto *state = states + index;
		// 争抢取出权
		while (!__sync_bool_compare_and_swap(state, State::VALID, State::TAKING))
			wait();
		T data = *slot;
		// 必然有且只有一个线程（即当前线程）会修改state，所以必然一次性成功
		DO_WITH_ASSERT(__sync_lock_test_and_set(state, State::IDLE), _ret_ == State::TAKING);
		if (block_push)
			DO_WITH_ASSERT(sem_post(&slot_available), _ret_ == 0);
		return data;
	}
	
	// 获取当前队列长度（多线程安全）
	size_t Length() {
		size_t head_snap = head.load();
		size_t tail_snap = tail.load();
		if (head_snap < tail_snap)
			return tail_snap - head_snap;
		else
			return 0;
	}
};
