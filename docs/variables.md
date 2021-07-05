<h1>Variables</h1>

In megascript, all top level code is wrapped into an implicit `main` function and then executed, 
which means local variables are allowed in the top level code.
A local variable can be declared, which does nothing but tells the compiler it exists, and so it gets initialised to `nil`
```
var myVariable
```
It can also be given a starting value while declaring 
```
var count = 200
```
The starting value can be any of the datatypes mentioned earlier.
To access what the variable contains, you can directly use the name that was given to the variable 
```
var number1 = 45
var number2 = 32
var sum = number1 + number2     // contains 77
```
All variable declaration starting with `var` are considered 'local' variables.
The concept of locals will come into play later on, but when they are declared in a smaller body (or scope), 
they are only local to the inner body, and anything outside can't access it.

<h3>Global Variables</h3>

Global variables are accessible to everyone, whether it is an inner body or an outer body, they can be declared like so:
```
global count
```
Or have themselves set to an initial value 
```
global count = 200
```
They behave like normal local variables in many ways, they can be accessed using the name.
```
var number1 = 33
global number2 = 50
var difference = number2 - number1
```
<h4>Modifying the values the variables contain</h4>

You might want to change the values of the variables later on, after they have been declared.
You can do that using the `=` assignment operator. 
```
var number1 = 200
number1 = 500
```
When changing the values, you dont need to add a `var` or `global` because the variable isn't being declared, it is being updated to a new value<br>
This syntax works both for local and global variables. 
<h3>Name conflicts</h3>

In case a name conflict occurs, local variables are always given the highest priority.
```
var test = 10
global test = 20

var testValue = test    // stores 10
```
<h4>Shortcut Assignment</h4>

The shortcut assignment operators, `+=`, `-=`, `*=`, `/=` and `^=` come into play now.
```
var test = 65
test = test + 10
```
and
```
var test = 65
test += 10
```
Are the exact same thing 

[previous](/docs/operators.md) | [next](/docs/functions.md) | [index](/docs/documentation.md)
