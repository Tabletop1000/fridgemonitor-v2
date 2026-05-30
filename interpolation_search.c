/*
 * interpolation_search.c
 *
 *  Created on: 29 Mar 2025
 *      Author: jdutra
 */


// C program to implement interpolation search
// with recursion
#include <stdio.h>

// If x is present in arr[0..n-1], then returns
// index of it, else returns -1.


void reverseArray(float arr[], int n) {

    // Two pointers
    int l = 0, r = n - 1;
    while (l < r) {

        // Swap the elements
        int temp = arr[l];
        arr[l] = arr[r];
        arr[r] = temp;

        // Move pointers towards middle
        l++;
        r--;
    }
}

int interpolationSearch(float const arr[], int lo, int hi, float x)
{
    int pos;
    // Since array is sorted, an element present
    // in array must be in range defined by corner
    float arr_lo = arr[lo];
    float arr_hi = arr[hi];
    if ((lo <= hi) && (x >= arr_lo) && (x <= arr_hi)) {
        // Probing the position with keeping
        // uniform distribution in mind.
        pos = lo
              + (((double)(hi - lo) / (arr[hi] - arr[lo]))
                 * (x - arr[lo]));

        // Condition of target found
        if (arr[pos] == x)
            return pos;

        // If x is larger, x is in right sub array
        if (arr[pos] < x)
            return interpolationSearch(arr, pos + 1, hi, x);

        // If x is smaller, x is in left sub array
        if (arr[pos] > x)
            return interpolationSearch(arr, lo, pos - 1, x);
    }
    return -1;
}
