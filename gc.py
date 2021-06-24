from time import perf_counter
start = perf_counter()

for i in range(0, 10**7):
    print("1")

print(perf_counter() - start)
