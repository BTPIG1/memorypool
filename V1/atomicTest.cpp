#include<iostream>
#include<atomic>
using namespace std;

int main() {

	atomic<int> a;
	a = 2;
	int b = a.load(memory_order_relaxed);
	a.store(5, memory_order_release);
	a = -1;
	cout << a<<b;
}