<h1>Datatypes</h1>

Megascript has the following native datatypes:

1. `number`
2. `boolean`
3. `nil`

And the following secondary datatypes:

1. `string`
2. `function`
3. `class`
4. `array`
5. `table`
<hr>

<h2>Number</h2>

Numbers in megascript internally are 64-bit double precision floating point numbers.
They can be an integer `43`, a negative number `-323` or a floating point number `3.432211`.
Numbers support all common binary operators, and they are truthy values, `0` unlike in other programming languages is <i>not</i> falsey. 

<h2>Boolean</h2>

Boolean values include `true` and `false`, `false` being falsey of course.

<h2>Nil</h2>

In megascript, the datatype representing 'no value' is `nil`, it is falsey.

<h2>String</h2>

Strings are pieces of text which represent one single string value, and are of datatype `string`.
They can be enclosed in double quotes `"This is a string"` or single quotes `'This is also a string'`

Strings support the following escape sequences:
1. `\n` : The newline character
2. `\t` : The tab character
3. `\b` : The backspace character 
4. `\"` : An escaped double quote 
5. `\'` : An escaped single quote
6. `\\` : An escaped backslash character 

Escape sequences can be used within strings when special behaviour is necessary, for example adding newline in a string `"Hello\nWorld"`
and then printing it will result in the escape character being treated as a newline
```
Hello
World
```

<h3>String Bound Methods</h3>
Following are the methods bound to every string object 

1. `string.capture(start, end)` : Captures characters from a given string, and returns them in a new string.<br>
Example:
```
"foobar".capture(0, 2)      // = "foo"
```

2. `string.getAscii()` : Returns the ascii code of the characters in the string.<br>
If the string is only a single character, direct number is returned, otherwise the ascii code of every character is filled into an array in order and returned.
Example:
```
"a".getAscii()        // = 97 
```
<h4> Functions, Classes, Arrays and Tables will be discussed later on, but each have a corresponding datatype of their own </h4>

[next](/docs/operators.md) | [index](/docs/documentation.md)
