def twoSum(nums, target):
        data=[]
        for i in range(len(nums)):
            for j in range(len(data)):
                if(nums[i] is not data[j]):
                    pass
                else:
                    return [j,i]
            data.append(target-nums[i])
        return None
def main():
    nums = [2,7,11,15]
    target =17
    result=twoSum(nums,target)
    print result


if __name__ == '__main__':
    main()