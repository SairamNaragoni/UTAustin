#include <iostream>

// Function to calculate prefix sum
void prefixSum(int arr[], int n, int prefixSum[]) {
    prefixSum[0] = arr[0];
    for (int i = 1; i < n; i++) {
        prefixSum[i] = prefixSum[i - 1] + arr[i];
    }
}

// Function to print an array
void printArray(int arr[], int n) {
    for (int i = 0; i < n; i++) {
        std::cout << arr[i] << " ";
    }
    std::cout << std::endl;
}

int main() {
    int arr[] = {1, 2, 3, 4, 5};
    int n = sizeof(arr) / sizeof(arr[0]);
    int prefixSumArr[n];

    // Calculate prefix sum
    prefixSum(arr, n, prefixSumArr);

    // Print original array
    std::cout << "Original Array: ";
    printArray(arr, n);

    // Print prefix sum array
    std::cout << "Prefix Sum Array: ";
    printArray(prefixSumArr, n);

    return 0;
}