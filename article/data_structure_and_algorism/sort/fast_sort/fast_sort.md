
快速排序：

快速排序思路：将第一个数作为 key，从右往左找小于等于 key 的，从左往右找大于 key 的，互换这两个数，
				必须是从右往左先找，而且必须是从左往右找大于key的，因为第一个数就是等于key。  
			  
注意：从右往左找，直到找到 j=i 时，表示右边没有数据小于key。再从左往右找，现需要判断是否 j=i
		找到最后，必定 i=j，且data[i] <= key,然后再将 data[0](也就是key) 与 data[i]互换。
		这样，就达到了目的：key左边小于等于key，key 右边大于key
		然后以key为分界线，递归调用。


 66 void fast_sort(int *data,int data_len)
 67 {   
 68     int i=0,j=0;
 69     int key = data[0]; 
 70     if(NULL == data || data_len <= 1)
 71         return ;
 72     for(i = 0;i<data_len;i++)
 73     {
 74         printf("%d.",data[i]);
 75     }
 76     printf("\n"); 
 77     for(i = 0,j = data_len-1;i<j;)
 78     {
 79         while(data[j] > key )
 80         {
 81             j--; 
 82             if(i >= j)
 83                 break;
 84         }
 85         while(data[i] <= key)
 86         {
 87             if(i >= j)
 88                 break;
 89             i++;
 90         }
 91         if(i<j)
 92         {
 93             int temp = data[i];
 94             data[i] = data[j];
 95             data[j] = temp;
 96         }
 97         else
 98         {
 99             int temp = data[0];
100             data[0] = data[i];
101             data[i] = temp;
102             printf("i = %d.\n",i);
103         }
104     }
105     fast_sort(&data[0],i);
106     fast_sort(&data[i+1],data_len-1-i);
107 }



