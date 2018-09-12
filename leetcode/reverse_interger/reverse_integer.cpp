#include <iostream>

using namespace std;
int reverse_num(int x)
{
	int val=0;
	while(x)
	{
		val=val*10+x%10;
		x=x/10;
	}
	return val;
}



int main()
{
	int val=reverse_num(-3245454);
	cout<<val<<endl;
}
