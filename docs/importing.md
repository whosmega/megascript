<h1>Importing files</h1>

<b>Please note that this feature is a work in progress</b>

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
Individual functions/variables/classes can also be imported, which are inserted into the global environment using the `from` keyword.
```
from "test" import "add"

print(add(1, 2))
```

[previous](/docs/looping.md) | [next](/docs/builtin.md) | [index](/docs/documentation.md)
