func test_or_and():
    if (5 and 4 or 3) != 4:
         return false
    end
    if !(false or 4):
        return false
    end
    if !(5 and 6):
        return false
    end
    return true
end

func test_lvars():
    if true:
        var a = 100
    end
    if a:
        return false
    end

    var a = 100
    if !a:
        return false
    end
    return true
end

func test_lfunc():
     var a = 100
     func b():
         if a:
              return false
         end
         return true
     end
     if !b():
         return false
     end
     if !a:
         return false
     end
     return true
end

func test_array():
     var array = [1, 2]
     if array[0] != 1 or array[1] != 2:
        return false
     end
     
     array[1] = 6
     array[0] = 10
  
     if array[0] != 10 or array[1] != 6:
         return false
    end

    if #[1..10] != 10:
        return false
    end
    return true
end

func test_while():
    var counter = 0
    while counter != 10:
        counter += 1
    end

    if counter != 10:
        return false 
    end
    return true
end

func test_for():
    var counter = 0
    var first = nil
    var last = nil
    var array = [10..20]
    for i, v in array:
        if i == 0: first = v end
        if i == #array - 1: last = v end
        counter += 1
    end

    if counter != #array:
        return false
    end
    if first != 10 or last != 20:
         return false
    end

   counter = 0
   for i in 1, 100:
        counter += 1
   end
   if counter != 100:
       return false
   end
   return true
end

var tests = [
    test_or_and,
    test_lvars,
    test_lfunc,
    test_array,
    test_while,
    test_for
]

for i, v in tests:
    var output = v()
    print("Running test " + str(v) + " [" + str(i+1) + "/" + str(#tests) + "]: " + (output and "Success" or "Failed"))
end
