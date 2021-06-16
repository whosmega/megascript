var array = [0]
var start = clock()

for i in 0, 100000000:
    array[0] = i 
end 

print(clock() - start)
