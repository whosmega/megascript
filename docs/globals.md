<h1>Globals</h1>

Megascript offers some global functions which are built-in.

1. `print(args...) -> nil`
This is the print function, it prints all the arguments given to it to the `stdout`.

2. `str(value) -> string`
This function takes a single argument of any datatype, converts it to a suitable string and returns it.
```
print(str("12"))  
```

3. `num(string) -> number | nil`
This function takes a single argument of string type, and converts it to a number only if the string is a suitable double precision floating point number<br>
```
print(num("43.2")) // 43.2
print(num("abc")) // nil
```

4. `clock() -> number`
This function returns the time elapsed since the execution of the program started, it can be used for benchmarks.

5. `type(value) -> string`
This function returns the datatype of any value passed to it as a string
```
print(type(func() end))  // "function"
```

6. `input(string) -> string`
This function takes input from the `stdin` and returns it as a string. An optional string can be passed to it for initial input text  

[previous](/docs/importing.md) | [next](/docs/library.md) | [index](/docs/documentation.md)
