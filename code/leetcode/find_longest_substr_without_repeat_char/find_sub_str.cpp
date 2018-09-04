#include <iostream>
#include <map>
#include <vector>

using namespace std;


//vector<map<char,int>> char_map;
vector<string> sub_str;
string cur_str;
string test_str="qywueirofkdjfuryshncxklsiduhgjfeiywbdfkfj";


bool is_repeat(char c)
{
    int index=cur_str.find(c);
    //str does not contain c;
    if(string::npos==index)
    {
        cur_str+=c;
        return false;
    }
    else
    {
        sub_str.push_back(cur_str);
        test_str=test_str.substr(index+1);
        cur_str.clear();
        return true;
    }   
}



bool find_substr()
{
    for(int i=0;i<test_str.size();i++)
    {
        if(is_repeat(test_str[i]))
            return false;
    }
    sub_str.push_back(cur_str);
    return true;
}


string& max_substr(vector<string> &str_array)
{
    static string longest=str_array[0];
    for(int i=1;i<str_array.size();i++)
    {
        if(longest.size() < str_array[i].size())
        {
            longest=str_array[i];
        }
    }
    return longest;
}

int main()
{
    cout<<"test str=";
    while(false==find_substr())
    {
        cout<<test_str<<"->";
    }
    cout<<"end"<<endl;
    for(int i=0;i<sub_str.size();i++)
    {
        cout<<sub_str[i]<<"->";
    }
    cout<<"endl"<<endl;

    cout<<"so the longest subtring without repeating char of given string is "<<max_substr(sub_str)<<endl;
    return 1;
}