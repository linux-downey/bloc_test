def twoSum(nums, target):
        data=[]
        print len(nums)
        for i in range(len(nums)):
            print len(data)
            for j in range(len(data)):
                if(nums[i] != data[j]):
                    print 12
                else:
                    return [j,i]
            data.append(target-nums[i])
        return None
def main():
    nums = [22222,22222]
    target =44444
    result=twoSum(nums,target)
    print result


if __name__ == '__main__':
    main()
