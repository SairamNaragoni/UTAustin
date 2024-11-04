package bst

import (
	"fmt"
	"time"
)

func HashAllTreesSequential(trees []*TreeNode) FixedSizeMap {
	hashes := make(FixedSizeMap, fixed_size)
	start := time.Now()

	for i, tree := range trees {
		hash := tree.Hash()
		hashes.Insert(hash, i)
	}

	hashTime := time.Since(start).Seconds()
	fmt.Printf("hashTime: %f\n", hashTime)

	return hashes
}

func CompareTreesSequential(t1, t2 *TreeNode) bool {
	var values1, values2 []int
	t1.InOrder(&values1)
	t2.InOrder(&values2)
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

func CompareHashGroupsSequential(hashes FixedSizeMap, trees []*TreeNode) [][]int {
	var groups [][]int
	visited := make(map[int]bool)

	for _, ids := range hashes {
		if len(ids) > 1 {
			for i := 0; i < len(ids); i++ {
				group := []int{ids[i]}
				for j := i + 1; j < len(ids); j++ {
					if CompareTreesSequential(trees[ids[i]], trees[ids[j]]) {
						group = append(group, ids[j])
						visited[ids[j]] = true
					}
				}
				if len(group) > 1 && !visited[ids[i]] {
					groups = append(groups, group)
					visited[ids[i]] = true
				}
			}
		}
	}

	return groups
}
