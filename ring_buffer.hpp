#ifndef RING_BUFFER
#define RING_BUFFER

#include "common.h"
#include <unistd.h>
#include <semaphore.h>

namespace CarpLog
{

template <typename T>
class RingBuffer
{
private:
    enum State
    {
        IDLE = 0,
        PUTTING,
        VALID,
        TAKING,
    };

    size_t capacity;
    T* slots;
    State* states;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    sem_t data_available;
    sem_t slot_available;

private:
    void yield()
    {
        sleep(0);
    }

public:
    RingBuffer(size_t _capacity) : capacity(_capacity), head(0), tail(0)
    {
        assert(capacity > 0);
        slots = new T[capacity];
        states = new State[capacity];
        for(size_t i = 0; i < capacity; ++i)
            states[i] = State::IDLE;
        DO_WITH_ASSERT(sem_init(&data_available, 0, 0), _ret_ == 0);
        DO_WITH_ASSERT(sem_init(&slot_available, 0, capacity), _ret_ == 0);
    }

    ~RingBuffer()
    {
        delete[] slots;
        delete[] states;
        DO_WITH_ASSERT(sem_destroy(&data_available), _ret_ == 0);
        DO_WITH_ASSERT(sem_destroy(&slot_available), _ret_ == 0);
    }

    void Push(T data)
    {
        DO_WITH_ASSERT(sem_wait(&slot_available), _ret_ == 0);
        auto index = (tail++) % capacity;
        auto* slot = slots + index;
        auto* state = states + index;
        while(!__sync_bool_compare_and_swap(state, State::IDLE, State::PUTTING))
            yield();
        *slot = data;
        DO_WITH_ASSERT(__sync_lock_test_and_set(state, State::VALID), _ret_ == State::PUTTING);
        DO_WITH_ASSERT(sem_post(&data_available), _ret_ == 0);
    }

    T Pop()
    {
        DO_WITH_ASSERT(sem_wait(&data_available), _ret_ == 0);
        auto index = (head++) % capacity;
        auto* slot = slots + index;
        auto* state = states + index;
        while(!__sync_bool_compare_and_swap(state, State::VALID, State::TAKING))
            yield();
        T data = *slot;
        DO_WITH_ASSERT(__sync_lock_test_and_set(state, State::IDLE), _ret_ == State::TAKING);
        DO_WITH_ASSERT(sem_post(&slot_available), _ret_ == 0);
        return data;
    }
};

}
#endif