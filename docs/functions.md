<h1>Functions</h1>

Functions are blocks of code that can be ran when necessary. 
Megascript gives you a choice to define local functions and global functions.
The syntax for local functions is:
```
func foo():
  // code
end
```
And global functions:
```
global func foo():
  // code
end
```
A function can be called with the normal calling convention, which is the name followed by brackets. It runs all the code written in the function.
```
foo()
```
Functions are mainly where the scene of local variables and global variables comes into play<br>
Local variables defined inside a function's body are deleted once the function returns or finishes running, and the outer body cannot access those variables.
However, a function can declare / update a global variable while it is running, and the change takes place for every other body
```
func foo():
  var test = 100
end 

foo()
var value = test      // value = nil
```
```
func foo():
  global test = 100
end
var value = test      // value = 100
```
Functions can have parameters, which get set to the arguments which are passed when calling
Functions can also return values back using the `return` statement.<br>
By default, if nothing is being returned, `nil` is returned.
```
func add(n1, n2):
  return n1 + n2
end

var sum = add(10, 20)         // sum gets set to 30
```
If extra parameters are specified, they get set to `nil`, and if extra arguments are supplied, they simply get discarded<br>
Functions can also be set to variables, or get passed as arguments to other functions. They have the datatype `function`
```
func call(f):
  return f()
end 

func myFunction():
end 

call(myFunction)        // calls myfunction by passing it as an argument
```
<h4>Variadic Functions</h4>

Sometimes, the number of parameters a function takes might not be fixed, in that case, the function can be declared<br>
to have a variadic number of inputs being passed when called.
```
func variadic(args...):

end
```
The arguments then are added to an array and that array is set to the name given, which is `args` in this case. A set of fixed arguments can also be declared <br>
which get proceeded by a variadic parameter. There cannot be more than 1 variadic parameter, and it always has to be the last parameter declared.<br>
More about arrays later.

<h4>Closures</h4>

Megascript also fully supports the closure mechanism, local variables are not deleted, if they are getting used outside of the function body.<br>
They however get deleted only when its safe to do so.
```
func test():
  var variable = "abc"
  func test2():
    return variable
  end
  
  return test
end

var result = test()()       // this is still set to "abc"
```

[previous](/docs/variables.md) | [next](/docs/arrays.md) | [index](/docs/documentation.md)
