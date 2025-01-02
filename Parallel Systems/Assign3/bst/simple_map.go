package bst

type FixedSizeMap [][]int

const fixed_size int = 1000

func (fm *FixedSizeMap) Insert(hash uint32, index int) {
	(*fm)[hash] = append((*fm)[hash], index)
}

func (fm *FixedSizeMap) Get(hash uint32) []int {
	return (*fm)[hash]
}
