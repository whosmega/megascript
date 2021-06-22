clock = os.clock

function fib(n)
    if n < 2 then return n end 
    return fib(n - 2) + fib(n - 1) 
end 

local start = clock()
fib(35) 
print(clock() - start) 



