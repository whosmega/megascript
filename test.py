from time import perf_counter
array = [0]
start = perf_counter()

for i in range(0, 100000000):
    array[0] = i 

print(perf_counter() - start)
