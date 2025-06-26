#pragma once
#include <atomic>
#include <cassert>
#include <memory>

// Single‐consumer, multi‐producer, lock‐free queue.
// T must be MoveConstructible.
template<typename T>
class MPSCQueue 
{
private:
    std::atomic<Node*> head;
    Node* tail;

    struct Node 
    {
        std::atomic<Node*> next{ nullptr };
        T                   value;
        Node(T&& v) : value(std::move(v)) {}
    };

public:

    MPSCQueue() 
    {
        // create a dummy node; head & tail point at it
        auto dummy = new Node(T{});
        head.store(dummy, std::memory_order_relaxed);
        tail = dummy;
    }

    ~MPSCQueue() 
    {
        clear();           // delete any remaining real nodes
        delete tail;       // delete the dummy
    }

    // Producers call this
    void enqueue(T&& item) {
        auto* node = new Node(std::move(item));
        node->next.store(nullptr, std::memory_order_relaxed);

        // swap in new head, then link old head → new node
        Node* prev = head.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // Consumer calls this periodically to grab everything
    // Returns nullptr if queue was empty.
    // Otherwise returns the first real node in a singly‐linked list.
    Node* dequeueAll() 
    {
        Node* first = tail;
        Node* new_head = head.exchange(first, std::memory_order_acq_rel);

        // nodes from first->next up through new_head
        Node* start = first->next.load(std::memory_order_acquire);
        tail = new_head;
        return start;
    }

    // Helper to delete all outstanding nodes (including ones you just dequeued)
    void clear() 
    {
        Node* n = dequeue_all();
        while (n) 
        {
            Node* next = n->next.load(std::memory_order_relaxed);
            delete n;
            n = next;
        }
    }
};
