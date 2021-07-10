<h1>Importing files</h1>


Other files can be imported using the `import` keyword. It expects a string name of the file path, an extension is not required if<br>
megascript files are imported (with `.meg` extension), but other file formats are also supported like `.so`/`.dll`, which can be used to execute functions<br>
written in C, which have access to the VM and full megascript api.<br><br>
In case the file is a normal megascript text file, it is compiled and executed, and the global environment of the file is then copied into a `table`. This table is then added<br>
to the global environment of the current script with the name being the name of the file.<br>
<br>
Individual functions and global methods/classes can be then accessed through the global variable.
```
// test.meg
global func add(args...):
  var total = 0
  for i, v in args:
    total += v
  end
  return total
end 
```
```
// main.meg

import "test"
print(test.add(10, 20, 30))
```
Something to be noted is that only global variables/global functions/global classes are accessible. This also allows file wide encapsulation of certain functions.
<br>

<h3> Importing Dynamic Link Libraries </h3>

These can be included directly using the `import` keyword. 
Functions then have to be queried using the `query()` method
found in the DLL Object inserted into the global environment.
<br>

The file also needs to be closed when not needed, when the DLL object is being garbage collected and it is found to be open, a warning is generated to inform you that it has to be closed.
```
import "core/http.so" 

var returned = http.query("getRequest")("http://8.8.8.8")
http.close() 
```


[previous](/docs/looping.md) | [next](/docs/globals.md) | [index](/docs/documentation.md)
