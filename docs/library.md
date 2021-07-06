<h1>Library</h1>

Megascript currently lacks a proper standard library, but it is a work in progress.

`json` : A json parser and stringifier is provided which can parse json and convert megascript tables and arrays.
```
import "json"

print(json.parse('{"a" : 50}')["a"])    // 50
print(json.stringify([1, 2, 3])
```

[previous](/docs/globals.md) | [index](/docs/documentation.md)
