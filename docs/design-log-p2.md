# Design Log — Project 2

## Growth factor and amortized cost
Conversation stores messages in an array data_, with size_ which equals the messages stored and capacity_ which is the total amount of messages that can be stored. When the append function is called if the size_ is equal to the capacity_, the array is full. Which causes a new array with double the capacity of the original capacity (or a capacity of 1 if it was empty), then moves the old messages into it, frees the old array with delete[], and stores the new message.
The amortized cost is O(1) because in Conversation's worst case scenario, of having to append to a size_ == capacity_ list it will have O(n), each unit will have to be copied to the new array. However during most cases the complexity of appending is only O(1), it immediately indexes to the last place in the array and appends the new unit. 
This is proven by the formula Amortized cost = total cost/N = (2N-1)/N which is <2 thus O(1).
The reason why the array does not grow by 1 is because every single append would copy all existing messages: 1 + 2 + ... + n, which is about n^2 / 2 copies, or O(n^2). This is incredibly inefficient and would become increasingly worse as it is being used.


## Rule of Five evidence
I implemented the rule of 5 in Conversation owns memory through the raw pointer data_. The compiler's default copy would copy only the pointer, so two objects would free the same memory thus not following the rule of 5’s deep copying. I wrote all five functions to ensure the rule of 5 was followed:
Destructor: delete[] data_
This is one step in preventing memory leaks and orphaned units.
Copy constructor: allocates its own array and copies each message, so the two objects never share memory.
Copy assignment: builds the new copy first, then frees the old array, then switches over, so nothing is lost if allocation fails. It returns early on self-assignment.
Move constructor: takes the other object's pointer, size, and capacity, then sets the other to nullptr, 0, and 0 respectively.
 Nothing is deep copied, deleted, and no new memory is created. 
Move assignment: returns early on self-assignment, frees the old array, steals the values, and zeroes the source.
 Nothing is deep copied, deleted, and no new memory is created. 
Both move functions are noexcept. The new and delete commands only appear in conversation.cpp as per the instructions, and I did not use std::vector. The copies are deep since each element in the right side array is indexed and assigned to the left side array in its corresponding index. The moves only change the ownership of the memory, the old right side array is still usable, it just simply has all its values set to zero since it represents nothing. 



## Sentinel scanner: bounded pending_ proof
Since the sentinel length is equal to 20, let SL =20. Each call to feed runs the command: buf = pending_ + chunk. If the new buf’s contents contain the sentinel stop signal, it returns the text before it and clear pending_. Otherwise, set pending_ to the last min(buf.size(), SL - 1) characters and return everything before them.
Claim: After every feed, pending_.size() <= SL - 1. The variable size_t keep is set to be the smallest between the size of the buf or the size of the (sentinel_ -1). Next size_t safe_len is then set to equal to the difference between the size of buf - keep. If buf was smaller than sentinel_ -1 then the difference is 0. Next pending_ is made to equal the part of the buf string starting at safe_len to the end. Since safe_len is the difference between the size of buf and keep, and keep can be at a max of (sentinel -1) it results in pending equaling at most (sentinel_ -1) characters in the string. 



## What I would change differently
One thing I would do differently would be to test the code in a different environment than Adobe visual studio code. It took me a while to figure out how to test my code and I had a lot of compiler issues, which notably never had an issue with just C. Next time I will use either Clion or Microsoft studio code to run C projects since they are better suited and won’t have issues with libraries like Adobe Visual Studio code did. Another thing I would do differently would be to add comments in for myself later, when writing this report I had to relearn what I had coded in order to write about it. 

