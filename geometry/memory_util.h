#include <emscripten/heap.h>
#include <malloc.h>
#include <stdio.h>

#include <cstdint>

/*
  size_t hblkhd;    // Space allocated in mmapped regions (bytes)
  size_t usmblks;   // See below
  size_t fsmblks;   // Space in freed fastbin blocks (bytes)
  size_t uordblks;  // Total allocated space (bytes)
  size_t fordblks;  // Total free space (bytes)
  size_t keepcost;  // Top-most, releasable space (bytes)
*/

extern "C" {
uint32_t memory_used() {
  struct mallinfo info = mallinfo();
  return info.uordblks;
}

uint32_t memory_free() {
  struct mallinfo info = mallinfo();
  return info.fordblks;
}

uint32_t memory_exhaust() {
  struct chunk {
    struct chunk *next;
  };
  struct chunk *head = NULL;
  size_t total = 0;
  size_t increment = 1024 * 1024 * 100;
  for (;;) {
    total += increment;
    struct chunk *c = (struct chunk *)malloc(increment);
    if (!c) {
      std::cout << "QQ/memory_exhaust: exhausted" << std::endl;
      while (head) {
        c = head;
        head = head->next;
        free(c);
      }
      return total;
    } else {
      c->next = head;
      head = c;
      std::cout << "QQ/memory_exhaust: total=" << total
                << " used=" << memory_used() << " free=" << memory_free()
                << std::endl;
    }
  }
}

uint32_t memory_heap_size() { return emscripten_get_heap_size(); }

uint32_t memory_heap_limit() { return emscripten_get_heap_max(); }

uint32_t memory_heap_free() { return memory_heap_limit() - memory_heap_size(); }
}
