package main

import (
	"ASSIGN3/src/bst"
	"flag"
	"fmt"
	"time"
)

func PrintHashGroups(hashes bst.FixedSizeMap) {
	for hash, ids := range hashes {
		if len(ids) > 1 {
			fmt.Printf("%d:", hash)
			for _, id := range ids {
				fmt.Printf(" %d", id)
			}
			fmt.Println()
		}
	}
}

func PrintCompareGroups(groups [][]int) {
	for i, group := range groups {
		fmt.Printf("group %d:", i)
		for _, id := range group {
			fmt.Printf(" %d", id)
		}
		fmt.Println()
	}
}

func main() {
	// Parse command-line flags
	hashWorkers := flag.Int("hash-workers", 1, "number of hash workers")
	dataWorkers := flag.Int("data-workers", 0, "number of data workers")
	compWorkers := flag.Int("comp-workers", 0, "number of comparison workers")
	useChannels := flag.Bool("use-channels", false, "use channels for fine grain control for optional implementation, default false")
	useBuffer := flag.Bool("use-buffer", true, "use buffer for tree comparison, default true")
	useWorkers := flag.Bool("use-workers", true, "use workers for hashing, default true")
	inputFile := flag.String("input", "", "path to input file")
	debug := true

	flag.Parse()

	// Check if input file is provided
	if *inputFile == "" {
		fmt.Println("Please provide an input file using -input flag")
		return
	}

	// Step 1: Build Trees from input file
	trees, err := bst.BuildTrees(*inputFile)
	if err != nil {
		fmt.Printf("Error reading file: %v\n", err)
		return
	}

	if *compWorkers == 1 {
		*useBuffer = false
	}

	//sequential
	if *hashWorkers == 1 {
		// Step 2: calculate Hashes
		startTime := time.Now()

		hashes := bst.HashAllTreesSequential(trees)

		hashGroupTime := time.Since(startTime).Seconds()

		if *dataWorkers == 0 {
			return
		}

		fmt.Printf("hashGroupTime: %f\n", hashGroupTime)

		if debug {
			PrintHashGroups(hashes)
		}

		if *compWorkers == 0 {
			return
		}

		// Step 3: Compare trees with identical hashes
		compareStartTime := time.Now()

		groups := bst.CompareHashGroupsSequential(hashes, trees)

		compareTreeTime := time.Since(compareStartTime).Seconds()

		fmt.Printf("compareTreeTime: %f\n", compareTreeTime)
		if debug {
			PrintCompareGroups(groups)
		}
	}

	if *hashWorkers > 1 {
		// Parallel hash computation
		startTime := time.Now()

		hashes := bst.HashAllTreesParallel(trees, *hashWorkers, *dataWorkers, *useChannels, *useWorkers)

		hashGroupTime := time.Since(startTime).Seconds()

		if *dataWorkers == 0 {
			return
		}
		fmt.Printf("hashGroupTime: %f\n", hashGroupTime)
		if debug {
			PrintHashGroups(hashes)
		}

		if *compWorkers == 0 {
			return
		}

		// Step 3: Compare trees with identical hashes
		compareStartTime := time.Now()
		var adj_matrix [][]int

		if *useBuffer {
			adj_matrix = bst.CompareHashGroupsParallelWithBuffer(hashes, trees, *compWorkers)
		} else {
			adj_matrix = bst.CompareHashGroupsParallel(hashes, trees)
		}

		compareTreeTime := time.Since(compareStartTime).Seconds()

		fmt.Printf("compareTreeTime: %f\n", compareTreeTime)
		groups := bst.GetGroups(trees, adj_matrix)
		if debug {
			PrintCompareGroups(groups)
		}
	}

}
