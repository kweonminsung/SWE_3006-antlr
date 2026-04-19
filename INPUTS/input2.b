// Binary Search in C

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "array.h"

auto initialize (auto n) {
  auto i = 0;
  
  while(i<n) {
    setArr(i, i+3);
    i = i + 1;
  }  
  return n;
}

auto binarySearch(auto x, auto low, auto high) {
  if (high >= low) {
    auto mid;
    mid  = low + (high - low) / 2;

    // If found at mid, then return it
    if (getArr(mid) == x)
      return mid;

    // Search the left half
    if (getArr(mid) > x)
      return binarySearch(x, low, mid - 1);

    // Search the right half
    return binarySearch(x, mid + 1, high);
  }

  return -1;
}

auto main() {
  auto n, x = 4, result;

  n = 10; 
  initialize(n);
  result = binarySearch(x, 0, n - 1);

  if (result == -1)
    printf("Not found!!\n");
  else
    printf("Element is found at index %d\n", result);
  
  return result;
}
