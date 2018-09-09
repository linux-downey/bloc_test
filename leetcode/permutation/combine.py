def combine(data, step, select_data, target_num):
    if len(select_data) == target_num:   
        print(select_data)
        return
    if step >= len(data):               
        return
    select_data.append(data[step])            
    combine(data, step + 1, select_data, target_num)
    select_data.pop()                         
    combine(data, step + 1, select_data, target_num) 
if __name__ == '__main__':
    data = [1, 2, 3, 4, 5, 6,7]
    combine(data, 0, [], 3)
