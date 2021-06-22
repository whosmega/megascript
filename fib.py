from time import perf_counter

def fib(n):
    if n < 2:
        return n 

    return fib(n - 2) + fib(n - 1) 

start = perf_counter()
fib(35) 
print(perf_counter() - start) 


