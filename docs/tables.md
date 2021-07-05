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

<h2>Bound methods</h2>
All table objects have the following methods bound to them:
1. `table.keys()`:
    This method returns all the keys of the table in a single array

[previous](/docs/arrays.md) | [next](/docs/) | [index](/docs/documentation.md)
