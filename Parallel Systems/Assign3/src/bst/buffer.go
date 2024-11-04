package bst

import (
	"sync"
)

type BoundedBuffer struct {
	buffer   [][2]int
	capacity int
	mutex    sync.Mutex
	notEmpty *sync.Cond
	notFull  *sync.Cond
}

func NewBoundedBuffer(capacity int) *BoundedBuffer {
	b := &BoundedBuffer{
		buffer:   make([][2]int, 0, capacity),
		capacity: capacity,
	}
	b.notEmpty = sync.NewCond(&b.mutex)
	b.notFull = sync.NewCond(&b.mutex)
	return b
}

func (b *BoundedBuffer) Insert(pair [2]int) {
	b.mutex.Lock()
	defer b.mutex.Unlock()

	for len(b.buffer) == b.capacity {
		b.notFull.Wait()
	}

	b.buffer = append(b.buffer, pair)

	b.notEmpty.Signal()
}

func (b *BoundedBuffer) Remove() [2]int {
	b.mutex.Lock()
	defer b.mutex.Unlock()

	for len(b.buffer) == 0 {
		b.notEmpty.Wait()
	}

	pair := b.buffer[0]
	b.buffer = b.buffer[1:]

	b.notFull.Signal()

	return pair
}
