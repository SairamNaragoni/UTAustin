package bst

import (
	"bufio"
	"os"
	"strconv"
	"strings"
)

type TreeNode struct {
	Value int
	Left  *TreeNode
	Right *TreeNode
}

// Hash the BST by generating a hash from in-order traversal
func (n *TreeNode) Hash() uint32 {
	var values []int
	n.InOrder(&values)

	// initial hash value
	var hash_value uint32 = 1
	for _, v := range values {
		new_value := uint32(v + 2)
		hash_value = (hash_value*new_value + new_value) % 1000
	}

	return hash_value
}

// Insert inserts a value into the BST.
func (n *TreeNode) Insert(value int) {
	if n == nil {
		return
	} else if value <= n.Value {
		if n.Left == nil {
			n.Left = &TreeNode{Value: value}
		} else {
			n.Left.Insert(value)
		}
	} else {
		if n.Right == nil {
			n.Right = &TreeNode{Value: value}
		} else {
			n.Right.Insert(value)
		}
	}
}

// InOrder traversal of BST.
func (n *TreeNode) InOrder(values *[]int) {
	if n == nil {
		return
	}
	n.Left.InOrder(values)
	*values = append(*values, n.Value)
	n.Right.InOrder(values)
}

// Read input file and build trees
func BuildTrees(filePath string) ([]*TreeNode, error) {
	file, err := os.Open(filePath)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	var trees []*TreeNode

	for scanner.Scan() {
		line := scanner.Text()
		values := strings.Fields(line)
		var tree *TreeNode
		for _, v := range values {
			num, _ := strconv.Atoi(v)
			if tree == nil {
				tree = &TreeNode{Value: num}
			} else {
				tree.Insert(num)
			}
		}
		trees = append(trees, tree)
	}

	if err := scanner.Err(); err != nil {
		return nil, err
	}

	return trees, nil
}
