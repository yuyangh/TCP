#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <atomic>
#include <common.h>
#include <unistd.h>
#include <semaphore.h>

namespace CAPLog
{

template <typename T>
class RingBuffer
{
private:
    struct Slot
    {
        T data;
        volatile bool valid;

        Slot() : valid(false) {}
    };

    Slot* slots;
    size_t capacity;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    sem_t data_available;
    sem_t slot_available;

    void yield()
    {
        sleep(0);
    }

public:
    RingBuffer(size_t _capacity) : capacity(_capacity), head(0), tail(0)
    {
        assert(capacity > 0);
        slots = new Slot[capacity];
        DO_WITH_ASSERT(sem_init(&data_available, 0, 0), _ret_ == 0);
        DO_WITH_ASSERT(sem_init(&slot_available, 0, capacity), _ret_ == 0);
    }

    ~RingBuffer()
    {
        delete slots;
        DO_WITH_ASSERT(sem_destroy(&data_available), _ret_ == 0);
        DO_WITH_ASSERT(sem_destroy(&slot_available), _ret_ == 0);
    }

    void Push(T data)
    {
        DO_WITH_ASSERT(sem_wait(&slot_available), _ret_ == 0);
        auto index = (tail++) % capacity;
        auto* slot = slots + index;
        while(slot->valid)
            yield();
        slot->data = data;
        __builtin_ia32_sfence();
        slot->valid = true;
        DO_WITH_ASSERT(sem_post(&data_available), _ret_ == 0);
    }

    T Pop()
    {
        DO_WITH_ASSERT(sem_wait(&data_available), _ret_ == 0);
        auto index = (head++) % capacity;
        auto* slot = slots + index;
        while(!slot->valid)
            yield();
        T data = slot->data;
        __builtin_ia32_sfence();
        slot->valid = false;
        DO_WITH_ASSERT(sem_post(&slot_available), _ret_ == 0);
        return data;
    }
};

}
#endif
