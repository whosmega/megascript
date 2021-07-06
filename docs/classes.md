<h1>Classes</h1>

Megascript supports Object Oriented Programming natively by including classes.
They can be declared like so:
```
class Car:

end
```

Field declarations are allowed within class declarations, which are just inital values of fields.
```
class Car:
  fuel = 100
end
```

Methods can also be declared, which is just the function syntax but within a class declaration.
Megascript follows the class calling syntax for making new instances, like Python.
`self` is auto defined to the the instance the method is operating on.
```
class Car:
  fuel = 0
  
  func refill():
    self.fuel = 100
  end
end 

car = Car()
car.refill()
```
Class methods can normally accept input when calling through normal parameter-argument syntax.

<h4>Inheritance</h4>

Inheritance can be done using the `inherit` keyword. 
```
class A:
  func method():
    
  end 
end 

class B inherits A: end 

var instance = B()
instance.method()
```
<h4>Method Overriding</h4>

Methods can be overriden by simply defining it in the class which is inheriting.
```
class A:
  func method():
    print("A")
  end 
end 

class B inherits A:
  func method():
    print("B")
  end
end

B().method()        // "B"
```

<h4>The super keyword</h4>

Megascript allows accessing methods and fields from a class's superclass, through the `super` keyword.
The `super` keyword is lexically scoped and always operates relatively. 
For example,
```
class A:
  func method():
    return "A"
  end 
end 

class B inherits A:
  func method():
    return "B"
  end
  func test():
    return super.method()
  end
end

class C inherits B: end 
print(C().test())       
```
Since `super` resolves the superclass relative to the method it was defined in, this will print "A" and not "B"
`super` can be used to access fields aswell:
```
class A:
  field = 100
end 

class B inherits A: 
  func test():
    return super.field 
  end 
end

print(B().test())       // 100
```
`super` and `self` are reserved keywords and 'special variables' that cannot be used normally by users. `super` cannot be accessed individually. 

<h4>Special Methods</h4>

Some special methods are defined in classes which are automatically executed when certain things happen. 

1. `_init` : This method when defined is automatically called with the arguments passed while calling the class.
This method can return normally, but cannot return a specific user defined value. It always internally returns `self`. 
```
class A:
  func _init(a, b, c):
    print(a + b + c)
  end
end

A(10, 20, 30)
```

Classes can also be declared global using the `global` keyword
```
global class Car:
  fuel = 100
end
```

[previous](/docs/tables.md) | [next](/docs/keywords.md) | [index](/docs/documentation.md)
