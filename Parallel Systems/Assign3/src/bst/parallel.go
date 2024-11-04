package bst

import (
	"fmt"
	"sync"
	"time"
)

func ComputeHashesPerTree(trees []*TreeNode, hashChan chan<- [2]uint32, hashes FixedSizeMap, done chan<- struct{}, hashMutexes []sync.Mutex) {
	var hashDone sync.WaitGroup
	start := time.Now()

	for treeID, tree := range trees {
		hashDone.Add(1)

		go func(treeID int, tree *TreeNode) {
			defer hashDone.Done()

			hash := tree.Hash()

			if hashChan != nil {
				hashChan <- [2]uint32{hash, uint32(treeID)}
			}

			if hashes != nil {
				idx := hash % uint32(len(hashMutexes))
				hashMutexes[idx].Lock()
				hashes.Insert(hash, treeID)
				hashMutexes[idx].Unlock()
			}
		}(treeID, tree)
	}

	hashDone.Wait()

	hashTime := time.Since(start).Seconds()
	fmt.Printf("hashTime: %f\n", hashTime)

	if hashChan != nil {
		close(hashChan)
	}

	done <- struct{}{}
}

func ComputeHashesParallel(trees []*TreeNode, hashWorkers int, hashChan chan<- [2]uint32, hashes FixedSizeMap, done chan<- struct{}, hashMutexes []sync.Mutex) {
	workerChan := make(chan [2]int, len(trees))
	var hashDone sync.WaitGroup
	hashDone.Add(hashWorkers)

	start := time.Now()

	// Start the hash workers
	for i := 0; i < hashWorkers; i++ {
		go func(i int) {
			defer hashDone.Done()
			for pair := range workerChan {
				treeID := pair[0]
				tree := trees[treeID]
				hash := tree.Hash()

				//impl where hashWorkers = i, dataWorkers = 1 or j, i!=j
				if hashChan != nil {
					hashChan <- [2]uint32{hash, uint32(treeID)}
				}

				if hashes != nil {
					//fine grain control when reading from a channel
					//Lock the selected mutex for this hash entry
					idx := hash % uint32(len(hashMutexes))
					hashMutexes[idx].Lock()
					hashes.Insert(hash, treeID)
					hashMutexes[idx].Unlock()
				}
			}
		}(i)
	}

	for i := range trees {
		workerChan <- [2]int{i, i}
	}
	close(workerChan)

	hashDone.Wait()

	// Hashing is complete here
	hashTime := time.Since(start).Seconds()
	fmt.Printf("hashTime: %f\n", hashTime)

	if hashChan != nil {
		close(hashChan)
	}

	done <- struct{}{}
}

func ProcessHashesParallel(dataWorkers int, hashChan <-chan [2]uint32, hashes FixedSizeMap, done chan<- struct{}, hashMutexes []sync.Mutex) {
	var dataDone sync.WaitGroup
	dataDone.Add(dataWorkers)

	for i := 0; i < dataWorkers; i++ {
		go func() {
			defer dataDone.Done()
			for pair := range hashChan {
				hash, treeID := pair[0], int(pair[1])

				if dataWorkers < 2 {
					// Single thread, hence no lock is needed
					hashes.Insert(hash, treeID)
				} else {
					//fine grain control when reading from a channel
					mutexIndex := hash % uint32(len(hashMutexes))
					hashMutexes[mutexIndex].Lock()
					hashes.Insert(hash, treeID)
					hashMutexes[mutexIndex].Unlock()
				}
			}
		}()
	}

	dataDone.Wait()
	done <- struct{}{}
}

func HashComputeParallel(trees []*TreeNode, hashWorkers int, hashChan chan<- [2]uint32, hashes FixedSizeMap, done chan<- struct{}, hashMutexes []sync.Mutex, useHashWorkers bool) {
	if useHashWorkers {
		go ComputeHashesParallel(trees, hashWorkers, hashChan, hashes, done, hashMutexes)
	} else {
		go ComputeHashesPerTree(trees, hashChan, hashes, done, hashMutexes)
	}
}

