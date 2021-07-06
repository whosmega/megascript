<h1>Looping and Breaking</h1>

Megascript allows `while` loops and `for` loops. Which continuosly run code given to them in a block until a certain condition is met.

<h3>While Loops</h3>

While loops run till the condition given to them is false.
```
var index = 0
while index <= 5:
  index += 1
end 
```
It is possible to break out of the loop without the condition having to be true, using the `break` keyword 
```
var index = 0
while true:
  index += 1
  if index > 10: break end 
end
```

<h3>For loops</h3>

Megascript allows 2 types of `for` loops, with the temporary variables being directly built into syntax.
<h4>Numeric for loops</h4>

A for loop can be made to run from one numeric range to other, with an optional increment, which default to 1.
In the example below, the temporary variable is called `i`, but it can be named anything as long as it is a suitable identifier.<br>
The name never conflicts with any outer variable because it is implicitly declared in a lower scope.
```
for i in 0, 5:        // start, end 
  print(i)
end 

for i in 5, 0, -1:    // start, end, increment 
  print(i)
end 
```
Numeric for loops can be broken out of using the `break` keyword aswell.
```
for i in 0, 10:
  if i == 5: break end 
end 
```

<h3>Array iterating for loops</h3>

Arrays natively support iteration, the current index and value can be directly accessed using 2 user-named temporary variables<br>
In the following example, the temporary variables are `i` and `v` and stand for index and value respectively. 
```
for i, v in ["Apple", "Orange", "Mango"]:
  print(i, v)
end 
```
For convinience, the value variable can be skipped when required.
```
for i in [10, 20, 30]:
end 
```

[previous](/docs/controlflow.md) | [next](/docs/importing.md) | [index](/docs/documentation.md)
