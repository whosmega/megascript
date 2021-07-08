<h1>Tables</h1>

Tables in megascript are normal key-value pair hash tables.<br>
Given a string key, any value can be accessed from the table.<br>
This is a useful structure for storing data, when data is associated with a string. 
<br>
Tables can be created using the `{}` brackets, with inital values being direct key value pairs separated by commas<br>
```
var table = {"id" = 34, "username" = "mega"}
```
Accessing a key's value and setting it is similar to indexing arrays<br>
```
var table = {"name" = ""}

table["name"] = "mega"
var name = table["name"]        // set to "mega"    
```

Tables can also be used with the dot syntax
```
var table = {}
table.name = "mega"
print(table.name)
```

<h2>Special Keys</h2>

Tables have special keys which when set to a function, get used for special events. 

`table._nokey(table, key)` : This key when set to a function, gets invoked<br>
when a key wasn't found in the table, the arguments passed are the table itself and the<br>
string key. The value returned by this function is the value returned by the unknown key index. 
<br>
`table._nokeycall(table, key, args[])` : This key when set to a function, gets invoked when<br>
a key is invoked directly even when the value itself is `nil`. This is similar to `_nokey` except 
it passes the arguments given to the invoke in an array. The return value of this function is 
also set to the return value of the main invoke<br>

<h2>Bound methods</h2>
All table objects have the following methods bound to them:<br>

1. `table.keys()`:
    This method returns all the keys of the table in a single array

[previous](/docs/arrays.md) | [next](/docs/classes.md) | [index](/docs/documentation.md)
