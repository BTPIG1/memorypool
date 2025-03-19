#include<iostream>
#include<array>
using namespace std;

class Base { virtual void foo() {} };

class Derived :public Base {

};



int main() {
	char* c = new char('a');
	cout << reinterpret_cast<void*>(c) << endl; // 输出的地址是以字节为基础单位
	c++;
	cout << reinterpret_cast<void*>(c) << endl;
	long long a[] = { 13,14 };
	long long* p = a;
	cout << p << endl;
	cout << ++p << endl;

	std::array<int,2> arr2;


	Base* d = new Derived();
	Base* b = new Base();

	Derived* st = static_cast<Derived*>(d);
	cout << st << endl;
	st = static_cast<Derived*>(d);
	cout << st << endl;

	Derived* dc = dynamic_cast<Derived*>(d);  // 如果Base没有虚函数就不是多态这里不能用dynamic_cast
	cout << dc << endl;
	dc = dynamic_cast<Derived*>(b);
	cout << dc << endl; // 只有你不同

	int* db = new int(0.1);
	//double* dbint = dynamic_cast<double*>(db);double报错不是完整类类型
	void* potr = dynamic_cast<void*>(b);
	potr = dynamic_cast<void*>(d);
	//dc = dynamic_cast<Derived*>(potr); 报错potr不是完整类类型

	// *reinterpret_cast<void**>
	char* ptr = reinterpret_cast<char*>(p);
	cout << ptr << endl;
	cout << *reinterpret_cast<void**>(ptr) << endl;
	cout << reinterpret_cast<void*>(ptr) << endl;
	cout << reinterpret_cast<void*>(reinterpret_cast<char*>(ptr)) << endl;
	cout << reinterpret_cast<void*>(reinterpret_cast<char*>(ptr+1))<< endl;
	p++;
	cout << *reinterpret_cast<void**>(p) << endl;
	cout << reinterpret_cast<void**>(p+6) << endl;
}