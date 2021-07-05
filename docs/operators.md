<h1>Operators</h1>

Operators get divided into the following:

<h3>Arithmetic Operators</h3>

1. `+` (addition)
2. `-` (subtraction)
3. `*` (multiplication)
4. `/` (division)
5. `^` (exponent) 

<h3>Unary Operators</h3>

1. `-` (negation of numbers)
2. `!` (the 'not' operator which inverts values)
3. `#` (the length operator)

<h3>Logical Operators</h3>

1. `and`
2. `or`

<h3>Assignment Operators</h3>

1. `=` (Assigning values to variables / arrays / fields)
2. `+=` (`a += b` is shorthand for `a = a + b`)
3. `-=` (`a -= b` is shorthand for `a = a - b`)
4. `*=` (`a *= b` is shorthand for `a = a * b`)
5. `/=` (`a /= b` is shorthand for `a = a / b`)
6. `^=` (`a ^= b` is shorthand for `a = a^b`)
<hr>

<h2>Arithmetic Operators</h2>

1. `+`

When used on numbers, it yields the sum   `5 + 4` -> `9`
When used on strings, it concatenates the strings   `"foo" + "bar"` -> `"foobar"`

<b> The other arithmetic operators are number only </b>

2. `-` : `54 - 3` -> `51`
3. `*` : `4 * 3` -> `12`
4. `/` : `10 / 5` -> `2`
5. `^` : `2^3` -> `8`

<h2>Unary Operators</h2>

1. `-`

This operator can only be used on numbers, and it simply negates a number `-(5 - 4)` -> `-1` 

2. `!`

The unary 'not' operator can be used on any datatype, it negates it based on it's falsiness.
When used with truthy values, it always yields `false`    
```
!"abc"  -> false
!4      -> false   
!true   -> false
```
And otherwise, when being used with falsey values, it always yields `true`    
```
!nil    -> true   
!false  -> true
```

3. `#`

The length operator when used on `strings` and `arrays` returns the length or the number of characters / elements 
`#"abc"` -> `3`,  `#arrayVariable` -> element count 
    
<h2>Logical Operators</h2>

1. `and` 

The logical `and` operator yields values based on the truthyness of the operands on the right and left side 
Both the value on the left, and the value on the right need to be truthy, for `and` to yield a truthy value,
Otherwise, it yields a falsy value.

In case a truthy value needs to be yielded, it yields the <i>last truthy value it finds</i>
In case a falsy value needs to be yielded, it yields the <i>first falsy value it finds</i>

`5 and 3`       -> `3`      (truthy result)<br>
`nil and 2`     -> `nil`    (falsy result)<br>
`60 and false`  -> `false`  (falsy result)<br>
`nil and false` -> `nil`    (falsy result)<br>

The `and` operator sometimes may not evaluate the operand on the right, which means it 'short-circuits'
This usually does not happen, but when function calls are involved.
When it knows the operand on the left is already falsy, it skips evaluating the right operand completely,
which does not invoke the function.

2. `or`

The logical `or` operator also yields values based on the truthyness of the operands on the right and left sides.
It can yield a truthy value when either of the values given to it are truthy, the only case where it yields a falsy value 
is when both the values given to it are falsy. 

In case a truthy value needs to be yielded, it yields the <i>first truthy value it finds</i>
In case a falsy value needs to be yielded, it yields the <i>last falsy value it finds</i>

`5 or 4`        -> `5`      (truthy result)<br>
`nil or 7`      -> `7`      (truthy result)<br>
`8 or false`    -> `8`      (truthy result)<br>
`nil or false`  -> `false`  (falsy result)<br>

The `or` operator will also skip evaluating the right side if the first value it finds it truthy.

<h2>Assignment Operators</h2>

<b>The assignment operators are used to give variables, arrays, tables and fields their values, they
will be discussed in detail later</b>