func HashAllTreesParallel(trees []*TreeNode, hashWorkers int, dataWorkers int, use_channel bool, useHashWorkers bool) FixedSizeMap {
	hashes := make(FixedSizeMap, fixed_size)
	hashDone := make(chan struct{})

	if dataWorkers == 1 {
		use_channel = true
	}

	if dataWorkers == 0 {
		HashComputeParallel(trees, hashWorkers, nil, nil, hashDone, nil, useHashWorkers)
		<-hashDone
		return hashes
	}

	hashMutexes := make([]sync.Mutex, dataWorkers)

	if hashWorkers == dataWorkers {
		//impl where hashWorkers = i = dataWorkers
		HashComputeParallel(trees, hashWorkers, nil, hashes, hashDone, hashMutexes, useHashWorkers)
		<-hashDone
	} else {
		//impl where hashWorkers = i, dataWorkers = 1 or j, j != i
		if use_channel {
			//fine grain with channel for optional impl
			processDone := make(chan struct{})
			hashChan := make(chan [2]uint32, len(trees)) // Buffered channel
			HashComputeParallel(trees, hashWorkers, hashChan, nil, hashDone, nil, useHashWorkers)
			go ProcessHashesParallel(dataWorkers, hashChan, hashes, processDone, hashMutexes)

			<-hashDone
			<-processDone
		} else {
			//fine grain without channel for optional impl
			HashComputeParallel(trees, hashWorkers, nil, hashes, hashDone, hashMutexes, useHashWorkers)
			<-hashDone
		}
	}

	return hashes
}

func CompareTreesParallel(t1, t2 *TreeNode) bool {
	var values1, values2 []int
	var wg sync.WaitGroup

	wg.Add(2)

	go func() {
		defer wg.Done()
		t1.InOrder(&values1)
	}()

	go func() {
		defer wg.Done()
		t2.InOrder(&values2)
	}()

	wg.Wait()

	if len(values1) != len(values2) {
		return false
	}

	for i := range values1 {
		if values1[i] != values2[i] {
			return false
		}
	}

	return true
}

func CompareHashGroupsParallel(hashes FixedSizeMap, trees []*TreeNode) [][]int {
	var mu sync.Mutex
	var wg sync.WaitGroup

	adj := make([][]int, len(trees))
	for i := range adj {
		adj[i] = make([]int, len(trees))
	}

	for _, ids := range hashes {
		if len(ids) > 1 {
			for i := 0; i < len(ids); i++ {
				for j := i + 1; j < len(ids); j++ {
					wg.Add(1)
					go func(i, j int) {
						defer wg.Done()
						if CompareTreesParallel(trees[ids[i]], trees[ids[j]]) {
							mu.Lock()
							adj[ids[i]][ids[j]] = 1
							adj[ids[j]][ids[i]] = 1
							mu.Unlock()
						}
					}(i, j)
				}
			}
		}
	}

	wg.Wait()
	return adj
}

func GetGroups(trees []*TreeNode, adj [][]int) [][]int {
	var groups [][]int
	visited := make([]bool, len(trees))
	for i := 0; i < len(trees); i++ {
		if !visited[i] {
			group := []int{i}
			for j := i + 1; j < len(trees); j++ {
				if adj[i][j] == 1 && !visited[j] {
					group = append(group, j)
					visited[j] = true
				}
			}
			if len(group) > 1 {
				groups = append(groups, group)
			}
			visited[i] = true
		}
	}
	return groups
}

func CompareHashGroupsParallelWithBuffer(hashes FixedSizeMap, trees []*TreeNode, compWorkers int) [][]int {
	var mu sync.Mutex
	var wg sync.WaitGroup

	// Initialize the adjacency matrix
	adj := make([][]int, len(trees))
	for i := range adj {
		adj[i] = make([]int, len(trees))
	}

	buffer := NewBoundedBuffer(compWorkers)
	wg.Add(compWorkers)

	// Worker threads for tree comparisons
	for i := 0; i < compWorkers; i++ {
		go func() {
			defer wg.Done()
			for {
				pair := buffer.Remove()

				if pair[0] == -1 && pair[1] == -1 {
					break
				}
				if CompareTreesParallel(trees[pair[0]], trees[pair[1]]) {
					mu.Lock()
					adj[pair[0]][pair[1]] = 1
					adj[pair[1]][pair[0]] = 1
					mu.Unlock()
				}
			}
		}()
	}

	// Fill the buffer with work (tree comparisons)
	for _, ids := range hashes {
		if len(ids) > 1 {
			for i := 0; i < len(ids); i++ {
				for j := i + 1; j < len(ids); j++ {
					buffer.Insert([2]int{ids[i], ids[j]})
				}
			}
		}
	}

	// Signal completion
	for i := 0; i < compWorkers; i++ {
		buffer.Insert([2]int{-1, -1})
	}

	// Wait for all workers to finish
	wg.Wait()

	return adj
}
